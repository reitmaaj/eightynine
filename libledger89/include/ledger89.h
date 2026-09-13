#ifndef LEDGER89_H
#define LEDGER89_H

/*
 * ledger89.h - durable, append-only segmented record sequence (ISO C89).
 *
 * libledger89 owns exactly one thing: a local, durable, append-only
 * sequence of opaque records stored in immutable rotated segments, with
 * crash recovery and deterministic iteration. It knows nothing about Raft
 * terms, leaders, agents, SQL, indexes, queries, capabilities, or
 * application semantics, and it never interprets record payloads.
 *
 * Model:
 *
 *   - indices are caller-supplied unsigned longs starting at 1;
 *   - an append batch is atomic: all records or none;
 *   - ledger89_sync() is the crash-durability barrier for appends;
 *   - sealed segments are immutable; only active.seg changes;
 *   - truncate_after removes a suffix; discard_before removes a prefix;
 *   - iteration is always ascending by ledger index.
 *
 * Durability contract (normative):
 *
 *   After a successful ledger89_sync(), every preceding successful append
 *   remains recoverable after process crash, OS crash, or power loss,
 *   subject to the filesystem honoring the required durability primitives.
 *
 * Threading:
 *
 *   A handle is not internally synchronized; the caller serializes calls
 *   on one handle. Independent handles on independent ledgers may be used
 *   concurrently.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LEDGER89_VERSION_MAJOR 1
#define LEDGER89_VERSION_MINOR 0
#define LEDGER89_VERSION_PATCH 0

/* Maximum record payload accepted by ledger89_append. */
#define LEDGER89_MAX_RECORD_BYTES 16777216u

