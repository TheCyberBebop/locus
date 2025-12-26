/**
 * @file elf_image.h
 *
 * @brief Read-only ELF file image abstraction.
 *
 * This module defines the lowest-level ELF inspection primitive used by LOCUS.
 * It provides a stable, immutable view of an ELF file mapped into memory, but
 * performs no parsing or semantic validation.
 *
 * The elf_image abstraction establishes the trusted byte range against which
 * all subsequent ELF validation is performed. Higher-level components must
 * treat the mapped bytes as untrusted input and validate all offsets and sizes
 * before dereferencing.
 *
 * Design principles:
 * - Read-only, private memory mapping
 * - No implicit validation or parsing
 * - Clear separation between raw bytes and ELF semantics
 * - Defensive, bounds-checked access at all higher layers
 *
 * This mirrors the Linux kernel’s approach of separating file-backed memory
 * acquisition from ELF format interpretation.
 */

#ifndef ELF_IMAGE_H
#define ELF_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @struct elf_image_t
 *
 * @brief Read-only view of an ELF file mapped into memory.
 *
 * This structure represents a stable, immutable byte range backed by a
 * memory-mapped ELF file. It contains no parsed state and performs no
 * validation on its own.
 *
 * All ELF parsing and validation logic operates relative to the byte range
 * defined by @p base and @p size. Defensive bounds checking must ensure that
 * all accessed offsets remain within this range.
 */
typedef struct {
    const uint8_t* base; /**< Base address of the read-only memory mapping */
    size_t size;         /**< Total size of the mapped file in bytes */
    const char* path;    /**< File path, used for logging and diagnostics */
} elf_image_t;

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
typedef struct {
    uint8_t ei_class;      /**< EI_CLASS: ELFCLASS32 or ELFCLASS64 */
    uint8_t ei_data;       /**< EI_DATA:  ELFDATA2LSB or ELFDATA2MSB */
    uint8_t ei_version;    /**< EI_VERSION (expect EV_CURRENT) */
    uint8_t ei_osabi;      /**< EI_OSABI */
    uint8_t ei_abiversion; /**< EI_ABIVERSION */
} elf_ident_info_t;

/**
 * @brief Open and memory-map an ELF file for read-only inspection.
 *
 * This function opens the file at @p path, obtains its size, and creates a
 * private, read-only memory mapping covering the entire file. The resulting
 * mapping is stored in @p img and provides a stable byte view for subsequent
 * ELF parsing and validation.
 *
 * The file descriptor is closed after mapping; the mapping remains valid
 * for the lifetime of the process or until explicitly unmapped.
 *
 * @param path Path to the ELF file to open.
 * @param img  Pointer to an @ref elf_image_t structure to initialize.
 *
 * @return 0 on success, or a negative errno-style value on failure.
 */
int elf_image_open(const char* path, elf_image_t* img);

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
 *         -EINVAL  malformed / not ELF / too small
 *         -ENOTSUP valid ELF but unsupported class/encoding
 */
int elf_validate_ident(const elf_image_t* img, elf_ident_info_t* out);

#ifdef __cplusplus
}
#endif

#endif /* ELF_IMAGE_H */
