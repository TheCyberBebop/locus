/**
 * @file elf_read.h
 * @brief Endian-aware, bounds-checked helpers for reading ELF fields.
 *
 * This module provides low-level primitives for safely reading multi-byte
 * integer values from an ELF image. All reads are:
 *  - bounds-checked against the mapped file size
 *  - alignment-safe (using memcpy)
 *  - decoded according to the ELF file's declared endianness
 *
 * These helpers form the foundation for parsing ELF headers and tables.
 * All multi-byte ELF fields must be read through this interface.
 *
 * All offsets passed to these functions are file offsets (indices into the
 * mapped image), not virtual addresses.
 */

#ifndef ELF_READ_H
#define ELF_READ_H

#include <stddef.h>
#include <stdint.h>

#include "elf_ident.h"
#include "elf_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read a 16-bit unsigned value from an ELF image.
 *
 * Reads a 16-bit value at the specified byte offset within the ELF image,
 * decoding it according to the ELF file's endianness as specified in
 * @ref elf_ident_info_t.
 *
 * The returned value is always converted to host byte order.
 *
 * @param img   Pointer to a valid ELF image view.
 * @param ident Pointer to validated ELF identification information.
 * @param offset Byte offset within the ELF image to read from.
 * @param out   Output pointer to receive the decoded value.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid or the read would exceed bounds.
 */
int elf_read_u16(const elf_image_t* img,
                 const elf_ident_info_t* ident,
                 size_t offset,
                 uint16_t* out);

/**
 * @brief Read a 32-bit unsigned value from an ELF image.
 *
 * Reads a 32-bit value at the specified byte offset within the ELF image,
 * decoding it according to the ELF file's declared endianness.
 *
 * @param img   Pointer to a valid ELF image view.
 * @param ident Pointer to validated ELF identification information.
 * @param offset Byte offset within the ELF image to read from.
 * @param out   Output pointer to receive the decoded value.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid or the read would exceed bounds.
 */
int elf_read_u32(const elf_image_t* img,
                 const elf_ident_info_t* ident,
                 size_t offset,
                 uint32_t* out);

/**
 * @brief Read a 64-bit unsigned value from an ELF image.
 *
 * Reads a 64-bit value at the specified byte offset within the ELF image,
 * decoding it according to the ELF file's declared endianness.
 *
 * @param img   Pointer to a valid ELF image view.
 * @param ident Pointer to validated ELF identification information.
 * @param offset Byte offset within the ELF image to read from.
 * @param out   Output pointer to receive the decoded value.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid or the read would exceed bounds.
 */
int elf_read_u64(const elf_image_t* img,
                 const elf_ident_info_t* ident,
                 size_t offset,
                 uint64_t* out);

/* Helper macro: call elf_read_* and return on failure. Scoped to this file
 * only; undefined after use. */
#define READ_OR_RETURN(fn, img, ident, off, dst)      \
    do {                                              \
        int _rc = (fn)((img), (ident), (off), (dst)); \
        if (_rc != 0) {                               \
            return _rc;                               \
        }                                             \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* ELF_READ_H */
