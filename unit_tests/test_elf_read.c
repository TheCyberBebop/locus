#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "elf_ident.h"
#include "elf_image.h"
#include "elf_read.h"
#include "test_suites.h"
#include "test_utilities.h"

typedef int (*read_fn_t)(const elf_image_t* img,
                         const elf_ident_info_t* ident,
                         size_t offset,
                         void* out);

/* Wrapper functions so u16/u32/u64 can share one table-driven test. */
static int elf_read_u16_wrapper(const elf_image_t* img,
                                const elf_ident_info_t* ident,
                                size_t offset,
                                void* out) {
    return elf_read_u16(img, ident, offset, (uint16_t*)out);
}

static int elf_read_u32_wrapper(const elf_image_t* img,
                                const elf_ident_info_t* ident,
                                size_t offset,
                                void* out) {
    return elf_read_u32(img, ident, offset, (uint32_t*)out);
}

static int elf_read_u64_wrapper(const elf_image_t* img,
                                const elf_ident_info_t* ident,
                                size_t offset,
                                void* out) {
    return elf_read_u64(img, ident, offset, (uint64_t*)out);
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
static void test_elf_read_invalid_params_table(void** state) {
    (void)state;  // Unused; no filesystem setup required

    elf_image_t img = {0};
    elf_ident_info_t ident = {0};
    uint16_t out16 = 0;
    uint32_t out32 = 0;
    uint64_t out64 = 0;

    struct {
        const char* name;
        read_fn_t fn;
        void* out;
    } cases[] = {
        {"u16", elf_read_u16_wrapper, &out16},
        {"u32", elf_read_u32_wrapper, &out32},
        {"u64", elf_read_u64_wrapper, &out64},
    };

    // Validate each parameter as NULL
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert_int_equal(cases[i].fn(NULL, &ident, 0, cases[i].out), -EINVAL);
        assert_int_equal(cases[i].fn(&img, NULL, 0, cases[i].out), -EINVAL);
        assert_int_equal(cases[i].fn(&img, &ident, 0, NULL), -EINVAL);
    }
}

/*
 * Verify that elf_read_u32() rejects an elf_image_t with a NULL base pointer.
 *
 * The elf_read_u* helpers require a valid byte view (img->base/img->size).
 * A NULL base indicates an uninitialized/corrupted image and must fail.
 */
static void test_elf_read_u32_image_base_null(void** state) {
    (void)state;  // Unused; pure in-memory test

    elf_image_t img = {
        .base = NULL, /* Ensure we fail on base == NULL */
        .size = 64,
        .path = "base_null_test",
    };

    elf_ident_info_t ident = {0};
    ident.ei_data = ELFDATA2LSB;

    uint32_t out = 0;
    assert_int_equal(elf_read_u32(&img, &ident, 0, &out), -EINVAL);
}

/*
 * Verify that elf_read_u32() rejects unsupported ELF data encodings.
 *
 * The read helpers must know the file's endianness (EI_DATA) in order to
 * decode multi-byte fields. Any EI_DATA value other than ELFDATA2LSB or
 * ELFDATA2MSB must be rejected with -ENOTSUP.
 */
static void test_elf_read_u32_invalid_ei_data(void** state) {
    (void)state;  // Unused; pure in-memory test

    uint8_t bytes[8] = {0};

    elf_image_t img = {
        .base = bytes,
        .size = sizeof(bytes),
        .path = "invalid_ei_data_test",
    };

    elf_ident_info_t ident = {0};
    ident.ei_data = 0xFFu;  // Unsupported endianness encoding

    uint32_t out = 0xDEADBEEFu;
    assert_int_equal(elf_read_u32(&img, &ident, 0, &out), -ENOTSUP);
    assert_int_equal(out, 0xDEADBEEFu);  // Ensure out is not altered
}

/*
 * Verify that elf_read_u16/u32/u64() enforce bounds correctly.
 *
 * For each width, this test checks the three key edge cases:
 *  - offset == size             → failure
 *  - offset == size - width + 1 → failure
 *  - offset == size - width     → success
 */
