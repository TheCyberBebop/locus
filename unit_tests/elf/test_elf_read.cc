#include <cerrno>
#include <cstddef>
#include <cstring>

#include <gtest/gtest.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "elf_read.h"
#include "gtest_fixtures.hpp"
#include "gtest_utilities.h"

namespace {

constexpr uint32_t kBadValue = 0xDEADBEEFu;

using ReadFn = int (*)(const elf_image_t* img,
                       const elf_ident_t* ident,
                       size_t offset,
                       void* out);

/* Wrapper functions so u16/u32/u64 can share one table-driven test. */
int ElfReadU16Wrapper(const elf_image_t* img,
                             const elf_ident_t* ident,
                             size_t offset,
                             void* out) {
    return elf_read_u16(img, ident, offset, static_cast<uint16_t*>(out));
}

int ElfReadU32Wrapper(const elf_image_t* img,
                             const elf_ident_t* ident,
                             size_t offset,
                             void* out) {
    return elf_read_u32(img, ident, offset, static_cast<uint32_t*>(out));
}

int ElfReadU64Wrapper(const elf_image_t* img,
                             const elf_ident_t* ident,
                             size_t offset,
                             void* out) {
    return elf_read_u64(img, ident, offset, static_cast<uint64_t*>(out));
}

/* -------------------------------------------------------------------------- */
/* elf_read_u*() tests                                                        */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_read_u16/u32/u64() defensively reject invalid arguments.
 *
 * This table-driven test applies the same invalid-parameter scenarios to each
 * width-specific read helper.
 */
TEST(ElfReadTest, RejectsInvalidParams) {
    elf_image_t img{};
    elf_ident_t ident{};
    uint16_t out16 = 0;
    uint32_t out32 = 0;
    uint64_t out64 = 0;

    struct ReadCase {
        const char* name;
        ReadFn fn;
        void* out;
    };

    const ReadCase cases[] = {
        {"u16", ElfReadU16Wrapper, &out16},
        {"u32", ElfReadU32Wrapper, &out32},
        {"u64", ElfReadU64Wrapper, &out64},
    };

    for (const auto& test_case : cases) {
        EXPECT_EQ(test_case.fn(nullptr, &ident, 0, test_case.out), -EINVAL)
            << test_case.name;
        EXPECT_EQ(test_case.fn(&img, nullptr, 0, test_case.out), -EINVAL)
            << test_case.name;
        EXPECT_EQ(test_case.fn(&img, &ident, 0, nullptr), -EINVAL)
            << test_case.name;
    }
}

/*
 * Verify that elf_read_u32() rejects an elf_image_t with a NULL base pointer.
 *
 * The elf_read_u* helpers require a valid byte view (img->base/img->size).
 * A NULL base indicates an uninitialized/corrupted image and must fail.
 */
TEST(ElfReadTest, RejectsImageBaseNull) {
    elf_image_t img = {
        .base = NULL, /* Ensure we fail on base == NULL */
        .size = 64,
        .path = "base_null_test",
    };

    elf_ident_t ident{};
    ident.ei_data = ELFDATA2LSB;  // Set to little-endian

    uint32_t out = 0;
    ASSERT_EQ(elf_read_u32(&img, &ident, 0, &out), -EINVAL);
}

/*
 * Verify that elf_read_u32() rejects unsupported ELF data encodings.
 *
 * The read helpers must know the file's endianness (EI_DATA) in order to
 * decode multi-byte fields. Any EI_DATA value other than ELFDATA2LSB or
 * ELFDATA2MSB must be rejected with -ENOTSUP.
 */
TEST(ElfReadTest, RejectsInvalidEiData) {
    uint8_t bytes[8]{};

    elf_image_t img{
        .base = bytes,
        .size = sizeof(bytes),
        .path = "invalid_ei_data_test",
    };

    elf_ident_t ident{};
    ident.ei_data = 0xFFu;

    uint32_t out = kBadValue;

    EXPECT_EQ(elf_read_u32(&img, &ident, 0, &out), -ENOTSUP);
    EXPECT_EQ(out, kBadValue);
}

/*
 * Verify that elf_read_u16/u32/u64() enforce bounds correctly.
 *
 * For each width, this test checks the three key edge cases:
 *  - offset == size             → failure
 *  - offset == size - width + 1 → failure
 *  - offset == size - width     → success
 */
TEST(ElfReadTest, EnforcesBoundsChecks) {
    // Common buffer size large enough for u64 tests
    uint8_t bytes[16]{};

    elf_image_t img{
        .base = bytes,
        .size = sizeof(bytes),
        .path = "bounds_edges_table",
    };

    elf_ident_t ident{};
    ident.ei_data = ELFDATA2LSB;  // Set to little-endian

    struct ReadCase {
        const char* name;
        ReadFn fn;
        size_t width;
        size_t last_ok_offset;
        uint64_t expected;
    };

    const ReadCase cases[] = {
        {"u16", ElfReadU16Wrapper, 2, 14, 0xAABBu},
        {"u32", ElfReadU32Wrapper, 4, 12, 0xAABBCCDDu},
        {"u64", ElfReadU64Wrapper, 8, 8, 0x1122334455667788ULL},
    };

    for (const auto& test_case : cases) {
        // Clear buffer each iteration
        std::memset(bytes, 0, sizeof(bytes));

        // Write the value in little-endian order at last_ok_offset
        const size_t offset = test_case.last_ok_offset;
        const uint64_t value = test_case.expected;

        for (size_t byte = 0; byte < test_case.width; ++byte) {
            bytes[offset + byte] =
                static_cast<uint8_t>((value >> (8 * byte)) & 0xFFu);
        }

        // offset == file_size -> out-of-bounds
        uint64_t out64 = 0;
        EXPECT_EQ(test_case.fn(&img, &ident, img.size, &out64), -EINVAL)
            << test_case.name;

        /* offset == file_size - width + 1 → out-of-bounds (img->size - offset <
         * width) */
        out64 = 0;
        EXPECT_EQ(test_case.fn(&img, &ident, offset + 1, &out64), -EINVAL)
            << test_case.name;

        // offset == file_size - width → success
        out64 = 0;
        EXPECT_EQ(test_case.fn(&img, &ident, offset, &out64), 0)
            << test_case.name;

        // Validate success with specific read size results
        if (test_case.width == 2) {
            EXPECT_EQ(static_cast<uint16_t>(out64),
                      static_cast<uint16_t>(test_case.expected))
                << test_case.name;
        } else if (test_case.width == 4) {
            EXPECT_EQ(static_cast<uint32_t>(out64),
                      static_cast<uint32_t>(test_case.expected))
                << test_case.name;
        } else {
            EXPECT_EQ(out64, test_case.expected) << test_case.name;
        }
    }
}

/*
 * Verify that elf_read_u16/u32/u64() correctly decode big-endian (MSB) values.
 *
 * This test exercises the byte-swap path when the ELF file encoding is
 * ELFDATA2MSB. It is portable across host endianness because the expected
 * values are compared in host order.
 */
TEST(ElfReadTest, DecodesBigEndianValues) {
    // Common buffer size large enough for u64 tests
    uint8_t bytes[16]{};

    elf_image_t img{
        .base = bytes,
        .size = sizeof(bytes),
        .path = "success_msb_table",
    };

    elf_ident_t ident{};
    ident.ei_data = ELFDATA2MSB;  // Set to big-endian

    struct ReadCase {
        const char* name;
        ReadFn fn;
        size_t width;
        size_t offset;
        uint64_t expected;
    };

    /* Non-zero offset to verify alignment-safe reads (memcpy-based) */
    const ReadCase cases[] = {
        {"u16", ElfReadU16Wrapper, 2, 2, 0xAABBu},
        {"u32", ElfReadU32Wrapper, 4, 2, 0xAABBCCDDu},
        {"u64", ElfReadU64Wrapper, 8, 2, 0x1122334455667788ULL},
    };

    for (const auto& test_case : cases) {
        // Clear buffer each iteration
        std::memset(bytes, 0, sizeof(bytes));

        // Write expected value in big-endian order at the requested offset
        const uint64_t value = test_case.expected;
        for (size_t byte = 0; byte < test_case.width; ++byte) {
            const size_t shift = 8 * (test_case.width - 1 - byte);

            bytes[test_case.offset + byte] =
                static_cast<uint8_t>((value >> shift) & 0xFFu);
        }

        uint64_t out64 = 0;
        EXPECT_EQ(test_case.fn(&img, &ident, test_case.offset, &out64), 0)
            << test_case.name;

        // Validate success with specific read size results
        if (test_case.width == 2) {
            EXPECT_EQ(static_cast<uint16_t>(out64),
                      static_cast<uint16_t>(test_case.expected))
                << test_case.name;
        } else if (test_case.width == 4) {
            EXPECT_EQ(static_cast<uint32_t>(out64),
                      static_cast<uint32_t>(test_case.expected))
                << test_case.name;
        } else {
            EXPECT_EQ(out64, test_case.expected) << test_case.name;
        }
    }
}

/*
 * Verify that elf_read_u16/u32/u64() correctly decode little-endian values
 * at unaligned offsets.
 *
 * This test exercises the memcpy-based read path (alignment-safe) and ensures
 * each width-specific helper returns the expected host-order value.
 */
TEST(ElfReadTest, DecodesLittleEndianValuesAtUnalignedOffsets) {
    // Common buffer size large enough for u64 tests
    uint8_t bytes[16]{};

    elf_image_t img{
        .base = bytes,
        .size = sizeof(bytes),
        .path = "success_lsb_unaligned_table",
    };

    elf_ident_t ident{};
    ident.ei_data = ELFDATA2LSB;  // Set to little-endian

    struct ReadCase {
        const char* name;
        ReadFn fn;
        size_t width;
        size_t offset;
        uint64_t expected;
    };

    /* Non-zero offset to verify alignment-safe reads (memcpy-based) */
    const ReadCase cases[] = {
        {"u16", ElfReadU16Wrapper, 2, 2, 0xAABBu},
        {"u32", ElfReadU32Wrapper, 4, 2, 0xAABBCCDDu},
        {"u64", ElfReadU64Wrapper, 8, 2, 0x1122334455667788ULL},
    };

    for (const auto& test_case : cases) {
        // Clear buffer each iteration
        std::memset(bytes, 0, sizeof(bytes));

        // Write expected value in little-endian order at the requested offset
        const uint64_t value = test_case.expected;
        for (size_t byte = 0; byte < test_case.width; ++byte) {
            bytes[test_case.offset + byte] =
                static_cast<uint8_t>((value >> (8 * byte)) & 0xFFu);
        }

        uint64_t out64 = 0;
        EXPECT_EQ(test_case.fn(&img, &ident, test_case.offset, &out64), 0)
            << test_case.name;

        // Validate success with specific read size results
        if (test_case.width == 2) {
            EXPECT_EQ(static_cast<uint16_t>(out64),
                      static_cast<uint16_t>(test_case.expected))
                << test_case.name;
        } else if (test_case.width == 4) {
            EXPECT_EQ(static_cast<uint32_t>(out64),
                      static_cast<uint32_t>(test_case.expected))
                << test_case.name;
        } else {
            EXPECT_EQ(out64, test_case.expected) << test_case.name;
        }
    }
}

/*
 * Verify that elf_read_u32() rejects offsets that would overflow bounds checks.
 *
 * This test is a defensive invariant: the bounds logic must not use
 * `offset + width` directly (which can wrap). The implementation should use an
 * overflow-safe form such as:
 *   offset > size || size - offset < width
 *
 * We pass offset = SIZE_MAX, which must be rejected cleanly with -EINVAL.
 */
TEST(ElfReadTest, RejectsOffsetOverflow) {
    uint8_t bytes[16]{};

    elf_image_t img{
        .base = bytes,
        .size = sizeof(bytes),
        .path = "offset_overflow",
    };

    elf_ident_t ident{};
    ident.ei_data = ELFDATA2LSB;  // Set to little-endian

    // Seed out to confirm failure doesn't clobber it
    uint32_t out = kBadValue;

    EXPECT_EQ(elf_read_u32(&img, &ident, static_cast<size_t>(-1), &out),
              -EINVAL);
    EXPECT_EQ(out, kBadValue);
}

}  // namespace
