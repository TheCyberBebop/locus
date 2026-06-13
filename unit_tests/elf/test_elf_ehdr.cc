#include <elf.h>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include "elf_ehdr.h"
#include "elf_ident.h"
#include "elf_image.h"
#include "gtest_fixtures.hpp"
#include "gtest_utilities.h"

namespace {

constexpr size_t kValidateImageSize =
    ELF64_EHDR_SIZE + ELF64_PHDR_SIZE + ELF64_SHDR_SIZE;
constexpr uint8_t kUnsupportedElfClass = 0xFFu;
constexpr uint8_t kUnsupportedElfData = 0xFFu;
constexpr uint8_t kUnsupportedElfVersion = 0xFFu;
constexpr uint16_t kUnsupportedElfType = 0xFFFFu;

void WriteLe16(uint8_t* buf, std::size_t offset, uint16_t value) {
    buf[offset + 0] = static_cast<uint8_t>(value & 0xFFu);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
}

void WriteLe32(uint8_t* buf, size_t offset, uint32_t value) {
    buf[offset + 0] = static_cast<uint8_t>(value & 0xFFu);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    buf[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    buf[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

void WriteLe64(uint8_t* buf, size_t offset, uint64_t value) {
    buf[offset + 0] = static_cast<uint8_t>(value & 0xFFu);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    buf[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    buf[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
    buf[offset + 4] = static_cast<uint8_t>((value >> 32) & 0xFFu);
    buf[offset + 5] = static_cast<uint8_t>((value >> 40) & 0xFFu);
    buf[offset + 6] = static_cast<uint8_t>((value >> 48) & 0xFFu);
    buf[offset + 7] = static_cast<uint8_t>((value >> 56) & 0xFFu);
}

elf_ehdr_t MakeValidElf64Ehdr() {
    elf_ehdr_t ehdr{};

    ehdr.ei_class = ELFCLASS64;
    ehdr.ei_data = ELFDATA2LSB;
    ehdr.e_type = ET_EXEC;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;

    ehdr.e_ehsize = ELF64_EHDR_SIZE;

    ehdr.e_phoff = ELF64_EHDR_SIZE;
    ehdr.e_phentsize = ELF64_PHDR_SIZE;
    ehdr.e_phnum = 1;

    ehdr.e_shoff = ELF64_EHDR_SIZE + ELF64_PHDR_SIZE;
    ehdr.e_shentsize = ELF64_SHDR_SIZE;
    ehdr.e_shnum = 1;
    ehdr.e_shstrndx = 0;

    return ehdr;
}

elf_image_t MakeImage(size_t size) {
    static uint8_t bytes[4096]{};

    elf_image_t img{};
    img.base = bytes;
    img.size = size;
    img.path = "ehdr_validate_test";

    return img;
}

}  // namespace

class ElfEhdrFsTest : public FilesystemTest {};

/* -------------------------------------------------------------------------- */
/* elf_ehdr_parse() tests                                                     */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_ehdr_parse() rejects invalid arguments.
 *
 * This test exercises only parameter validation logic.
 */
TEST(ElfEhdrParseTest, RejectsInvalidParams) {
    elf_image_t img{};
    elf_ident_t ident{};
    elf_ehdr_t out{};

    EXPECT_EQ(elf_ehdr_parse(nullptr, &ident, &out), -EINVAL);
    EXPECT_EQ(elf_ehdr_parse(&img, nullptr, &out), -EINVAL);
    EXPECT_EQ(elf_ehdr_parse(&img, &ident, nullptr), -EINVAL);
}

/*
 * Verify that elf_ehdr_parse() rejects unsupported ELF classes.
 */
TEST(ElfEhdrParseTest, RejectsInvalidClass) {
    elf_image_t img{};
    elf_ident_t ident{};
    elf_ehdr_t out{};

    ident.ei_class = kUnsupportedElfClass;

    EXPECT_EQ(elf_ehdr_parse(&img, &ident, &out), -ENOTSUP);
}

/*
 * Verify that elf_ehdr_parse() rejects unsupported ELF data encodings.
 */
TEST(ElfEhdrParseTest, RejectsInvalidData) {
    elf_image_t img{};
    elf_ident_t ident{};
    elf_ehdr_t out{};

    ident.ei_class = ELFCLASS32;
    ident.ei_data = kUnsupportedElfData;

    EXPECT_EQ(elf_ehdr_parse(&img, &ident, &out), -ENOTSUP);
}

/*
 * Verify that elf_ehdr_parse() propagates read failures when an ELF32 header
 * is truncated.
 *
 * The parser reads fields using the bounds-checked elf_read_u*() helpers.
 * This test provides an image that ends before the final ELF32 header field,
 * ensuring that a failed read is returned to the caller.
 */
TEST(ElfEhdrParseTest, PropagatesReadFailureForTruncatedElf32Header) {
    uint8_t buf[OFF32_E_SHSTRNDX]{};

    elf_image_t img{};
    img.base = buf;
    img.size = sizeof(buf);
    img.path = "ehdr32_truncated";

    elf_ident_t ident{};
    ident.ei_class = ELFCLASS32;
    ident.ei_data = ELFDATA2LSB;

    elf_ehdr_t out{};

    EXPECT_EQ(elf_ehdr_parse(&img, &ident, &out), -EINVAL);
}

/*
 * Verify that elf_ehdr_parse() propagates read failures when an ELF64 header
 * is truncated.
 *
 * The parser reads fields using the bounds-checked elf_read_u*() helpers.
 * This test provides an image that ends before the final ELF64 header field,
 * ensuring that a failed read is returned to the caller.
 */
TEST(ElfEhdrParseTest, PropagatesReadFailureForTruncatedElf64Header) {
    uint8_t buf[OFF64_E_SHSTRNDX]{};

    elf_image_t img{};
    img.base = buf;
    img.size = sizeof(buf);
    img.path = "ehdr64_truncated";

    elf_ident_t ident{};
    ident.ei_class = ELFCLASS64;
    ident.ei_data = ELFDATA2LSB;

    elf_ehdr_t out{};

    EXPECT_EQ(elf_ehdr_parse(&img, &ident, &out), -EINVAL);
}

/*
 * Verify that elf_ehdr_parse() correctly decodes a valid ELF32 header.
 *
 * The test constructs an ELF32 header in a temporary file and verifies that
 * all parsed fields are populated correctly in the normalized elf_ehdr_t
 * representation.
 */
TEST_F(ElfEhdrFsTest, ParsesElf32HeaderAndZeroExtendsOffsets) {
    uint8_t buf[ELF32_EHDR_SIZE]{};

    WriteLe16(buf, OFF_E_TYPE, ET_EXEC);
    WriteLe16(buf, OFF_E_MACHINE, EM_386);
    WriteLe32(buf, OFF_E_VERSION, EV_CURRENT);

    WriteLe32(buf, OFF_E_ENTRY, 0x12345678u);
    WriteLe32(buf, OFF32_E_PHOFF, 0x34u);
    WriteLe32(buf, OFF32_E_SHOFF, 0x80u);
    WriteLe32(buf, OFF32_E_FLAGS, 0xABCDEF01u);

    WriteLe16(buf, OFF32_E_EHSIZE, ELF32_EHDR_SIZE);
    WriteLe16(buf, OFF32_E_PHENTSIZE, ELF32_PHDR_SIZE);
    WriteLe16(buf, OFF32_E_PHNUM, 2);
    WriteLe16(buf, OFF32_E_SHENTSIZE, ELF32_SHDR_SIZE);
    WriteLe16(buf, OFF32_E_SHNUM, 4);
    WriteLe16(buf, OFF32_E_SHSTRNDX, 3);

    elf_image_t img{};
    CreateImage("ehdr32_success.bin", buf, sizeof(buf), &img);

    elf_ident_t ident{};
    ident.ei_class = ELFCLASS32;
    ident.ei_data = ELFDATA2LSB;
    ident.ei_osabi = ELFOSABI_NONE;
    ident.ei_abiversion = 0;

    elf_ehdr_t out{};

    ASSERT_EQ(elf_ehdr_parse(&img, &ident, &out), 0);

    EXPECT_EQ(out.ei_class, ELFCLASS32);
    EXPECT_EQ(out.ei_data, ELFDATA2LSB);
    EXPECT_EQ(out.ei_osabi, ELFOSABI_NONE);
    EXPECT_EQ(out.ei_abiversion, 0);

    EXPECT_EQ(out.e_type, ET_EXEC);
    EXPECT_EQ(out.e_machine, EM_386);
    EXPECT_EQ(out.e_version, EV_CURRENT);

    EXPECT_EQ(out.e_entry, 0x12345678ULL);
    EXPECT_EQ(out.e_phoff, 0x34ULL);
    EXPECT_EQ(out.e_shoff, 0x80ULL);
    EXPECT_EQ(out.e_flags, 0xABCDEF01u);

    EXPECT_EQ(out.e_ehsize, ELF32_EHDR_SIZE);
    EXPECT_EQ(out.e_phentsize, ELF32_PHDR_SIZE);
    EXPECT_EQ(out.e_phnum, 2);
    EXPECT_EQ(out.e_shentsize, ELF32_SHDR_SIZE);
    EXPECT_EQ(out.e_shnum, 4);
    EXPECT_EQ(out.e_shstrndx, 3);

    EXPECT_EQ(elf_image_close(&img), 0);
}

/*
 * Verify that elf_ehdr_parse() correctly decodes a valid ELF64 header.
 *
 * The test constructs an ELF64 header in a temporary file and verifies that
 * all parsed fields are populated correctly in the normalized elf_ehdr_t
 * representation.
 */
TEST_F(ElfEhdrFsTest, ParsesElf64Header) {
    uint8_t buf[ELF64_EHDR_SIZE]{};

    // Decoder configuration
    buf[EI_CLASS] = ELFCLASS64;
    buf[EI_DATA] = ELFDATA2LSB;
    buf[EI_OSABI] = ELFOSABI_NONE;
    buf[EI_ABIVERSION] = 0;

    // ELF header fields
    WriteLe16(buf, OFF_E_TYPE, ET_EXEC);
    WriteLe16(buf, OFF_E_MACHINE, EM_X86_64);
    WriteLe32(buf, OFF_E_VERSION, EV_CURRENT);

    WriteLe64(buf, OFF_E_ENTRY, 0x1122334455667788ULL);
    WriteLe64(buf, OFF64_E_PHOFF, 0x40ULL);
    WriteLe64(buf, OFF64_E_SHOFF, 0x100ULL);

    WriteLe32(buf, OFF64_E_FLAGS, 0x12345678u);

    WriteLe16(buf, OFF64_E_EHSIZE, 64);
    WriteLe16(buf, OFF64_E_PHENTSIZE, 56);
    WriteLe16(buf, OFF64_E_PHNUM, 7);

    WriteLe16(buf, OFF64_E_SHENTSIZE, 64);
    WriteLe16(buf, OFF64_E_SHNUM, 11);
    WriteLe16(buf, OFF64_E_SHSTRNDX, 10);

    elf_image_t img{};
    CreateImage("ehdr64_success.bin", buf, sizeof(buf), &img);

    elf_ident_t ident{};
    ident.ei_class = ELFCLASS64;
    ident.ei_data = ELFDATA2LSB;
    ident.ei_osabi = ELFOSABI_NONE;
    ident.ei_abiversion = 0;

    elf_ehdr_t out{};

    ASSERT_EQ(elf_ehdr_parse(&img, &ident, &out), 0);

    EXPECT_EQ(out.ei_class, ELFCLASS64);
    EXPECT_EQ(out.ei_data, ELFDATA2LSB);
    EXPECT_EQ(out.ei_osabi, ELFOSABI_NONE);
    EXPECT_EQ(out.ei_abiversion, 0);

    EXPECT_EQ(out.e_type, ET_EXEC);
    EXPECT_EQ(out.e_machine, EM_X86_64);
    EXPECT_EQ(out.e_version, EV_CURRENT);

    EXPECT_EQ(out.e_entry, 0x1122334455667788ULL);
    EXPECT_EQ(out.e_phoff, 0x40ULL);
    EXPECT_EQ(out.e_shoff, 0x100ULL);

    EXPECT_EQ(out.e_flags, 0x12345678u);

    EXPECT_EQ(out.e_ehsize, 64);
    EXPECT_EQ(out.e_phentsize, 56);
    EXPECT_EQ(out.e_phnum, 7);

    EXPECT_EQ(out.e_shentsize, 64);
    EXPECT_EQ(out.e_shnum, 11);
    EXPECT_EQ(out.e_shstrndx, 10);

    EXPECT_EQ(elf_image_close(&img), 0);
}

/* -------------------------------------------------------------------------- */
/* elf_ehdr_validate() tests                                                  */
/* -------------------------------------------------------------------------- */

/*
 * Verify that elf_ehdr_validate() rejects invalid arguments.
 */
TEST(ElfEhdrValidateTest, RejectsInvalidParams) {
    elf_image_t img{};
    elf_ehdr_t ehdr{};

    EXPECT_EQ(elf_ehdr_validate(nullptr, &ehdr), -EINVAL);
    EXPECT_EQ(elf_ehdr_validate(&img, nullptr), -EINVAL);
}

/*
 * Verify that elf_ehdr_validate() rejects unsupported ELF versions.
 */
TEST(ElfEhdrValidateTest, RejectsInvalidVersion) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_version = kUnsupportedElfVersion;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that elf_ehdr_validate() accepts a valid ELF64 executable header.
 */
TEST(ElfEhdrValidateTest, AcceptsValidElf64ExecutableHeader) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), 0);
}

/*
 * Verify that elf_ehdr_validate() rejects unsupported object file types.
 */
TEST(ElfEhdrValidateTest, RejectsUnsupportedType) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_type = kUnsupportedElfType;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -ENOTSUP);
}

