#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "elf_ehdr.h"
#include "elf_ident.h"
#include "elf_image.h"
#include "elf_read.h"
#include "logger.h"

/* ELF ABI-defined structure sizes (bytes). */
#define ELF32_EHDR_SIZE 52u
#define ELF64_EHDR_SIZE 64u
#define ELF32_PHDR_SIZE 32u
#define ELF64_PHDR_SIZE 56u
#define ELF32_SHDR_SIZE 40u
#define ELF64_SHDR_SIZE 64u
#define ELF32_SH_SIZE_OFFSET 20u
#define ELF64_SH_SIZE_OFFSET 32u
#define ELF32_SH_LINK_OFFSET 24u
#define ELF64_SH_LINK_OFFSET 40u

/* ELF header field offsets (absolute, from file start).
 *
 * Intentionally avoids casting the mapped file to Elf{32,64}_Ehdr. Instead,
 * each field is read at its specified offset using elf_read_u*(), which
 * enforces bounds checks and correct endianness.
 */
enum {
    /* After e_ident[16] */
    OFF_E_TYPE = 0x10,
    OFF_E_MACHINE = 0x12,
    OFF_E_VERSION = 0x14,
    OFF_E_ENTRY = 0x18,
    /* 32-bit */
    OFF32_E_PHOFF = 0x1C,
    OFF32_E_SHOFF = 0x20,
    OFF32_E_FLAGS = 0x24,
    OFF32_E_EHSIZE = 0x28,
    OFF32_E_PHENTSIZE = 0x2A,
    OFF32_E_PHNUM = 0x2C,
    OFF32_E_SHENTSIZE = 0x2E,
    OFF32_E_SHNUM = 0x30,
    OFF32_E_SHSTRNDX = 0x32,
    /* 64-bit */
    OFF64_E_PHOFF = 0x20,
    OFF64_E_SHOFF = 0x28,
    OFF64_E_FLAGS = 0x30,
    OFF64_E_EHSIZE = 0x34,
    OFF64_E_PHENTSIZE = 0x36,
    OFF64_E_PHNUM = 0x38,
    OFF64_E_SHENTSIZE = 0x3A,
    OFF64_E_SHNUM = 0x3C,
    OFF64_E_SHSTRNDX = 0x3E,
};

/* Helper macro: call elf_read_* and return on failure. Scoped to this file
 * only; undefined after use. */
#define READ_OR_RETURN(fn, img, ident, off, dst)      \
    do {                                              \
        int _rc = (fn)((img), (ident), (off), (dst)); \
        if (_rc != 0) {                               \
            return _rc;                               \
        }                                             \
    } while (0)

