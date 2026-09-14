#ifndef LEDGER89_INTERNAL_H
#define LEDGER89_INTERNAL_H

/* ledger89_internal.h - private types, on-disk codecs, I/O abstraction, and
 * shared helpers. Never installed; tests may include it for white-box
 * access. */

#include <stddef.h>

#include <ledger89.h>

/*
 * 64-bit internal arithmetic under strict C89. The public API exposes the
 * portable ledger89_u64 struct; internals use unsigned long long. The
 * __extension__ keyword is accepted by both GCC and Clang in C89 and C23
 * modes and keeps the source free of compiler-identity conditionals.
 */
__extension__ typedef unsigned long long led89_u64;

typedef unsigned int led89_u32;
typedef unsigned short led89_u16;

#define LED89_FORMAT_VERSION 2u

#define LED89_CURRENT_SIZE 32u
#define LED89_MANIFEST_HEADER_SIZE 64u
#define LED89_SEALED_DESC_SIZE 24u
#define LED89_ACTIVE_DESC_SIZE 16u
#define LED89_MANIFEST_CRC_SIZE 4u
#define LED89_PART_HEADER_SIZE 64u
#define LED89_BATCH_HEADER_SIZE 32u
#define LED89_BATCH_FOOTER_SIZE 24u
#define LED89_RECORD_LENGTH_SIZE 4u
#define LED89_RECORD_CRC_SIZE 4u
#define LED89_MARKER_SIZE 64u
#define LED89_MARKER_TRAILER_SIZE 8u
#define LED89_SEALED_FOOTER_SIZE 64u
#define LED89_MIN_BATCH_BYTES                                                  \
    (LED89_BATCH_HEADER_SIZE + LED89_BATCH_FOOTER_SIZE)

#define LED89_NAME_MAX 32
#define LED89_LOCK_NAME "lock"
#define LED89_CURRENT_NAME "CURRENT"
#define LED89_CURRENT_TMP_NAME "CURRENT.tmp"
#define LED89_PART_PREFIX "part."
#define LED89_MANIFEST_PREFIX "MANIFEST."

/* Default rotation target; white-box tests may lower it. */
#define LED89_PART_TARGET_BYTES ((led89_u64)67108864)

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
 *
 * entropy fills len bytes from a source suitable for ledger identity. The
 * library never uses clocks; tests inject deterministic bytes.
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
    int (*lock)(void *ctx, const char *path, int exclusive, int create,
                led89_fd *fd);
    int (*list_open)(void *ctx, const char *path, led89_dir **out);
    int (*list_next)(void *ctx, led89_dir *dir, char *name, size_t cap,
                     int *done);
    int (*list_close)(void *ctx, led89_dir *dir);
    int (*entropy)(void *ctx, unsigned char *out, size_t len);
} led89_io;

const led89_io *led89_io_posix(void);

/* Scalar helpers -------------------------------------------------------- */

led89_u64 led89_from_public(ledger89_u64 v);
ledger89_u64 led89_to_public(led89_u64 v);
int led89_u64_add(led89_u64 a, led89_u64 b, led89_u64 *out);
led89_u64 led89_u64_inc(led89_u64 a);
led89_u64 led89_u64_dec(led89_u64 a);
int led89_u64_cmp(led89_u64 a, led89_u64 b);
int led89_u64_is_zero(led89_u64 a);
int led89_u64_to_u32(led89_u64 v, led89_u32 *out);
int led89_u64_to_size(led89_u64 v, size_t *out);
int led89_size_to_u64(size_t v, led89_u64 *out);

/* CRC-32C. Seed 0 for a fresh checksum; chain by passing the result back. */
led89_u32 led89_crc32c(led89_u32 crc, const void *data, size_t len);

/* Byte comparison that never reads beyond n bytes. Returns 1 when equal. */
int led89_bytes_equal(const unsigned char *a, const unsigned char *b, size_t n);

/* Little-endian scalar codecs. */
void led89_put_u16(unsigned char *out, led89_u16 v);
void led89_put_u32(unsigned char *out, led89_u32 v);
void led89_put_u64(unsigned char *out, led89_u64 v);
led89_u16 led89_get_u16(const unsigned char *in);
led89_u32 led89_get_u32(const unsigned char *in);
led89_u64 led89_get_u64(const unsigned char *in);

