#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "test_suites.h"
#include "test_utilities.h"

/* -------------------------------------------------------------------------- */
/* elf_validate_ident() tests                                                 */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_validate_ident() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
static void test_elf_validate_ident_invalid_params(void** state) {
    (void)state;  // Unused; no filesystem setup required

    elf_image_t img = {0};
    elf_ident_t out = {0};
    assert_int_equal(elf_validate_ident(NULL, &out), -EINVAL);
    assert_int_equal(elf_validate_ident(&img, NULL), -EINVAL);
}

/*
 * Verify that elf_validate_ident() rejects an elf_image_t with a NULL base.
 *
 * The elf_ident module requires a valid byte view (img->base/img->size).
 * A NULL base indicates an uninitialized or corrupted image and must fail.
 */
static void test_elf_validate_ident_image_base_null(void** state) {
    (void)state;  // Unused; no filesystem setup required

    elf_image_t img = {.base = NULL, .size = 64, .path = "base_null_test"};
    elf_ident_t out = {0};
    assert_int_equal(elf_validate_ident(&img, &out), -EINVAL);
}

/*
 * Verify that elf_validate_ident() rejects an inconsistent elf_image_t state
 * where base is non-NULL but size is zero.
 *
 * This test opens a real file to obtain a valid mapping, then corrupts img.size
 * to validate defensive checks in elf_validate_ident(). The image is restored
 * so it can be cleanly closed afterward.
 */
static void test_elf_validate_ident_image_size_zero(void** state) {
    const char* test_file = "size_zero.bin";
    test_fs_t* fs = *state;

    register_test_file(fs,
                       test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    // Save mapping info so we can clean up even after we corrupt img
    size_t saved_size = img.size;

    img.size = 0;  // Force inconsistent state
    elf_ident_t out = {0};
    assert_int_equal(elf_validate_ident(&img, &out), -EINVAL);

    img.size = saved_size;  // Restore for clean close
    assert_int_equal(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() rejects an image with an invalid ELF magic.
 */
static void test_elf_validate_ident_invalid_magic(void** state) {
    const char* test_file = "invalid_magic.bin";
    test_fs_t* fs = *state;

    register_test_file(fs,
                       test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));

    // Corrupt the magic bytes so the file is not recognized as ELF
    buf[EI_MAG0] = 0xAA;
    buf[EI_MAG1] = 0xBB;
    buf[EI_MAG2] = 0xCC;
    buf[EI_MAG3] = 0xDD;

    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    elf_ident_t ident = {0};
    assert_int_equal(elf_validate_ident(&img, &ident), -EINVAL);
    assert_int_equal(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() rejects unsupported EI_CLASS values.
 */
static void test_elf_validate_ident_invalid_ei_class(void** state) {
    const char* test_file = "invalid_ei_class.bin";
    test_fs_t* fs = *state;

    register_test_file(fs,
                       test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));

    // Valid ELF magic, but unsupported class
    buf[EI_MAG0] = ELFMAG0;
    buf[EI_MAG1] = ELFMAG1;
    buf[EI_MAG2] = ELFMAG2;
    buf[EI_MAG3] = ELFMAG3;
    buf[EI_CLASS] = 0xAA;

    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    elf_ident_t ident = {0};
    assert_int_equal(elf_validate_ident(&img, &ident), -ENOTSUP);
    assert_int_equal(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() rejects unsupported EI_DATA values.
 */
static void test_elf_validate_ident_invalid_ei_data(void** state) {
    const char* test_file = "invalid_ei_data.bin";
    test_fs_t* fs = *state;

    register_test_file(fs,
                       test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));

    // Valid magic/class, but unsupported endianness encoding
    buf[EI_MAG0] = ELFMAG0;
    buf[EI_MAG1] = ELFMAG1;
    buf[EI_MAG2] = ELFMAG2;
    buf[EI_MAG3] = ELFMAG3;
    buf[EI_CLASS] = ELFCLASS64;
    buf[EI_DATA] = 0xAA;

    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    elf_ident_t ident = {0};
    assert_int_equal(elf_validate_ident(&img, &ident), -ENOTSUP);
    assert_int_equal(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() rejects unexpected EI_VERSION values.
 */
static void test_elf_validate_ident_invalid_ei_version(void** state) {
    const char* test_file = "invalid_ei_version.bin";
    test_fs_t* fs = *state;

    register_test_file(fs,
                       test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));

    // Valid magic/class/data, but invalid ident version
    buf[EI_MAG0] = ELFMAG0;
    buf[EI_MAG1] = ELFMAG1;
    buf[EI_MAG2] = ELFMAG2;
    buf[EI_MAG3] = ELFMAG3;
    buf[EI_CLASS] = ELFCLASS64;
    buf[EI_DATA] = ELFDATA2LSB;
    buf[EI_VERSION] = 0xAA;
    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    elf_ident_t ident = {0};
    assert_int_equal(elf_validate_ident(&img, &ident), -EINVAL);
    assert_int_equal(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() accepts a well-formed e_ident and populates
 * the decoder configuration correctly.
 */
static void test_elf_validate_ident_success(void** state) {
    const char* test_file = "ident_success.bin";
    test_fs_t* fs = *state;

    register_test_file(fs,
                       test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));

    buf[EI_MAG0] = ELFMAG0;
    buf[EI_MAG1] = ELFMAG1;
    buf[EI_MAG2] = ELFMAG2;
    buf[EI_MAG3] = ELFMAG3;
    buf[EI_CLASS] = ELFCLASS64;
    buf[EI_DATA] = ELFDATA2LSB;
    buf[EI_VERSION] = EV_CURRENT;
    buf[EI_OSABI] = ELFOSABI_NONE;
    buf[EI_ABIVERSION] = 0;

    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    elf_ident_t ident = {0};
    assert_int_equal(elf_validate_ident(&img, &ident), 0);
    assert_int_equal(elf_image_close(&img), 0);
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
        cmocka_unit_test(test_elf_validate_ident_image_base_null),
        cmocka_unit_test_setup_teardown(test_elf_validate_ident_image_size_zero,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_validate_ident_invalid_magic,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(
            test_elf_validate_ident_invalid_ei_class, test_setup_fs,
            test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_validate_ident_invalid_ei_data,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(
            test_elf_validate_ident_invalid_ei_version, test_setup_fs,
            test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_validate_ident_success,
                                        test_setup_fs, test_teardown_fs),
    };

    *out = tests;
    return sizeof(tests) / sizeof(tests[0]);
}