int elf_ehdr_parse(const elf_image_t* img,
                   const elf_ident_info_t* ident,
                   elf_ehdr_parsed_t* out) {
    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == img || NULL == ident || NULL == out) {
        ERROR("Invalid parameter (img=%p ident=%p out=%p)", (void*)img,
              (void*)ident, (void*)out);
        return -EINVAL;
    }

    const char* path = img->path ? img->path : "(unknown)";  // Not critical
    (void)path;

    // Verify ident was properly populated and contains supported values
    if (ELFCLASS32 != ident->ei_class && ELFCLASS64 != ident->ei_class) {
        ERROR("'%s': unsupported ELF ei_class (%" PRIu8 ")", path,
              ident->ei_class);
        return -ENOTSUP;
    }
    if (ident->ei_data != ELFDATA2LSB && ident->ei_data != ELFDATA2MSB) {
        ERROR("'%s': unsupported ELF ei_data (%" PRIu8 ")", path,
              ident->ei_data);
        return -ENOTSUP;
    }

    memset(out, 0, sizeof(*out));

    // Copy previously e_ident-derived identoder configuration (elf_ident.c)
    out->ei_class = ident->ei_class;
    out->ei_data = ident->ei_data;
    out->ei_osabi = ident->ei_osabi;
    out->ei_abiversion = ident->ei_abiversion;

    READ_OR_RETURN(elf_read_u16, img, ident, OFF_E_TYPE, &out->e_type);
    READ_OR_RETURN(elf_read_u16, img, ident, OFF_E_MACHINE, &out->e_machine);
    READ_OR_RETURN(elf_read_u32, img, ident, OFF_E_VERSION, &out->e_version);

    if (ELFCLASS32 == ident->ei_class) {
        /* Zero-extended 32-bit results from e_entry, e_phoff, and e_shoff to
         * the appropriate elf_ehdr_parsed_t 64-bit fields. */
        uint32_t temp_u32 = 0;
        READ_OR_RETURN(elf_read_u32, img, ident, OFF_E_ENTRY, &temp_u32);
        out->e_entry = (uint64_t)temp_u32;
        READ_OR_RETURN(elf_read_u32, img, ident, OFF32_E_PHOFF, &temp_u32);
        out->e_phoff = (uint64_t)temp_u32;
        READ_OR_RETURN(elf_read_u32, img, ident, OFF32_E_SHOFF, &temp_u32);
        out->e_shoff = (uint64_t)temp_u32;

        READ_OR_RETURN(elf_read_u32, img, ident, OFF32_E_FLAGS, &out->e_flags);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF32_E_EHSIZE,
                       &out->e_ehsize);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF32_E_PHENTSIZE,
                       &out->e_phentsize);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF32_E_PHNUM, &out->e_phnum);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF32_E_SHENTSIZE,
                       &out->e_shentsize);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF32_E_SHNUM, &out->e_shnum);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF32_E_SHSTRNDX,
                       &out->e_shstrndx);
    } else if (ELFCLASS64 == ident->ei_class) {
        READ_OR_RETURN(elf_read_u64, img, ident, OFF_E_ENTRY, &out->e_entry);
        READ_OR_RETURN(elf_read_u64, img, ident, OFF64_E_PHOFF, &out->e_phoff);
        READ_OR_RETURN(elf_read_u64, img, ident, OFF64_E_SHOFF, &out->e_shoff);
        READ_OR_RETURN(elf_read_u32, img, ident, OFF64_E_FLAGS, &out->e_flags);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF64_E_EHSIZE,
                       &out->e_ehsize);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF64_E_PHENTSIZE,
                       &out->e_phentsize);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF64_E_PHNUM, &out->e_phnum);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF64_E_SHENTSIZE,
                       &out->e_shentsize);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF64_E_SHNUM, &out->e_shnum);
        READ_OR_RETURN(elf_read_u16, img, ident, OFF64_E_SHSTRNDX,
                       &out->e_shstrndx);
    } else {
        ERROR("'%s': invalid ELF class: %" PRIu8, path, ident->ei_class);
        return -EINVAL;
    }

    TRACE("Finished %s", __func__);
    return 0;
}

#undef READ_OR_RETURN

/* Overflow-safe check: verifies [offset, offset + num * entsize) fits in file.
 */
static int byte_range_fits_file(uint64_t file_size,
                                uint64_t offset,
                                uint64_t num,
                                uint64_t entsize) {
    TRACE("Entered %s", __func__);

    // Zero entries means an empty table; always valid
    if (0 == num) {
        TRACE("Finished %s", __func__);
        return 0;
    }

    /* entsize must be non-zero to avoid division by zero and invalid byte
     * ranges. */
    if (0 == entsize) {
        return -EINVAL;
    }

    // Check multiplication overflow: num * entsize
    if (num > UINT64_MAX / entsize) {
        return -EINVAL;
    }

    const uint64_t size = num * entsize;

    // Check addition overflow: offset + (num * entsize)
    if (offset > UINT64_MAX - size) {
        return -EINVAL;
    }

    // Final bounds check against file size
    if (offset + size > file_size) {
        return -EINVAL;
    }

    TRACE("Finished %s", __func__);
    return 0;
}

/* Resolve extended section numbering (SHN_XINDEX) if indicated by the ELF
 * header.
 *
 * ELF may use "extended section numbering" when 16-bit fields in the ELF header
 * are insufficient:
 * - If e_shnum == 0, the real section count may be stored in SHDR[0].sh_size.
 * - If e_shstrndx == SHN_XINDEX, the real shstrndx may be stored in
 *   SHDR[0].sh_link.
 *
 * NOTE: e_shnum == 0 is also used by some ET_EXEC/ET_DYN binaries to indicate
 * "no section header table". This helper only attempts extended resolution when
 * the header indicates it (via the sentinel values) and the SHT is present.
 *
 * On success, returns effective values to use for subsequent validation. This
 * function does not modify *ehdr.
 */