/* Format structures ----------------------------------------------------- */

typedef struct led89_current
{
    led89_u64 generation;
} led89_current;

typedef struct led89_part_desc
{
    led89_u64 file_id;
    led89_u64 first;
    led89_u64 end;
} led89_part_desc;

typedef struct led89_manifest
{
    led89_u64 generation;
    unsigned char uuid[16];
    led89_u64 revision;
    led89_u64 first;
    led89_u32 sealed_count;
    led89_part_desc *sealed;
    led89_part_desc active;
} led89_manifest;

typedef struct led89_part_header
{
    unsigned char uuid[16];
    led89_u64 file_id;
    led89_u64 revision;
    led89_u64 first;
} led89_part_header;

typedef struct led89_batch_header
{
    led89_u32 count;
    led89_u64 first;
    led89_u64 bytes;
} led89_batch_header;

typedef struct led89_batch_footer
{
    led89_u32 count;
    led89_u64 last;
} led89_batch_footer;

typedef struct led89_marker
{
    unsigned char uuid[16];
    led89_u64 file_id;
    led89_u64 revision;
    led89_u64 end;
} led89_marker;

typedef struct led89_sealed_footer
{
    unsigned char uuid[16];
    led89_u64 file_id;
    led89_u64 first;
    led89_u64 end;
    led89_u64 records;
    led89_u32 digest;
} led89_sealed_footer;

void led89_current_encode(unsigned char *out, const led89_current *c);
int led89_current_decode(const unsigned char *in, led89_current *c);

size_t led89_manifest_bytes(led89_u32 sealed_count);
void led89_manifest_encode(unsigned char *out, const led89_manifest *m);
int led89_manifest_decode(const unsigned char *in, size_t size,
                          led89_manifest *m);
void led89_manifest_free(led89_manifest *m);

void led89_part_header_encode(unsigned char *out, const led89_part_header *h);
int led89_part_header_decode(const unsigned char *in, led89_part_header *h);

void led89_batch_header_encode(unsigned char *out, const led89_batch_header *h);
int led89_batch_header_decode(const unsigned char *in, led89_batch_header *h);

void led89_batch_footer_encode(unsigned char *out, const led89_batch_footer *f);
int led89_batch_footer_decode(const unsigned char *in, led89_batch_footer *f);

void led89_marker_encode(unsigned char *out, const led89_marker *m);
int led89_marker_decode(const unsigned char *in, led89_marker *m);

void led89_sealed_footer_encode(unsigned char *out,
                                const led89_sealed_footer *f);
int led89_sealed_footer_decode(const unsigned char *in, led89_sealed_footer *f);

/* Returns 1 when the four bytes are a batch header magic. */
int led89_is_batch_header(const unsigned char *in);

/* Returns 1 when the four bytes are a batch footer magic. */
int led89_is_batch_footer(const unsigned char *in);

/* Returns 1 when the eight bytes are a marker magic. */
int led89_is_marker(const unsigned char *in);

/* Pure path/name helpers. */
int led89_path_join(char *out, size_t cap, const char *dir, const char *name);
void led89_part_name(char *out, led89_u64 file_id);
void led89_manifest_name(char *out, led89_u64 generation);
int led89_part_name_parse(const char *name, led89_u64 *file_id);
int led89_manifest_name_parse(const char *name, led89_u64 *generation);

/* Handle state ---------------------------------------------------------- */

typedef struct led89_part
{
    led89_part_desc desc;
    int sealed;
    int open;
    led89_fd fd;
    led89_u64 bytes; /* physical bytes for the active part */
} led89_part;

typedef struct led89_batch_dir_entry
{
    led89_u64 first;
    led89_u64 count;
    led89_u64 offset;
    led89_u64 bytes;
    size_t part;
} led89_batch_dir_entry;

