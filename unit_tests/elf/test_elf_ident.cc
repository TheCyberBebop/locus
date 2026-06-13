#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "gtest_fixtures.hpp"
#include "gtest_utilities.h"

namespace {

constexpr size_t kPathSize = 512;
constexpr size_t kTestImageSize = 64;

}  // namespace

class ElfIdentFsTest : public FilesystemTest {};

/* -------------------------------------------------------------------------- */
/* elf_validate_ident() tests                                                 */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_validate_ident() defensively rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
TEST(ElfValidateIdentTest, RejectsInvalidParams) {
    elf_image_t img{};
    elf_ident_t out{};

    EXPECT_EQ(elf_validate_ident(nullptr, &out), -EINVAL);
    EXPECT_EQ(elf_validate_ident(&img, nullptr), -EINVAL);
}

/*
 * Verify that elf_validate_ident() rejects an elf_image_t with a nullptr base.
 *
 * The elf_ident module requires a valid byte view (img->base/img->size).
 * A nullptr base indicates an uninitialized or corrupted image and must fail.
 */
TEST(ElfValidateIdentTest, RejectsImageBaseNull) {
    elf_image_t img{};
    elf_ident_t out{};

    img.base = nullptr;
    img.size = 64;
    img.path = "base_null_test";

    EXPECT_EQ(elf_validate_ident(&img, &out), -EINVAL);
}

/*
 * Verify that elf_validate_ident() rejects an inconsistent elf_image_t state
 * where base is non-nullptr but size is zero.
 *
 * This test opens a real file to obtain a valid mapping, then corrupts img.size
 * to validate defensive checks in elf_validate_ident(). The image is restored
 * so it can be cleanly closed afterward.
 */
TEST_F(ElfIdentFsTest, RejectsImageSizeZero) {
    char path[kPathSize]{};
    elf_image_t img{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    CreateImage("size_zero.bin", buf, sizeof(buf), &img, path, sizeof(path));

    // Save mapping info so we can clean up even after we corrupt img
    size_t saved_size = img.size;

    img.size = 0;  // Force inconsistent state
    elf_ident_t out{};
    EXPECT_EQ(elf_validate_ident(&img, &out), -EINVAL);

    img.size = saved_size;  // Restore for clean close
    EXPECT_EQ(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() rejects an image with an invalid ELF magic.
 */
TEST_F(ElfIdentFsTest, RejectsInvalidMagic) {
    elf_image_t img{};
    elf_ident_t ident{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    // Corrupt the magic bytes so the file is not recognized as ELF
    buf[EI_MAG0] = 0xAA;
    buf[EI_MAG1] = 0xBB;
    buf[EI_MAG2] = 0xCC;
    buf[EI_MAG3] = 0xDD;

    CreateImage("invalid_magic.bin", buf, sizeof(buf), &img);

    EXPECT_EQ(elf_validate_ident(&img, &ident), -EINVAL);
    EXPECT_EQ(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() rejects unsupported EI_CLASS values.
 */
TEST_F(ElfIdentFsTest, RejectsInvalidClass) {
    elf_image_t img{};
    elf_ident_t ident{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    // Valid ELF magic, but unsupported class
    buf[EI_MAG0] = ELFMAG0;
    buf[EI_MAG1] = ELFMAG1;
    buf[EI_MAG2] = ELFMAG2;
    buf[EI_MAG3] = ELFMAG3;
    buf[EI_CLASS] = 0xAA;

    CreateImage("invalid_ei_class.bin", buf, sizeof(buf), &img);

    EXPECT_EQ(elf_validate_ident(&img, &ident), -ENOTSUP);
    EXPECT_EQ(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() rejects unsupported EI_DATA values.
 */
TEST_F(ElfIdentFsTest, RejectsInvalidData) {
    elf_image_t img{};
    elf_ident_t ident{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    // Valid magic/class, but unsupported endianness encoding
    buf[EI_MAG0] = ELFMAG0;
    buf[EI_MAG1] = ELFMAG1;
    buf[EI_MAG2] = ELFMAG2;
    buf[EI_MAG3] = ELFMAG3;
    buf[EI_CLASS] = ELFCLASS64;
    buf[EI_DATA] = 0xAA;

    CreateImage("invalid_ei_data.bin", buf, sizeof(buf), &img);

    EXPECT_EQ(elf_validate_ident(&img, &ident), -ENOTSUP);
    EXPECT_EQ(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() rejects unexpected EI_VERSION values.
 */
TEST_F(ElfIdentFsTest, RejectsInvalidVersion) {
    elf_image_t img{};
    elf_ident_t ident{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    // Valid magic/class/data, but invalid ident version
    buf[EI_MAG0] = ELFMAG0;
    buf[EI_MAG1] = ELFMAG1;
    buf[EI_MAG2] = ELFMAG2;
    buf[EI_MAG3] = ELFMAG3;
    buf[EI_CLASS] = ELFCLASS64;
    buf[EI_DATA] = ELFDATA2LSB;
    buf[EI_VERSION] = 0xAA;

    CreateImage("invalid_ei_version.bin", buf, sizeof(buf), &img);

    EXPECT_EQ(elf_validate_ident(&img, &ident), -EINVAL);
    EXPECT_EQ(elf_image_close(&img), 0);
}

/*
 * Verify that elf_validate_ident() accepts a well-formed e_ident and populates
 * the decoder configuration correctly.
 */
TEST_F(ElfIdentFsTest, AcceptsValidElf) {
    elf_image_t img{};
    elf_ident_t ident{};
    uint8_t buf[kTestImageSize];

    std::memset(buf, 0xFF, sizeof(buf));

    buf[EI_MAG0] = ELFMAG0;
    buf[EI_MAG1] = ELFMAG1;
    buf[EI_MAG2] = ELFMAG2;
    buf[EI_MAG3] = ELFMAG3;
    buf[EI_CLASS] = ELFCLASS64;
    buf[EI_DATA] = ELFDATA2LSB;
    buf[EI_VERSION] = EV_CURRENT;
    buf[EI_OSABI] = ELFOSABI_NONE;
    buf[EI_ABIVERSION] = 0;

    CreateImage("ident_success.bin", buf, sizeof(buf), &img);

    EXPECT_EQ(elf_validate_ident(&img, &ident), 0);
    EXPECT_EQ(elf_image_close(&img), 0);
}