static int resolve_extended_section_numbering(const elf_image_t* img,
                                              const elf_ehdr_parsed_t* ehdr,
                                              uint64_t* effective_shnum,
                                              uint64_t* effective_shstrndx,
                                              uint16_t expected_shentsize) {
    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == img || NULL == ehdr || NULL == effective_shnum ||
        NULL == effective_shstrndx) {
        return -EINVAL;
    }

    int rc = 0;
    uint64_t shnum_ext = (uint64_t)ehdr->e_shnum;
    uint64_t shstrndx_ext = (uint64_t)ehdr->e_shstrndx;
    const uint64_t file_size = (uint64_t)img->size;
    const bool need_ext_shnum = (0 == ehdr->e_shnum);
    const bool need_ext_shstrndx = (SHN_XINDEX == ehdr->e_shstrndx);
    const char* path = (img->path != NULL) ? img->path : "(unknown)";
    (void)path;

    // Header contains the effective values directly; so return success
    if (false == need_ext_shnum && false == need_ext_shstrndx) {
        *effective_shnum = shnum_ext;
        *effective_shstrndx = shstrndx_ext;
        TRACE("Finished %s", __func__);
        return 0;
    }

    /* Extended section header numbering (validated before this) requires the
     * section header table to exist so we can read SHDR[0]. */
    if (0 == ehdr->e_shoff) {
        ERROR("Extended section header numbering identified, but e_shoff==0");
        return -EINVAL;
    }

    /* e_shentsize must match the class ABI size; otherwise we can't locate
     * fields reliably. */
    if (expected_shentsize != ehdr->e_shentsize) {
        ERROR("'%s': e_shentsize mismatch detected: %" PRIu16
              ", (expected %" PRIu16 ")",
              path, ehdr->e_shentsize, expected_shentsize);
        return -EINVAL;
    }

    // Ensure SHDR[0] fits in file
    if (0 != byte_range_fits_file(file_size, ehdr->e_shoff, 1,
                                  (uint64_t)expected_shentsize)) {
        ERROR("'%s': SHDR[0] does not fit in file (shoff=0x%" PRIx64
              " shentsize=%" PRIu16 ")",
              path, ehdr->e_shoff, expected_shentsize);
        return -EINVAL;
    }

    /* Build a minimal decoder config for elf_read_u*().
     * NOTE: Currently only ei_data is the only field required. */
    elf_ident_info_t ident;
    memset(&ident, 0, sizeof(ident));
    ident.ei_class = ehdr->ei_class;
    ident.ei_data = ehdr->ei_data;
    ident.ei_osabi = ehdr->ei_osabi;
    ident.ei_abiversion = ehdr->ei_abiversion;

    if (ELFCLASS32 == ehdr->ei_class) {
        uint32_t v32 = 0;

        if (need_ext_shnum) {
            uint64_t off = ehdr->e_shoff + ELF32_SH_SIZE_OFFSET;

            if (off > (uint64_t)SIZE_MAX) {
                ERROR(
                    "'%s': section header offset overflows size_t: 0x%" PRIx64,
                    path, off);
                return -EOVERFLOW;
            }

            // Real section count stored in SHDR[0].sh_size
            rc = elf_read_u32(img, &ident, (size_t)off, &v32);
            if (0 != rc) {
                return rc;
            }
            shnum_ext = (uint64_t)v32;
        }

        if (need_ext_shstrndx) {
            // Real shstrndx stored in SHDR[0].sh_link
            rc = elf_read_u32(img, &ident,
                              (size_t)(ehdr->e_shoff + ELF32_SH_LINK_OFFSET),
                              &v32);
            if (0 != rc) {
                return rc;
            }
            shstrndx_ext = (uint64_t)v32;
        }
    } else if (ELFCLASS64 == ehdr->ei_class) {
        if (need_ext_shnum) {
            uint64_t v64 = 0;
            uint64_t off = ehdr->e_shoff + ELF64_SH_SIZE_OFFSET;

            if (off > (uint64_t)SIZE_MAX) {
                ERROR(
                    "'%s': section header offset overflows size_t: 0x%" PRIx64,
                    path, off);
                return -EOVERFLOW;
            }

            // Real section count stored in SHDR[0].sh_size
            rc = elf_read_u64(img, &ident, (size_t)off, &v64);
            if (0 != rc) {
                return rc;
            }
            shnum_ext = v64;
        }

        if (need_ext_shstrndx) {
            uint32_t v32 = 0;

            // Real shstrndx stored in SHDR[0].sh_link
            rc = elf_read_u32(img, &ident,
                              (size_t)(ehdr->e_shoff + ELF64_SH_LINK_OFFSET),
                              &v32);
            if (0 != rc) {
                return rc;
            }
            shstrndx_ext = (uint64_t)v32;
        }
    } else {
        ERROR("'%s': invalid ELF class: %" PRIu8, path, ehdr->ei_class);
        return -EINVAL;
    }

    *effective_shnum = shnum_ext;
    *effective_shstrndx = shstrndx_ext;
    TRACE("Finished %s", __func__);
    return 0;
}

