"""ctypes bindings for the libledger89 public API.

The shared object is built by `just build-shared` at
build/shared/libledger89.so; LED89_LIB overrides the path.

Structs are bound exactly as the ABI passes them: ledger89_u64 is a pair of
u32 fields and is passed and returned by value; all handles are opaque
pointers. Every wrapper returns raw results so tables can assert on them.
"""

import ctypes
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_LIB = os.path.join(ROOT, "build", "shared", "libledger89.so")
LIB_PATH = os.environ.get("LED89_LIB", DEFAULT_LIB)

OK = 0
DONE = 1
EINVAL = -1
ENOMEM = -2
EIO = -3
ENOENT = -4
EEXIST = -5
EBUSY = -6
EROFS = -7
ECORRUPT = -8
EFORMAT = -9
ESTALE = -10
EGONE = -11
ERANGE = -12
EUNSTABLE = -13
ETOOSMALL = -14
EOVERFLOW = -15
EPOISONED = -16

OPEN_RDONLY = 0x0001
OPEN_RDWR = 0x0002
OPEN_CREATE = 0x0004
OPEN_EXCL = 0x0008

MAX_RECORD_BYTES = 16777216
UINT32_MAX = 0xFFFFFFFF
UINT64_MAX = 0xFFFFFFFFFFFFFFFF

RESULT_NAMES = {
    OK: "OK",
    DONE: "DONE",
    EINVAL: "EINVAL",
    ENOMEM: "ENOMEM",
    EIO: "EIO",
    ENOENT: "ENOENT",
    EEXIST: "EEXIST",
    EBUSY: "EBUSY",
    EROFS: "EROFS",
    ECORRUPT: "ECORRUPT",
    EFORMAT: "EFORMAT",
    ESTALE: "ESTALE",
    EGONE: "EGONE",
    ERANGE: "ERANGE",
    EUNSTABLE: "EUNSTABLE",
    ETOOSMALL: "ETOOSMALL",
    EOVERFLOW: "EOVERFLOW",
    EPOISONED: "EPOISONED",
}


class U64(ctypes.Structure):
    _fields_ = [("hi", ctypes.c_uint32), ("lo", ctypes.c_uint32)]

    def value(self):
        return (self.hi << 32) | self.lo

    def __repr__(self):
        return "U64(%d)" % self.value()


class Id(ctypes.Structure):
    _fields_ = [("bytes", ctypes.c_ubyte * 16)]


class State(ctypes.Structure):
    _fields_ = [
        ("id", Id),
        ("revision", U64),
        ("first", U64),
        ("stable_end", U64),
        ("end", U64),
    ]


class Slice(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("size", ctypes.c_size_t)]


class Iter(ctypes.Structure):
    _fields_ = [
        ("ledger", ctypes.c_void_p),
        ("next", U64),
        ("revision", U64),
        ("cursor_entry", ctypes.c_size_t),
        ("cursor_offset", U64),
        ("cursor_valid", ctypes.c_int),
    ]


def u64(value):
    if isinstance(value, U64):
        return value
    value &= UINT64_MAX
    return U64(hi=(value >> 32) & UINT32_MAX, lo=value & UINT32_MAX)


def u64_value(value):
    return value.value()


_lib = ctypes.CDLL(LIB_PATH)

