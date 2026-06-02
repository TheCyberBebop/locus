#ifndef GTEST_FIXTURES_HPP
#define GTEST_FIXTURES_HPP

#include <gtest/gtest.h>

#include "elf_image.h"
#include "gtest_utilities.h"

/*
 * Shared GoogleTest fixture for tests that require a temporary filesystem.
 *
 * Each test receives an isolated temporary directory created during SetUp()
 * and removed during TearDown(). Helper functions provide common operations
 * such as path construction and creation of ELF-backed test files.
 */
class FilesystemTest : public ::testing::Test {
   protected:
    void* state = nullptr;
    test_fs_t* fs = nullptr;

    /*
     * Create per-test filesystem state and expose the typed test context.
     */
    void SetUp() override {
        ASSERT_EQ(test_setup_fs(&state), 0);
        ASSERT_NE(state, nullptr);
        fs = static_cast<test_fs_t*>(state);
    }

    /*
     * Remove test artifacts and temporary directories created by the test.
     */
    void TearDown() override {
        EXPECT_EQ(test_teardown_fs(&state), 0);
        EXPECT_EQ(state, nullptr);
        fs = nullptr;
    }

    /*
     * Construct a path within the test's temporary directory and register
     * the file for automatic cleanup during TearDown().
     */
    void MakePath(const char* file_name, char* path, size_t path_size) {
        register_test_file(fs, file_name);

        snprintf(path, path_size, "%s/%s", fs->temp_path, file_name);
    }

    /*
     * Create a test file, write the supplied contents, and open it as an
     * elf_image_t. The generated path is internal to the helper.
     */
    void CreateImage(const char* file_name,
                     const uint8_t* buf,
                     size_t len,
                     elf_image_t* img) {
        char path[512]{};

        MakePath(file_name, path, sizeof(path));

        ASSERT_EQ(test_write_file(path, buf, len), 0);
        ASSERT_EQ(elf_image_open(path, img), 0);
    }

    /*
     * Create a test file, write the supplied contents, and open it as an
     * elf_image_t. The caller receives the generated filesystem path.
     */
    void CreateImage(const char* file_name,
                     const uint8_t* buf,
                     size_t len,
                     elf_image_t* img,
                     char* path,
                     size_t path_size) {
        MakePath(file_name, path, path_size);

        ASSERT_EQ(test_write_file(path, buf, len), 0);
        ASSERT_EQ(elf_image_open(path, img), 0);
    }

    /*
     * Fill a buffer with non-zero data so tests can selectively overwrite
     * fields relevant to the scenario being validated.
     */
    static void FillBuffer(uint8_t* buf, size_t size) {
        memset(buf, 0xFF, size);
    }
};

#endif  // GTEST_FIXTURES_HPP
