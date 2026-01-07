```text
██╗      ██████╗  ██████╗ ██╗   ██╗ ███████╗
██║     ██╔═══██╗██╔════╝ ██║   ██║ ██╔════╝
██║     ██║   ██║██║      ██║   ██║ ███████╗
██║     ██║   ██║██║      ██║   ██║ ╚════██║
███████╗╚██████╔╝╚██████╗ ╚██████╔╝ ███████║
╚══════╝ ╚═════╝  ╚═════╝  ╚═════╝  ╚══════╝

Loader Of Compiled Universal Systems
```

**LOCUS** is an educational userspace ELF loader for Linux and is intended to be
an application that manually loads ELF files and, over time, performs in-memory
ELF loading without relying on the kernel’s default `execve` path.

The primary goal of this project is learning and exploration, not production use.

## Project Goals

The goals of LOCUS are to:

- Understand the ELF loading process in depth
- Reproduce, step by step, what the Linux kernel loader does
- Explore ELF behavior across multiple architectures and Application Binary
  Interfaces (ABIs)
- Experiment with in-memory loading techniques
- Investigate ways to detect or mitigate malicious or malformed ELF loading
- Build tooling that is transparent, verbose, and inspectable

This project prioritizes clarity and correctness over performance or completeness.

## Scope

LOCUS focuses on:

- ELF validation and parsing
- ELF headers and program headers
- Segment mapping and memory layout
- Entry point resolution
- Architecture- and ABI-specific behavior
- Educational instrumentation and logging

It does not aim to replace the system dynamic loader (`ld.so`) or kernel ELF
loader, and it intentionally avoids unsafe or exploit-oriented behavior.

All test binaries are self-built and non-malicious.

## Educational Focus

In addition to ELF internals, LOCUS is also used as a vehicle to explore:

- Advanced GNU Make techniques
  - multi-architecture builds
  - explicit dependency tracking
  - strict correctness checks
- Cross-compilation toolchains
- Doxygen-based documentation for C projects
- Clean API design and documentation discipline
- Debugging and introspection using standard Unix tools

The codebase is written to be readable first, with extensive logging and
assertions where appropriate.

## Repository Layout

`locus/`        → loader source code  
`tests/`        → small test programs (hello world, bss, pie, etc.)  
`build/`        → intermediate build artifacts (.o, .d), per-architecture  
`artifacts/`    → final binaries only, per-architecture  
`docs/`         → generated documentation (Doxygen)

## Documentation

LOCUS uses Doxygen for API and internal documentation.

To generate documentation:

```bash
    $ make docs
```

The generated HTML documentation can be found at: `docs/html/index.html`

Strict documentation warnings are enabled to encourage clear and complete API
documentation.

## Supported Architectures (LOCUS Build)

### Native
- **native** — Host architecture (build & run on the container’s native CPU)

### ARM
- **armhf** — ARM 32-bit, little-endian, hard-float (EABIhf)
- **armel** — ARM 32-bit, little-endian, soft-float (EABI)
- **aarch64** — ARM 64-bit, little-endian (AArch64)

### MIPS
- **mips** — MIPS 32-bit, big-endian
- **mipsel** — MIPS 32-bit, little-endian
- **mips64** — MIPS 64-bit, big-endian (ABI64)
- **mips64el** — MIPS 64-bit, little-endian (ABI64)

### PowerPC
- **powerpc** — PowerPC 32-bit, big-endian
- **ppc64** — PowerPC 64-bit, big-endian
- **ppc64le** — PowerPC 64-bit, little-endian

### RISC-V
- **riscv64** — RISC-V 64-bit, little-endian

### IBM Z
- **s390x** — IBM Z / s390x, 64-bit, big-endian

### SPARC
- **sparc64** — SPARC 64-bit, big-endian

### x86
- **i686** — x86 32-bit (IA-32)
- **x86_64** — x86 64-bit (AMD64 / System V ABI)
- **x32** — x86-64 ISA with 32-bit pointers (x32 ABI)

## Milestones

The milestones below reflect the intended progression of LOCUS.
They are feature and understanding-driven, not time-based.

### Initial Environment and Build Setup
- [X] Create initial Makefile
- [X] Create Dockerfile with toolchains
- [X] Integrate with VS Code Dev Containers extension
- [X] Create simple test program(s)
- [X] Implement logging functionality

### Core ELF Parsing & Validation
- [X] Open and validate ELF files
- [ ] Parse and log ELF headers
- [X] Validate ELF class, endianness, and ABI
- [ ] Parse and log program headers
- [ ] Identify loadable segments (PT_LOAD)
- [ ] Parse and inspect auxiliary program headers (PT_INTERP, PT_DYNAMIC, etc.)

### Memory Mapping & Layout
- [ ] Map ELF segments into memory (mmap)
- [ ] Handle segment permissions and alignment
- [ ] Reproduce kernel-style memory layout decisions
- [ ] Support PIE vs non-PIE binaries
- [ ] Establish initial process memory image

### Dynamic Linking & Relocation
- [ ] Parse dynamic section entries
- [ ] Load and inspect the dynamic linker (PT_INTERP)
- [ ] Parse relocation tables
- [ ] Apply relocations in userspace
- [ ] Resolve symbols across objects
- [ ] Support basic shared library loading

### Execution Preparation
- [ ] Construct initial stack layout
- [ ] Populate argc, argv, and envp
- [ ] Set up auxiliary vectors (auxv)
- [ ] Transfer control to the ELF entry point
- [ ] Validate execution correctness

### In-Memory Loading
- [ ] Load ELF binaries from memory buffers
- [ ] Support execution without backing files
- [ ] Explore self-contained ELF loading
- [ ] Investigate loader behavior without filesystem dependencies

### Cross-Architecture Exploration
- [X] Build and inspect ELF binaries for multiple architectures
- [ ] Validate architecture-specific ELF differences
- [X] Handle endianness variations
- [ ] Compare ABI and calling convention behavior

### Security & Hardening Experiments
- [X] Detect structurally malformed ELF files
- [ ] Explore mitigation of malicious ELF constructs
- [ ] Validate bounds and invariants during loading
- [ ] Experiment with defensive loading strategies

### Tooling & Infrastructure
- [X] Maintain strict build correctness with GNU Make
- [X] Preserve verbose, structured logging
- [X] Generate and maintain Doxygen documentation
- [ ] Add diagnostic and inspection utilities
- [X] Keep the codebase readable and auditable

## Disclaimer

LOCUS is a research and learning project.

It is not hardened, audited, or intended for production use.
Do not use it to load untrusted binaries outside of a controlled environment.

## Philosophy

LOCUS is derived from the Gundam Universal Century concept of a locus—the point
where systems converge and outcomes are determined.

In this project, LOCUS is the point where a compiled binary transitions from a
static artifact into an executable reality.
