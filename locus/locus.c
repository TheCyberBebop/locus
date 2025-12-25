#include <stdio.h>
#include <stdlib.h>

#include "elf_image.h"
#include "logger.h"

int main(int argc, char* argv[]) {
    elf_image_t img;

    // Expect exactly one argument: the path to an ELF file to inspect
    if (2 != argc) {
        ERROR("usage: %s <elf-file>", argv[0]);
        return EXIT_FAILURE;
    }

    DEBUG("path: %s passed in", argv[1]);

    /* Open and memory-map the ELF file into a read-only image, which will be
     * used for subsequent validation and parsing */
    int ret = elf_image_open(argv[1], &img);
    if (ret < 0) {
        ERROR("failed to open ELF image");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
