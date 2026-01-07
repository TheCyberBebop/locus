#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf_ehdr.h"
#include "elf_ident.h"
#include "elf_image.h"
#include "logger.h"

int main(int argc, char* argv[]) {
    int rc = 0;
    elf_image_t img;
    elf_ident_info_t ident;
    elf_ehdr_parsed_t ehdr;

    TRACE("Entered %s", __func__);

    // Expect exactly one argument: the path to an ELF file to inspect
    if (2 != argc) {
        ERROR("usage: %s <elf-file>", argv[0]);
        return EXIT_FAILURE;
    }

    DEBUG("path: '%s' passed in", argv[1]);
    memset(&img, 0, sizeof(img));
    memset(&ident, 0, sizeof(ident));
    memset(&ehdr, 0, sizeof(ehdr));

    /* Open and memory-map the ELF file into a read-only image, which will
     * be used for subsequent validation and parsing */
    rc = elf_image_open(argv[1], &img);
    if (rc < 0) {
        ERROR("failed to open ELF image (%d)", rc);
        goto cleanup;
    }

    /* Validate the ELF identification fields (e_ident) and populate the
     * decoder configuration, which determines how all subsequent ELF
     * headers and segments must be interpreted (class and endianness) */
    rc = elf_validate_ident(&img, &ident);
    if (rc < 0) {
        ERROR("ELF identification validation failed (%d)", rc);
        goto cleanup;
    }

    /* Parse the remaining ELF header fields and populate the parsed ELF header
     */
    rc = elf_ehdr_parse(&img, &ident, &ehdr);
    if (rc < 0) {
        ERROR("ELF header parsing failed (%d)", rc);
        goto cleanup;
    }

    /* Validate invariants before trusting offsets/counts for table walking. */
    rc = elf_ehdr_validate(&img, &ehdr);
    if (rc < 0) {
        ERROR("ELF header validation failed (%d)", rc);
        goto cleanup;
    } else {
        elf_ehdr_log(&ehdr);
    }

cleanup:
    /* Release the memory mapping associated with an ELF image previously
     * opened with elf_image_open() and reset the elf_image_t structure
     * to an empty state */
    if (0 != elf_image_close(&img)) {
        ERROR("failed to close ELF image (%d)", rc);
    }

    TRACE("Finished %s", __func__);
    return EXIT_SUCCESS;
}
