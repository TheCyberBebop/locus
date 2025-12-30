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
 * @struct elf_image_t
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
typedef struct elf_image {
    const uint8_t* base; /**< Base address of the read-only memory mapping */
    size_t size;         /**< Total size of the mapped file in bytes */
    const char* path;    /**< Borrowed pointer used for diagnostics (must
                              outlive elf_image_t) */
} elf_image_t;

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
 * @brief Unmap and release an ELF image.
 *
 * Releases the memory mapping associated with an ELF image previously
 * opened with elf_image_open(). After this call, the elf_image_t
 * structure is reset to an empty state and must not be used unless
 * reinitialized.
 *
 * @param img Pointer to an initialized elf_image_t structure.
 *
 * @return 0 on success, or a negative errno-style value on failure.
 */
int elf_image_close(elf_image_t* img);

#ifdef __cplusplus
}
#endif

#endif /* ELF_IMAGE_H */