/*
 * Verify that elf_ehdr_validate() rejects unsupported ELF classes.
 */
TEST(ElfEhdrValidateTest, RejectsUnsupportedClass) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.ei_class = kUnsupportedElfClass;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -ENOTSUP);
}

/*
 * Verify that elf_ehdr_validate() rejects ELF header size mismatches.
 */
TEST(ElfEhdrValidateTest, RejectsWrongElfHeaderSize) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_ehsize = ELF64_EHDR_SIZE - 1;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that e_phoff may equal file size when no program headers exist.
 */
TEST(ElfEhdrValidateTest, AllowsNoProgramHeadersAtEndOfFile) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_phnum = 0;
    ehdr.e_phoff = img.size;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), 0);
}

/*
 * Verify that e_phoff is rejected when no program headers exist but the
 * offset is beyond the file.
 */
TEST(ElfEhdrValidateTest, RejectsProgramHeaderOffsetPastEndWhenNoHeaders) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_phnum = 0;
    ehdr.e_phoff = img.size + 1;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that elf_ehdr_validate() rejects program header entry size mismatch.
 */
TEST(ElfEhdrValidateTest, RejectsWrongProgramHeaderEntrySize) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_phentsize = ELF64_PHDR_SIZE - 1;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that elf_ehdr_validate() rejects program headers extending beyond
 * the mapped file.
 */
