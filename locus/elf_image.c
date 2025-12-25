#include <elf.h>
#include <errno.h>
#include <fcntl.h>
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
        ERROR("invalid parameter passed in");
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
        ERROR("fstat() failed: %s (%d)", strerror(errno), errno);
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
