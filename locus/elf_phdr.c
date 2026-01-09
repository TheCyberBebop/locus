#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "elf_phdr.h"
#include "elf_read.h"
#include "logger.h"

/* TODO */
int elf_phdr_parse(const elf_image_t* img,
                   const elf_ident_info_t* ident,
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