TEST(ElfEhdrValidateTest, RejectsProgramHeaderTableOutOfBounds) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_phoff = img.size;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that e_shoff may be zero when no section headers exist.
 */
TEST(ElfEhdrValidateTest, AllowsNoSectionHeaders) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_shnum = 0;
    ehdr.e_shstrndx = 0;
    ehdr.e_shoff = 0;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), 0);
}

/*
 * Verify that extended section numbering requires SHDR[0] to fit in the file.
 */
TEST(ElfEhdrValidateTest, RejectsExtendedSectionNumberingPastEndOfFile) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_shnum = 0;
    ehdr.e_shstrndx = 0;
    ehdr.e_shoff = img.size;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that e_shoff is rejected when no section headers exist but the
 * offset is beyond the file.
 */
TEST(ElfEhdrValidateTest, RejectsSectionHeaderOffsetPastEndWhenNoHeaders) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_shnum = 0;
    ehdr.e_shstrndx = 0;
    ehdr.e_shoff = img.size + 1;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that elf_ehdr_validate() rejects section header entry size mismatch.
 */
TEST(ElfEhdrValidateTest, RejectsWrongSectionHeaderEntrySize) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_shentsize = ELF64_SHDR_SIZE - 1;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that elf_ehdr_validate() rejects section headers extending beyond
 * the mapped file.
 */
TEST(ElfEhdrValidateTest, RejectsSectionHeaderTableOutOfBounds) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_shoff = img.size;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}

/*
 * Verify that elf_ehdr_validate() rejects a section-name string table index
 * outside the effective section-header count.
 */
TEST(ElfEhdrValidateTest, RejectsSectionStringTableIndexOutOfRange) {
    elf_image_t img = MakeImage(kValidateImageSize);
    elf_ehdr_t ehdr = MakeValidElf64Ehdr();

    ehdr.e_shnum = 2;
    ehdr.e_shstrndx = 2;

    EXPECT_EQ(elf_ehdr_validate(&img, &ehdr), -EINVAL);
}
