#ifndef LEDGER89_INTERNAL_H
#define LEDGER89_INTERNAL_H

/* ledger89_internal.h - private types, on-disk format codecs, I/O
 * abstraction, and shared helpers. Never installed; tests may include it
 * for white-box access. */

#include <stddef.h>

#include <ledger89.h>

/*
 * 64-bit internal arithmetic under strict C89. The public API uses
 * unsigned long; the on-disk format uses fixed 64-bit little-endian fields.
 * __extension__ is accepted by both GCC and Clang in C89 and C23 modes and
 * keeps the source free of compiler-identity conditionals.
 */
__extension__ typedef unsigned long long led89_u64;

typedef unsigned int led89_u32;
typedef unsigned short led89_u16;

#define LED89_FORMAT_VERSION 1u

#define LED89_SEGMENT_HEADER_SIZE 32u
#define LED89_SEGMENT_FOOTER_SIZE 48u
#define LED89_BATCH_HEADER_SIZE 24u
#define LED89_BATCH_FOOTER_SIZE 24u
#define LED89_RECORD_HEADER_SIZE 32u
#define LED89_RECORD_CRC_SIZE 4u
#define LED89_MIN_BATCH_BYTES                                                  \
    (LED89_BATCH_HEADER_SIZE + LED89_BATCH_FOOTER_SIZE)

#define LED89_NAME_MAX 32
#define LED89_ACTIVE_NAME "active.seg"
#define LED89_LOCK_NAME "lock"
#define LED89_TMP_NAME ".tmp-rewrite"

/* I/O abstraction ------------------------------------------------------- */

typedef int led89_fd;
typedef struct led89_dir led89_dir;

#define LED89_OPEN_READ 1
#define LED89_OPEN_WRITE 2
#define LED89_OPEN_CREATE 4

/*
 * Minimal filesystem surface used by the library. Every callback returns
 * LEDGER89_OK or a negative status. The default implementation is POSIX;
 * tests substitute a deterministic model filesystem for crash and fault
 * injection. pwrite must write all bytes or fail; a failure may leave a
 * prefix of the bytes durably written (a torn write).
 */
typedef struct led89_io
{
    void *ctx;

    int (*open)(void *ctx, const char *path, int flags, led89_fd *fd);
    int (*close)(void *ctx, led89_fd fd);
    int (*pread)(void *ctx, led89_fd fd, void *buf, size_t len,
                 led89_u64 offset);
    int (*pwrite)(void *ctx, led89_fd fd, const void *buf, size_t len,
                  led89_u64 offset);
    int (*size)(void *ctx, led89_fd fd, led89_u64 *out);
    int (*truncate)(void *ctx, led89_fd fd, led89_u64 size);
    int (*sync)(void *ctx, led89_fd fd);
    int (*rename)(void *ctx, const char *from, const char *to);
    int (*unlink)(void *ctx, const char *path);
    int (*mkdir)(void *ctx, const char *path);
    int (*sync_dir)(void *ctx, const char *path);
    int (*lock)(void *ctx, const char *path, led89_fd *fd);
    int (*list_open)(void *ctx, const char *path, led89_dir **out);
    int (*list_next)(void *ctx, led89_dir *dir, char *name, size_t cap,
                     int *done);
    int (*list_close)(void *ctx, led89_dir *dir);
} led89_io;

const led89_io *led89_io_posix(void);

/* Format structures ----------------------------------------------------- */

typedef struct led89_seg_header
{
    led89_u64 first_index;
    led89_u32 flags;
} led89_seg_header;

typedef struct led89_rec_header
{
    led89_u64 index;
    led89_u64 tag;
    led89_u32 payload_size;
    led89_u32 flags;
} led89_rec_header;

typedef struct led89_batch_header
{
    led89_u32 record_count;
    led89_u64 first_index;
    led89_u64 batch_bytes;
} led89_batch_header;

typedef struct led89_batch_footer
{
    led89_u32 record_count;
    led89_u64 last_index;
    led89_u32 crc;
} led89_batch_footer;

typedef struct led89_seg_footer
{
    led89_u64 last_index;
    led89_u64 record_count;
    led89_u32 segment_digest;
    led89_u64 body_size;
} led89_seg_footer;

/* CRC-32C. Seed 0 for a fresh checksum; chain by passing the result back. */
led89_u32 led89_crc32c(led89_u32 crc, const void *data, size_t len);

/* Byte comparison that never reads beyond n bytes. Returns 1 when equal. */
int led89_bytes_equal(const unsigned char *a, const unsigned char *b, size_t n);

/* Returns 1 when the first four bytes are a batch header magic. */
int led89_is_batch_header(const unsigned char *in);

/* Little-endian scalar codecs. */
void led89_put_u16(unsigned char *out, led89_u16 v);
void led89_put_u32(unsigned char *out, led89_u32 v);
void led89_put_u64(unsigned char *out, led89_u64 v);
led89_u16 led89_get_u16(const unsigned char *in);
led89_u32 led89_get_u32(const unsigned char *in);
led89_u64 led89_get_u64(const unsigned char *in);

/* Structure codecs. Decoders return LEDGER89_OK or LEDGER89_ERR_CORRUPT. */
void led89_seg_header_encode(unsigned char *out, const led89_seg_header *h);
int led89_seg_header_decode(const unsigned char *in, led89_seg_header *h);
void led89_rec_header_encode(unsigned char *out, const led89_rec_header *h);
int led89_rec_header_decode(const unsigned char *in, led89_rec_header *h);
void led89_batch_header_encode(unsigned char *out, const led89_batch_header *h);
int led89_batch_header_decode(const unsigned char *in, led89_batch_header *h);
void led89_batch_footer_encode(unsigned char *out, const led89_batch_footer *f);
int led89_batch_footer_decode(const unsigned char *in, led89_batch_footer *f);
void led89_seg_footer_encode(unsigned char *out, const led89_seg_footer *f);
int led89_seg_footer_decode(const unsigned char *in, led89_seg_footer *f);