_lib.ledger89_open.argtypes = [
    ctypes.POINTER(ctypes.c_void_p),
    ctypes.c_char_p,
    ctypes.c_ulong,
]
_lib.ledger89_open.restype = ctypes.c_int
_lib.ledger89_close.argtypes = [ctypes.c_void_p]
_lib.ledger89_close.restype = None
_lib.ledger89_get_state.argtypes = [ctypes.c_void_p, ctypes.POINTER(State)]
_lib.ledger89_get_state.restype = ctypes.c_int
_lib.ledger89_appendv.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(Slice),
    ctypes.c_size_t,
    ctypes.POINTER(U64),
]
_lib.ledger89_appendv.restype = ctypes.c_int
_lib.ledger89_appendv_at.argtypes = [
    ctypes.c_void_p,
    U64,
    U64,
    ctypes.POINTER(Slice),
    ctypes.c_size_t,
    ctypes.POINTER(U64),
]
_lib.ledger89_appendv_at.restype = ctypes.c_int
_lib.ledger89_append.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(U64),
]
_lib.ledger89_append.restype = ctypes.c_int
_lib.ledger89_sync.argtypes = [ctypes.c_void_p, ctypes.POINTER(U64)]
_lib.ledger89_sync.restype = ctypes.c_int
_lib.ledger89_read.argtypes = [
    ctypes.c_void_p,
    U64,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
]
_lib.ledger89_read.restype = ctypes.c_int
_lib.ledger89_iter_init.argtypes = [ctypes.POINTER(Iter), ctypes.c_void_p, U64]
_lib.ledger89_iter_init.restype = ctypes.c_int
_lib.ledger89_iter_next.argtypes = [
    ctypes.POINTER(Iter),
    ctypes.POINTER(U64),
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
]
_lib.ledger89_iter_next.restype = ctypes.c_int
_lib.ledger89_truncate_from.argtypes = [ctypes.c_void_p, U64]
_lib.ledger89_truncate_from.restype = ctypes.c_int
_lib.ledger89_prune_before.argtypes = [
    ctypes.c_void_p,
    U64,
    ctypes.POINTER(U64),
]
_lib.ledger89_prune_before.restype = ctypes.c_int
_lib.ledger89_rotate.argtypes = [ctypes.c_void_p]
_lib.ledger89_rotate.restype = ctypes.c_int
_lib.ledger89_verify.argtypes = [ctypes.c_void_p]
_lib.ledger89_verify.restype = ctypes.c_int
_lib.ledger89_strerror.argtypes = [ctypes.c_int]
_lib.ledger89_strerror.restype = ctypes.c_char_p
_lib.ledger89_u64_cmp.argtypes = [U64, U64]
_lib.ledger89_u64_cmp.restype = ctypes.c_int
_lib.ledger89_u64_equal.argtypes = [U64, U64]
_lib.ledger89_u64_equal.restype = ctypes.c_int
_lib.ledger89_u64_zero.argtypes = []
_lib.ledger89_u64_zero.restype = U64
_lib.ledger89_u64_from_u32.argtypes = [ctypes.c_uint32]
_lib.ledger89_u64_from_u32.restype = U64


def open_ledger(path, flags):
    """Open a ledger and return (rc, handle-or-None)."""
    if isinstance(path, str):
        path = path.encode()
    out = ctypes.c_void_p(0xDEADBEEF)
    rc = _lib.ledger89_open(ctypes.byref(out), path, flags)
    if rc != OK:
        return rc, None
    return rc, out.value


def close(handle):
    _lib.ledger89_close(handle)


def get_state(handle):
    state = State()
    rc = _lib.ledger89_get_state(handle, ctypes.byref(state))
    if rc != OK:
        return rc, None
    return rc, state


def make_slices(payloads):
    """Build a Slice array plus the payload buffers that keep it alive."""
    buffers = []
    array = (Slice * max(len(payloads), 1))()
    for i, payload in enumerate(payloads):
        if payload is None:
            array[i].data = None
            array[i].size = 0
        elif isinstance(payload, int):
            array[i].data = None
            array[i].size = payload
        else:
            buf = ctypes.create_string_buffer(bytes(payload), len(payload))
            buffers.append(buf)
            array[i].data = ctypes.cast(buf, ctypes.c_void_p)
            array[i].size = len(payload)
    return array, buffers


def appendv(handle, payloads, null_array=False):
    return appendv_raw(handle, payloads, len(payloads), null_array)


def appendv_raw(handle, payloads, count, null_array=False):
    """Append payloads with an explicit count (for boundary rows)."""
    first = U64()
    if null_array:
        rc = _lib.ledger89_appendv(handle, None, count, ctypes.byref(first))
    else:
        array, buffers = make_slices(payloads)
        rc = _lib.ledger89_appendv(handle, array, count, ctypes.byref(first))
        del buffers
    if rc != OK:
        return rc, None
    return rc, first.value()