struct ledger89
{
    const led89_io *io;
    char *path;
    char *scratch;
    size_t scratch_cap;
    char *scratch2;
    size_t scratch2_cap;
    int writable;
    int poisoned;
    int dirty;
    ledger89_id id;
    led89_u64 revision;
    led89_u64 first;
    led89_u64 stable_end;
    led89_u64 end;
    led89_u64 generation;
    led89_u64 next_file_id;
    led89_u64 part_target;
    size_t active_index;
    led89_part *parts;
    size_t part_count;
    size_t part_cap;
    led89_batch_dir_entry *dir;
    size_t dir_count;
    size_t dir_cap;
    unsigned char *buf;
    size_t buf_cap;
    led89_fd lock_fd;
    int lock_open;
};

/* Internal API shared across modules. */
int led89_open_io(ledger89 **out, const char *path, unsigned long flags,
                  const led89_io *io);
void led89_free(ledger89 *l);

int led89_io_error(ledger89 *l, int rc);
led89_u64 led89_u64_left(led89_u64 total, led89_u64 done);
size_t led89_chunk_of(led89_u64 left);
led89_u64 led89_at_add(led89_u64 at, led89_u64 n);
void led89_close_quiet(ledger89 *l, led89_fd fd);
void led89_unlink_quiet(ledger89 *l, const char *path);
void led89_list_close_quiet(ledger89 *l, led89_dir *dir);
void led89_sync_dir_quiet(ledger89 *l);

/* manifest.c */
int led89_current_read(ledger89 *l, led89_current *c);
int led89_current_publish(ledger89 *l, led89_u64 generation);
int led89_manifest_read(ledger89 *l, led89_u64 generation, led89_manifest *m);
int led89_manifest_publish(ledger89 *l, const led89_manifest *m);
int led89_gc_orphans(ledger89 *l, const led89_manifest *keep);

/* segment.c */
int led89_part_open(ledger89 *l, led89_u64 file_id, int flags, led89_fd *fd);
int led89_dir_add(ledger89 *l, size_t part, led89_u64 first, led89_u64 count,
                  led89_u64 offset, led89_u64 bytes);
int led89_part_create(ledger89 *l, led89_u64 file_id, led89_u64 first,
                      led89_u64 revision, size_t *index_out);
void led89_part_close(ledger89 *l, led89_part *p);
int led89_part_scan(ledger89 *l, size_t part_index, int sealed,
                    led89_u64 *stable_bytes);
int led89_active_append(ledger89 *l, const ledger89_slice *records,
                        size_t count, led89_u64 *first_out);
int led89_batch_total(const ledger89_slice *records, size_t count,
                      led89_u64 *out);
int led89_write_marker(ledger89 *l, led89_fd fd, led89_u64 file_id,
                       led89_u64 revision, led89_u64 *offset, led89_u64 end);
int led89_write_sealed_footer(ledger89 *l, led89_part *p, led89_u64 end);
int led89_sync_impl(ledger89 *l);
int led89_seal_active(ledger89 *l);
int led89_rotate_impl(ledger89 *l);
int led89_emit_batch(ledger89 *l, led89_fd fd, led89_u64 *offset,
                     const ledger89_slice *records, size_t count,
                     led89_u64 first);

/* recover.c */
int led89_recover(ledger89 *l);
int led89_find_last_marker(ledger89 *l, led89_fd fd, led89_u64 size,
                           led89_marker *marker, led89_u64 *offset);
int led89_create_ledger(ledger89 *l);
int led89_clean_dir_for_create(ledger89 *l);

/* read.c */
int led89_read_impl(ledger89 *l, led89_u64 index, void *data_out,
                    size_t capacity, size_t *size_out);
int led89_dir_find(ledger89 *l, led89_u64 index, size_t *entry);
int led89_batch_read_record(ledger89 *l, const led89_batch_dir_entry *e,
                            led89_u64 index, void *data_out, size_t capacity,
                            size_t *size_out);

/* truncate.c */
int led89_truncate_impl(ledger89 *l, led89_u64 from);
int led89_prune_impl(ledger89 *l, led89_u64 requested, led89_u64 *actual);

/* util.c */
int led89_handle_reserve_parts(ledger89 *l, size_t need);
int led89_handle_reserve_dir(ledger89 *l, size_t need);
int led89_buf_reserve(ledger89 *l, size_t need);
int led89_check_writable(ledger89 *l);
int led89_check_mutable(ledger89 *l);

#endif /* LEDGER89_INTERNAL_H */
