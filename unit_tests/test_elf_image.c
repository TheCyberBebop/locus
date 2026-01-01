#include <errno.h>
#include <stddef.h>

#include "elf_image.h"
#include "test_suites.h"

/*
 * Verify that elf_image_open() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
static void test_elf_image_open_invalid_params(void** state) {
    (void)state;  // Silence compile warning

    elf_image_t img = {0};

    // Passing a NULL path must be rejected
    assert_int_equal(elf_image_open(NULL, &img), -EINVAL);
    // Passing a NULL elf_image_t must be rejected
    assert_int_equal(elf_image_open("/tmp/fake_file", NULL), -EINVAL);
}

/*
 * Verify that elf_image_close() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
static void test_elf_image_close_invalid_params(void** state) {
    (void)state;  // Silence compile warning

    // Passing a NULL elf_image_t must be rejected
    assert_int_equal(elf_image_close(NULL), -EINVAL);
}

/*
 * Register all elf_image unit tests with the test runner.
 *
 * The returned tests array is static and remains valid for the lifetime of
 * the process. The caller is responsible for invoking cmocka with the
 * returned pointer and count.
 */
size_t register_elf_image_tests(struct CMUnitTest** out) {
    static struct CMUnitTest tests[] = {
        cmocka_unit_test(test_elf_image_open_invalid_params),
        cmocka_unit_test(test_elf_image_close_invalid_params),
    };

    *out = tests;
    return sizeof(tests) / sizeof(tests[0]);
}
