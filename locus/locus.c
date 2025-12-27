#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "logger.h"

static void cleanup(elf_image_t* img) {
    int rc = elf_image_close(img);
    if (rc < 0) {
        ERROR("failed to close ELF image (%d)", rc);
    }
}

int main(int argc, char* argv[]) {
    int rc = 0;
    elf_image_t img;
    elf_ident_info_t ident;

    TRACE("Entered %s", __func__);

    // Expect exactly one argument: the path to an ELF file to inspect
    if (2 != argc) {
        ERROR("usage: %s <elf-file>", argv[0]);
        return EXIT_FAILURE;
    }

    DEBUG("path: '%s' passed in", argv[1]);
    memset(&img, 0, sizeof(img));
    memset(&ident, 0, sizeof(ident));

    /* Open and memory-map the ELF file into a read-only image, which will
     * be used for subsequent validation and parsing */
    rc = elf_image_open(argv[1], &img);
    if (rc < 0) {
        ERROR("failed to open ELF image (%d)", rc);
        return EXIT_FAILURE;
    }

    /* Validate the ELF identification fields (e_ident) and populate the
     * decoder configuration, which determines how all subsequent ELF
     * headers and segments must be interpreted (class and endianness) */
    rc = elf_validate_ident(&img, &ident);
    if (rc < 0) {
        ERROR("ELF identification validation failed (%d)", rc);
        cleanup(&img);
        return EXIT_FAILURE;
    }

    /* Release the memory mapping associated with an ELF image previously
     * opened with elf_image_open() and reset the elf_image_t structure
     * to an empty state */
    cleanup(&img);

    TRACE("Finished %s", __func__);
    return EXIT_SUCCESS;
}
