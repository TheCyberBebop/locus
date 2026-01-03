#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "logger.h"

static bool check_magic(const uint8_t* e_ident) {
    return e_ident[EI_MAG0] == ELFMAG0 && e_ident[EI_MAG1] == ELFMAG1 &&
           e_ident[EI_MAG2] == ELFMAG2 && e_ident[EI_MAG3] == ELFMAG3;
}

#if LOG_LEVEL <= LOG_LEVEL_INFO
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
#endif

int elf_validate_ident(const elf_image_t* img, elf_ident_info_t* out) {
    uint8_t ei_class = 0;
    uint8_t ei_data = 0;
    uint8_t ei_version = 0;
    const uint8_t* ident = NULL;

    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == img || NULL == out) {
        ERROR("invalid parameter (img=%p out=%p)", (void*)img, (void*)out);
        return -EINVAL;
    }

    // Validate elf_image_t base and size
    if (NULL == img->base) {
        ERROR("invalid elf_image_t->base");
        return -EINVAL;
    }

#if LOG_LEVEL <= LOG_LEVEL_ERROR
    const char path = img->path ? img->path : "(unknown)";  // Not critical
#endif

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

    INFO("ELF identification: class=%s, data=%s, version=%" PRIu8
         ", osabi=%s (%" PRIu8 "), abiversion=%" PRIu8,
         get_class_str(out->ei_class), get_data_str(out->ei_data),
         out->ei_version, get_osabi_str(out->ei_osabi), out->ei_osabi,
         out->ei_abiversion);
    TRACE("Finished %s", __func__);
    return 0;
}
