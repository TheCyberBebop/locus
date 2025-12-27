/**
 * @file elf_ident.h
 * @brief ELF identification parsing and decoder configuration.
 *
 * This module validates the ELF identification bytes (e_ident) at the
 * beginning of an ELF file and extracts the information required to
 * correctly decode all subsequent ELF headers and tables.
 *
 * The identification fields define:
 *  - ELF class (32-bit or 64-bit)
 *  - byte order (little-endian or big-endian)
 *  - ELF version
 *  - OS ABI and ABI version
 *
 * No ELF structures beyond e_ident are interpreted here. Successful
 * validation establishes the decoding rules that all later parsing
 * stages must follow.
 */

#ifndef ELF_IDENT_H
#define ELF_IDENT_H

#include <elf.h>
#include <stdint.h>

#include "elf_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct elf_ident_info_t
 * @brief Decoder configuration derived from an ELF file's e_ident[] bytes.
 *
 * This structure captures the minimum information needed to interpret an ELF
 * file safely (class and byte order), plus a few identification fields that are
 * useful for logging and diagnostics.
 *
 * Once populated by elf_validate_ident(), higher-level parsing code should use
 * these values to select the correct header layouts (ELF32 vs ELF64) and to
 * decode all multi-byte fields according to the file's endianness.
 */
typedef struct elf_ident_info {
    uint8_t ei_class;      /**< EI_CLASS: ELFCLASS32 or ELFCLASS64 */
    uint8_t ei_data;       /**< EI_DATA:  ELFDATA2LSB or ELFDATA2MSB */
    uint8_t ei_version;    /**< EI_VERSION (expect EV_CURRENT) */
    uint8_t ei_osabi;      /**< EI_OSABI */
    uint8_t ei_abiversion; /**< EI_ABIVERSION */
} elf_ident_info_t;

/**
 * @brief Validate the ELF identification bytes (e_ident) and populate decoder
 * info.
 *
 * Checks:
 *  - ELF magic
 *  - class (ELF32/ELF64)
 *  - endianness (LSB/MSB)
 *  - ident version (EV_CURRENT)
 *
 * On success, fills @p out with the decoder configuration required for all
 * subsequent parsing (class/endianness).
 *
 * @param img Mapped ELF image (read-only byte view).
 * @param out Output decoder info to populate on success.
 *
 * @return 0 on success, or negative errno-style value on failure.
 * @return -EINVAL  malformed / not ELF / too small
 * @return -ENOTSUP valid ELF but unsupported class/encoding
 */
int elf_validate_ident(const elf_image_t* img, elf_ident_info_t* out);

#ifdef __cplusplus
}
#endif

#endif /* ELF_IDENT_H */
