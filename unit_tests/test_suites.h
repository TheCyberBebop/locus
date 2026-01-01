/**
 * @defgroup locus_tests Unit Tests
 * @brief Unit tests for the LOCUS loader.
 *
 * This group contains all unit tests that validate the internal components of
 * the LOCUS ELF loader.
 */

#ifndef TEST_SUITES_H
#define TEST_SUITES_H

// These headers MUST be included prior to including cmocka.h
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>
#include <stddef.h>

size_t register_elf_image_tests(struct CMUnitTest** out);
size_t register_elf_ident_tests(struct CMUnitTest** out);
size_t register_elf_read_tests(struct CMUnitTest** out);

#endif /* TEST_SUITES_H */