/* Width conversions. Return LEDGER89_ERR_RANGE when not representable. */
int led89_u64_to_index(led89_u64 v, ledger89_index *out);
int led89_u64_to_size(led89_u64 v, size_t *out);
int led89_size_to_u64(size_t v, led89_u64 *out);
int led89_u64_to_u32(led89_u64 v, led89_u32 *out);

/* Pure path/name helpers. */
int led89_path_join(char *out, size_t cap, const char *dir, const char *name);
void led89_name_format(char *out, led89_u64 index);
int led89_name_parse(const char *name, led89_u64 *index);

/* Handle state ---------------------------------------------------------- */

typedef struct led89_segment
{
    led89_u64 first_index;
    led89_u64 last_index;
    char name[LED89_NAME_MAX];
} led89_segment;

/*
 * Sequential record walker over one segment. Validates record CRCs and
 * batch framing as it advances; yields payload pointers into its own
 * reusable buffer.
 */
typedef struct led89_walk
{
    const led89_io *io;
    led89_fd fd;
    led89_u64 end;
    led89_u64 batch_start;
    led89_u64 batch_bytes;
    led89_u64 next_index;
    led89_u64 offset;
    led89_u32 batch_count;
    led89_u32 records_left;
    led89_u32 batch_crc;
    int batch_active;
    unsigned char *buf;
    size_t cap;
    size_t payload_size;
} led89_walk;

struct ledger89_iter
{
    ledger89 *l;
    led89_u64 epoch;
    led89_u64 next_index;
    led89_u64 end_index;
    size_t seg_pos;
    led89_fd fd;
    int own_fd;
    int in_active;
    int walk_open;
    led89_walk walk;
    struct ledger89_iter *next;
};

struct ledger89
{
    const led89_io *io;
    char *path;
    char *scratch;
    size_t scratch_cap;
    char *scratch2;
    size_t scratch2_cap;
    led89_u64 seg_max_bytes;
    led89_u64 seg_max_records;
    led89_u64 base;
    led89_u64 first_index;
    led89_u64 last_index;
    led89_segment *segments;
    size_t segment_count;
    size_t segment_cap;
    led89_fd active_fd;
    int active_open;
    led89_u64 active_first;
    led89_u64 active_offset;
    led89_u64 active_records;
    int dirty;
    int faulted;
    ledger89_observer_fn observer;
    void *observer_ctx;
    led89_u64 epoch;
    led89_fd lock_fd;
    int lock_open;
    struct ledger89_iter *iters;
    led89_walk read_walk;
};

/* Internal API shared across modules. */
int led89_open_io(ledger89 **out, const ledger89_config *config,
                  const led89_io *io);
void led89_free(ledger89 *l);

int led89_recover(ledger89 *l);
int led89_scan_batch(const led89_io *io, led89_fd fd, led89_u64 offset,
                     led89_u64 end, led89_u64 *next, led89_u64 *first,
                     led89_u64 *last, led89_u32 *count);

void led89_walk_init(led89_walk *w, const led89_io *io, led89_fd fd,
                     led89_u64 start, led89_u64 end);
void led89_walk_restart(led89_walk *w, const led89_io *io, led89_fd fd,
                        led89_u64 start, led89_u64 end);
int led89_walk_next(led89_walk *w, led89_rec_header *rh,
                    const unsigned char **payload);
void led89_walk_free(led89_walk *w);

int led89_active_append(ledger89 *l, const ledger89_record *records,
                        size_t count);
int led89_write_record(ledger89 *l, led89_fd fd, led89_u64 *offset,
                       led89_u32 *bcrc, const ledger89_record *r);
int led89_write_batch(ledger89 *l, led89_fd fd, led89_u64 *offset,
                      const ledger89_record *records, size_t count,
                      led89_u64 first_index, led89_u64 batch_bytes);
int led89_segment_find(const ledger89 *l, led89_u64 index, size_t *pos,
                       int *is_active);
int led89_digest_range(ledger89 *l, led89_fd fd, led89_u64 start, led89_u64 end,
                       led89_u32 *out);
int led89_active_create(ledger89 *l, led89_u64 first);
int led89_active_path(ledger89 *l);
void led89_active_close(ledger89 *l);
int led89_install_tmp(ledger89 *l, const char *target_name);
int led89_write_active_header(ledger89 *l, led89_u64 first);
int led89_segments_add(ledger89 *l, const char *name, led89_u64 first,
                       led89_u64 last);
int led89_seal_active(ledger89 *l);
int led89_should_rotate(const ledger89 *l, led89_u64 count, led89_u64 bytes);
led89_u64 led89_batch_bytes(const ledger89_record *records, size_t count);
size_t led89_chunk_size(size_t cap, led89_u64 left);

int led89_read_impl(ledger89 *l, led89_u64 index, ledger89_view *out);
int led89_iter_open_impl(ledger89 *l, led89_u64 first, led89_u64 last,
                         ledger89_iter **out);
int led89_iter_next_impl(ledger89_iter *it, ledger89_view *out);
void led89_iter_free(ledger89_iter *it);

#endif /* LEDGER89_INTERNAL_H */
