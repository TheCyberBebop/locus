FROM debian:13.2

ENV DEBIAN_FRONTEND=noninteractive

# Install required tools & toolchains
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    gdb \
    make \
    pkg-config \
    git \
    python3 \
    python3-pip \
    file \
    sudo \
    binutils \
    doxygen \
    graphviz \
    zsh \
    curl \
    strace \
    ltrace \
    gdb-multiarch \
    elfutils \
    patchelf \
    xz-utils \
    unzip \
    less \
    vim \
    # ==========================
    # Native (host: amd64, little-endian)
    # ==========================
    build-essential \
    gcc \
    g++ \
    # ==========================
    # ARM 64-bit (AArch64, little-endian)
    # ==========================
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    libc6-dev-arm64-cross \
    # ==========================
    # ARM 32-bit (armhf, hard-float, little-endian)
    # ==========================
    gcc-arm-linux-gnueabihf \
    g++-arm-linux-gnueabihf \
    libc6-dev-armhf-cross \
    # ==========================
    # ARM 32-bit (armel, soft-float, little-endian)
    # ==========================
    gcc-arm-linux-gnueabi \
    g++-arm-linux-gnueabi \
    libc6-dev-armel-cross \
    # ==========================
    # MIPS 32-bit (big-endian)
    # ==========================
    gcc-mips-linux-gnu \
    g++-mips-linux-gnu \
    libc6-dev-mips-cross \
    # ==========================
    # MIPS 32-bit (little-endian)
    # ==========================
    gcc-mipsel-linux-gnu \
    g++-mipsel-linux-gnu \
    libc6-dev-mipsel-cross \
    # ==========================
    # MIPS 64-bit (big-endian)
    # ==========================
    gcc-mips64-linux-gnuabi64 \
    g++-mips64-linux-gnuabi64 \
    libc6-dev-mips64-cross \
    # ==========================
    # MIPS 64-bit (little-endian)
    # ==========================
    gcc-mips64el-linux-gnuabi64 \
    g++-mips64el-linux-gnuabi64 \
    libc6-dev-mips64el-cross \
    # ==========================
    # PowerPC 32-bit (big-endian)
    # ==========================
    gcc-powerpc-linux-gnu \
    g++-powerpc-linux-gnu \
    libc6-dev-powerpc-cross \
    # ==========================
    # PowerPC 64-bit (big-endian)
    # ==========================
    gcc-powerpc64-linux-gnu \
    g++-powerpc64-linux-gnu \
    libc6-dev-ppc64-cross \
    # ==========================
    # PowerPC 64-bit (little-endian)
    # ==========================
    gcc-powerpc64le-linux-gnu \
    g++-powerpc64le-linux-gnu \
    libc6-dev-ppc64el-cross \
    # ==========================
    # RISC-V 64-bit (little-endian)
    # ==========================
    gcc-riscv64-linux-gnu \
    g++-riscv64-linux-gnu \
    libc6-dev-riscv64-cross \
    # ==========================
    # s390x (IBM Z, big-endian)
    # ==========================
    gcc-s390x-linux-gnu \
    g++-s390x-linux-gnu \
    libc6-dev-s390x-cross \
    # ==========================
    # SPARC 64-bit (big-endian)
    # ==========================
    gcc-sparc64-linux-gnu \
    g++-sparc64-linux-gnu \
    libc6-dev-sparc64-cross \
    # ==========================
    # x32 ABI (x86_64 ISA, 32-bit pointers, little-endian)
    # ==========================
    gcc-x86-64-linux-gnux32 \
    g++-x86-64-linux-gnux32 \
    libc6-dev-x32 \
    # ==========================
    # x86 32-bit (i686, little-endian)
    # ==========================
    gcc-i686-linux-gnu \
    g++-i686-linux-gnu \
    libc6-dev-i386-amd64-cross \
    # ==========================
    # x86_64 64-bit (little-endian)
    # ==========================
    gcc-x86-64-linux-gnu \
    g++-x86-64-linux-gnu \
    libc6-dev-amd64-cross \
    # ==========================
    # Execute cross-built tests via emulation
    # ==========================
    qemu-user \
    qemu-user-binfmt \
    # ==========================
    # Target runtime sysroots (needed for dynamic qemu-user execution)
    # ==========================
    libc6-arm64-cross \
    libc6-armhf-cross \
    libc6-armel-cross \
    libc6-mips-cross \
    libc6-mipsel-cross \
    libc6-mips64-cross \
    libc6-mips64el-cross \
    libc6-powerpc-cross \
    libc6-ppc64-cross \
    libc6-ppc64el-cross \
    libc6-riscv64-cross \
    libc6-s390x-cross \
    libc6-sparc64-cross \
    # ==========================
    # GCC runtime (needed for many dynamic C binaries)
    # ==========================
    libgcc-s1-arm64-cross \
    libgcc-s1-armhf-cross \
    libgcc-s1-armel-cross \
    libgcc-s1-mips-cross \
    libgcc-s1-mipsel-cross \
    libgcc-s1-mips64-cross \
    libgcc-s1-mips64el-cross \
    libgcc-s1-powerpc-cross \
    libgcc-s1-ppc64-cross \
    libgcc-s1-ppc64el-cross \
    libgcc-s1-riscv64-cross \
    libgcc-s1-s390x-cross \
    libgcc-s1-sparc64-cross \
    # ==========================
    # i686 runtime sysroot (for dynamic execution)
    # ==========================
    libc6-i386-cross \
    libgcc-s1-i386-cross \
    && rm -rf /var/lib/apt/lists/*

# Non-root dev user
RUN useradd -ms /usr/bin/zsh dev \
    && echo "dev ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/dev \
    && chmod 0440 /etc/sudoers.d/dev \
    && mkdir -p /workspace \
    && chown -R dev:dev /workspace
WORKDIR /workspace
USER dev

# Install Oh-My-ZSH
RUN sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended

# Keep container running
CMD ["sleep", "infinity"]
