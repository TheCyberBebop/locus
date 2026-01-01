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
DOXYGEN      ?= doxygen

# -------- Build config --------
RM             := rm -rf
ARTIFACTS_DIR  := artifacts
BUILD_ROOT_DIR := build
BUILD_DIR      := $(BUILD_ROOT_DIR)/$(LOG)
DOCS_DIR       := docs
DOXYFILE       := Doxyfile
CC_NATIVE      := $(CC)

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
# Compile flags for test binaries
CFLAGS_TESTS    := -O0 -g -fno-omit-frame-pointer -MMD -MP

# -------- Source files --------
# LOCUS library sources (everything except the locus entrypoint)
LOCUS_LIB_SRC := $(filter-out locus/locus.c,$(wildcard locus/*.c))
LOCUS_APP_SRC := locus/locus.c
TEST_SRCS     := $(wildcard tests/*.c)
# Isolate test file names for building
TEST_NAMES    := $(notdir $(basename $(TEST_SRCS)))

# -------- Unit tests (cmocka) --------
UNIT_DIR   := unit_tests
UNIT_SRCS  := $(wildcard $(UNIT_DIR)/*.c)
UNIT_BUILD := $(BUILD_DIR)/unit_tests
UNIT_BIN   := $(UNIT_BUILD)/locus_tests

# Object files for unit tests (native only)
UNIT_OBJS := $(patsubst $(UNIT_DIR)/%.c,$(UNIT_BUILD)/%.o,$(UNIT_SRCS))

# LOCUS objects compiled for unit tests (native compile flags)
UNIT_LOCUS_OBJS := $(patsubst locus/%.c,$(UNIT_BUILD)/locus/%.o,$(LOCUS_LIB_SRC))

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
TOOLCHAINS_SELECTED := $(foreach tc,$(TOOLCHAINS),$(if $(filter $(ARCH),$(OUT_$(tc))),$(tc),))
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
        $(wildcard $(BUILD_DIR)/*/tests/*.d) \
        $(wildcard $(UNIT_BUILD)/*.d) \
        $(wildcard $(UNIT_BUILD)/locus/*.d)
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
OBJS_$(1) := $$(patsubst locus/%.c,$$(ARCH_BUILD_DIR_$(1))/locus/%.o,$(LOCUS_LIB_SRC) $(LOCUS_APP_SRC))

# Test objects: build/<arch>/tests/<testname>.o
TEST_OBJS_$(1) := $$(patsubst tests/%.c,$$(ARCH_BUILD_DIR_$(1))/tests/%.o,$(TEST_SRCS))

# ---- Directory rules (order-only) ----
$$(ARCH_ART_DIR_$(1)): | $(ARTIFACTS_DIR)
	$(Q)mkdir -p $$@

$$(ARCH_ART_DIR_$(1))/tests: | $$(ARCH_ART_DIR_$(1))
	$(Q)mkdir -p $$@

$$(ARCH_BUILD_DIR_$(1))/locus: | $(BUILD_ROOT_DIR)
	$(Q)mkdir -p $$@

$$(ARCH_BUILD_DIR_$(1))/tests: | $(BUILD_ROOT_DIR)
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

# ---- 3) Compile tests .c -> build/<arch>/tests/*.o (+ .d) ----
$$(ARCH_BUILD_DIR_$(1))/tests/%.o: tests/%.c | $$(ARCH_BUILD_DIR_$(1))/tests
	@echo "==> [$$(ARCH_NAME_$(1))] CC $$<"
	$(Q)$$(CC_$(1)) $$(CFLAGS_TESTS) -c -o $$@ $$<

# ---- 4a) Link tests -> artifacts/<arch>/tests/<testname> ----
$$(ARCH_ART_DIR_$(1))/tests/%: $$(ARCH_BUILD_DIR_$(1))/tests/%.o | $$(ARCH_ART_DIR_$(1))/tests
	@echo "==> [$$(ARCH_NAME_$(1))] LD test $$*"
	$(Q)$$(CC_$(1)) -o $$@ $$<

# ---- 4b) Link static tests -> artifacts/<arch>/tests/<testname>_static ----
$$(ARCH_ART_DIR_$(1))/tests/%_static: $$(ARCH_BUILD_DIR_$(1))/tests/%.o | $$(ARCH_ART_DIR_$(1))/tests
	@echo "==> [$$(ARCH_NAME_$(1))] LD test $$* (static)"
	$(Q)$$(CC_$(1)) -static -o $$@ $$<
endef

$(foreach tc,$(TOOLCHAINS),$(eval $(call GEN_CROSS_BUILDS,$(tc))))

# -------- Build root directory targets --------
$(ARTIFACTS_DIR):
	$(Q)mkdir -p $@

$(BUILD_ROOT_DIR):
	$(Q)mkdir -p $@

# -------- Unit tests (native, cmocka) --------
$(UNIT_BUILD): | $(BUILD_ROOT_DIR)
	$(Q)mkdir -p $@

$(UNIT_BUILD)/locus: | $(UNIT_BUILD)
	$(Q)mkdir -p $@

# Compile unit test sources -> build/.../unit_tests/*.o
$(UNIT_BUILD)/%.o: $(UNIT_DIR)/%.c | $(UNIT_BUILD)
	@echo "==> [unit] CC $<"
	$(Q)$(CC_NATIVE) $(CPPFLAGS_COMMON) $(CFLAGS_COMMON) -I$(UNIT_DIR) -c -o $@ $<

# Compile locus sources for unit tests -> build/.../unit_tests/locus/*.o
$(UNIT_BUILD)/locus/%.o: locus/%.c | $(UNIT_BUILD)/locus
	@echo "==> [unit] CC $<"
	$(Q)$(CC_NATIVE) $(CPPFLAGS_COMMON) $(CFLAGS_COMMON) -I$(UNIT_DIR) -c -o $@ $<

# Link unit test runner (links cmocka)
$(UNIT_BIN): $(UNIT_OBJS) $(UNIT_LOCUS_OBJS) | $(UNIT_BUILD)
	@echo "==> [unit] LD $@"
	$(Q)$(CC_NATIVE) -o $@ $^ -lcmocka

# -------- Build target expansion --------
ALL_TESTS           := $(foreach tc,$(TOOLCHAINS_SELECTED), \
                       $(foreach t,$(TEST_NAMES),$(ARTIFACTS_DIR)/$(OUT_$(tc))/tests/$(t)))
ALL_TESTS_STATIC    := $(foreach tc,$(TOOLCHAINS_SELECTED), \
                       $(foreach t,$(TEST_NAMES),$(ARTIFACTS_DIR)/$(OUT_$(tc))/tests/$(t)_static))
ALL_LOCUS           := $(foreach tc,$(TOOLCHAINS_SELECTED),$(ARTIFACTS_DIR)/$(OUT_$(tc))/locus)
ALL_LOCUS_STATIC    := $(foreach tc,$(TOOLCHAINS_SELECTED),$(ARTIFACTS_DIR)/$(OUT_$(tc))/locus_static)
NATIVE_TESTS        := $(foreach t,$(TEST_NAMES),$(ARTIFACTS_DIR)/$(OUT_native)/tests/$(t))
NATIVE_TESTS_STATIC := $(foreach t,$(TEST_NAMES),$(ARTIFACTS_DIR)/$(OUT_native)/tests/$(t)_static)
NATIVE_LOCUS        := $(ARTIFACTS_DIR)/$(OUT_native)/locus
NATIVE_LOCUS_STATIC := $(ARTIFACTS_DIR)/$(OUT_native)/locus_static

all: tests tests_static locus locus_static docs

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

unit_tests: $(UNIT_BIN)
	@echo ""
	@echo "==> Running unit tests"
	@echo ""
	$(Q)$(UNIT_BIN)
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
	@echo "  make [target] [LOG=LOG_LEVEL_INFO] [ARCH=<arch>] [VERBOSE=1] [VERY_VERBOSE=1] [DEBUG=m|v|b|a]"
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
