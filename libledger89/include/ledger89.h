#ifndef LEDGER89_H
#define LEDGER89_H

/*
 * ledger89.h - durable ordered sequence of opaque byte records (ISO C89).
 *
 * libledger89 owns exactly one thing:
 *
 *   a locally durable, ordered, rewindable sequence of opaque byte
 *   records, addressed by stable logical positions.
 *
 * It is not a WAL, message log, or Raft log. Those are interpretations
 * supplied by higher layers. The ledger never interprets payload bytes.
 *
 * Logical state:
 *
 *     [first, stable_end)  locally durable records
 *     [stable_end, end)    appended but not yet durable
 *
 * Indices are contiguous within [first, end). A freshly created ledger has
 * first == stable_end == end == 1; position 0 is never a record position.
 *
 * Structural operations:
 *
 *     appendv        moves end right
 *     sync           moves stable_end right
 *     truncate_from  moves end/stable_end left
 *     prune_before   moves first right
 *     rotate         seals the active part and starts a new one
 *
 * Suffix truncation increments revision because an index may subsequently
 * refer to a different record. Prefix pruning does not change revision.
 *
 * Durability vocabulary:
 *
 *     stable_end is the exact local crash-recovery frontier. It means only
 *     that the local storage contract guarantees recovery of this prefix
 *     after a successful sync. It never means consensus commit, message
 *     acknowledgement, or transaction commit.
 *
 * Threading:
 *
 *     A handle is not internally synchronized; the caller serializes calls
 *     on one handle. A writable open owns the ledger exclusively; read-only
 *     opens may coexist.
 *
 * Portability:
 *
 *     The public API is ISO C89. The implementation uses the GCC/Clang
 *     unsigned-long-long extension for internal 64-bit arithmetic and a
 *     POSIX storage backend; see the README for the exact contract.
 */

#include <limits.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LEDGER89_VERSION_MAJOR 2
#define LEDGER89_VERSION_MINOR 0
#define LEDGER89_VERSION_PATCH 0

/* Maximum record payload accepted by the append operations. */
#define LEDGER89_MAX_RECORD_BYTES 16777216u

    /* -------------------------------------------------------------------------
     * Portable 32/64-bit scalar types
     *
     * libledger89 requires an exact unsigned 32-bit C integer type. This
     * remains valid strict C89 and works on ordinary ILP32 and LP64
     * implementations.
     * -------------------------------------------------------------------------
     */

#if UINT_MAX == 4294967295U
    typedef unsigned int ledger89_u32;
