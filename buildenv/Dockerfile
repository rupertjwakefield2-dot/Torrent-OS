FROM --platform=linux/amd64 randomdude/gcc-cross-x86_64-elf

RUN apt-get update && apt-get install -y \
    nasm \
    xorriso \
    grub-pc-bin \
    grub-common \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

VOLUME /root/env
WORKDIR /root/env
