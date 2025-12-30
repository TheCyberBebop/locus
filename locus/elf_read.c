#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "elf_read.h"
#include "logger.h"

/* Determine host endianness once and cache it. This avoids relying on
 * compiler builtins and keeps the module freestanding/portable. */
static inline bool host_is_little_endian(void) {
    static bool cached;
    static bool initialized;

    if (!initialized) {
        const uint16_t x = 0xBEEF;
        cached = (*((const uint8_t*)&x) == 0xEF);
        initialized = true;
    }

    return cached;
}

/* Returns true if host and file endianness differ (XOR), meaning multi-byte
 * values must be byte-swapped after memcpy. */
static inline bool elf_need_swap(uint8_t ei_data) {
    const bool file_is_little = (ei_data == ELFDATA2LSB);
    const bool host_is_little = host_is_little_endian();
    return file_is_little != host_is_little;  // XOR
}

static inline uint16_t bswap16(uint16_t value) {
    return (uint16_t)((value >> 8) | (value << 8));
}

static inline uint32_t bswap32(uint32_t value) {
    return ((value & 0x000000FFU) << 24) | ((value & 0x0000FF00U) << 8) |
           ((value & 0x00FF0000U) >> 8) | ((value & 0xFF000000U) >> 24);
}

static inline uint64_t bswap64(uint64_t value) {
    return ((value & 0x00000000000000FFULL) << 56) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0xFF00000000000000ULL) >> 56);
}

static inline int validate_read_args(const elf_image_t* img,
                                     const elf_ident_info_t* ident,
                                     void* out,
                                     size_t offset,
                                     size_t width) {
    const char* path = NULL;

    // Validate function parameters
    if (NULL == img || NULL == ident || NULL == out) {
        ERROR("invalid parameter (img=%p ident=%p out=%p)", (void*)img,
              (void*)ident, out);
        return -EINVAL;
    }

    // Validate elf_image_t->base
    if (NULL == img->base) {
        ERROR("invalid elf_image_t->base");
        return -EINVAL;
    }

    path = img->path ? img->path : "(unknown)";  // Not critical

    /* We must know the file endianness to correctly decode multi-byte fields.
     * elf_ident validated EI_DATA, but we defensively re-check here so these
     * primitives remain safe even if called incorrectly. */
    if (ELFDATA2LSB != ident->ei_data && ELFDATA2MSB != ident->ei_data) {
        ERROR("'%s': unsupported ELF data encoding (%" PRIu8 ")", path,
              ident->ei_data);
        return -ENOTSUP;
    }

    /* Bounds check (overflow-safe): ensure [offset, offset+width) is within the
     * file. Handles elf_image_t->size == 0 as well. */
    if (offset > img->size || img->size - offset < width) {
        ERROR("'%s': OOB read (offset=%zu, width=%zu, size=%zu)", path, offset,
              width, img->size);
        return -EINVAL;
    }

    TRACE("validated read: offset=%zu width=%zu within file size=%zu", offset,
          width, img->size);
    return 0;
}

int elf_read_u16(const elf_image_t* img,
                 const elf_ident_info_t* ident,
                 size_t offset,
                 uint16_t* out) {
    int ret = 0;
    uint16_t value = 0;

    TRACE("Entered %s", __func__);

    /* Validate pointers, ELF encoding, and that the full [offset, offset+width)
     * range lies inside the mapped image before we memcpy from img->base. */
    ret = validate_read_args(img, ident, out, offset, sizeof(uint16_t));
    if (0 != ret) {
        return ret;
    }

    /* memcpy keeps reads alignment-safe and avoids strict-aliasing/UB that
     * would come from casting file bytes to integer pointers. */
    memcpy(&value, img->base + offset, sizeof(value));

    // Decode according to ELF file endianness
    const bool swap = elf_need_swap(ident->ei_data);
    TRACE("ELF ei_data=%" PRIu8 ", host_little=%d, swap=%d", ident->ei_data,
          host_is_little_endian(), swap);
    if (swap) {
        value = bswap16(value);
    }

    *out = value;

    TRACE("read u16 @ offset=%zu -> %" PRIu16, offset, *out);
    TRACE("Finished %s", __func__);
    return ret;
}

int elf_read_u32(const elf_image_t* img,
                 const elf_ident_info_t* ident,
                 size_t offset,
                 uint32_t* out) {
    int ret = 0;
    uint32_t value = 0;

    TRACE("Entered %s", __func__);

    /* Validate pointers, ELF encoding, and that the full [offset, offset+width)
     * range lies inside the mapped image before we memcpy from img->base. */
    ret = validate_read_args(img, ident, out, offset, sizeof(uint32_t));
    if (0 != ret) {
        return ret;
    }

    /* memcpy keeps reads alignment-safe and avoids strict-aliasing/UB that
     * would come from casting file bytes to integer pointers. */
    memcpy(&value, img->base + offset, sizeof(value));

    // Decode according to ELF file endianness
    const bool swap = elf_need_swap(ident->ei_data);
    TRACE("ELF ei_data=%" PRIu8 ", host_little=%d, swap=%d", ident->ei_data,
          host_is_little_endian(), swap);
    if (swap) {
        value = bswap32(value);
    }

    *out = value;

    TRACE("read u32 @ offset=%zu -> %" PRIu32, offset, *out);
    TRACE("Finished %s", __func__);
    return ret;
}

int elf_read_u64(const elf_image_t* img,
                 const elf_ident_info_t* ident,
                 size_t offset,
                 uint64_t* out) {
    int ret = 0;
    uint64_t value = 0;

    TRACE("Entered %s", __func__);

    /* Validate pointers, ELF encoding, and that the full [offset, offset+width)
     * range lies inside the mapped image before we memcpy from img->base. */
    ret = validate_read_args(img, ident, out, offset, sizeof(uint64_t));
    if (0 != ret) {
        return ret;
    }

    /* memcpy keeps reads alignment-safe and avoids strict-aliasing/UB that
     * would come from casting file bytes to integer pointers. */
    memcpy(&value, img->base + offset, sizeof(value));

    // Decode according to ELF file endianness
    const bool swap = elf_need_swap(ident->ei_data);
    TRACE("ELF ei_data=%" PRIu8 ", host_little=%d, swap=%d", ident->ei_data,
          host_is_little_endian(), swap);
    if (swap) {
        value = bswap64(value);
    }

    *out = value;

    TRACE("read u64 @ offset=%zu -> %" PRIu64, offset, *out);
    TRACE("Finished %s", __func__);
    return ret;
}
