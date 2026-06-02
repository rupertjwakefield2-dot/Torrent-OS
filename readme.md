# TorrentOS

A custom 64-bit operating system built entirely from scratch in C and NASM.
No Linux, no Arch, no existing kernel — everything is hand-written bare metal.

## Contents

- [Quick Start](#quick-start)
- [Build](#build)
- [Run](#run)
- [First Boot](#first-boot)
- [Shell Commands](#shell-commands)
- [What's Working](#whats-working)
- [Known Limits](#known-limits)

## Quick Start

```bash
# Build the Docker environment (once)
docker build -t torrentos-buildenv buildenv/

# Start the build container
docker run --rm -it -v $(pwd):/root/env torrentos-buildenv

# Inside the container:
make build-x86_64
exit

# Boot in QEMU
qemu-system-x86_64 \
  -drive file=dist/x86_64/TorrentOS.iso,format=raw,media=cdrom,readonly=on \
  -boot d \
  -m 256M
```

## Build

The repo builds inside the `torrentos-buildenv` Docker image
(`randomdude/gcc-cross-x86_64-elf` base), which provides
`x86_64-elf-gcc`, `nasm`, and `grub-mkrescue`.

## Run

```bash
qemu-system-x86_64 \
  -drive file=dist/x86_64/TorrentOS.iso,format=raw,media=cdrom,readonly=on \
  -boot d \
  -m 256M
```

Add `-netdev user,id=n0 -device rtl8139,netdev=n0` when testing network code.

## First Boot

On first boot TorrentOS runs the **Setup Wizard** (5 steps):

1. **Welcome** — introduction screen
2. **Username** — pick your username (letters, digits, underscore)
3. **Timezone** — select your UTC offset from a scrollable list
4. **Password** — set a password (4–100 characters, must be confirmed)
5. **Done** — summary screen before login

After setup the **Login Screen** appears. You have **10 attempts** to enter the
correct password; on the 10th failure the system halts for security.

## Shell Commands

| Command           | Description                            |
|-------------------|----------------------------------------|
| `help`            | Show all commands                      |
| `clear`           | Clear the screen                       |
| `echo <text>`     | Print text                             |
| `ver`             | OS version                             |
| `uptime`          | System uptime and tick counter         |
| `mem`             | Physical memory and heap usage         |
| `date`            | Current date, time, and timezone       |
| `whoami`          | Current username                       |
| `wifi`            | WiFi adapter status (PCI scan)         |
| `bluetooth`       | Bluetooth adapter status (PCI scan)    |
| `timezone [±N]`   | Show or set UTC offset (e.g. `+8`)     |
| `darkmode`        | Toggle dark / light theme              |
| `battery`         | Power status and battery-saver flag    |
| `browser`         | Open the text browser                  |
| `apps`            | Open the App Store / launcher          |
| `settings`        | Open Settings (theme, battery saver)   |
| `halt`            | Power off                              |
| `reboot`          | Restart                                |

### Browser pages

| URL               | Content                              |
|-------------------|--------------------------------------|
| `about:home`      | Home / bookmarks page                |
| `about:help`      | Keyboard shortcuts                   |
| `about:system`    | Hardware and OS information          |

Navigate with **Tab** (address bar), **Up/Down** (scroll), **Backspace**
(home), **Q / ESC** (quit).

## What's Working

- Multiboot2 boot via GRUB
- 64-bit long mode with identity-mapped paging (1 GB)
- IDT — all 32 CPU exception handlers (red panic screen with register dump)
- PIC remapping (IRQ 0–15 → vectors 32–47)
- PIT timer at 1000 Hz (1 ms resolution)
- PS/2 keyboard — interrupt-driven, shift/caps/arrows/history
- Physical memory manager (bitmap, up to 256 MB)
- Kernel heap (kmalloc/kfree with coalescing)
- **CMOS RTC** — real date/time with timezone offset
- **PCI bus scan** — detects WiFi and Bluetooth adapters
- **Battery module** — reports AC power; battery-saver software flag
- **Setup Wizard** — username, timezone, password (4-100 chars)
- **Login screen** — 10-attempt lockout
- **Status bar** — macOS-style top bar: OS name, username, clock, power
- **Dark / light theme** — toggled with `darkmode` or via Settings
- **Text browser** — `about:home`, `about:help`, `about:system`
- **App Store / launcher** — Browser, Settings, System Info
- **Interactive shell** with readline (arrows, history ×16, insert/delete)
- macOS-style prompt: `username@torrentos:~ % `

## Known Limits

- No storage drivers (ATA/AHCI/NVMe) — nothing persists across reboots
- No network stack — WiFi/BT hardware detected but not driven
- VGA text mode only (80×25) — no graphics framebuffer
- Paging is a flat identity map — no per-process virtual memory
- No userspace / ring-3 / syscalls
- No package manager (app store is a launcher only)
