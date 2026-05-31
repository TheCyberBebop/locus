#include <sys/mman.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

extern "C" {
#include "elf_image.h"
#include "test_utilities.h"
}

class ElfImageFsTest : public ::testing::Test {
   protected:
    void* state = nullptr;
    test_fs_t* fs = nullptr;

    void SetUp() override {
        ASSERT_EQ(test_setup_fs(&state), 0);
        ASSERT_NE(state, nullptr);
        fs = static_cast<test_fs_t*>(state);
    }

    void TearDown() override {
        EXPECT_EQ(test_teardown_fs(&state), 0);
        EXPECT_EQ(state, nullptr);
        fs = nullptr;
    }
};

/* -------------------------------------------------------------------------- */
/* elf_image_open() tests                                                     */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_image_open() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
TEST(ElfImageOpenTest, RejectsInvalidParams) {
    elf_image_t img{};

    EXPECT_EQ(elf_image_open(NULL, &img), -EINVAL);
    EXPECT_EQ(elf_image_open("/tmp/fake_file", NULL), -EINVAL);

    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
}

/*
 * Verify that elf_image_open() rejects a non-existent filesystem path.
 *
 * This test ensures that attempting to open a path that does not exist
 * fails cleanly and does not partially initialize the elf_image_t structure.
 */
TEST(ElfImageOpenTest, OpenRejectsInvalidPath) {
    elf_image_t img{};
    EXPECT_EQ(elf_image_open("/fake/file/path", &img), -ENOENT);

    // Verify fields remained cleared on error
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0);
    EXPECT_EQ(img.path, nullptr);
}

/*
 * Verify that elf_image_open() rejects directory paths.
 *
 * The elf_image abstraction is defined only for regular files. Attempting
 * to open a directory must fail without creating a mapping or modifying
 * the elf_image_t state.
 */
TEST_F(ElfImageFsTest, OpenRejectsDirectoryPath) {
    elf_image_t img{};

    EXPECT_EQ(elf_image_open(fs->temp_path, &img), -EINVAL);

    // Verify fields remained cleared on error
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
}

/*
 * Verify that elf_image_open() rejects files too small to contain an ELF
 * header.
 *
 * This test constructs a file smaller than sizeof(Elf32_Ehdr) and confirms
 * that elf_image_open() fails defensively, without mapping the file or
 * partially initializing elf_image_t.
 */
