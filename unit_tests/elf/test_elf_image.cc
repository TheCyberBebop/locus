#include <sys/mman.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include "elf_image.h"
#include "gtest_fixtures.hpp"
#include "gtest_utilities.h"

namespace {

constexpr size_t kPathSize = 512;
constexpr size_t kTestImageSize = 64;

void ExpectImageCleared(const elf_image_t& img) {
    EXPECT_EQ(img.base, nullptr);
    EXPECT_EQ(img.size, 0u);
    EXPECT_EQ(img.path, nullptr);
}

void ExpectImageMapped(const elf_image_t& img,
                       const uint8_t* expected_bytes,
                       size_t expected_size,
                       const char* expected_path) {
    ASSERT_NE(img.base, nullptr);
    EXPECT_EQ(img.size, expected_size);
    EXPECT_EQ(img.path, expected_path);
    EXPECT_EQ(std::memcmp(img.base, expected_bytes, expected_size), 0);
}

}  // namespace

class ElfImageFsTest : public FilesystemTest {};

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

    EXPECT_EQ(elf_image_open(nullptr, &img), -EINVAL);
    EXPECT_EQ(elf_image_open("/tmp/fake_file", nullptr), -EINVAL);

    ExpectImageCleared(img);
}

/*
 * Verify that elf_image_open() rejects a non-existent filesystem path.
 *
 * This test ensures that attempting to open a path that does not exist
 * fails cleanly and does not partially initialize the elf_image_t structure.
 */
TEST(ElfImageOpenTest, RejectsNonexistentPath) {
    elf_image_t img{};

    EXPECT_EQ(elf_image_open("/fake/file/path", &img), -ENOENT);

    ExpectImageCleared(img);
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

    ExpectImageCleared(img);
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
    char path[kPathSize]{};
    elf_image_t img{};
    uint8_t buf[16];  // Intentionally too small to contain an ELF header

    std::memset(buf, 0xFF, sizeof(buf));

    MakePath("too_small.bin", path, sizeof(path));

    ASSERT_EQ(test_write_file(path, buf, sizeof(buf)), 0);
    EXPECT_EQ(elf_image_open(path, &img), -EINVAL);

    ExpectImageCleared(img);
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
    char path[kPathSize]{};
    elf_image_t img{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    CreateImage("open_success.bin", buf, sizeof(buf), &img, path, sizeof(path));
    ExpectImageMapped(img, buf, sizeof(buf), path);

    EXPECT_EQ(elf_image_close(&img), 0);
    ExpectImageCleared(img);
}

/* -------------------------------------------------------------------------- */
/* elf_image_close() tests                                                    */
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
 * Verify that elf_image_close() detects inconsistent state (base != nullptr,
 * size == 0) and still clears the image safely.
 *
 * This test intentionally corrupts elf_image_t after a successful open to
 * validate defensive cleanup behavior.
 */
TEST_F(ElfImageFsTest, CloseRejectsImageSizeZeroAndClearsImage) {
    char path[kPathSize]{};
    elf_image_t img{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    CreateImage("size_zero.bin", buf, sizeof(buf), &img, path, sizeof(path));

    const uint8_t* saved_base = img.base;
    size_t saved_size = img.size;
    img.size = 0;

    EXPECT_EQ(elf_image_close(&img), -EINVAL);

    ExpectImageCleared(img);
    EXPECT_EQ(munmap(const_cast<uint8_t*>(saved_base), saved_size), 0);
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
    char path[kPathSize]{};
    elf_image_t img{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    CreateImage("munmap_error.bin", buf, sizeof(buf), &img, path, sizeof(path));

    ASSERT_NE(img.base, nullptr);
    ASSERT_GT(img.size, 0u);
    ASSERT_EQ(munmap(const_cast<uint8_t*>(img.base), img.size), 0);

    // Force munmap() to fail while preserving a non-zero mapping size
    img.base = reinterpret_cast<const uint8_t*>(0xDEADBEEFu);

    EXPECT_EQ(elf_image_close(&img), -EINVAL);

    ExpectImageCleared(img);
}

/*
 * Verify that elf_image_close() detects inconsistent state (base == nullptr,
 * size != 0) and returns an error while still clearing the image.
 *
 * This test intentionally corrupts an elf_image_t after a successful open to
 * validate defensive cleanup behavior. Because clearing img.base would
 * otherwise leak the mapping, the test saves the original mapping information
 * and performs explicit munmap() cleanup.
 */
TEST_F(ElfImageFsTest, CloseRejectsImageBaseNullAndClearsImage) {
    char path[kPathSize]{};
    elf_image_t img{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    CreateImage("base_null.bin", buf, sizeof(buf), &img, path, sizeof(path));

    const uint8_t* saved_base = img.base;
    size_t saved_size = img.size;
    // Corrupt the image after a successful open to exercise defensive cleanup
    img.base = nullptr;

    EXPECT_EQ(elf_image_close(&img), -EINVAL);
    ExpectImageCleared(img);
    EXPECT_EQ(munmap(const_cast<uint8_t*>(saved_base), saved_size), 0);
}

/*
 * Verify that elf_image_close() can be performed multiple times.
 *
 * Closing a successfully-opened image must succeed, clear fields, and remain
 * safe to call again on the same elf_image_t (no double-unmap or reuse bugs).
 */
TEST_F(ElfImageFsTest, CloseAllowsDoubleClose) {
    char path[kPathSize]{};
    elf_image_t img{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xAA, sizeof(buf));

    CreateImage("double_close.bin", buf, sizeof(buf), &img, path, sizeof(path));

    EXPECT_EQ(elf_image_close(&img), 0);
    ExpectImageCleared(img);

    EXPECT_EQ(elf_image_close(&img), 0);
    ExpectImageCleared(img);
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

    ExpectImageCleared(img);
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
    char path1[kPathSize]{};
    char path2[kPathSize]{};
    elf_image_t img{};
    uint8_t buf1[kTestImageSize];
    uint8_t buf2[kTestImageSize];

    std::memset(buf1, 0xFF, sizeof(buf1));
    std::memset(buf2, 0xAA, sizeof(buf2));

    CreateImage("reuse_one.bin", buf1, sizeof(buf1), &img, path1,
                sizeof(path1));
    ExpectImageMapped(img, buf1, sizeof(buf1), path1);

    EXPECT_EQ(elf_image_close(&img), 0);
    ExpectImageCleared(img);

    CreateImage("reuse_two.bin", buf2, sizeof(buf2), &img, path2,
                sizeof(path2));
    ExpectImageMapped(img, buf2, sizeof(buf2), path2);

    EXPECT_EQ(elf_image_close(&img), 0);
    ExpectImageCleared(img);
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
    char path[kPathSize]{};
    elf_image_t img{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    CreateImage("close_success.bin", buf, sizeof(buf), &img, path,
                sizeof(path));

    EXPECT_EQ(elf_image_close(&img), 0);
    ExpectImageCleared(img);
}
