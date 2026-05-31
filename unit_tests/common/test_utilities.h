#ifndef LOCUS_TEST_UTILITIES_H
#define LOCUS_TEST_UTILITIES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_MAX_FILES 64

/* Per-test filesystem state for elf_image unit tests. Using per-test state
 * avoids shared mutable globals and keeps tests isolated. */
typedef struct test_fs {
    char temp_path[256];  // Temporary directory
    size_t file_count;
    const char*
        test_files[TEST_MAX_FILES]; /* filenames relative to temp_path */
} test_fs_t;

/* Validate test_fs_t and add a test file to be removed during teardown */
int register_test_file(test_fs_t* fs, const char* name);

/* Write an exact buffer to a file (create/truncate). Returns 0 or -errno. */
int test_write_file(const char* path, const uint8_t* buf, size_t len);

/* cmocka setup/teardown for per-test temp directory state. */
int test_setup_fs(void** state); /* Returns 0 or -errno. */
int test_teardown_fs(void** state);

#ifdef __cplusplus
}
#endif

#endif /* LOCUS_TEST_UTILITIES_H */
