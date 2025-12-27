#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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
    int saved = 0;
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
        saved = errno;
        close(fd);
        return -saved;
    }

    // Verify path is a regular file (i.e., not a directory, etc.)
    if (!S_ISREG(st.st_mode)) {
        ERROR("'%s' is not a regular file", path);
        close(fd);
        return -EINVAL;
    }

    /* Verify file size: must be large enough to contain at least an ELF32
     * header */
    if (st.st_size <= 0) {
        ERROR("'%s' has invalid file size (%" PRIdMAX ")", path,
              (intmax_t)st.st_size);
        close(fd);
        return -EINVAL;
    }

    if ((size_t)st.st_size < sizeof(Elf32_Ehdr)) {
        ERROR(
            "'%s' is too small to be an ELF file (size=%zu bytes, minimum=%zu "
            "bytes)",
            path, (size_t)st.st_size, sizeof(Elf32_Ehdr));
        close(fd);
        return -EINVAL;
    }

    /* Map the entire file into memory as a read-only, private mapping. Creates
     * a stable, immutable byte view of the ELF for inspection, without copying
     * data or allowing accidental modification. */
    addr = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == addr) {
        ERROR("mmap() failed: %s (%d)", strerror(errno), errno);
        saved = errno;
        close(fd);
        return -saved;
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

int elf_image_close(elf_image_t* img) {
    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == img) {
        ERROR("invalid function parameter");
        return -EINVAL;
    }

    /* If the image is currently mapped, unmap it and reset the structure.
     * This function is safe to call multiple times on the same elf_image_t. */
    if (NULL != img->base && img->size > 0) {
        if (munmap((void*)img->base, img->size) < 0) {
            ERROR("munmap() failed: %s (%d)", strerror(errno), errno);
            return -errno;
        }
    }

    // Clear fields to prevent accidental reuse
    img->base = NULL;
    img->size = 0;
    img->path = NULL;

    TRACE("Finished %s", __func__);
    return 0;
}
