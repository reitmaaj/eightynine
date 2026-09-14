/* faultshim.c - LD_PRELOAD I/O fault and crash injector for the strict
 * suite. Test-only. Interposes the file primitives libledger89 uses and
 * fails, shortens, EINTRs, or kills exactly the configured call.
 *
 * Environment (read on every intercepted call):
 *   LED89_FAULT_OPS    comma list of op names (open, close, pread, pwrite,
 *                      fstat, ftruncate, fsync, fdatasync, rename, unlink,
 *                      mkdir, flock, opendir, readdir, closedir, read)
 *   LED89_FAULT_NTH    fire on the Nth matching call (0 = every call)
 *   LED89_FAULT_ERR    errno name or number (default EIO)
 *   LED89_FAULT_MODE   fail | short | eintr | kill
 *   LED89_FAULT_ROOT   path prefix; only matching paths/fds are intercepted
 *   LED89_FAULT_MARK   file touched before a kill
 *   LED89_FAULT_COUNT  file that receives one line per fired op
 *   LED89_FAULT_ENTROPY  when set, "read" faults apply to /dev/urandom
 */

#define _GNU_SOURCE

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FD_MAX 1024
#define DIR_MAX 256
#define PATH_CAP 4096

enum
{
    OP_OPEN = 0,
    OP_CLOSE,
    OP_PREAD,
    OP_PWRITE,
    OP_FSTAT,
    OP_FTRUNCATE,
    OP_FSYNC,
    OP_FDATASYNC,
    OP_RENAME,
    OP_UNLINK,
    OP_MKDIR,
    OP_FLOCK,
    OP_OPENDIR,
    OP_READDIR,
    OP_CLOSEDIR,
    OP_READ,
    OP_COUNT
};

static const char *g_op_names[OP_COUNT] = {
    "open",    "close",     "pread",    "pwrite", "fstat", "ftruncate",
    "fsync",   "fdatasync", "rename",   "unlink", "mkdir", "flock",
    "opendir", "readdir",   "closedir", "read"};

static unsigned long g_counts[OP_COUNT];
static int g_injecting;
static int g_entropy_fd = -1;
static int g_fd_used[FD_MAX];
static char g_fd_paths[FD_MAX][PATH_CAP];
static int g_dir_used[DIR_MAX];
static DIR *g_dirs[DIR_MAX];

static int (*g_real_open)(const char *, int, ...);
static int (*g_real_close)(int);
static ssize_t (*g_real_pread)(int, void *, size_t, off_t);
static ssize_t (*g_real_pwrite)(int, const void *, size_t, off_t);
static int (*g_real_fstat)(int, struct stat *);
static int (*g_real_ftruncate)(int, off_t);
static int (*g_real_fsync)(int);
static int (*g_real_fdatasync)(int);
static int (*g_real_rename)(const char *, const char *);
static int (*g_real_unlink)(const char *);
static int (*g_real_mkdir)(const char *, mode_t);
static int (*g_real_flock)(int, int);
static DIR *(*g_real_opendir)(const char *);
static struct dirent *(*g_real_readdir)(DIR *);
static int (*g_real_closedir)(DIR *);
static ssize_t (*g_real_read)(int, void *, size_t);

static void init_real(void)
{
    static int ready;

    if (ready)
    {
        return;
    }
    *(void **)(&g_real_open) = dlsym(RTLD_NEXT, "open");
    *(void **)(&g_real_close) = dlsym(RTLD_NEXT, "close");
    *(void **)(&g_real_pread) = dlsym(RTLD_NEXT, "pread");
    *(void **)(&g_real_pwrite) = dlsym(RTLD_NEXT, "pwrite");
    *(void **)(&g_real_fstat) = dlsym(RTLD_NEXT, "fstat");
    *(void **)(&g_real_ftruncate) = dlsym(RTLD_NEXT, "ftruncate");
    *(void **)(&g_real_fsync) = dlsym(RTLD_NEXT, "fsync");
    *(void **)(&g_real_fdatasync) = dlsym(RTLD_NEXT, "fdatasync");
    *(void **)(&g_real_rename) = dlsym(RTLD_NEXT, "rename");
    *(void **)(&g_real_unlink) = dlsym(RTLD_NEXT, "unlink");
    *(void **)(&g_real_mkdir) = dlsym(RTLD_NEXT, "mkdir");
    *(void **)(&g_real_flock) = dlsym(RTLD_NEXT, "flock");
    *(void **)(&g_real_opendir) = dlsym(RTLD_NEXT, "opendir");
    *(void **)(&g_real_readdir) = dlsym(RTLD_NEXT, "readdir");
    *(void **)(&g_real_closedir) = dlsym(RTLD_NEXT, "closedir");
    *(void **)(&g_real_read) = dlsym(RTLD_NEXT, "read");
    ready = 1;
}

