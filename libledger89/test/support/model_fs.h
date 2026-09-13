#ifndef MODEL_FS_H
#define MODEL_FS_H

/* model_fs.h - deterministic in-memory filesystem implementing the internal
 * led89_io vtable. Models a page cache (live state) and durable state:
 * fsync promotes file bytes, directory fsync promotes names, and a power
 * crash discards everything not promoted. Supports per-call failure
 * injection and torn writes. Not part of the library. */

#include <stddef.h>

#include "ledger89_internal.h"

#define MFS_MAX_PATH 128

/* Torn-write request: persist half of the write instead of the full length. */
#define MFS_TORN_HALF (-2)

/* Operations tracked for crash injection. */
enum
{
    MFS_OP_OPEN = 0,
    MFS_OP_CLOSE,
    MFS_OP_PREAD,
    MFS_OP_PWRITE,
    MFS_OP_SIZE,
    MFS_OP_TRUNCATE,
    MFS_OP_SYNC,
    MFS_OP_RENAME,
    MFS_OP_UNLINK,
    MFS_OP_MKDIR,
    MFS_OP_SYNC_DIR,
    MFS_OP_LOCK,
    MFS_OP_LIST_OPEN,
    MFS_OP_LIST_NEXT,
    MFS_OP_LIST_CLOSE,
    MFS_OP_COUNT
};

typedef struct mfs_file
{
    char name[MFS_MAX_PATH];
    int live;         /* entry exists in the current (page-cache) namespace */
    int durable_live; /* entry exists in the durable namespace */
    unsigned char *live_data;
    size_t live_size;
    unsigned char *dur_data;
    size_t dur_size;
} mfs_file;

typedef struct mfs
{
    mfs_file *files;
    size_t count;
    size_t cap;

    /* Failure injection: fail the next N calls of the named operation. */
    int fail_open;
    int fail_close;
    int fail_pread;
    int fail_pwrite;
    int fail_size;
    int fail_truncate;
    int fail_sync;
    int fail_rename;
    int fail_unlink;
    int fail_mkdir;
    int fail_sync_dir;
    int fail_lock;
    int fail_list;

    /* Torn write: when >= 0, the next pwrite persists only this many bytes
     * and reports failure; MFS_TORN_HALF persists half. Consumed by that
     * call. */
    long torn_bytes;

    /* Crash injection: crash on the (skip+1)-th call of crash_op. Once the
     * crash fires, every later call is a no-op failure. */
    int crash_op;
    int crash_skip;
    int crash_armed;
    int crash_torn;
    int crashed;
    int dead;

    /* Fail exactly one call: the (skip+1)-th call of fail_op. */
    int fail_at_op;
    int fail_at_skip;
    int fail_at_armed;
    int fail_at_fired;
} mfs;

void mfs_init(mfs *fs);
void mfs_destroy(mfs *fs);
void mfs_bind(led89_io *io, mfs *fs);

/* Power loss: discard every byte and name not made durable. */
void mfs_crash(mfs *fs);

/* Arm a crash on the (skip+1)-th call of op. skip == 0 crashes the first. */
void mfs_arm_crash(mfs *fs, int op, int skip);

/* Arm a torn crash on the (skip+1)-th pwrite: half the bytes persist. */
void mfs_arm_crash_torn(mfs *fs, int skip);

/* Fail exactly the (skip+1)-th call of op; later calls succeed. */
void mfs_fail_at(mfs *fs, int op, int skip);

/* Return 1 once the armed call failure has fired. */
int mfs_fail_fired(const mfs *fs);

/* Return 1 once an armed crash has fired. */
int mfs_crashed(const mfs *fs);

/* Deep-copy the filesystem state (not the injection settings). */
void mfs_clone(mfs *dst, const mfs *src);

/* Direct access for test assertions. */
mfs_file *mfs_find(mfs *fs, const char *name);
size_t mfs_live_size(mfs *fs, const char *name);
int mfs_exists(mfs *fs, const char *name);

/* Create a durable file directly (pre-state setup). */
int mfs_put(mfs *fs, const char *name, const void *data, size_t size);

/* Insert bytes into a live file at an offset (corruption/tear setup). */
int mfs_insert(mfs *fs, const char *name, size_t offset, const void *data,
               size_t size);

/* Replace one live byte (corruption setup). */
int mfs_poke(mfs *fs, const char *name, size_t offset, unsigned char value);

#endif /* MODEL_FS_H */
