#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "test_utilities.h"

int register_test_file(test_fs_t* fs, const char* name) {
    if (NULL == fs || NULL == name) {
        return -EINVAL;
    }

    if (fs->file_count >= TEST_MAX_FILES) {
        return -ENOSPC;
    }

    fs->test_files[fs->file_count++] = name;
    return 0;
}

int test_write_file(const char* path, const uint8_t* buf, size_t len) {
    ssize_t ret = 0;
    int saved = 0;
    int fd = -1;

    // Create or truncate the file and open it for writing
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return -errno;
    }

    // Attempt to write the entire buffer in a single call
    ret = write(fd, buf, len);
    saved = errno;

    // Close unconditionally; the write result determines success/failure
    close(fd);

    if (ret < 0) {
        return -saved;
    }

    // Treat short writes as errors to avoid masking unexpected I/O behavior
    if (len != (size_t)ret) {
        return -EIO;
    }

    return 0;
}

int test_setup_fs(void** state) {
    char path_template[] = "/tmp/locus-unit-tests-XXXXXX";
    char* temp_path = NULL;
    test_fs_t* fs = NULL;

    // Allocate memory for per-test filesystem state (temporary directory path)
    fs = (test_fs_t*)calloc(1, sizeof(*fs));
    if (NULL == fs) {
        return -ENOMEM;
    }

    // Create a unique temporary directory for this test instance
    temp_path = mkdtemp(path_template);
    if (NULL == temp_path) {
        int saved = errno;
        free(fs);
        return -saved;
    }

    // Persist generated directory path in test state
    snprintf(fs->temp_path, sizeof(fs->temp_path), "%s", temp_path);

    *state = fs;  // Expose per-test filesystem state to the test body
    return 0;
}

int test_teardown_fs(void** state) {
    char path[512] = {0};
    test_fs_t* fs = (test_fs_t*)(*state);

    if (NULL != fs) {
        /* Known test artifacts created under the temporary directory. Each
         * entry is a filename relative to fs->temp_path. */
        for (size_t i = 0; i < fs->file_count; i++) {
            snprintf(path, sizeof(path), "%s/%s", fs->temp_path,
                     fs->test_files[i]);
            unlink(path);  // Best-effort cleanup
        }

        // Remove temporary directory
        rmdir(fs->temp_path);

        // Cleanup state
        free(fs);
        *state = NULL;
    }

    return 0;
}