int elf_ehdr_validate(const elf_image_t* img, const elf_ehdr_parsed_t* ehdr) {
    uint16_t expected_ehsize = 0;
    uint16_t expected_phentsize = 0;
    uint16_t expected_shentsize = 0;
    uint64_t shnum_ext = 0;
    uint64_t shstrndx_ext = 0;

    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == img || NULL == ehdr) {
        ERROR("Invalid parameter (img=%p ehdr=%p)", (void*)img, (void*)ehdr);
        return -EINVAL;
    }

    const char* path = img->path ? img->path : "(unknown)";  // Not critical
    (void)path;

    // 1) Verify e_version (must be EV_CURRENT for valid ELF)
    if (EV_CURRENT != ehdr->e_version) {
        ERROR("'%s': invalid e_version (%" PRIu32 ")", path, ehdr->e_version);
        return -EINVAL;
    }

    /* 2) Object type policy:
     * - ET_EXEC / ET_DYN: load-path candidates
     * - ET_REL: supported for analysis / future section-based object loading
     */
    if (ET_REL != ehdr->e_type && ET_EXEC != ehdr->e_type &&
        ET_DYN != ehdr->e_type) {
        ERROR("'%s': unsupported e_type (%" PRIu16 ")", path, ehdr->e_type);
        return -ENOTSUP;
    }

    // 3) Class-specific expected sizes (ELF ABI constants)
    if (ELFCLASS32 == ehdr->ei_class) {
        expected_ehsize = ELF32_EHDR_SIZE;
        expected_phentsize = ELF32_PHDR_SIZE;
        expected_shentsize = ELF32_SHDR_SIZE;
    } else if (ELFCLASS64 == ehdr->ei_class) {
        expected_ehsize = ELF64_EHDR_SIZE;
        expected_phentsize = ELF64_PHDR_SIZE;
        expected_shentsize = ELF64_SHDR_SIZE;
    } else {
        ERROR("'%s': unsupported ei_class: (%" PRIu8 ")", path, ehdr->ei_class);
        return -ENOTSUP;
    }

    // 4) Verify ELF header size matches ABI expectations for the class
    if (expected_ehsize != ehdr->e_ehsize) {
        ERROR("'%s': e_ehsize mismatch detected: %" PRIu16
              ", (expected %" PRIu16 ")",
              path, ehdr->e_ehsize, expected_ehsize);
        return -EINVAL;
    }

    const uint64_t file_size = (uint64_t)img->size;

    /* 5) Program Header Table (PHT) validation.
     *
     * ET_REL commonly has no PHT (e_phnum == 0, e_phoff == 0).
     * For ET_EXEC/ET_DYN, PHT is typically present, but we keep validation
     * generic: if phnum == 0, allow it as long as phoff is in-bounds.
     */
    if (0 == ehdr->e_phnum) {
        if (ehdr->e_phoff > file_size) {
            ERROR(
                "'%s': e_phoff out of bounds with e_phnum==0: phoff=0x%" PRIx64
                " size=0x%" PRIx64,
                path, ehdr->e_phoff, file_size);
            return -EINVAL;
        }
    } else {
        if (expected_phentsize != ehdr->e_phentsize) {
            ERROR("'%s': e_phentsize mismatch detected: %" PRIu16
                  ", (expected %" PRIu16 ")",
                  path, ehdr->e_phentsize, expected_phentsize);
            return -EINVAL;
        }

        if (0 != byte_range_fits_file(file_size, ehdr->e_phoff,
                                      (uint64_t)ehdr->e_phnum,
                                      (uint64_t)ehdr->e_phentsize)) {
            ERROR(
                "'%s': program header table exceeds file bounds "
                "(phoff=0x%" PRIx64 " phnum=%" PRIu16 " phentsize=%" PRIu16
                " size=0x%" PRIx64 ")",
                path, ehdr->e_phoff, ehdr->e_phnum, ehdr->e_phentsize,
                file_size);
            return -EINVAL;
        }
    }

    /* 6) Section Header Table (SHT) validation + SHN_XINDEX support
     *
     * e_shnum == 0 can mean:
     * - no SHT present (common in stripped ET_EXEC/ET_DYN)
     * - extended section numbering (real values stored in SHDR[0])
     *
     * First, resolve effective values first, then validate spans/indexes.
     */
    int rc = resolve_extended_section_numbering(
        img, ehdr, &shnum_ext, &shstrndx_ext, expected_shentsize);
    if (0 != rc) {
        ERROR("'%s': failed to resolve extended section numbering (%d)", path,
              rc);
        return rc;
    }

    if (0 == shnum_ext) {
        /* No section header table (or truly zero). This is legal for
         * executables/shared objects. For ET_REL, section headers are normally
         * required. */
        if (ehdr->e_shoff > file_size) {
            ERROR(
                "'%s': e_shoff out of bounds with shnum_ext==0: "
                "shoff=0x%" PRIx64 " size=0x%" PRIx64,
                path, ehdr->e_shoff, file_size);
            return -EINVAL;
        }
    } else {
        if (ehdr->e_shentsize != expected_shentsize) {
            ERROR("'%s': e_shentsize mismatch detected: %" PRIu16
                  ", (expected %" PRIu16 ")",
                  path, ehdr->e_shentsize, expected_shentsize);
            return -EINVAL;
        }

        if (byte_range_fits_file(file_size, ehdr->e_shoff, shnum_ext,
                                 (uint64_t)ehdr->e_shentsize) < 0) {
            ERROR(
                "'%s': section header table exceeds file bounds "
                "(shoff=0x%" PRIx64 " shnum=%" PRIu64 " shentsize=%" PRIu16
                " size=0x%" PRIx64 ")",
                path, ehdr->e_shoff, shnum_ext, ehdr->e_shentsize, file_size);
            return -EINVAL;
        }

        /* If a section-name string table is specified (non-zero), it must refer
         * to a valid section header index. */
        if (0 != shstrndx_ext && shstrndx_ext >= shnum_ext) {
            ERROR("'%s': e_shstrndx out of range: %" PRIu64 " (shnum=%" PRIu64
                  ")",
                  path, shstrndx_ext, shnum_ext);
            return -EINVAL;
        }
    }

    TRACE("Finished %s", __func__);
    return 0;
}