static int list_has(const char *list, const char *name)
{
    size_t want;

    want = strlen(name);
    while (*list != '\0')
    {
        const char *end;

        end = strchr(list, ',');
        if (end == NULL)
        {
            end = list + strlen(list);
        }
        if ((size_t)(end - list) == want && strncmp(list, name, want) == 0)
        {
            return 1;
        }
        if (*end == '\0')
        {
            break;
        }
        list = end + 1;
    }
    return 0;
}

static int under_root(const char *path)
{
    const char *root;

    root = getenv("LED89_FAULT_ROOT");
    if (root == NULL || root[0] == '\0')
    {
        return 1;
    }
    return strncmp(path, root, strlen(root)) == 0;
}

static unsigned long env_nth(void)
{
    const char *text;
    char *end;
    unsigned long value;

    text = getenv("LED89_FAULT_NTH");
    if (text == NULL || text[0] == '\0')
    {
        return 1ul;
    }
    end = NULL;
    value = strtoul(text, &end, 0);
    if (end == text)
    {
        return 1ul;
    }
    return value;
}

static int env_errno(void)
{
    const char *text;
    char *end;
    unsigned long value;

    text = getenv("LED89_FAULT_ERR");
    if (text == NULL || text[0] == '\0')
    {
        return EIO;
    }
    if (strcmp(text, "EIO") == 0)
    {
        return EIO;
    }
    if (strcmp(text, "ENOSPC") == 0)
    {
        return ENOSPC;
    }
    if (strcmp(text, "EDQUOT") == 0)
    {
        return EDQUOT;
    }
    if (strcmp(text, "EFBIG") == 0)
    {
        return EFBIG;
    }
    if (strcmp(text, "EINTR") == 0)
    {
        return EINTR;
    }
    if (strcmp(text, "EACCES") == 0)
    {
        return EACCES;
    }
    if (strcmp(text, "EROFS") == 0)
    {
        return EROFS;
    }
    end = NULL;
    value = strtoul(text, &end, 0);
    if (end == text)
    {
        return EIO;
    }
    return (int)value;
}

static int env_mode(void)
{
    const char *text;

    text = getenv("LED89_FAULT_MODE");
    if (text == NULL)
    {
        return 1;
    }
    if (strcmp(text, "fail") == 0)
    {
        return 1;
    }
    if (strcmp(text, "short") == 0)
    {
        return 2;
    }
    if (strcmp(text, "eintr") == 0)
    {
        return 3;
    }
    if (strcmp(text, "kill") == 0)
    {
        return 4;
    }
    return 1;
}

static void record_fire(const char *op)
{
    const char *path;
    FILE *file;

    path = getenv("LED89_FAULT_COUNT");
    if (path == NULL || path[0] == '\0')
    {
        return;
    }
    file = fopen(path, "ab");
    if (file == NULL)
    {
        return;
    }
    fprintf(file, "%s\n", op);
    fclose(file);
}

static void touch_mark(void)
{
    const char *path;
    FILE *file;

    path = getenv("LED89_FAULT_MARK");
    if (path == NULL || path[0] == '\0')
    {
        return;
    }
    file = fopen(path, "ab");
    if (file != NULL)
    {
        fputs("kill\n", file);
        fclose(file);
    }
}

/* Returns 1 fail, 2 short, 3 eintr, 4 kill, 0 no fire. */
static int maybe_fire(int op, const char *path)
{
    const char *ops;
    unsigned long nth;
    int mode;

    if (g_injecting)
    {
        return 0;
    }
    if (path != NULL && !under_root(path))
    {
        return 0;
    }
    ops = getenv("LED89_FAULT_OPS");
    if (ops == NULL || !list_has(ops, g_op_names[op]))
    {
        return 0;
    }
    ++g_counts[op];
    nth = env_nth();
    if (nth != 0ul && g_counts[op] != nth)
    {
        return 0;
    }
    g_injecting = 1;
    record_fire(g_op_names[op]);
    mode = env_mode();
    if (mode == 4)
    {
        touch_mark();
        fflush(NULL);
        _exit(137);
    }
    g_injecting = 0;
    return mode;
}

