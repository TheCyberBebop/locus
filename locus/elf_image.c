#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "elf_image.h"
#include "logger.h"

int elf_image_open(const char* path, elf_image_t* img) {
    int fd = -1;
    struct stat st;
    void* addr = NULL;

    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == path || NULL == img) {
        ERROR("invalid function parameter");
        return -EINVAL;
    }

    // Open path read-only; O_CLOEXEC prevents file descriptor leaks across exec
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (-1 == fd) {
        ERROR("open('%s') failed: %s (%d)", path, strerror(errno), errno);
        return -errno;
    }

    // Retrieve file metadata (size, type, permissions, etc.)
    if (fstat(fd, &st) < 0) {
        ERROR("fstat('%s') failed: %s (%d)", path, strerror(errno), errno);
        close(fd);
        return -errno;
    }

    /* Verify file size: must be large enough to contain at least an ELF32
     * header */
    if ((size_t)st.st_size < sizeof(Elf32_Ehdr)) {
        ERROR("'%s' is too small to be an ELF file (cur size=%zu, min req=%zu)",
              path, (size_t)st.st_size, sizeof(Elf32_Ehdr));
        close(fd);
        return -EINVAL;
    }

    /* Map the entire file into memory as a read-only, private mapping. Creates
     * a stable, immutable byte view of the ELF for inspection, without copying
     * data or allowing accidental modification. */
    addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == addr) {
        ERROR("mmap() failed: %s (%d)", strerror(errno), errno);
        close(fd);
        return -errno;
    }

    close(fd);  // File descriptor no longer needed; mmap keeps the file pinned

    // Initialize the ELF image abstraction
    img->base = (const uint8_t*)addr;
    img->size = (size_t)st.st_size;
    img->path = path;

    INFO("Mapped ELF '%s' (size=%zu bytes at %p)", path, img->size, img->base);
    TRACE("Finished %s", __func__);

    return 0;
}

static bool check_magic(const uint8_t* e_ident) {
    return e_ident[EI_MAG0] == ELFMAG0 && e_ident[EI_MAG1] == ELFMAG1 &&
           e_ident[EI_MAG2] == ELFMAG2 && e_ident[EI_MAG3] == ELFMAG3;
}

static const char* get_class_str(uint8_t class) {
    switch (class) {
        case ELFCLASS32:
            return "ELF32";
        case ELFCLASS64:
            return "ELF64";
        default:
            return "UNKNOWN";
    }
}

static const char* get_data_str(uint8_t data) {
    switch (data) {
        case ELFDATA2LSB:
            return "LSB (little-endian)";
        case ELFDATA2MSB:
            return "MSB (big-endian)";
        default:
            return "UNKNOWN";
    }
}

static const char* get_osabi_str(uint8_t osabi) {
    switch (osabi) {
        case ELFOSABI_NONE:
            return "SYSV";
        case ELFOSABI_HPUX:
            return "HP-UX";
        case ELFOSABI_NETBSD:
            return "NetBSD";
        case ELFOSABI_GNU:
            return "GNU/Linux";
        case ELFOSABI_SOLARIS:
            return "Solaris";
        case ELFOSABI_AIX:
            return "AIX";
        case ELFOSABI_IRIX:
            return "IRIX";
        case ELFOSABI_FREEBSD:
            return "FreeBSD";
        case ELFOSABI_TRU64:
            return "TRU64";
        case ELFOSABI_MODESTO:
            return "Novell Modesto";
        case ELFOSABI_OPENBSD:
            return "OpenBSD";
        case ELFOSABI_ARM_AEABI:
            return "ARM EABI";
        case ELFOSABI_ARM:
            return "ARM";
        case ELFOSABI_STANDALONE:
            return "Standalone";
        default:
            return "Other/Unknown";
    }
}

int elf_validate_ident(const elf_image_t* img, elf_ident_info_t* out) {
    uint8_t ei_class = 0;
    uint8_t ei_data = 0;
    uint8_t ei_version = 0;
    const uint8_t* ident = NULL;
    const char* path = NULL;

    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == img || NULL == out) {
        ERROR("invalid function parameter");
        return -EINVAL;
    }

    // Validate elf_image_t base and size
    if (NULL == img->base) {
        ERROR("invalid elf_image_t->base");
        return -EINVAL;
    }

    path = img->path ? img->path : "(unknown)";  // Not critical

    /* Size needs at least EI_NIDENT bytes to safely index e_ident fields.
     * Handles elf_image_t->size == 0 as well. */
    if (img->size < EI_NIDENT) {
        ERROR("'%s': file too small for e_ident (size=%zu, need=%d)", path,
              img->size, EI_NIDENT);
        return -EINVAL;
    }

    ident = img->base;

    // Verify magic number
    if (!check_magic(ident)) {
        ERROR("'%s': not an ELF file (magic=%02" PRIX8 " %02" PRIX8 " %02" PRIX8
              " %02" PRIX8 ")",
              path, ident[EI_MAG0], ident[EI_MAG1], ident[EI_MAG2],
              ident[EI_MAG3]);
        return -EINVAL;
    }

    // Check bitness (32-bit or 64-bit)
    ei_class = ident[EI_CLASS];
    if (ei_class != ELFCLASS32 && ei_class != ELFCLASS64) {
        ERROR("'%s': unsupported ELF ei_class (%" PRIu8 ")", path, ei_class);
        return -ENOTSUP;
    }

    // Check data (i.e., endianness - little or big)
    ei_data = ident[EI_DATA];
    if (ELFDATA2LSB != ei_data && ELFDATA2MSB != ei_data) {
        ERROR("'%s': unsupported ELF ei_data (%" PRIu8 ")", path, ei_data);
        return -ENOTSUP;
    }

    // Check version
    ei_version = ident[EI_VERSION];
    if (EV_CURRENT != ei_version) {
        ERROR("'%s': invalid ELF ei_version (%" PRIu8 ")", path, ei_version);
        return -EINVAL;
    }

    // Populate decoder struct
    out->ei_class = ei_class;
    out->ei_data = ei_data;
    out->ei_version = ei_version;
    out->ei_osabi = ident[EI_OSABI];
    out->ei_abiversion = ident[EI_ABIVERSION];

    INFO("ELF identification: ei_class=%s, ei_data=%s, ei_osabi=%s (%" PRIu8
         "), ei_abiversion=%" PRIu8,
         get_class_str(out->ei_class), get_data_str(out->ei_data),
         get_osabi_str(out->ei_osabi), out->ei_osabi, out->ei_abiversion);
    TRACE("Finished %s", __func__);

    return 0;
}
