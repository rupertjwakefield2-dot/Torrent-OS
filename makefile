# ─────────────────────────────────────────────────────────────────────────────
# TorrentOS build system
# Targets: build-x86_64   build-aarch64   build-all   clean
# ─────────────────────────────────────────────────────────────────────────────

CFLAGS_COMMON = -I src/intf -ffreestanding -O2 -Wall -Wextra -Wno-unused-parameter

# ── toolchains ────────────────────────────────────────────────────────────────
CC_X86   = x86_64-elf-gcc
LD_X86   = x86_64-elf-ld
NASM     = nasm
CFLAGS_X86 = -c $(CFLAGS_COMMON)

CC_ARM   = aarch64-linux-gnu-gcc
LD_ARM   = aarch64-linux-gnu-ld
CFLAGS_ARM = -c $(CFLAGS_COMMON) -march=armv8-a -mgeneral-regs-only \
             -nostdlib -nostartfiles

# ── x86_64 source lists ───────────────────────────────────────────────────────
x86_kernel_src := $(shell find src/impl/kernel -name '*.c')
x86_plat_c_src := $(shell find src/impl/x86_64  -name '*.c')
x86_plat_a_src := $(shell find src/impl/x86_64  -name '*.asm')

x86_kernel_obj := $(patsubst src/impl/kernel/%.c,  build/x86_64/kernel/%.o,  $(x86_kernel_src))
x86_plat_c_obj := $(patsubst src/impl/x86_64/%.c,  build/x86_64/plat/%.o,    $(x86_plat_c_src))
x86_plat_a_obj := $(patsubst src/impl/x86_64/%.asm,build/x86_64/plat/%.o,    $(x86_plat_a_src))
x86_all_obj    := $(x86_kernel_obj) $(x86_plat_c_obj) $(x86_plat_a_obj)

# ── aarch64 source lists ──────────────────────────────────────────────────────
# shared kernel except main.c (aarch64 has its own)
arm_kernel_src := $(filter-out src/impl/kernel/main.c, \
                    $(shell find src/impl/kernel -name '*.c'))
arm_plat_c_src := $(shell find src/impl/aarch64 -name '*.c')
arm_plat_s_src := $(shell find src/impl/aarch64 -name '*.S')

arm_kernel_obj := $(patsubst src/impl/kernel/%.c,    build/aarch64/kernel/%.o, $(arm_kernel_src))
arm_plat_c_obj := $(patsubst src/impl/aarch64/%.c,   build/aarch64/plat/%.o,   $(arm_plat_c_src))
arm_plat_s_obj := $(patsubst src/impl/aarch64/%.S,   build/aarch64/plat/%.o,   $(arm_plat_s_src))
arm_all_obj    := $(arm_kernel_obj) $(arm_plat_c_obj) $(arm_plat_s_obj)

# ── x86_64 compile rules ──────────────────────────────────────────────────────
$(x86_kernel_obj): build/x86_64/kernel/%.o : src/impl/kernel/%.c
	@mkdir -p $(dir $@)
	$(CC_X86) $(CFLAGS_X86) $< -o $@

$(x86_plat_c_obj): build/x86_64/plat/%.o : src/impl/x86_64/%.c
	@mkdir -p $(dir $@)
	$(CC_X86) $(CFLAGS_X86) $< -o $@

$(x86_plat_a_obj): build/x86_64/plat/%.o : src/impl/x86_64/%.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

# ── aarch64 compile rules ─────────────────────────────────────────────────────
$(arm_kernel_obj): build/aarch64/kernel/%.o : src/impl/kernel/%.c
	@mkdir -p $(dir $@)
	$(CC_ARM) $(CFLAGS_ARM) $< -o $@

$(arm_plat_c_obj): build/aarch64/plat/%.o : src/impl/aarch64/%.c
	@mkdir -p $(dir $@)
	$(CC_ARM) $(CFLAGS_ARM) $< -o $@

$(arm_plat_s_obj): build/aarch64/plat/%.o : src/impl/aarch64/%.S
	@mkdir -p $(dir $@)
	$(CC_ARM) -c -march=armv8-a -I src/intf $< -o $@

# ── x86_64 link + ISO ─────────────────────────────────────────────────────────
.PHONY: build-x86_64
build-x86_64: $(x86_all_obj)
	@mkdir -p dist/x86_64
	$(LD_X86) -n -T targets/x86_64/linker.ld \
	    -o dist/x86_64/torrentos-ker.bin $(x86_all_obj)
	cp dist/x86_64/torrentos-ker.bin targets/x86_64/iso/boot/torrentos-ker.bin
	grub-mkrescue /usr/lib/grub/i386-pc \
	    -o dist/x86_64/TorrentOS.iso \
	    targets/x86_64/iso
	@printf "\n  x86_64 done:  dist/x86_64/TorrentOS.iso\n"
	@printf "  Run: qemu-system-x86_64 \\\n"
	@printf "    -drive file=dist/x86_64/TorrentOS.iso,format=raw,media=cdrom,readonly=on \\\n"
	@printf "    -boot d -m 256M\n\n"

# ── aarch64 link + flat binary ────────────────────────────────────────────────
.PHONY: build-aarch64
build-aarch64: $(arm_all_obj)
	@mkdir -p dist/aarch64
	$(LD_ARM) -T targets/aarch64/linker.ld \
	    -o dist/aarch64/torrentos-arm64.elf $(arm_all_obj)
	aarch64-linux-gnu-objcopy -O binary \
	    dist/aarch64/torrentos-arm64.elf \
	    dist/aarch64/torrentos-arm64.bin
	@printf "\n  aarch64 done: dist/aarch64/torrentos-arm64.bin\n"
	@printf "  Run: qemu-system-aarch64 -M virt -cpu cortex-a72 \\\n"
	@printf "    -m 256M -nographic \\\n"
	@printf "    -kernel dist/aarch64/torrentos-arm64.bin\n\n"

.PHONY: build-all
build-all: build-x86_64 build-aarch64

.PHONY: clean
clean:
	rm -rf build dist targets/x86_64/iso/boot/torrentos-ker.bin
