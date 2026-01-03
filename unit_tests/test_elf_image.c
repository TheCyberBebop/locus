#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "elf_image.h"
#include "test_suites.h"
#include "test_utilities.h"

/* -------------------------------------------------------------------------- */
/* elf_image_open() tests                                                     */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_image_open() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
static void test_elf_image_open_invalid_params(void** state) {
    (void)state;  // Unused; no filesystem setup required

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(NULL, &img), -EINVAL);
    assert_int_equal(elf_image_open("/tmp/fake_file", NULL), -EINVAL);

    // Verify fields remained cleared on error
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/*
 * Verify that elf_image_open() rejects a non-existent filesystem path.
 *
 * This test ensures that attempting to open a path that does not exist
 * fails cleanly and does not partially initialize the elf_image_t structure.
 */
static void test_elf_image_open_nonexistent_path(void** state) {
    (void)state;  // Unused; no filesystem setup required

    elf_image_t img = {0};
    assert_int_equal(elf_image_open("/fake/file/path", &img), -ENOENT);

    // Verify fields remained cleared on error
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/*
 * Verify that elf_image_open() rejects directory paths.
 *
 * The elf_image abstraction is defined only for regular files. Attempting
 * to open a directory must fail without creating a mapping or modifying
 * the elf_image_t state.
 */
static void test_elf_image_open_invalid_file(void** state) {
    test_fs_t* fs = *state;

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(fs->temp_path, &img), -EINVAL);

    // Verify fields remained cleared on error
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/*
 * Verify that elf_image_open() rejects files too small to contain an ELF
 * header.
 *
 * This test constructs a file smaller than sizeof(Elf32_Ehdr) and confirms
 * that elf_image_open() fails defensively, without mapping the file or
 * partially initializing elf_image_t.
 */
static void test_elf_image_open_file_too_small(void** state) {
    const char* test_file = "too_small.bin";
    test_fs_t* fs = *state;

    register_test_file(fs, test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Intentionally smaller than sizeof(Elf32_Ehdr)
    uint8_t buf[16];
    memset(buf, 0xFF, sizeof(buf));
    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), -EINVAL);

    // Verify fields remained cleared on error
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/*
 * Verify that elf_image_open() successfully maps a valid file and populates
 * the elf_image_t structure with correct metadata and contents.
 *
 * This test validates:
 *  - successful mapping of the entire file
 *  - correct size reporting
 *  - preservation of the input path for diagnostics
 *  - mapped bytes exactly match file contents
 *  - successful cleanup via elf_image_close()
 */
static void test_elf_image_open_success(void** state) {
    const char* test_file = "open_success.bin";
    test_fs_t* fs = *state;

    register_test_file(fs, test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write a known byte pattern to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    // Verify success
    assert_non_null(img.base);
    assert_int_equal(img.size, sizeof(buf));
    assert_ptr_equal(img.path, path);

    // Observable mapping behavior: mapped bytes must match file contents
    assert_memory_equal(img.base, buf, sizeof(buf));

    // Close must succeed and clear fields
    assert_int_equal(elf_image_close(&img), 0);
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/* -------------------------------------------------------------------------- */
/* elf_image_close() tests */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_image_close() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
static void test_elf_image_close_invalid_params(void** state) {
    (void)state;  // Unused; no filesystem setup required

    // Passing a NULL elf_image_t must be rejected
    assert_int_equal(elf_image_close(NULL), -EINVAL);
}

/*
 * Verify that elf_image_close() detects inconsistent state (base != NULL,
 * size == 0) and still clears the image safely.
 *
 * This test intentionally corrupts elf_image_t after a successful open to
 * validate defensive cleanup behavior.
 */
static void test_elf_image_close_img_size_zero(void** state) {
    const char* test_file = "size_zero.bin";
    test_fs_t* fs = *state;

    register_test_file(fs, test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512] = {0};
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    // Save mapping info so we can clean up even after we corrupt img
    const uint8_t* saved_base = img.base;
    size_t saved_size = img.size;

    // Force inconsistent state: base is valid but size is zero
    img.size = 0;
    assert_int_equal(elf_image_close(&img), -EINVAL);

    // Close must clear fields even on error
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);

    // Test cleanup: mapping would otherwise leak
    assert_int_equal(munmap((void*)saved_base, saved_size), 0);
}

/*
 * Verify that elf_image_close() reports an error if munmap() fails, while still
 * clearing the elf_image_t fields to prevent reuse.
 *
 * This test intentionally corrupts img.base after unmapping the real mapping,
 * forcing elf_image_close() to call munmap() with an invalid address and take
 * its error path. A successful test confirms:
 *  - elf_image_close() returns -errno from the failed munmap()
 *  - the image is cleared even on failure (idempotent/safe cleanup behavior)
 */
static void test_elf_image_close_munmap_error(void** state) {
    const char* test_file = "munmap_error.bin";
    test_fs_t* fs = *state;

    register_test_file(fs, test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    /* Test-only cleanup: unmap the real mapping first so we don't leak it when
     * we corrupt img.base to force a munmap() failure path. */
    assert_non_null(img.base);
    assert_true(img.size > 0);
    assert_int_equal(munmap((void*)img.base, img.size), 0);

    /* Force munmap() failure: provide an invalid base address while keeping a
     * non-zero size so elf_image_close() attempts munmap(). */
    img.base = (const uint8_t*)0xDEADBEEF;

    /* munmap() should fail with EINVAL for an invalid address, and
     * elf_image_close() should return -errno. */
    assert_int_equal(elf_image_close(&img), -EINVAL);

    // Close must clear fields even on error
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/*
 * Verify that elf_image_close() detects inconsistent state (base == NULL,
 * size != 0) and returns an error while still clearing the image.
 *
 * This test intentionally corrupts an elf_image_t after a successful open to
 * validate defensive cleanup behavior. Because clearing img.base would
 * otherwise leak the mapping, the test saves the original mapping information
 * and performs explicit munmap() cleanup.
 */
static void test_elf_image_close_img_base_null(void** state) {
    const char* test_file = "base_null.bin";
    test_fs_t* fs = *state;

    register_test_file(fs, test_file);  // Ensure file is removed during teardown

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
    const uint8_t* saved_base = img.base;
    size_t saved_size = img.size;

    // Force inconsistent state: size is valid but base is NULL
    img.base = NULL;
    assert_int_equal(elf_image_close(&img), -EINVAL);

    // Close must clear fields even on error
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);

    // Test cleanup: mapping would otherwise leak
    assert_int_equal(munmap((void*)saved_base, saved_size), 0);
}

/*
 * Verify that elf_image_close() can be performed multiple times.
 *
 * Closing a successfully-opened image must succeed, clear fields, and remain
 * safe to call again on the same elf_image_t (no double-unmap or reuse bugs).
 */
static void test_elf_image_close_double_close(void** state) {
    const char* test_file = "double_close.bin";
    test_fs_t* fs = *state;

    register_test_file(fs, test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xAA, sizeof(buf));
    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);

    // First close must succeed and clear fields
    assert_int_equal(elf_image_close(&img), 0);
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);

    // Second close must also succeed and leave fields cleared
    assert_int_equal(elf_image_close(&img), 0);
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/*
 * Verify that elf_image_close() succeeds on an empty (never-opened) image.
 *
 * Callers often perform unconditional cleanup in error paths. This test ensures
 * that closing a zero-initialized elf_image_t is a safe no-op that returns 0
 * and leaves fields in an empty state.
 */
static void test_elf_image_close_empty_image(void** state) {
    (void)state;  // Unused; no filesystem setup required

    elf_image_t img = {0};
    assert_int_equal(elf_image_close(&img), 0);

    // Close must leave fields in an empty state
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/*
 * Verify that an elf_image_t can be safely reused across multiple open/close
 * cycles.
 *
 * This test opens and closes two different files sequentially using the same
 * elf_image_t. Each open must fully reinitialize the structure, and each close
 * must fully clear it.
 */
static void test_elf_image_open_close_reuse(void** state) {
    const char* test_file1 = "reuse_one.bin";
    const char* test_file2 = "reuse_two.bin";
    test_fs_t* fs = *state;

    register_test_file(fs, test_file1);  // Ensure file is removed during teardown
    register_test_file(fs, test_file2);  // Ensure file is removed during teardown

    char path1[512];
    char path2[512];
    snprintf(path1, sizeof(path1), "%s/%s", fs->temp_path, test_file1);
    snprintf(path2, sizeof(path2), "%s/%s", fs->temp_path, test_file2);

    // Write different patterns so we can distinguish mappings
    uint8_t buf1[64];
    uint8_t buf2[64];
    memset(buf1, 0xFF, sizeof(buf1));
    memset(buf2, 0xAA, sizeof(buf2));

    assert_int_equal(test_write_file(path1, buf1, sizeof(buf1)), 0);
    assert_int_equal(test_write_file(path2, buf2, sizeof(buf2)), 0);

    elf_image_t img = {0};

    // First open/close cycle
    assert_int_equal(elf_image_open(path1, &img), 0);
    assert_non_null(img.base);
    assert_int_equal(img.size, sizeof(buf1));
    assert_ptr_equal(img.path, path1);
    assert_memory_equal(img.base, buf1, sizeof(buf1));

    assert_int_equal(elf_image_close(&img), 0);
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);

    // Second open/close cycle reusing the same struct
    assert_int_equal(elf_image_open(path2, &img), 0);
    assert_non_null(img.base);
    assert_int_equal(img.size, sizeof(buf2));
    assert_ptr_equal(img.path, path2);
    assert_memory_equal(img.base, buf2, sizeof(buf2));

    assert_int_equal(elf_image_close(&img), 0);
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
}

/*
 * Verify that elf_image_close() succeeds for a valid, mapped image and clears
 * all fields afterward.
 *
 * This test covers the normal success path: open a file, map it, then close it.
 * On success, elf_image_close() must return 0 and leave the image in an empty,
 * safe-to-reuse state.
 */
static void test_elf_image_close_success(void** state) {
    const char* test_file = "close_success.bin";
    test_fs_t* fs = *state;

    register_test_file(fs, test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Write arbitrary test data to the file
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    assert_int_equal(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img = {0};
    assert_int_equal(elf_image_open(path, &img), 0);
    assert_int_equal(elf_image_close(&img), 0);

    // Close must clear fields even on success
    assert_null(img.base);
    assert_int_equal(img.size, 0);
    assert_null(img.path);
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
        /* elf_image_open() tests */
        cmocka_unit_test(test_elf_image_open_invalid_params),
        cmocka_unit_test(test_elf_image_open_nonexistent_path),
        cmocka_unit_test_setup_teardown(test_elf_image_open_invalid_file,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_image_open_file_too_small,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_image_open_success,
                                        test_setup_fs, test_teardown_fs),
        /* elf_image_close() tests */
        cmocka_unit_test(test_elf_image_close_invalid_params),
        cmocka_unit_test_setup_teardown(test_elf_image_close_img_size_zero,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_image_close_munmap_error,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_image_close_img_base_null,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_image_close_double_close,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test(test_elf_image_close_empty_image),
        cmocka_unit_test_setup_teardown(test_elf_image_open_close_reuse,
                                        test_setup_fs, test_teardown_fs),
        cmocka_unit_test_setup_teardown(test_elf_image_close_success,
                                        test_setup_fs, test_teardown_fs),
    };

    *out = tests;
    return sizeof(tests) / sizeof(tests[0]);
}
