#ifndef ATOMICFILE_H
#define ATOMICFILE_H

#include <stddef.h>

/* Writes data to <path>.tmp.<pid>, flushes to disk, then renames over <path>.
   Returns 1 on success. */
int file_write_atomic(const char *path, const void *data, size_t len);

/* Deletes orphaned <dir>/<file>.tmp.<pid> leftovers from previous crashes. */
void file_cleanup_tmp(const char *dir);

#endif