#elif ULONG_MAX == 4294967295UL
typedef unsigned long ledger89_u32;
#else
#error "libledger89 requires an exact 32-bit unsigned integer type"
#endif

    typedef struct ledger89_u64
    {
        ledger89_u32 hi;
        ledger89_u32 lo;
    } ledger89_u64;

    typedef ledger89_u64 ledger89_index;
    typedef ledger89_u64 ledger89_revision;

    /*
     * Scalar helpers.
     *
     * cmp() returns <0, 0, >0 according to a < b, a == b, a > b.
     */
    int ledger89_u64_cmp(ledger89_u64 a, ledger89_u64 b);

    int ledger89_u64_equal(ledger89_u64 a, ledger89_u64 b);

    ledger89_u64 ledger89_u64_zero(void);

    ledger89_u64 ledger89_u64_from_u32(ledger89_u32 value);

    /* -------------------------------------------------------------------------
     * Errors and statuses
     * -------------------------------------------------------------------------
     */

    enum ledger89_result
    {
        LEDGER89_OK = 0,

        /*
         * Non-error status.
         *
         * Returned by ledger89_iter_next() when no record currently exists at
         * the iterator position.
         */
        LEDGER89_DONE = 1,

        LEDGER89_EINVAL = -1,
        LEDGER89_ENOMEM = -2,
        LEDGER89_EIO = -3,
        LEDGER89_ENOENT = -4,
        LEDGER89_EEXIST = -5,
        LEDGER89_EBUSY = -6,
        LEDGER89_EROFS = -7,
        /*
         * Corruption detected while performing the requested operation.
         *
         * Integrity checking is operation-specific: open validates what
         * recovery requires, read validates a payload when it is read, and
         * verify performs exhaustive validation of the ledger.
         */
        LEDGER89_ECORRUPT = -8,
        LEDGER89_EFORMAT = -9,
        LEDGER89_ESTALE = -10,
        LEDGER89_EGONE = -11,
        LEDGER89_ERANGE = -12,
        LEDGER89_EUNSTABLE = -13,
        LEDGER89_ETOOSMALL = -14,
        LEDGER89_EOVERFLOW = -15,
        LEDGER89_EPOISONED = -16
    };

    const char *ledger89_strerror(int result);

    /* -------------------------------------------------------------------------
     * Opaque ledger handle
     * -------------------------------------------------------------------------
     */

    typedef struct ledger89 ledger89;

    /* -------------------------------------------------------------------------
     * Ledger identity and state
     * -------------------------------------------------------------------------
     */

    /*
     * Persistent identity of one ledger.
     *
     * Applications should treat bytes as opaque. The identity remains
     * constant across close/open, append, truncate, and prune operations.
     */
    typedef struct ledger89_id
    {
        unsigned char bytes[16];
    } ledger89_id;

    /*
     * All boundaries are exclusive on the right.
     *
     * Records currently available:
     *
     *     first <= index < end
     *
     * Records guaranteed locally durable:
     *
     *     first <= index < stable_end
     *
     * Invariants:
     *
     *     first <= stable_end <= end
     */
    typedef struct ledger89_state
    {
        ledger89_id id;
        ledger89_revision revision;
        ledger89_index first;
        ledger89_index stable_end;
        ledger89_index end;
    } ledger89_state;

    /*
     * Obtain one coherent snapshot of ledger state.
     */
    int ledger89_get_state(ledger89 *ledger, ledger89_state *state_out);

    /* -------------------------------------------------------------------------
     * Open / close
     * -------------------------------------------------------------------------
     */

