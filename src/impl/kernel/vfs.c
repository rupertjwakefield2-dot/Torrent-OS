#include "vfs.h"
#include "str.h"

/* ── built-in file table ── */
static const VfsFile files[] = {
    {
        "/etc/os-release",
        "NAME=TorrentOS\n"
        "VERSION=0.1-dev\n"
        "ARCH=x86_64\n"
        "BUILD=C+NASM baremental\n"
        "HOME_URL=https://github.com/rupertjwakefield2-dot/Torrent-OS\n"
    },
    {
        "/etc/hostname",
        "torrentos\n"
    },
    {
        "/etc/motd",
        "Welcome to TorrentOS v0.1-dev\n"
        "A custom 64-bit OS built from scratch.\n"
        "Type 'help' for available commands.\n"
    },
    {
        "/etc/timezone",
        "UTC\n"
    },
    {
        "/proc/version",
        "TorrentOS v0.1-dev (x86_64) built with gcc-cross-x86_64-elf + nasm\n"
    },
    {
        "/proc/cpuinfo",
        "Architecture : x86_64\n"
        "Mode         : 64-bit long mode\n"
        "Paging       : 4-level PT, 2MB huge pages, 1GB identity map\n"
        "Note         : run 'cpuinfo' in the shell for live CPU details\n"
    },
    {
        "/proc/meminfo",
        "MemTotal: see 'mem' command\n"
    },
    {
        "/bin/help",
        "See 'help' command in the shell.\n"
    },
    {
        "/boot/grub/grub.cfg",
        "menuentry \"TorrentOS\" {\n"
        "    multiboot2 /boot/torrentos-ker.bin\n"
        "    boot\n"
        "}\n"
    },
    {
        "/readme.txt",
        "TorrentOS v0.1-dev\n"
        "==================\n"
        "This is a custom 64-bit operating system written from scratch.\n"
        "No Linux. No Arch. Pure bare-metal C and NASM.\n"
        "\n"
        "Shell commands:  type 'help'\n"
        "File system:     ls <dir>   cat <file>\n"
        "Browser:         browser\n"
        "App store:       apps\n"
    },
    { (void*)0, (void*)0 },
};

void vfs_init(void) { /* static table — nothing to init */ }

int vfs_exists(const char *path) {
    for (int i = 0; files[i].path; i++)
        if (str_eq(files[i].path, path)) return 1;
    return 0;
}

const char *vfs_read(const char *path) {
    for (int i = 0; files[i].path; i++)
        if (str_eq(files[i].path, path)) return files[i].content;
    return (void*)0;
}

int vfs_list(const char *dir, const char *out[], int max_out) {
    int count = 0;
    size_t dlen = str_len(dir);
    for (int i = 0; files[i].path && count < max_out; i++) {
        const char *p = files[i].path;
        if (!str_pfx(p, dir)) continue;
        /* must not have another '/' after the prefix */
        const char *rest = p + dlen;
        if (*rest == '/') rest++;
        int is_direct = 1;
        for (const char *c = rest; *c; c++)
            if (*c == '/') { is_direct = 0; break; }
        if (is_direct && *rest) out[count++] = p;
    }
    return count;
}
