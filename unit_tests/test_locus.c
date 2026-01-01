#include <stdio.h>

#include "test_suites.h"

int main(void) {
    int rc = 0;
    struct CMUnitTest* image_tests = NULL;
    struct CMUnitTest* ident_tests = NULL;
    struct CMUnitTest* read_tests = NULL;
    const size_t num_image = register_elf_image_tests(&image_tests);
    const size_t num_ident = register_elf_ident_tests(&ident_tests);
    const size_t num_read = register_elf_read_tests(&read_tests);

    // Bail if registration is broken for any tests
    if (NULL == image_tests || 0 == num_image) {
        fprintf(stderr, "ERROR: elf_image registered 0 tests\n");
        return 1;
    }
    if (NULL == ident_tests || 0 == num_ident) {
        fprintf(stderr, "ERROR: elf_ident registered 0 tests\n");
        return 1;
    }
    if (NULL == read_tests || 0 == num_read) {
        fprintf(stderr, "ERROR: elf_read registered 0 tests\n");
        return 1;
    }

    /* The cmocka_run_group_tests() macro calls _cmocka_run_group_tests
     * internally. This function accepts an explicit count, so it works with
     * out dynamically provided test arrays.
     */
    rc |= _cmocka_run_group_tests("elf_image_tests", image_tests, num_image,
                                  NULL, NULL);
    rc |= _cmocka_run_group_tests("elf_ident_tests", ident_tests, num_ident,
                                  NULL, NULL);
    rc |= _cmocka_run_group_tests("elf_read_tests", read_tests, num_read, NULL,
                                  NULL);
    return rc;
}