static void test_elf_read_bounds_edges_table(void** state) {
    (void)state;  // Unused; pure in-memory test

    // Common buffer size large enough for u64 tests
    uint8_t bytes[16] = {0};

    elf_image_t img = {
        .base = bytes,
        .size = sizeof(bytes),
        .path = "bounds_edges_table",
    };

    elf_ident_info_t ident = {.ei_data = ELFDATA2LSB};

    struct {
        const char* name;
        read_fn_t fn;
        size_t width;
        size_t last_ok_offset;
        uint64_t expected;
    } cases[] = {
        {"u16", elf_read_u16_wrapper, 2, 14, 0xAABBu},
        {"u32", elf_read_u32_wrapper, 4, 12, 0xAABBCCDDDu},
        {"u64", elf_read_u64_wrapper, 8, 8, 0x1122334455667788ULL},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        // Clear buffer each iteration
        memset(bytes, 0, sizeof(bytes));

        // Write the value in little-endian order at last_ok_offset
        size_t cur_offset = cases[i].last_ok_offset;
        uint64_t value = cases[i].expected;

        for (size_t byte = 0; byte < cases[i].width; byte++) {
            bytes[cur_offset + byte] = (uint8_t)((value >> (8 * byte)) & 0xFFu);
        }

        // offset == file_size → out-of-bounds
        uint64_t out64 = 0;
        assert_int_equal(cases[i].fn(&img, &ident, img.size, &out64), -EINVAL);

        /* offset == file_size - width + 1 → out-of-bounds (img->size - offset <
         * width) */
        out64 = 0;
        assert_int_equal(cases[i].fn(&img, &ident, cur_offset + 1, &out64),
                         -EINVAL);

        // offset == file_size - width → success
        out64 = 0;
        assert_int_equal(cases[i].fn(&img, &ident, cur_offset, &out64), 0);

        // Validate success with specific read size results
        if (2 == cases[i].width) {
            assert_int_equal((uint16_t)out64, (uint16_t)cases[i].expected);
        } else if (4 == cases[i].width) {
            assert_int_equal((uint32_t)out64, (uint32_t)cases[i].expected);
        } else {
            assert_int_equal(out64, cases[i].expected);
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
static void test_elf_read_success_msb_table(void** state) {
    (void)state;  // Unused; pure in-memory test

    // Common buffer size large enough for u64 tests
    uint8_t bytes[16] = {0};

    elf_image_t img = {
        .base = bytes,
        .size = sizeof(bytes),
        .path = "success_msb_table",
    };

    elf_ident_info_t ident = {.ei_data = ELFDATA2MSB};  // Set to big-endian

    struct {
        const char* name;
        read_fn_t fn;
        size_t width;
        size_t offset;
        uint64_t expected;
    } cases[] = {
        /* Non-zero offset to verify alignment-safe reads (memcpy-based) */
        {"u16", elf_read_u16_wrapper, 2, 2, 0xAABBu},
        {"u32", elf_read_u32_wrapper, 4, 2, 0xAABBCCDDu},
        {"u64", elf_read_u64_wrapper, 8, 2, 0x1122334455667788ULL},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        // Clear buffer each iteration
        memset(bytes, 0, sizeof(bytes));

        // Write expected value in big-endian order at the requested offset
        uint64_t value = cases[i].expected;
        for (size_t byte = 0; byte < cases[i].width; byte++) {
            size_t shift = 8 * (cases[i].width - 1 - byte);
            bytes[cases[i].offset + byte] = (uint8_t)((value >> shift) & 0xFFu);
        }

        uint64_t out64 = 0;
        assert_int_equal(cases[i].fn(&img, &ident, cases[i].offset, &out64), 0);

        // Validate success with specific read size results
        if (2 == cases[i].width) {
            assert_int_equal((uint16_t)out64, (uint16_t)cases[i].expected);
        } else if (4 == cases[i].width) {
            assert_int_equal((uint32_t)out64, (uint32_t)cases[i].expected);
        } else {
            assert_int_equal((uint64_t)out64, (uint64_t)cases[i].expected);
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
static void test_elf_read_success_lsb_unaligned_table(void** state) {
    (void)state;  // Unused; pure in-memory test

    // Common buffer size large enough for u64 tests
    uint8_t bytes[16] = {0};

    elf_image_t img = {
        .base = bytes,
        .size = sizeof(bytes),
        .path = "success_lsb_unaligned_table",
    };

    elf_ident_info_t ident = {.ei_data = ELFDATA2LSB};  // Set to little-endian

    struct {
        const char* name;
        read_fn_t fn;
        size_t width;
        size_t offset;
        uint64_t expected;
    } cases[] = {
        /* Non-zero offset to verify alignment-safe reads (memcpy-based) */
        {"u16", elf_read_u16_wrapper, 2, 2, 0xAABBu},
        {"u32", elf_read_u32_wrapper, 4, 2, 0xAABBCCDDu},
        {"u64", elf_read_u64_wrapper, 8, 2, 0x1122334455667788ULL},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        // Clear buffer each iteration
        memset(bytes, 0, sizeof(bytes));

        // Write expected value in little-endian order at the requested offset
        uint64_t value = cases[i].expected;
        for (size_t byte = 0; byte < cases[i].width; byte++) {
            bytes[cases[i].offset + byte] =
                (uint8_t)((value >> (8 * byte)) & 0xFFu);
        }

        uint64_t out64 = 0;
        assert_int_equal(cases[i].fn(&img, &ident, cases[i].offset, &out64), 0);

        // Validate success with specific read size results
        if (2 == cases[i].width) {
            assert_int_equal((uint16_t)out64, (uint16_t)cases[i].expected);
        } else if (4 == cases[i].width) {
            assert_int_equal((uint32_t)out64, (uint32_t)cases[i].expected);
        } else {
            assert_int_equal(out64, cases[i].expected);
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
static void test_elf_read_u32_offset_overflow(void** state) {
    (void)state;  // Unused; pure in-memory test

    uint8_t bytes[16] = {0};

    elf_image_t img = {
        .base = bytes,
        .size = sizeof(bytes),
        .path = "offset_overflow",
    };

    elf_ident_info_t ident = {.ei_data = ELFDATA2LSB};  // Set to little-endian

    // Seed out to confirm failure doesn't clobber it
    uint32_t out = 0xDEADBEEFu;

    assert_int_equal(elf_read_u32(&img, &ident, (size_t)-1, &out), -EINVAL);
    assert_int_equal(out, 0xDEADBEEFu);
}

/*
 * Register all elf_read unit tests with the test runner.
 *
 * The returned tests array is static and remains valid for the lifetime of
 * the process. The caller is responsible for invoking cmocka with the
 * returned pointer and count.
 */
size_t register_elf_read_tests(struct CMUnitTest** out) {
    static struct CMUnitTest tests[] = {
        cmocka_unit_test(test_elf_read_invalid_params_table),
        cmocka_unit_test(test_elf_read_u32_image_base_null),
        cmocka_unit_test(test_elf_read_u32_invalid_ei_data),
        cmocka_unit_test(test_elf_read_bounds_edges_table),
        cmocka_unit_test(test_elf_read_u32_offset_overflow),
        cmocka_unit_test(test_elf_read_success_msb_table),
        cmocka_unit_test(test_elf_read_success_lsb_unaligned_table),
    };

    *out = tests;
    return sizeof(tests) / sizeof(tests[0]);
}
