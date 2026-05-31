# Keep intermediate build products (.o/.d) instead of deleting them after a successful build
.SECONDARY:

# If a recipe fails, delete the target it was trying to build
.DELETE_ON_ERROR:

# Disable built-in implicit rules/variables and catch typos in variable names
MAKEFLAGS += --no-builtin-rules --no-builtin-variables --warn-undefined-variables
SHELL := /bin/sh

# -------- User-configurable options --------
ARCH         ?=
VERBOSE      ?=
VERY_VERBOSE ?=
DEBUG        ?=
LOG          ?= LOG_LEVEL_INFO
CC           ?= gcc
CXX          ?= g++
DOXYGEN      ?= doxygen

# -------- Build config --------
RM                   := rm -rf
ARTIFACTS_DIR        := artifacts
BUILD_ROOT_DIR       := build
TEST_FILES_DIR       := test_files
UNIT_TESTS_DIR       := unit_tests
UNIT_TEST_COMMON_DIR := $(UNIT_TESTS_DIR)/common
UNIT_TEST_ELF_DIR    := $(UNIT_TESTS_DIR)/elf
BUILD_DIR            := $(BUILD_ROOT_DIR)/$(LOG)
DOCS_DIR             := docs
DOXYFILE             := Doxyfile
CC_NATIVE            := $(CC)
CXX_NATIVE           := $(CXX)

# -------- Build Debug --------
# Usage:
#   make         # quiet builds
#   make DEBUG=m # "--debug=m" (makefile parsing)
#   make DEBUG=v # "--debug=v" (variable expansion)
#   make DEBUG=a # "--debug=a" (everything)
#
# Default is quiet
ifneq ($(strip $(DEBUG)),)
ifneq ($(filter m v b a,$(DEBUG)),$(DEBUG))
$(error Invalid DEBUG '$(DEBUG)'. Use one of: m v b a)
endif
endif

ifeq ($(origin DEBUG),command line)
ifneq ($(strip $(DEBUG)),)
ifeq ($(MAKE_RESTARTS),)
$(info Re-running make with --debug=$(DEBUG))
MAKEFLAGS += --debug=$(DEBUG)
endif
endif
endif

# -------- Build Verbosity --------
# Usage:
#   make           # quiet builds
#   make VERBOSE=1 # prints full compiler/link commands
# 
# Default is quiet ("==>" and finished lines only)
ifeq ($(strip $(VERBOSE)),1)
Q :=
else
Q := @
endif
# Usage:
#   make                # quiet builds
#   make VERY_VERBOSE=1 # enable shell tracing
# 
# Default is quiet
ifeq ($(strip $(VERY_VERBOSE)),1)
SHELL := /bin/sh -x
endif

# -------- Doxygen verbosity (tied to VERBOSE) --------
ifeq ($(strip $(VERBOSE)),1)
DOXYGEN_FLAGS :=
else
DOXYGEN_FLAGS := -q
endif

ifeq ($(strip $(VERY_VERBOSE)),1)
DOXYGEN_FLAGS :=
endif

# Preprocessor flags common to locus
CPPFLAGS_COMMON := -Ilocus -DLOG_LEVEL=$(LOG)
# Compile flags for locus binary
# -O0                      - Disable optimizations (preserve source ↔ assembly mapping)
# -g                       - Emit debug information (DWARF)
# -Wall                    - Enable most common compiler warnings
# -Wextra                  - Enable additional warnings beyond -Wall
# -Werror                  - Treat all warnings as errors
# -Wpedantic               - Enforce strict ISO C compliance
# -fno-omit-frame-pointer  - Always keep frame pointers (reliable stack traces)
# -MMD                     - Generate header dependency files (exclude system headers)
# -MP                      - Add phony targets for headers (safe incremental builds)
CFLAGS_COMMON   := -O0 -g -Wall -Wextra -Werror -Wpedantic -fno-omit-frame-pointer \
                 -MMD -MP
# Compile flags for standalone test binaries
CFLAGS_TESTS    := -O0 -g -fno-omit-frame-pointer -MMD -MP

# GoogleTest requires C++ compilation
CXXFLAGS_TESTS  := -O0 -g -Wall -Wextra -Werror \
                 -fno-omit-frame-pointer -MMD -MP \
                 -std=c++17

# GoogleTest / GoogleMock libraries.
GTEST_LIBS      := -lgmock -lgtest_main -lgtest -pthread