/* Helper function for printing ELF addresses/offsets with leading zeros. */
static inline int elf_addr_width(uint8_t ei_class) {
    return (ei_class == ELFCLASS32) ? 8 : 16;
}

void elf_ehdr_log(const elf_ehdr_parsed_t* ehdr) {
    TRACE("Entered %s", __func__);
    if (NULL != ehdr) {
        const int width = elf_addr_width(ehdr->ei_class);
        (void)width;

        INFO("ELF header:");
        INFO("  ident: class=%s data=%s osabi=%s abiversion=%" PRIu8,
             elf_ident_class_str(ehdr->ei_class),
             elf_ident_data_str(ehdr->ei_data),
             elf_ident_osabi_str(ehdr->ei_osabi), ehdr->ei_abiversion);
        INFO("  type=%" PRIu16 " machine=%" PRIu16 " version=%" PRIu32,
             ehdr->e_type, ehdr->e_machine, ehdr->e_version);
        INFO("  entry=0x%0*" PRIx64 " phoff=0x%0*" PRIx64 " shoff=0x%0*" PRIx64,
             width, ehdr->e_entry, width, ehdr->e_phoff, width, ehdr->e_shoff);
        INFO("  flags=0x%" PRIx32 " ehsize=%" PRIu16, ehdr->e_flags,
             ehdr->e_ehsize);
        INFO("  phentsize=%" PRIu16 " phnum=%" PRIu16, ehdr->e_phentsize,
             ehdr->e_phnum);
        INFO("  shentsize=%" PRIu16 " shnum=%" PRIu16 " shstrndx=%" PRIu16,
             ehdr->e_shentsize, ehdr->e_shnum, ehdr->e_shstrndx);

        if (0 == ehdr->e_shnum || SHN_XINDEX == ehdr->e_shstrndx) {
            INFO(
                "  note: extended section numbering indicated (real values "
                "stored in section header 0)");
        }
    }
    TRACE("Finished %s", __func__);
}