static int apply_fail(int mode)
{
    if (mode == 3)
    {
        errno = EINTR;
    }
    else
    {
        errno = env_errno();
    }
    return -1;
}

static int fd_slot(int fd)
{
    if (fd < 0 || fd >= FD_MAX)
    {
        return -1;
    }
    return fd;
}

static void track_fd(int fd, const char *path)
{
    int slot;

    slot = fd_slot(fd);
    if (slot < 0 || !under_root(path))
    {
        return;
    }
    g_fd_used[slot] = 1;
    strncpy(g_fd_paths[slot], path, PATH_CAP - 1u);
    g_fd_paths[slot][PATH_CAP - 1u] = '\0';
}

static void untrack_fd(int fd)
{
    int slot;

    slot = fd_slot(fd);
    if (slot < 0)
    {
        return;
    }
    g_fd_used[slot] = 0;
    g_fd_paths[slot][0] = '\0';
}

static int tracked_fd(int fd)
{
    int slot;

    slot = fd_slot(fd);
    if (slot < 0)
    {
        return 0;
    }
    return g_fd_used[slot];
}

static void track_dir(DIR *dirp, const char *path)
{
    size_t i;

    if (!under_root(path))
    {
        return;
    }
    for (i = 0u; i < DIR_MAX; ++i)
    {
        if (g_dir_used[i] == 0)
        {
            g_dir_used[i] = 1;
            g_dirs[i] = dirp;
            return;
        }
    }
}

static void untrack_dir(DIR *dirp)
{
    size_t i;

    for (i = 0u; i < DIR_MAX; ++i)
    {
        if (g_dir_used[i] != 0 && g_dirs[i] == dirp)
        {
            g_dir_used[i] = 0;
            g_dirs[i] = NULL;
            return;
        }
    }
}

static int tracked_dir(DIR *dirp)
{
    size_t i;

    for (i = 0u; i < DIR_MAX; ++i)
    {
        if (g_dir_used[i] != 0 && g_dirs[i] == dirp)
        {
            return 1;
        }
    }
    return 0;
}

int open(const char *path, int flags, ...)
{
    va_list ap;
    mode_t mode;
    int fire;
    int rc;

    init_real();
    mode = 0;
    if ((flags & O_CREAT) != 0)
    {
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, mode_t);
        va_end(ap);
    }
    fire = maybe_fire(OP_OPEN, path);
    if (fire != 0 && fire != 2)
    {
        return apply_fail(fire);
    }
    rc = g_real_open(path, flags, mode);
    if (rc >= 0)
    {
        if (strcmp(path, "/dev/urandom") == 0)
        {
            g_entropy_fd = rc;
        }
        else
        {
            track_fd(rc, path);
        }
    }
    return rc;
}

int close(int fd)
{
    int fire;
    int rc;

    init_real();
    if (fd_slot(fd) >= 0 && tracked_fd(fd))
    {
        fire = maybe_fire(OP_CLOSE, NULL);
        if (fire != 0 && fire != 2)
        {
            return apply_fail(fire);
        }
    }
    if (fd == g_entropy_fd)
    {
        g_entropy_fd = -1;
    }
    rc = g_real_close(fd);
    untrack_fd(fd);
    return rc;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
    int fire;
    ssize_t rc;

    init_real();
    if (fd_slot(fd) >= 0 && tracked_fd(fd))
    {
        fire = maybe_fire(OP_PREAD, NULL);
        if (fire != 0 && fire != 2)
        {
            return (ssize_t)apply_fail(fire);
        }
    }
    rc = g_real_pread(fd, buf, count, offset);
    return rc;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    int fire;
    ssize_t rc;

    init_real();
    if (fd_slot(fd) >= 0 && tracked_fd(fd))
    {
        fire = maybe_fire(OP_PWRITE, NULL);
        if (fire == 2)
        {
            size_t half;

            half = count / 2u;
            if (half > 0u)
            {
                (void)g_real_pwrite(fd, buf, half, offset);
            }
            errno = EIO;
            return -1;
        }
        if (fire != 0)
        {
            return (ssize_t)apply_fail(fire);
        }
    }
    rc = g_real_pwrite(fd, buf, count, offset);
    return rc;
}