# -------- Source files --------
# LOCUS library sources (everything except the locus entrypoint)
LOCUS_LIB_SRC := $(filter-out locus/locus.c,$(wildcard locus/*.c))
LOCUS_APP_SRC := locus/locus.c
TEST_SRCS     := $(wildcard $(TEST_FILES_DIR)/*.c)
# Isolate test file names for building
TEST_NAMES    := $(notdir $(basename $(TEST_SRCS)))

# -------- Unit tests (GoogleTest, native) --------
UNIT_TEST_SRCS := $(wildcard $(UNIT_TEST_ELF_DIR)/*.cc)
UNIT_TEST_BUILD := $(BUILD_DIR)/$(UNIT_TESTS_DIR)

# One GoogleTest executable per source file:
#   unit_tests/test_foo.cc
# becomes:
#   build/<log>/unit_tests/test_foo
UNIT_TEST_BINS := \
	$(patsubst $(UNIT_TEST_ELF_DIR)/%.cc,$(UNIT_TEST_BUILD)/%,\
	             $(UNIT_TEST_SRCS))

# LOCUS objects compiled for unit tests (native compile flags)
UNIT_LOCUS_OBJS := \
	$(patsubst locus/%.c,$(UNIT_TEST_BUILD)/locus/%.o,$(LOCUS_LIB_SRC))

# Shared C helper code used by GoogleTest suites
UNIT_HELPER_SRCS := $(UNIT_TEST_COMMON_DIR)/test_utilities.c
UNIT_HELPER_OBJS := \
	$(patsubst $(UNIT_TEST_COMMON_DIR)/%.c,$(UNIT_TEST_BUILD)/%.o,\
	             $(UNIT_HELPER_SRCS))

# -------- Toolchains and Architecture Mappings --------
# Includes host compiler (CC) with no prefix
TOOLCHAINS := \
  native \
  arm-linux-gnueabihf \
  arm-linux-gnueabi \
  aarch64-linux-gnu \
  mips-linux-gnu \
  mipsel-linux-gnu \
  mips64-linux-gnuabi64 \
  mips64el-linux-gnuabi64 \
  powerpc-linux-gnu \
  powerpc64-linux-gnu \
  powerpc64le-linux-gnu \
  riscv64-linux-gnu \
  s390x-linux-gnu \
  sparc64-linux-gnu \
  i686-linux-gnu \
  x86_64-linux-gnu \
  x86_64-linux-gnux32

# Output directory names per toolchain
OUT_native                    := native
# ARM
OUT_arm-linux-gnueabihf       := armhf
OUT_arm-linux-gnueabi         := armel
OUT_aarch64-linux-gnu         := aarch64
# MIPS
OUT_mips-linux-gnu            := mips
OUT_mipsel-linux-gnu          := mipsel
OUT_mips64-linux-gnuabi64     := mips64
OUT_mips64el-linux-gnuabi64   := mips64el
# PowerPC
OUT_powerpc-linux-gnu         := powerpc
OUT_powerpc64-linux-gnu       := ppc64
OUT_powerpc64le-linux-gnu     := ppc64le
# RISC-V
OUT_riscv64-linux-gnu         := riscv64
# IBM Z
OUT_s390x-linux-gnu           := s390x
# SPARC
OUT_sparc64-linux-gnu         := sparc64
# x86
OUT_i686-linux-gnu            := i686
OUT_x86_64-linux-gnu          := x86_64
OUT_x86_64-linux-gnux32       := x32

# Fail if missing toolchain mapping
$(foreach tc,$(TOOLCHAINS), \
  $(if $(strip $(OUT_$(tc))),, \
    $(error Missing OUT_ mapping for toolchain '$(tc)') \
  ) \
)

# -------- Build architecture --------
# Usage:
#   make             # build all toolchains
#   make ARCH=mipsel # build only one arch (by OUT_* name)
#
# Default is all architectures
ifeq ($(strip $(ARCH)),)
TOOLCHAINS_SELECTED := $(TOOLCHAINS)
else
TOOLCHAINS_SELECTED := \
	$(foreach tc,$(TOOLCHAINS),$(if $(filter $(ARCH),$(OUT_$(tc))),$(tc),))
endif

ifeq ($(strip $(ARCH)),)
# Do nothing
else
ifeq ($(strip $(TOOLCHAINS_SELECTED)),)
$(error Unknown ARCH '$(ARCH)'. Valid: $(foreach tc,$(TOOLCHAINS),$(OUT_$(tc))))
endif
endif

# -------- Include all generated dependency (.d) files (from build/) --------
DEPS := $(wildcard $(BUILD_DIR)/*/locus/*.d) \
        $(wildcard $(BUILD_DIR)/*/$(TEST_FILES_DIR)/*.d) \
        $(wildcard $(UNIT_TEST_BUILD)/*.d) \
        $(wildcard $(UNIT_TEST_BUILD)/locus/*.d)
-include $(DEPS)

# -------- Macro to generate rules for each toolchain --------
# Parameters:
#   $(1) = toolchain name, e.g. "arm-linux-gnueabihf" or "native"
#
# Computes:
#   - ARCH_NAME_<tc>      # output folder name (armhf, native, ...)
#   - ARCH_ART_DIR_<tc>   # artifacts/<arch>
#   - ARCH_BUILD_DIR_<tc> # build/<arch>
#   - CC_<tc>             # compiler command for that toolchain
define GEN_CROSS_BUILDS
ARCH_NAME_$(1)      := $$(OUT_$(1))
ARCH_ART_DIR_$(1)   := $$(ARTIFACTS_DIR)/$$(ARCH_NAME_$(1))
ARCH_BUILD_DIR_$(1) := $$(BUILD_DIR)/$$(ARCH_NAME_$(1))

# Choose toolchain or native compiler:
#   native -> $(CC)
#   others -> <toolchain>-gcc
ifeq ($(1),native)
CC_$(1) := $(CC)
else
CC_$(1) := $(1)-gcc
endif

# locus objects: build/<arch>/locus/<name>.o
OBJS_$(1) := \
	$$(patsubst locus/%.c,$$(ARCH_BUILD_DIR_$(1))/locus/%.o,$(LOCUS_LIB_SRC) $(LOCUS_APP_SRC))

# Test objects: build/<arch>/test_files/<testname>.o
TEST_OBJS_$(1) := \
	$$(patsubst $(TEST_FILES_DIR)/%.c,$$(ARCH_BUILD_DIR_$(1))/$(TEST_FILES_DIR)/%.o,$(TEST_SRCS))

# ---- Directory rules (order-only) ----
$$(ARCH_ART_DIR_$(1)): | $(ARTIFACTS_DIR)
	$(Q)mkdir -p $$@

$$(ARCH_ART_DIR_$(1))/$(TEST_FILES_DIR): | $$(ARCH_ART_DIR_$(1))
	$(Q)mkdir -p $$@

$$(ARCH_BUILD_DIR_$(1))/locus: | $(BUILD_ROOT_DIR)
	$(Q)mkdir -p $$@

$$(ARCH_BUILD_DIR_$(1))/$(TEST_FILES_DIR): | $(BUILD_ROOT_DIR)
	$(Q)mkdir -p $$@

# ---- 1) Compile locus .c -> build/<arch>/locus/*.o (+ .d) ----
$$(ARCH_BUILD_DIR_$(1))/locus/%.o: locus/%.c | $$(ARCH_BUILD_DIR_$(1))/locus
	@echo "==> [$$(ARCH_NAME_$(1))] CC $$<"
	$(Q)$$(CC_$(1)) $$(CPPFLAGS_COMMON) $$(CFLAGS_COMMON) -c -o $$@ $$<

# ---- 2a) Link dynamic locus -> artifacts/<arch>/locus ----
$$(ARCH_ART_DIR_$(1))/locus: $$(OBJS_$(1)) | $$(ARCH_ART_DIR_$(1))
	@echo "==> [$$(ARCH_NAME_$(1))] LD $$@"
	$(Q)$$(CC_$(1)) -o $$@ $$^

# ---- 2b) Link static locus -> artifacts/<arch>/locus_static ----
$$(ARCH_ART_DIR_$(1))/locus_static: $$(OBJS_$(1)) | $$(ARCH_ART_DIR_$(1))
	@echo "==> [$$(ARCH_NAME_$(1))] LD $$@ (static)"
	$(Q)$$(CC_$(1)) -static -o $$@ $$^

# ---- 3) Compile test_files .c -> build/<arch>/test_files/*.o (+ .d) ----
$$(ARCH_BUILD_DIR_$(1))/$(TEST_FILES_DIR)/%.o: \
	$(TEST_FILES_DIR)/%.c | $$(ARCH_BUILD_DIR_$(1))/$(TEST_FILES_DIR)
	@echo "==> [$$(ARCH_NAME_$(1))] CC $$<"
	$(Q)$$(CC_$(1)) $$(CFLAGS_TESTS) -c -o $$@ $$<

# ---- 4a) Link test_files -> artifacts/<arch>/test_files/<testname> ----
$$(ARCH_ART_DIR_$(1))/$(TEST_FILES_DIR)/%: \
	$$(ARCH_BUILD_DIR_$(1))/$(TEST_FILES_DIR)/%.o | $$(ARCH_ART_DIR_$(1))/$(TEST_FILES_DIR)
	@echo "==> [$$(ARCH_NAME_$(1))] LD test $$*"
	$(Q)$$(CC_$(1)) -o $$@ $$<

# ---- 4b) Link static test_files -> artifacts/<arch>/test_files/<testname>_static ----
$$(ARCH_ART_DIR_$(1))/$(TEST_FILES_DIR)/%_static: \
	$$(ARCH_BUILD_DIR_$(1))/$(TEST_FILES_DIR)/%.o | $$(ARCH_ART_DIR_$(1))/$(TEST_FILES_DIR)
	@echo "==> [$$(ARCH_NAME_$(1))] LD test $$* (static)"
	$(Q)$$(CC_$(1)) -static -o $$@ $$<
endef

$(foreach tc,$(TOOLCHAINS),$(eval $(call GEN_CROSS_BUILDS,$(tc))))

# -------- Build root directory targets --------
$(ARTIFACTS_DIR):
	$(Q)mkdir -p $@

$(BUILD_ROOT_DIR):
	$(Q)mkdir -p $@

# -------- GoogleTest unit tests (native) --------
$(UNIT_TEST_BUILD): | $(BUILD_ROOT_DIR)
	$(Q)mkdir -p $@

$(UNIT_TEST_BUILD)/locus: | $(UNIT_TEST_BUILD)
	$(Q)mkdir -p $@

# Compile unit test sources -> build/.../unit_tests/*.o
$(UNIT_TEST_BUILD)/%.o: $(UNIT_TEST_ELF_DIR)/%.cc | $(UNIT_TEST_BUILD)
	@echo "==> [unit] CXX $<"
	$(Q)$(CXX_NATIVE) \
		$(CPPFLAGS_COMMON) \
		$(CXXFLAGS_TESTS) \
		-I$(UNIT_TEST_COMMON_DIR) \
		-c -o $@ $<

# Compile locus sources for unit tests -> build/.../unit_tests/locus/*.o
$(UNIT_TEST_BUILD)/locus/%.o: locus/%.c | $(UNIT_TEST_BUILD)/locus
	@echo "==> [unit] CC $<"
	$(Q)$(CC_NATIVE) \
		$(CPPFLAGS_COMMON) \
		$(CFLAGS_COMMON) \
		-I$(UNIT_TEST_COMMON_DIR) \
		-c -o $@ $<

# Compile shared C test helpers
$(UNIT_TEST_BUILD)/%.o: $(UNIT_TEST_COMMON_DIR)/%.c | $(UNIT_TEST_BUILD)
	@echo "==> [unit] CC $<"
	$(Q)$(CC_NATIVE) \
		$(CPPFLAGS_COMMON) \
		$(CFLAGS_COMMON) \
		-I$(UNIT_TEST_COMMON_DIR) \
		-c -o $@ $<

# Link a standalone GoogleTest executable
$(UNIT_TEST_BUILD)/%: \
	$(UNIT_TEST_BUILD)/%.o \
	$(UNIT_HELPER_OBJS) \
	$(UNIT_LOCUS_OBJS) | $(UNIT_TEST_BUILD)
	@echo "==> [unit] CXXLD $@"
	$(Q)$(CXX_NATIVE) -o $@ $^ $(GTEST_LIBS)

# -------- Build target expansion --------
ALL_TESTS           := $(foreach tc,$(TOOLCHAINS_SELECTED), \
                       $(foreach t,$(TEST_NAMES),$(ARTIFACTS_DIR)/$(OUT_$(tc))/$(TEST_FILES_DIR)/$(t)))
ALL_TESTS_STATIC    := $(foreach tc,$(TOOLCHAINS_SELECTED), \
                       $(foreach t,$(TEST_NAMES),$(ARTIFACTS_DIR)/$(OUT_$(tc))/$(TEST_FILES_DIR)/$(t)_static))
ALL_LOCUS           := $(foreach tc,$(TOOLCHAINS_SELECTED),$(ARTIFACTS_DIR)/$(OUT_$(tc))/locus)
ALL_LOCUS_STATIC    := $(foreach tc,$(TOOLCHAINS_SELECTED),$(ARTIFACTS_DIR)/$(OUT_$(tc))/locus_static)
NATIVE_TESTS        := $(foreach t,$(TEST_NAMES),$(ARTIFACTS_DIR)/$(OUT_native)/$(TEST_FILES_DIR)/$(t))
NATIVE_TESTS_STATIC := $(foreach t,$(TEST_NAMES),$(ARTIFACTS_DIR)/$(OUT_native)/$(TEST_FILES_DIR)/$(t)_static)
NATIVE_LOCUS        := $(ARTIFACTS_DIR)/$(OUT_native)/locus
NATIVE_LOCUS_STATIC := $(ARTIFACTS_DIR)/$(OUT_native)/locus_static

all: unit_tests build_all

build_all: tests tests_static locus locus_static docs

native: $(NATIVE_LOCUS) $(NATIVE_TESTS)
	@echo ""
	@echo "Finished building native binaries."
	@echo ""

native_static: $(NATIVE_LOCUS_STATIC) $(NATIVE_TESTS_STATIC)
	@echo ""
	@echo "Finished building static native locus binaries."
	@echo ""

tests: check-toolchains $(ALL_TESTS)
	@echo ""
	@echo "Finished building test binaries."
	@echo ""

tests_static: check-toolchains $(ALL_TESTS_STATIC)
	@echo ""
	@echo "Finished building static test binaries."
	@echo ""

locus: check-toolchains $(ALL_LOCUS)
	@echo ""
	@echo "Finished building locus binaries."
	@echo ""

locus_static: check-toolchains $(ALL_LOCUS_STATIC)
	@echo ""
	@echo "Finished building static locus binaries."
	@echo ""

unit_tests: $(UNIT_TEST_BINS)
	@echo ""
	@echo "==> Running unit tests"
	@echo ""
	$(Q)set -e; for test_bin in $(UNIT_TEST_BINS); do \
		echo "==> Running $$test_bin"; \
		$$test_bin; \
	done
	@echo ""
	@echo "Finished running unit tests."
	@echo ""

# Fail if a cross compiler isn't installed (skip native)
check-toolchains:
	@for tc in $(TOOLCHAINS_SELECTED); do \
	  if [ "$$tc" = "native" ]; then continue; fi; \
	  command -v $$tc-gcc >/dev/null 2>&1 || { echo "Missing: $$tc-gcc"; exit 1; }; \
	done

clean:
	@echo ""
	@echo "Cleaning up artifacts, build, and docs ..."
	@echo ""
	$(Q)$(RM) $(ARTIFACTS_DIR) $(BUILD_ROOT_DIR) $(DOCS_DIR)   

# -------- Documentation (Doxygen) --------
# Generate a default Doxyfile if missing (one-time bootstrap)
$(DOXYFILE):
	@echo "==> Generating default $(DOXYFILE)"
	$(Q)$(DOXYGEN) -g $(DOXYFILE)

docs: $(DOXYFILE)
	$(Q)mkdir -p $(DOCS_DIR)
	@echo "==> Building docs (Doxygen)"
	$(Q)$(DOXYGEN) $(DOXYGEN_FLAGS) $(DOXYFILE)
	@echo ""
	@echo "Docs generated: $(DOCS_DIR)/html/index.html"
	@echo ""

help:
	@echo "Usage:"
	@echo "  make [target] [LOG=LOG_LEVEL_INFO]"
	@echo "       [ARCH=<arch>] [VERBOSE=1]"
	@echo "       [VERY_VERBOSE=1] [DEBUG=m|v|b|a]"
	@echo ""
	@echo "Targets: all native native_static tests tests_static locus locus_static unit_tests docs clean"
	@echo ""
	@echo "Valid ARCH values:"
	@echo "  $(foreach tc,$(TOOLCHAINS),$(OUT_$(tc)))"
	@echo ""
	@echo "Selected toolchains:"
	@echo "  $(TOOLCHAINS_SELECTED)"

.PHONY: all native native_static tests tests_static locus locus_static unit_tests \
        check-toolchains clean docs help
