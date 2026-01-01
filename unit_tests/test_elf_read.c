#include <errno.h>
#include <stddef.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "elf_read.h"
#include "test_suites.h"

/*
 * Verify that elf_read_u16() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
static void test_elf_read_u16_invalid_params(void** state) {
    (void)state;  // Silence compile warning

    elf_image_t img = {0};         // TODO Update struct contents
    elf_ident_info_t ident = {0};  // TODO Update struct contents
    uint16_t out;

    // Passing a NULL elf_image_t must be rejected
    assert_int_equal(elf_read_u16(NULL, &ident, 0, &out), -EINVAL);
    // Passing a NULL elf_ident_info_t must be rejected
    assert_int_equal(elf_read_u16(&img, NULL, 0, &out), -EINVAL);
    // Passing a NULL uint16_t must be rejected
    assert_int_equal(elf_read_u16(&img, &ident, 0, NULL), -EINVAL);
}

/*
 * Register all elf_read unit tests with the test runner.
 *
 * The returned tests array is static and remains valid for the lifetime of
 * the process. The caller is responsible for invoking cmocka with the
 * returned pointer and count.
 */
size_t register_elf_read_tests(struct CMUnitTest** out) {
    static struct CMUnitTest tests[] = {
        cmocka_unit_test(test_elf_read_u16_invalid_params),
    };

    *out = tests;
    return sizeof(tests) / sizeof(tests[0]);
}
