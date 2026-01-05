/**
 * @file elf_ehdr.h
 * @brief Parse and validate the ELF file header (Elf32_Ehdr / Elf64_Ehdr)
 * without struct casting.
 *
 * This module decodes the ELF header fields that follow @c e_ident[] using the
 * endian-aware, bounds-checked @c elf_read_u* helpers.
 *
 * Design goals:
 * - Never cast the mapped file to an ELF struct.
 * - Bounds-check all reads.
 * - Keep parsing class-neutral: represent offsets/addresses as 64-bit values
 *   even when reading an ELF32 header.
 *
 * Non-goals (at this stage):
 * - No program-header iteration
 * - No segment mapping
 * - No relocations or dynamic linking
 * - No jumping to @c e_entry
 */

/**
 * @defgroup elf_ehdr ELF Header Parsing
 * @brief Parse and validate the ELF file header (Ehdr).
 *
 * This module parses the fields of @c Elf32_Ehdr / @c Elf64_Ehdr (after
 * @c e_ident[]) into a class-neutral representation, using safe, endian-aware
 * reads.
 *
 * Typical users:
 * - Loader front-end that needs @c e_phoff / @c e_phnum to locate the Program
 *   Header Table (PHT).
 *
 * @ingroup locus
 * @{
 */

#ifndef ELF_EHDR_H
#define ELF_EHDR_H

#include <stddef.h>
#include <stdint.h>

#include "elf_ident.h"
#include "elf_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Class-neutral, parsed view of the ELF file header.
 *
 * This struct stores the decoded values from either an ELF32 or ELF64 header.
 *
 * Note on width:
 * - In ELF32, fields like @c e_entry, @c e_phoff, and @c e_shoff are 32-bit.
 * - In ELF64, they are 64-bit.
 *
 * LOCUS stores these fields as @c uint64_t unconditionally so later code can
 * operate on a single representation without duplicating logic for 32/64-bit
 * variants. When parsing an ELF32 header, the 32-bit values are zero-extended
 * into these 64-bit fields.
 */
typedef struct elf_ehdr_parsed {
    /** @name Values derived from e_ident[] (decoder configuration) */
    /**@{*/
    uint8_t ei_class;      /**< @c EI_CLASS: ELFCLASS32 or ELFCLASS64. */
    uint8_t ei_data;       /**< @c EI_DATA: ELFDATA2LSB or ELFDATA2MSB. */
    uint8_t ei_osabi;      /**< @c EI_OSABI: target OS/ABI identification. */
    uint8_t ei_abiversion; /**< @c EI_ABIVERSION: ABI version for the OSABI. */
    /**@}*/

    /** @name Core ELF header fields (decoded from the file header) */
    /**@{*/
    uint16_t e_type;    /**< Object file type (e.g., ET_EXEC, ET_DYN). */
    uint16_t e_machine; /**< Machine architecture (e.g., EM_X86_64). */
    uint32_t e_version; /**< ELF version; should be EV_CURRENT. */
    uint64_t
        e_entry; /**< Entry point virtual address (meaningful after mapping). */
    uint64_t e_phoff;     /**< File offset of the Program Header Table (PHT). */
    uint64_t e_shoff;     /**< File offset of the Section Header Table (SHT). */
    uint32_t e_flags;     /**< Architecture-specific flags. */
    uint16_t e_ehsize;    /**< ELF header size in bytes (class-dependent). */
    uint16_t e_phentsize; /**< Size in bytes of one program header entry. */
    uint16_t e_phnum;     /**< Number of program header entries. */
    uint16_t e_shentsize; /**< Size in bytes of one section header entry. */
    uint16_t e_shnum;     /**< Number of section header entries. */
    uint16_t e_shstrndx;  /**< Section header index of the section-name string
                             table. */
    /**@}*/
} elf_ehdr_parsed_t;

/**
 * @brief Parse the ELF header (after e_ident) into a class-neutral
 * representation.
 *
 * This function reads the remaining ELF header fields using
 * @c elf_read_u16/u32/u64, honoring the class and endianness provided by
 * @p ident.
 *
 * The function does not iterate program headers or section headers; it only
 * decodes the ELF header fields themselves.
 *
 * @param img   Opened ELF image (memory-mapped file).
 * @param ident Validated ELF identification info (class/endian/OSABI).
 * @param out   Output structure to receive parsed header fields.
 *
 * @return 0 on success, negative errno-style value on failure.
 */
int elf_ehdr_parse(const elf_image_t* img,
                   const elf_ident_info_t* ident,
                   elf_ehdr_parsed_t* out);

#ifdef __cplusplus
}
#endif

#endif /* ELF_EHDR_H */

/** @} */ /* end of group elf_ehdr */