TEST_F(ElfImageFsTest, OpenRejectsFileTooSmall) {
    const char* test_file = "too_small.bin";

    register_test_file(fs,
                       test_file);  // Ensure file is removed during teardown

    // Construct a test file path under the temporary directory
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    // Intentionally smaller than sizeof(Elf32_Ehdr)
    uint8_t buf[16];
    memset(buf, 0xFF, sizeof(buf));
    ASSERT_EQ(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img{};
    EXPECT_EQ(elf_image_open(path, &img), -EINVAL);

    // Verify fields remained cleared on error
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
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
TEST_F(ElfImageFsTest, OpenSuccess) {
    const char* test_file = "open_success.bin";
    register_test_file(fs, test_file);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    ASSERT_EQ(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img{};
    ASSERT_EQ(elf_image_open(path, &img), 0);

    ASSERT_NE(img.base, nullptr);
    EXPECT_EQ(img.size, sizeof(buf));
    EXPECT_EQ(img.path, path);
    EXPECT_EQ(memcmp(img.base, buf, sizeof(buf)), 0);

    EXPECT_EQ(elf_image_close(&img), 0);
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
}

/* -------------------------------------------------------------------------- */
/* elf_image_close() tests */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_image_close() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
TEST(ElfImageCloseTest, RejectsInvalidParams) {
    EXPECT_EQ(elf_image_close(nullptr), -EINVAL);
}

/*
 * Verify that elf_image_close() detects inconsistent state (base != NULL,
 * size == 0) and still clears the image safely.
 *
 * This test intentionally corrupts elf_image_t after a successful open to
 * validate defensive cleanup behavior.
 */
TEST_F(ElfImageFsTest, CloseRejectsImageSizeZeroAndClearsImage) {
    const char* test_file = "size_zero.bin";
    register_test_file(fs, test_file);

    char path[512]{};
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    ASSERT_EQ(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img{};
    ASSERT_EQ(elf_image_open(path, &img), 0);

    const uint8_t* saved_base = img.base;
    size_t saved_size = img.size;

    img.size = 0;
    EXPECT_EQ(elf_image_close(&img), -EINVAL);

    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);

    EXPECT_EQ(munmap((void*)saved_base, saved_size), 0);
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
TEST_F(ElfImageFsTest, CloseReturnsErrorWhenMunmapFails) {
    const char* test_file = "munmap_error.bin";
    register_test_file(fs, test_file);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    ASSERT_EQ(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img{};
    ASSERT_EQ(elf_image_open(path, &img), 0);

    ASSERT_NE(img.base, nullptr);
    ASSERT_GT(img.size, 0u);
    ASSERT_EQ(munmap((void*)img.base, img.size), 0);

    img.base = reinterpret_cast<const uint8_t*>(0xDEADBEEF);

    EXPECT_EQ(elf_image_close(&img), -EINVAL);

    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
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
TEST_F(ElfImageFsTest, CloseRejectsImageBaseNullAndClearsImage) {
    const char* test_file = "base_null.bin";
    register_test_file(fs, test_file);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    ASSERT_EQ(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img{};
    ASSERT_EQ(elf_image_open(path, &img), 0);

    const uint8_t* saved_base = img.base;
    size_t saved_size = img.size;

    img.base = nullptr;
    EXPECT_EQ(elf_image_close(&img), -EINVAL);

    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);

    EXPECT_EQ(munmap((void*)saved_base, saved_size), 0);
}

/*
 * Verify that elf_image_close() can be performed multiple times.
 *
 * Closing a successfully-opened image must succeed, clear fields, and remain
 * safe to call again on the same elf_image_t (no double-unmap or reuse bugs).
 */

TEST_F(ElfImageFsTest, CloseAllowsDoubleClose) {
    const char* test_file = "double_close.bin";
    register_test_file(fs, test_file);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    uint8_t buf[64];
    memset(buf, 0xAA, sizeof(buf));
    ASSERT_EQ(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img{};
    ASSERT_EQ(elf_image_open(path, &img), 0);

    EXPECT_EQ(elf_image_close(&img), 0);
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);

    EXPECT_EQ(elf_image_close(&img), 0);
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
}

/*
 * Verify that elf_image_close() succeeds on an empty (never-opened) image.
 *
 * Callers often perform unconditional cleanup in error paths. This test ensures
 * that closing a zero-initialized elf_image_t is a safe no-op that returns 0
 * and leaves fields in an empty state.
 */
TEST(ElfImageCloseTest, AllowsEmptyImage) {
    elf_image_t img{};

    EXPECT_EQ(elf_image_close(&img), 0);

    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
}

/*
 * Verify that an elf_image_t can be safely reused across multiple open/close
 * cycles.
 *
 * This test opens and closes two different files sequentially using the same
 * elf_image_t. Each open must fully reinitialize the structure, and each close
 * must fully clear it.
 */
TEST_F(ElfImageFsTest, OpenCloseReuseSameImage) {
    const char* test_file1 = "reuse_one.bin";
    const char* test_file2 = "reuse_two.bin";

    register_test_file(fs, test_file1);
    register_test_file(fs, test_file2);

    char path1[512];
    char path2[512];
    snprintf(path1, sizeof(path1), "%s/%s", fs->temp_path, test_file1);
    snprintf(path2, sizeof(path2), "%s/%s", fs->temp_path, test_file2);

    uint8_t buf1[64];
    uint8_t buf2[64];
    memset(buf1, 0xFF, sizeof(buf1));
    memset(buf2, 0xAA, sizeof(buf2));

    ASSERT_EQ(test_write_file(path1, buf1, sizeof(buf1)), 0);
    ASSERT_EQ(test_write_file(path2, buf2, sizeof(buf2)), 0);

    elf_image_t img{};

    ASSERT_EQ(elf_image_open(path1, &img), 0);
    ASSERT_NE(img.base, nullptr);
    EXPECT_EQ(img.size, sizeof(buf1));
    EXPECT_EQ(img.path, path1);
    EXPECT_EQ(memcmp(img.base, buf1, sizeof(buf1)), 0);

    EXPECT_EQ(elf_image_close(&img), 0);
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);

    ASSERT_EQ(elf_image_open(path2, &img), 0);
    ASSERT_NE(img.base, nullptr);
    EXPECT_EQ(img.size, sizeof(buf2));
    EXPECT_EQ(img.path, path2);
    EXPECT_EQ(memcmp(img.base, buf2, sizeof(buf2)), 0);

    EXPECT_EQ(elf_image_close(&img), 0);
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
}

/*
 * Verify that elf_image_close() succeeds for a valid, mapped image and clears
 * all fields afterward.
 *
 * This test covers the normal success path: open a file, map it, then close it.
 * On success, elf_image_close() must return 0 and leave the image in an empty,
 * safe-to-reuse state.
 */
TEST_F(ElfImageFsTest, CloseSuccess) {
    const char* test_file = "close_success.bin";
    register_test_file(fs, test_file);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", fs->temp_path, test_file);

    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    ASSERT_EQ(test_write_file(path, buf, sizeof(buf)), 0);

    elf_image_t img{};
    ASSERT_EQ(elf_image_open(path, &img), 0);
    EXPECT_EQ(elf_image_close(&img), 0);

    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
}
