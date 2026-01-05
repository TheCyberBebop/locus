#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "elf_ehdr.h"
#include "elf_ident.h"
#include "elf_image.h"
#include "elf_read.h"
#include "logger.h"

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

#if LOG_LEVEL <= LOG_LEVEL_ERROR
    const char* path = img->path ? img->path : "(unknown)";  // Not critical
#endif

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

    // Copy previously e_ident-derived decoder configuration (elf_ident.c)
    out->ei_class = ident->ei_class;
    out->ei_data = ident->ei_data;
    out->ei_osabi = ident->ei_osabi;
    out->ei_abiversion = ident->ei_abiversion;

    READ_OR_RETURN(elf_read_u16, img, ident, OFF_E_TYPE, &out->e_type);
    READ_OR_RETURN(elf_read_u16, img, ident, OFF_E_MACHINE, &out->e_machine);
    READ_OR_RETURN(elf_read_u32, img, ident, OFF_E_VERSION, &out->e_version);

    if (ELFCLASS32 == ident->ei_class) {
        /* Zero-extended 32-bit results from e_entry, e_phoff, and e_shoff to
         * the appropriate elf_ehdr_parsed_t 64-bit fields */
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
        ERROR("invalid ELF class: %u", out->ei_class);
        return -EINVAL;
    }

    DEBUG("ELF identification:");
    DEBUG("  class=%s data=%s osabi=%s (%" PRIu8 ") abiversion=%" PRIu8,
          elf_ident_class_str(out->ei_class), elf_ident_data_str(out->ei_data),
          elf_ident_osabi_str(out->ei_osabi), out->ei_osabi,
          out->ei_abiversion);
    DEBUG("ELF header:");
    DEBUG("  type=%" PRIu16 " machine=%" PRIu16 " version=%" PRIu32,
          out->e_type, out->e_machine, out->e_version);
    DEBUG("  entry=0x%" PRIx64 " flags=0x%" PRIx32, out->e_entry, out->e_flags);
    DEBUG("Program header table:");
    DEBUG("  pfoff=0x%" PRIx64 " phentsize=%" PRIu16 " phnum=%" PRIu16,
          out->e_phoff, out->e_phentsize, out->e_phnum);
    DEBUG("Section header table:");
    DEBUG("  shoff=0x%" PRIx64 " shentsize=%" PRIu16 " shnum=%" PRIu16
          " shstrndx=%" PRIu16,
          out->e_shoff, out->e_shentsize, out->e_shnum, out->e_shstrndx);
    TRACE("Finished %s", __func__);
    return 0;
}

#undef READ_OR_RETURN