def appendv_at(handle, revision, end, payloads):
    first = U64()
    array, buffers = make_slices(payloads)
    rc = _lib.ledger89_appendv_at(
        handle, u64(revision), u64(end), array, len(payloads), ctypes.byref(first)
    )
    del buffers
    if rc != OK:
        return rc, None
    return rc, first.value()


def append(handle, payload):
    index = U64()
    if payload is None:
        rc = _lib.ledger89_append(handle, None, 0, ctypes.byref(index))
    else:
        buf = ctypes.create_string_buffer(bytes(payload), len(payload))
        rc = _lib.ledger89_append(
            handle, ctypes.cast(buf, ctypes.c_void_p), len(payload),
            ctypes.byref(index),
        )
    if rc != OK:
        return rc, None
    return rc, index.value()


def sync(handle):
    stable = U64()
    rc = _lib.ledger89_sync(handle, ctypes.byref(stable))
    if rc != OK:
        return rc, None
    return rc, stable.value()


def read(handle, index, capacity, null_buffer=False):
    """Return (rc, size-or-None, bytes-or-None)."""
    size = ctypes.c_size_t(0)
    if null_buffer:
        rc = _lib.ledger89_read(
            handle, u64(index), None, capacity, ctypes.byref(size)
        )
        return rc, size.value, None
    buf = ctypes.create_string_buffer(max(capacity, 1))
    rc = _lib.ledger89_read(
        handle, u64(index), ctypes.cast(buf, ctypes.c_void_p), capacity,
        ctypes.byref(size),
    )
    if rc != OK:
        return rc, size.value, None
    return rc, size.value, bytes(buf.raw[: size.value])


def read_into(handle, index, capacity, fill=0x5A):
    """Read into a pre-filled sentinel buffer; returns (rc, size, raw)."""
    buf = ctypes.create_string_buffer(max(capacity, 1))
    ctypes.memset(buf, fill, max(capacity, 1))
    size = ctypes.c_size_t(0)
    rc = _lib.ledger89_read(
        handle, u64(index), ctypes.cast(buf, ctypes.c_void_p), capacity,
        ctypes.byref(size),
    )
    return rc, size.value, bytes(buf.raw[: max(capacity, 1)])


def iter_init(handle, from_index):
    it = Iter()
    rc = _lib.ledger89_iter_init(ctypes.byref(it), handle, u64(from_index))
    return rc, it


def iter_next(it, capacity, null_buffer=False):
    index = U64()
    size = ctypes.c_size_t(0)
    if null_buffer:
        rc = _lib.ledger89_iter_next(
            ctypes.byref(it), ctypes.byref(index), None, capacity,
            ctypes.byref(size),
        )
        return rc, index.value(), size.value, None
    buf = ctypes.create_string_buffer(max(capacity, 1))
    rc = _lib.ledger89_iter_next(
        ctypes.byref(it), ctypes.byref(index), ctypes.cast(buf, ctypes.c_void_p),
        capacity, ctypes.byref(size),
    )
    if rc != OK:
        return rc, index.value(), size.value, None
    return rc, index.value(), size.value, bytes(buf.raw[: size.value])


def truncate_from(handle, from_index):
    return _lib.ledger89_truncate_from(handle, u64(from_index))


def prune_before(handle, requested):
    actual = U64()
    rc = _lib.ledger89_prune_before(handle, u64(requested), ctypes.byref(actual))
    if rc != OK:
        return rc, None
    return rc, actual.value()


def rotate(handle):
    return _lib.ledger89_rotate(handle)


def verify(handle):
    return _lib.ledger89_verify(handle)


def strerror(rc):
    text = _lib.ledger89_strerror(rc)
    if text is None:
        return None
    return text.decode("utf-8", "replace")


def u64_cmp(a, b):
    return _lib.ledger89_u64_cmp(u64(a), u64(b))


def u64_equal(a, b):
    return _lib.ledger89_u64_equal(u64(a), u64(b))


def u64_zero():
    return _lib.ledger89_u64_zero().value()


def u64_from_u32(value):
    return _lib.ledger89_u64_from_u32(value).value()
