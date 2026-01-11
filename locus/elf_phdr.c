#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "elf_ehdr.h"
#include "elf_ident.h"
#include "elf_image.h"
#include "elf_phdr.h"
#include "elf_read.h"
#include "logger.h"

/* TODO */
static int elf_phdr_parse_at(const elf_image_t* img,
                             const elf_ident_t* ident,
                             const uint64_t phoff,
                             elf_phdr_t* out) {
    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == img || NULL == ident || NULL == out) {
        ERROR("Invalid parameter (img=%p ident=%p out=%p)", (void*)img,
              (void*)ident, (void*)out);
        return -EINVAL;
    }

    const char* path = img->path ? img->path : "(unknown)";  // Not critical
    (void)path;

    memset(out, 0, sizeof(*out));

    if (ELFCLASS32 == ident->ei_class) {
        /* Zero-extended 32-bit results from p_offset, p_vaddr, p_paddr,
         * p_filesz, p_memsz, and p_align to the appropriate elf_phdr_t 64-bit
         * fields. */
        uint32_t temp_u32 = 0;
        READ_OR_RETURN(elf_read_u32, img, ident, phoff, &out->p_type);
        READ_OR_RETURN(elf_read_u32, img, ident, phoff + 0x04u, &temp_u32);
        out->p_offset = (uint64_t)temp_u32;
        READ_OR_RETURN(elf_read_u32, img, ident, phoff + 0x08u, &temp_u32);
        out->p_vaddr = (uint64_t)temp_u32;
        READ_OR_RETURN(elf_read_u32, img, ident, phoff + 0x0Cu, &temp_u32);
        out->p_paddr = (uint64_t)temp_u32;
        READ_OR_RETURN(elf_read_u32, img, ident, phoff + 0x10u, &temp_u32);
        out->p_filesz = (uint64_t)temp_u32;
        READ_OR_RETURN(elf_read_u32, img, ident, phoff + 0x14u, &temp_u32);
        out->p_memsz = (uint64_t)temp_u32;
        READ_OR_RETURN(elf_read_u32, img, ident, phoff + 0x18u, &out->p_flags);
        READ_OR_RETURN(elf_read_u32, img, ident, phoff + 0x1Cu, &temp_u32);
        out->p_align = (uint64_t)temp_u32;
    } else if (ELFCLASS64 == ident->ei_class) {
        READ_OR_RETURN(elf_read_u32, img, ident, phoff, &out->p_type);
        READ_OR_RETURN(elf_read_u32, img, ident, phoff + 0x04u, &out->p_flags);
        READ_OR_RETURN(elf_read_u64, img, ident, phoff + 0x08u, &out->p_offset);
        READ_OR_RETURN(elf_read_u64, img, ident, phoff + 0x010u, &out->p_vaddr);
        READ_OR_RETURN(elf_read_u64, img, ident, phoff + 0x18u, &out->p_paddr);
        READ_OR_RETURN(elf_read_u64, img, ident, phoff + 0x20u, &out->p_filesz);
        READ_OR_RETURN(elf_read_u64, img, ident, phoff + 0x28u, &out->p_memsz);
        READ_OR_RETURN(elf_read_u64, img, ident, phoff + 0x30u, &out->p_align);
    } else {
        ERROR("'%s': invalid ELF class: %" PRIu8, path, ident->ei_class);
        return -EINVAL;
    }
    TRACE("Finished %s", __func__);
    return 0;
}

int elf_pht_parse_all(const elf_image_t* img,
                      const elf_ehdr_t* ehdr,
                      elf_pht_t** out) {
    int rc = 0;

    TRACE("Entered %s", __func__);

    // Validate function parameters
    if (NULL == img || NULL == ehdr || NULL == out) {
        ERROR("Invalid parameter (img=%p ehdr=%p out=%p)", (void*)img,
              (void*)ehdr, (void*)out);
        return -EINVAL;
    }

    *out = NULL;

    const uint16_t phnum = ehdr->e_phnum;
    const uint16_t phentsize = ehdr->e_phentsize;
    const uint64_t phoff = ehdr->e_phoff;
    const char* path = (img->path != NULL) ? img->path : "(unknown)";
    (void)path;

    // Missing program header team results in an error for executables
    if ((ET_EXEC == ehdr->e_type || ET_DYN == ehdr->e_type) && (0 == phnum)) {
        ERROR("ELF has no program headers (e_type=%" PRIu16 ")", ehdr->e_type);
        return -EINVAL;
    }

    // Validate entry size against ABI for the class
    if (ELFCLASS32 == ehdr->ei_class) {
        if (ELF32_PHDR_SIZE != phentsize) {
            ERROR("'%s': ELFCLASS32 e_phentsize mismatch detected: %" PRIu16
                  " expected=32)",
                  path, phentsize);
            return -EINVAL;
        }
    } else if (ELFCLASS64 == ehdr->ei_class) {
        if (ELF64_PHDR_SIZE != phentsize) {
            ERROR("'%s': ELFCLASS64 e_phentsize mismatch detected: %" PRIu16
                  " expected=56)",
                  path, phentsize);
            return -EINVAL;
        }
    } else {
        ERROR("'%s': invalid ELF class: %" PRIu8, path, ehdr->ei_class);
        return -EINVAL;
    }

    // No program header table is legal for ET_REL
    if (0 == phnum) {
        DEBUG("No program headers to parse (e_phnum=0)");
        TRACE("Finished %s", __func__);
        return 0;
    }

    // Build a minimal ident view for endian/class-aware reads
    elf_ident_t ident;
    memset(&ident, 0, sizeof(ident));
    ident.ei_class = ehdr->ei_class;
    ident.ei_data = ehdr->ei_data;

    // TODO add bounds validation

    // Allocate out container and array
    elf_pht_t* result = (elf_pht_t*)calloc(1u, sizeof(elf_pht_t));
    if (NULL == result) {
        ERROR("'%s': calloc failed for elf_pht_t", path);
        return -ENOMEM;
    }

    elf_phdr_t* pht = (elf_phdr_t*)calloc(phnum, sizeof(elf_phdr_t));
    if (NULL == pht) {
        ERROR("calloc failed for program headers (phnum=%" PRIu16 ")", phnum);
        return -ENOMEM;
    }

    for (size_t i = 0; i < (size_t)phnum; i++) {
        uint64_t entry_offset = phoff + ((uint64_t)i * phentsize);
        rc = elf_phdr_parse_at(img, &ident, entry_offset, &pht[i]);
        if (0 != rc) {
            ERROR("Failed to parse program header (i=%zu offset=%" PRIu64
                  " rc=%d)",
                  i, entry_offset, rc);
            *out = NULL;
            free(pht);
            return rc;
        }
    }

    result->pheaders = pht;
    result->count = phnum;
    *out = result;
    TRACE("Finished %s", __func__);
    return 0;
}
