#include <stdio.h>
#include <stdlib.h>

#include "elf_image.h"
#include "logger.h"

int main(int argc, char* argv[]) {
    elf_image_t img;
    elf_ident_info_t ident;

    TRACE("Entered %s", __func__);

    // Expect exactly one argument: the path to an ELF file to inspect
    if (2 != argc) {
        ERROR("usage: %s <elf-file>", argv[0]);
        return EXIT_FAILURE;
    }

    DEBUG("path: %s passed in", argv[1]);

    /* Open and memory-map the ELF file into a read-only image, which will be
     * used for subsequent validation and parsing */
    if (elf_image_open(argv[1], &img) < 0) {
        ERROR("failed to open ELF image");
        return EXIT_FAILURE;
    }

    /* Validate the ELF identification fields (e_ident) and populate the
     * decoder configuration, which determines how all subsequent ELF
     * headers and segments must be interpreted (class and endianness). */
    if (elf_validate_ident(&img, &ident) < 0) {
        ERROR("ELF identification validation failed");
        return EXIT_FAILURE;
    }

    TRACE("Finished %s", __func__);

    return EXIT_SUCCESS;
}
