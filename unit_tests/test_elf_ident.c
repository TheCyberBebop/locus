#include <errno.h>
#include <stddef.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "test_suites.h"

/*
 * Verify that elf_validate_ident() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
static void test_elf_validate_ident_invalid_params(void** state) {
    (void)state;  // Silence compile warning

    elf_image_t img = {0};  // TODO Update struct contents
    elf_ident_info_t out = {0};

    // Passing a NULL elf_image_t must be rejected
    assert_int_equal(elf_validate_ident(NULL, &out), -EINVAL);
    // Passing a NULL elf_ident_info_t must be rejected
    assert_int_equal(elf_validate_ident(&img, NULL), -EINVAL);
}

/*
 * Register all elf_ident unit tests with the test runner.
 *
 * The returned tests array is static and remains valid for the lifetime of
 * the process. The caller is responsible for invoking cmocka with the
 * returned pointer and count.
 */
size_t register_elf_ident_tests(struct CMUnitTest** out) {
    static struct CMUnitTest tests[] = {
        cmocka_unit_test(test_elf_validate_ident_invalid_params),
    };

    *out = tests;
    return sizeof(tests) / sizeof(tests[0]);
}