/* Default segment byte target used when max_segment_bytes is zero. */
#define LEDGER89_DEFAULT_SEGMENT_BYTES 67108864ul

    /* Record index. Index 0 is never a record index. */
    typedef unsigned long ledger89_index;

    /* Opaque ledger handle and iterator. Representations are private. */
    typedef struct ledger89 ledger89;
    typedef struct ledger89_iter ledger89_iter;

    /*
     * Result values. Positive values are states, negative values are
     * errors, matching the sibling 89-series convention.
     */
    enum ledger89_status
    {
        LEDGER89_OK = 0,

        /* Iteration finished normally. */
        LEDGER89_END = 1,

        /* Invalid argument or logical misuse. */
        LEDGER89_ERR_ARG = -1,

        /* I/O failure. */
        LEDGER89_ERR_IO = -2,

        /* Allocation failure. */
        LEDGER89_ERR_NOMEM = -3,

        /* Index outside the visible range. */
        LEDGER89_ERR_NOTFOUND = -4,

        /* Value not representable or outside structural limits. */
        LEDGER89_ERR_RANGE = -5,

        /* Non-contiguous append index. */
        LEDGER89_ERR_SEQUENCE = -6,

        /* Structural corruption or checksum failure. */
        LEDGER89_ERR_CORRUPT = -7,

        /* Another handle holds the ledger lock. */
        LEDGER89_ERR_BUSY = -8,

        /* Handle poisoned by an earlier I/O failure; reopen to recover. */
        LEDGER89_ERR_FAULTED = -9,

        /* Lifecycle or staleness violation. */
        LEDGER89_ERR_STATE = -10
    };

    /*
     * A record supplied to ledger89_append. data points to exactly size
     * bytes and may be NULL only when size is zero. The caller owns the
     * bytes, which need remain valid only for the duration of the call.
     */
    typedef struct ledger89_record
    {
        ledger89_index index;
        unsigned long tag;
        const void *data;
        size_t size;
    } ledger89_record;

    /*
     * A record returned by ledger89_read or ledger89_iter_next. data
     * points to exactly size bytes. The view remains valid until the next
     * call on the same handle (read) or the same iterator (iteration).
     */
    typedef struct ledger89_view
    {
        ledger89_index index;
        unsigned long tag;
        const void *data;
        size_t size;
    } ledger89_view;

    /*
     * Configuration.
     *
     * path names the ledger directory and is created when missing.
     * max_segment_bytes and max_segment_records are rotation targets
     * evaluated before an append. Zero means the default byte target or
     * unlimited records respectively. The structure is read only during
     * ledger89_open.
     */
    typedef struct ledger89_config
    {
        const char *path;
        unsigned long max_segment_bytes;
        unsigned long max_segment_records;
    } ledger89_config;

    /*
     * Advisory observer called once per record of a successful append, in
     * ascending index order, after logical visibility and before sync.
     * The view is valid only for the duration of the callback, which must
     * not call back into the ledger. Observers are never called for
     * rejected appends or during recovery, and are advisory only: a
     * consumer must be able to rebuild from iteration alone.
     */
    typedef void (*ledger89_observer_fn)(void *ctx, const ledger89_view *view);

    /*
     * Open or create a ledger. On success *out receives a handle. On every
     * failure *out is NULL. A second open of the same path returns
     * LEDGER89_ERR_BUSY. ledger89_open(NULL) and a NULL config or path
     * return LEDGER89_ERR_ARG.
     */
    int ledger89_open(ledger89 **out, const ledger89_config *config);

    /* Close a handle. NULL is a no-op. Live iterators must be closed first. */
    void ledger89_close(ledger89 *l);

    /*
     * Append one atomic batch. The first record index must equal
     * ledger89_last_index() + 1 and the rest must be consecutive. On
     * success all records become logically visible; durability requires a
     * later successful ledger89_sync. Logical rejection leaves the ledger
     * unchanged. An I/O failure poisons the handle.
     */
    int ledger89_append(ledger89 *l, const ledger89_record *records,
                        size_t count);

    /* Make every preceding successful append crash-durable. */
    int ledger89_sync(ledger89 *l);

    /*
     * Seal the active segment and start a new one. Rotating an empty
     * active segment is a no-op. Self-durable on success.
     */
    int ledger89_rotate(ledger89 *l);

    /*
     * Keep exactly the records with index <= index and remove every record
     * with index > index. index >= last_index is a no-op; index ==
     * first_index - 1 empties the ledger; smaller values return
     * LEDGER89_ERR_RANGE. Self-durable on success.
     */
    int ledger89_truncate_after(ledger89 *l, ledger89_index index);

    /*
     * Keep exactly the records with index >= index and remove every record
     * with index < index. index <= first_index is a no-op; values above
     * last_index clamp to last_index + 1. Self-durable on success.
     */
    int ledger89_discard_before(ledger89 *l, ledger89_index index);

    /*
     * Install or clear (fn == NULL) the advisory observer. Returns
     * LEDGER89_ERR_ARG for a NULL handle.
     */
    int ledger89_set_observer(ledger89 *l, ledger89_observer_fn fn, void *ctx);

    /* Smallest present record index; the base index when the ledger is empty.
     */
    ledger89_index ledger89_first_index(const ledger89 *l);

    /* Largest present record index; first_index - 1 when the ledger is empty.
     */
    ledger89_index ledger89_last_index(const ledger89 *l);

    /*
     * Read one record by index. Returns LEDGER89_ERR_NOTFOUND when the
     * index is outside [first_index, last_index]. The view is valid until
     * the next call on the same handle.
     */
    int ledger89_read(ledger89 *l, ledger89_index index, ledger89_view *out);

    /*
     * Open an ascending iterator over the inclusive range. first == 0
     * means from the first record; last == 0 means to the last record;
     * out-of-range bounds clamp; an explicit reversed range returns
     * LEDGER89_ERR_ARG. On success *out receives a handle that must be
     * released with ledger89_iter_close.
     */
    int ledger89_iter_open(ledger89 *l, ledger89_index first,
                           ledger89_index last, ledger89_iter **out);

    /*
     * Advance the iterator. Returns LEDGER89_OK with *out filled,
     * LEDGER89_END when finished, LEDGER89_ERR_STATE when the ledger was
     * structurally mutated after the iterator was opened, or a negative
     * error. The view is valid until the next iterator call.
     */
    int ledger89_iter_next(ledger89_iter *it, ledger89_view *out);

    /* Release an iterator. NULL is a no-op. */
    void ledger89_iter_close(ledger89_iter *it);

    /* Static human-readable name of a status value. */
    const char *ledger89_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif /* LEDGER89_H */
