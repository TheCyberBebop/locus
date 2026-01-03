#ifndef LOCUS_TEST_UTILITIES_H
#define LOCUS_TEST_UTILITIES_H

#include <stddef.h>
#include <stdint.h>

/* Per-test filesystem state for elf_image unit tests. Using per-test state
 * avoids shared mutable globals and keeps tests isolated. */
typedef struct image_test_fs {
    char temp_path[256];  // Temporary directory
} image_test_fs_t;

/* Write an exact buffer to a file (create/truncate). Returns 0 or -errno. */
int write_file(const char *path, const uint8_t *buf, size_t len);

/* cmocka setup/teardown for per-test temp directory state. */
int setup_image_tests(void **state);
int teardown_image_tests(void **state);

#endif /* LOCUS_TEST_UTILITIES_H */
