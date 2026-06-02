#pragma once
#include <stddef.h>

/*
 * Tiny read-only virtual filesystem (ramdisk).
 * Files are compiled into the kernel as string literals.
 */

typedef struct {
    const char *path;
    const char *content;
} VfsFile;

void        vfs_init(void);
int         vfs_exists(const char *path);
const char *vfs_read(const char *path);     /* NULL if not found */
/* list entries whose path starts with dir; returns count */
int         vfs_list(const char *dir, const char *out[], int max_out);
