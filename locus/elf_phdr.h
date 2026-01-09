#ifndef ELF_PHDR_H
#define ELF_PHDR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO */
typedef struct elf_phdr_t {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf_phdr_t;

#ifdef __cplusplus
}
#endif

#endif /* ELF_EPDR_H */