#define LEDGER89_OPEN_RDONLY 0x0001UL
#define LEDGER89_OPEN_RDWR 0x0002UL
#define LEDGER89_OPEN_CREATE 0x0004UL
#define LEDGER89_OPEN_EXCL 0x0008UL

    /*
     * Open a ledger identified by path.
     *
     * Exactly one of RDONLY and RDWR must be supplied.
     *
     * CREATE:
     *     Create the ledger if it does not exist.
     *     Valid only with RDWR.
     *
     * EXCL:
     *     With CREATE, fail with EEXIST if the ledger already exists.
     *
     * Open performs recovery before returning.
     *
     * Recovery validates the metadata, structure, and framing needed to
     * reconstruct the ledger and its durable frontier. Corruption detected
     * while doing so returns ECORRUPT.
     *
     * Open does not verify every record payload. Payload corruption not
     * needed to reconstruct the ledger may therefore remain undetected
     * until the affected record is read or ledger89_verify() is called.
     *
     * Incomplete/unstable tail data is discarded logically during recovery.
     *
     * A writable open obtains exclusive writer ownership and physically
     * discards the unstable tail. Failure to obtain ownership returns EBUSY.
     * A read-only open takes shared ownership and never mutates the ledger;
     * mutating operations return EROFS.
     *
     * On every failure path *ledger_out is NULL.
     */
    int ledger89_open(ledger89 **ledger_out, const char *path,
                      unsigned long flags);

    /*
     * Close the handle and release resources. NULL is a no-op.
     *
     * close() does NOT imply sync().
     *
     * Records in [stable_end, end) have no durability guarantee after close.
     */
    void ledger89_close(ledger89 *ledger);

    /* -------------------------------------------------------------------------
     * Append
     * -------------------------------------------------------------------------
     */

    typedef struct ledger89_slice
    {
        const void *data;
        size_t size;
    } ledger89_slice;

    /*
     * Append one or more records atomically as one append batch.
     *
     * count must be > 0.
     *
     * For each slice:
     *
     *     size > 0  => data must not be NULL
     *     size == 0 => data may be NULL
     *
     * On success:
     *
     *     *first_out = index assigned to records[0]
     *
     * and records receive consecutive indices. first_out may be NULL.
     *
     * Example:
     *
     *     old end = 42
     *     count   = 3
     *
     * creates records 42, 43, 44 and sets new end = 45.
     *
     * The records become visible immediately through this handle but are not
     * promised durable until sync() succeeds.
     *
     * An append failure does not expose a partial logical batch.
     */
    int ledger89_appendv(ledger89 *ledger, const ledger89_slice *records,
                         size_t count, ledger89_index *first_out);

    /*
     * Compare-and-append variant.
     *
     * The append occurs only if BOTH:
     *
     *     current revision == expected_revision
     *     current end      == expected_end
     *
     * Otherwise returns LEDGER89_ESTALE and performs no append.
     *
     * Comparing revision prevents the ABA case:
     *
     *     end = 100
     *     truncate
     *     append replacements
     *     end = 100 again
     *
     * expected_end alone could not detect that history changed.
     */
    int ledger89_appendv_at(ledger89 *ledger,
                            ledger89_revision expected_revision,
                            ledger89_index expected_end,
                            const ledger89_slice *records, size_t count,
                            ledger89_index *first_out);

    /*
     * Convenience operation for one record.
     */
    int ledger89_append(ledger89 *ledger, const void *data, size_t size,
                        ledger89_index *index_out);

    /* -------------------------------------------------------------------------
     * Durability
     * -------------------------------------------------------------------------
     */

    /*
     * Stabilize every successfully appended record currently visible through
     * this handle.
     *
     * On success:
     *
     *     stable_end == end
     *
     * and the prefix [first, stable_end) is guaranteed recoverable according
     * to the storage contract. Sync on a clean ledger is a no-op.
     *
     * stable_end_out may be NULL.
     *
     * A failure for which the implementation cannot determine the resulting
     * durable frontier poisons the writable handle. Subsequent mutating
     * operations return EPOISONED. Close and reopen performs recovery and
     * discovers the authoritative state.
     */
    int ledger89_sync(ledger89 *ledger, ledger89_index *stable_end_out);

    /* -------------------------------------------------------------------------
     * Random read
     * -------------------------------------------------------------------------
     */

    /*
     * Read one record.
     *
     * If index < first:
     *     returns EGONE
     *
     * If index >= end:
     *     returns ENOENT
     *
     * size_out is required and receives the complete record size.
     *
     * If data_out == NULL:
     *     no payload bytes are read or copied;
     *     returns OK after reporting the record size;
     *     the record payload checksum is not verified.
     *
     * If data_out != NULL and capacity < record size:
     *     returns ETOOSMALL;
     *     copies nothing;
     *     size_out still receives the required size;
     *     the record payload checksum is not verified.
     *
     * If data_out != NULL and capacity is sufficient:
     *     reads the complete payload and verifies its record checksum;
     *     returns ECORRUPT if the stored record framing or payload
     *     checksum is invalid.
     *
     * Successful open does not imply that every record payload has already
     * been verified.
     *
     * Reading an unstable record in [stable_end, end) is permitted. Such a
     * record may disappear after crash/reopen.
     */
    int ledger89_read(ledger89 *ledger, ledger89_index index, void *data_out,
                      size_t capacity, size_t *size_out);

    /* -------------------------------------------------------------------------
     * Sequential iteration
     * -------------------------------------------------------------------------
     */

    /*
     * Iterator state deliberately remains small and caller-owned.
     *
     * Applications must initialize it only through ledger89_iter_init() and
     * must not read or modify any field. An iterator must not outlive its
     * ledger handle.
     *
     * The trailing fields are private cursor state that lets sequential
     * iteration advance without rescanning a batch from its start.
     */
    typedef struct ledger89_iter
    {
        ledger89 *ledger;
        ledger89_index next;
        ledger89_revision revision;

        size_t cursor_entry;
        ledger89_u64 cursor_offset;
        int cursor_valid;
    } ledger89_iter;

    /*
     * Begin iteration at `from`.
     *
     * from may equal end.
     *
     * from < first returns EGONE.
     * from > end returns ERANGE.
     */
    int ledger89_iter_init(ledger89_iter *iter, ledger89 *ledger,
                           ledger89_index from);

    /*
     * Return the record at iter->next and advance the iterator.
     *
     * index_out may be NULL.
     * size_out is required.
     *
     * Payload buffer and integrity-checking rules match ledger89_read().
     *
     * In particular, if data_out == NULL, the iterator reports metadata and
     * advances without reading or verifying the record payload. The caller
     * may subsequently call ledger89_read() using index_out.
     *
     * If capacity is insufficient:
     *     returns ETOOSMALL
     *     reports required size
     *     does NOT advance
     *
     * If iter->next >= current end:
     *     returns DONE
     *     does NOT advance
     *
     * DONE is not permanent. An append may extend end; a later call may then
     * return another record.
     *
     * If suffix history changed since ledger89_iter_init():
     *     returns ESTALE
     *
     * If prefix pruning moved first beyond iter->next:
     *     returns EGONE
     *
     * Ordinary append and sync operations do not invalidate the iterator.
     */
    int ledger89_iter_next(ledger89_iter *iter, ledger89_index *index_out,
                           void *data_out, size_t capacity, size_t *size_out);

    /* -------------------------------------------------------------------------
     * Suffix truncation
     * -------------------------------------------------------------------------
     */

    /*
     * Remove every record with index >= from.
     *
     * Preconditions:
     *
     *     ledger opened RDWR
     *     end == stable_end
     *     first <= from <= end
     *
     * If end != stable_end:
     *     returns EUNSTABLE
     *
     * If from < first:
     *     returns EGONE
     *
     * If from > end:
     *     returns ERANGE
     *
     * If from == end:
     *     succeeds as a no-op;
     *     revision does not change.
     *
     * If records are removed:
     *
     *     end        = from
     *     stable_end = from
     *     revision   = revision + 1
     *
     * Successful truncation is durable before this function returns.
     *
     * Indices in the removed suffix may subsequently be reused by append.
     */
    int ledger89_truncate_from(ledger89 *ledger, ledger89_index from);

    /* -------------------------------------------------------------------------
     * Prefix pruning
     * -------------------------------------------------------------------------
     */

    /*
     * Forget old durable records before `requested_first`.
     *
     * This operation supplies the MECHANISM for retention, not retention
     * policy.
     *
     * Preconditions:
     *
     *     ledger opened RDWR
     *     requested_first <= stable_end
     *
     * If requested_first > stable_end:
     *     returns EUNSTABLE
     *
     * Implementations may prune only at safe physical boundaries such as
     * complete sealed parts. Therefore, on success:
     *
     *     old first <= actual_first <= requested_first
     *
     * and:
     *
     *     first = actual_first
     *
     * actual_first_out is required.
     *
     * Records with index < actual_first subsequently return EGONE.
     *
     * Prefix pruning does NOT change revision because no surviving index
     * changes meaning.
     *
     * Successful pruning is durable before this function returns.
     */
    int ledger89_prune_before(ledger89 *ledger, ledger89_index requested_first,
                              ledger89_index *actual_first_out);

    /* -------------------------------------------------------------------------
     * Rotation and verification (administrative)
     * -------------------------------------------------------------------------
     */

    /*
     * Seal the active part and start a new one. Rotating an active part that
     * holds no records is a no-op. Rotation syncs first when the active part
     * holds unstable records. Self-durable on success.
     */
    int ledger89_rotate(ledger89 *ledger);

    /*
     * Perform an exhaustive integrity check of the current ledger.
     *
     * Recompute every checksum in the ledger: sealed part digests, batch
     * checksums, record payload checksums, and the recovered active prefix.
     *
     * This is stronger than the validation performed by ledger89_open().
     * Successful open does not imply that ledger89_verify() will succeed.
     *
     * Returns LEDGER89_OK when every checksum matches, LEDGER89_ECORRUPT
     * when any does not, or a negative I/O error.
     */
    int ledger89_verify(ledger89 *ledger);

#ifdef __cplusplus
}
#endif

#endif /* LEDGER89_H */