int fstat(int fd, struct stat *buf)
{
    int fire;

    init_real();
    if (fd_slot(fd) >= 0 && tracked_fd(fd))
    {
        fire = maybe_fire(OP_FSTAT, NULL);
        if (fire != 0 && fire != 2)
        {
            return apply_fail(fire);
        }
    }
    return g_real_fstat(fd, buf);
}

int ftruncate(int fd, off_t length)
{
    int fire;

    init_real();
    if (fd_slot(fd) >= 0 && tracked_fd(fd))
    {
        fire = maybe_fire(OP_FTRUNCATE, NULL);
        if (fire != 0 && fire != 2)
        {
            return apply_fail(fire);
        }
    }
    return g_real_ftruncate(fd, length);
}

int fsync(int fd)
{
    int fire;

    init_real();
    if (fd_slot(fd) >= 0 && tracked_fd(fd))
    {
        fire = maybe_fire(OP_FSYNC, NULL);
        if (fire != 0 && fire != 2)
        {
            return apply_fail(fire);
        }
    }
    return g_real_fsync(fd);
}

int fdatasync(int fd)
{
    int fire;

    init_real();
    if (fd_slot(fd) >= 0 && tracked_fd(fd))
    {
        fire = maybe_fire(OP_FDATASYNC, NULL);
        if (fire != 0 && fire != 2)
        {
            return apply_fail(fire);
        }
    }
    return g_real_fdatasync(fd);
}

int rename(const char *from, const char *to)
{
    int fire;

    init_real();
    fire = maybe_fire(OP_RENAME, to);
    if (fire != 0 && fire != 2)
    {
        return apply_fail(fire);
    }
    return g_real_rename(from, to);
}

int unlink(const char *path)
{
    int fire;

    init_real();
    fire = maybe_fire(OP_UNLINK, path);
    if (fire != 0 && fire != 2)
    {
        return apply_fail(fire);
    }
    return g_real_unlink(path);
}

int mkdir(const char *path, mode_t mode)
{
    int fire;

    init_real();
    fire = maybe_fire(OP_MKDIR, path);
    if (fire != 0 && fire != 2)
    {
        return apply_fail(fire);
    }
    return g_real_mkdir(path, mode);
}

int flock(int fd, int operation)
{
    int fire;

    init_real();
    if (fd_slot(fd) >= 0 && tracked_fd(fd))
    {
        fire = maybe_fire(OP_FLOCK, NULL);
        if (fire != 0 && fire != 2)
        {
            return apply_fail(fire);
        }
    }
    return g_real_flock(fd, operation);
}

DIR *opendir(const char *path)
{
    DIR *result;
    int fire;

    init_real();
    fire = maybe_fire(OP_OPENDIR, path);
    if (fire != 0 && fire != 2)
    {
        (void)apply_fail(fire);
        return NULL;
    }
    result = g_real_opendir(path);
    if (result != NULL)
    {
        track_dir(result, path);
    }
    return result;
}

struct dirent *readdir(DIR *dirp)
{
    int fire;

    init_real();
    if (tracked_dir(dirp))
    {
        fire = maybe_fire(OP_READDIR, NULL);
        if (fire != 0 && fire != 2)
        {
            (void)apply_fail(fire);
            return NULL;
        }
    }
    return g_real_readdir(dirp);
}

int closedir(DIR *dirp)
{
    int fire;

    init_real();
    if (tracked_dir(dirp))
    {
        fire = maybe_fire(OP_CLOSEDIR, NULL);
        if (fire != 0 && fire != 2)
        {
            return apply_fail(fire);
        }
    }
    untrack_dir(dirp);
    return g_real_closedir(dirp);
}

ssize_t read(int fd, void *buf, size_t count)
{
    int fire;

    init_real();
    if (fd == g_entropy_fd && getenv("LED89_FAULT_ENTROPY") != NULL)
    {
        fire = maybe_fire(OP_READ, NULL);
        if (fire != 0 && fire != 2)
        {
            return (ssize_t)apply_fail(fire);
        }
    }
    return g_real_read(fd, buf, count);
}
