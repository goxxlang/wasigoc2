/* goclibc — file layer for the wasigocvm module.
 *
 * WASMWin32, WASMNix, and WASMDroid call the C library (fopen, fwrite,
 * stat, mkdir, opendir, …). The sysroot libc turns those into WASI
 * path_open, which this machine does not serve. These wrappers are the
 * libc those host ABIs already call. Bytes cross the client import
 * "goclibc". The static server does not instantiate the module.
 */
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  GOCL_RDONLY = 0,
  GOCL_WRONLY = 1,
  GOCL_RDWR = 2,
  GOCL_CREAT = 4,
  GOCL_TRUNC = 8,
  GOCL_APPEND = 16
};

enum { kGMax = 128, kGMagic = 0x474F434Cu, kGHost = 1, kGPipe = 2 };

__attribute__((import_module("goclibc"), import_name("open")))
extern int goclibc_host_open(int path, int path_len, int flags);
__attribute__((import_module("goclibc"), import_name("read")))
extern int goclibc_host_read(int fd, int ptr, int n);
__attribute__((import_module("goclibc"), import_name("write")))
extern int goclibc_host_write(int fd, int ptr, int n);
__attribute__((import_module("goclibc"), import_name("close")))
extern int goclibc_host_close(int fd);
__attribute__((import_module("goclibc"), import_name("seek")))
extern int goclibc_host_seek(int fd, int off, int whence);
__attribute__((import_module("goclibc"), import_name("flush")))
extern int goclibc_host_flush(int fd);
__attribute__((import_module("goclibc"), import_name("stat")))
extern int goclibc_host_stat(int path, int path_len, int out);
__attribute__((import_module("goclibc"), import_name("fstat")))
extern int goclibc_host_fstat(int fd, int out);
__attribute__((import_module("goclibc"), import_name("mkdir")))
extern int goclibc_host_mkdir(int path, int path_len);
__attribute__((import_module("goclibc"), import_name("unlink")))
extern int goclibc_host_unlink(int path, int path_len);
__attribute__((import_module("goclibc"), import_name("rmdir")))
extern int goclibc_host_rmdir(int path, int path_len);
__attribute__((import_module("goclibc"), import_name("rename")))
extern int goclibc_host_rename(int oldp, int oldn, int newp, int newn);
__attribute__((import_module("goclibc"), import_name("access")))
extern int goclibc_host_access(int path, int path_len);
__attribute__((import_module("goclibc"), import_name("getcwd")))
extern int goclibc_host_getcwd(int buf, int cap);
__attribute__((import_module("goclibc"), import_name("chdir")))
extern int goclibc_host_chdir(int path, int path_len);
__attribute__((import_module("goclibc"), import_name("readdir")))
extern int goclibc_host_readdir(int path, int path_len, int buf, int cap);
__attribute__((import_module("goclibc"), import_name("dup")))
extern int goclibc_host_dup(int fd);

extern FILE *__real_fopen(const char *, const char *);
extern int __real_fclose(FILE *);
extern size_t __real_fread(void *, size_t, size_t, FILE *);
extern size_t __real_fwrite(const void *, size_t, size_t, FILE *);
extern int __real_fflush(FILE *);
extern int __real_fseek(FILE *, long, int);
extern long __real_ftell(FILE *);
extern int __real_fileno(FILE *);
extern FILE *__real_fdopen(int, const char *);
extern int __real_feof(FILE *);
extern int __real_ferror(FILE *);
extern void __real_clearerr(FILE *);
extern void __real_rewind(FILE *);
extern int __real_stat(const char *, struct stat *);
extern int __real_lstat(const char *, struct stat *);
extern int __real_fstat(int, struct stat *);
extern int __real_access(const char *, int);
extern int __real_dup(int);
extern DIR *__real_opendir(const char *);
extern struct dirent *__real_readdir(DIR *);
extern int __real_closedir(DIR *);

struct GPipe {
  char *b;
  size_t n, cap, r;
  int readers, writers;
};

struct GFile {
  unsigned magic;
  int slot;
  int eof;
  int err;
};

struct GSlot {
  int used;
  int kind;
  int host;
  int pend;
  struct GPipe *pipe;
  struct GFile *file;
};

static struct GSlot g_slots[kGMax];

struct GDir {
  unsigned magic;
  char *blob;
  int len;
  int off;
  struct dirent *ent;
};

static int g_alloc(void) {
  for (int i = 3; i < kGMax; i++) {
    if (!g_slots[i].used) {
      memset(&g_slots[i], 0, sizeof g_slots[i]);
      g_slots[i].used = 1;
      return i;
    }
  }
  return -1;
}

static int g_writing(int flags) {
  if (flags & (GOCL_CREAT | GOCL_TRUNC | GOCL_APPEND)) return 1;
  return (flags & 3) != GOCL_RDONLY;
}

static int g_mode_flags(const char *mode) {
  int acc = GOCL_RDONLY;
  int extra = 0;
  int plus = 0;
  if (!mode) return GOCL_RDONLY;
  for (const char *p = mode; *p; p++) {
    if (*p == 'w') {
      acc = GOCL_WRONLY;
      extra |= GOCL_CREAT | GOCL_TRUNC;
    } else if (*p == 'a') {
      acc = GOCL_WRONLY;
      extra |= GOCL_CREAT | GOCL_APPEND;
    } else if (*p == 'r') {
      acc = GOCL_RDONLY;
    } else if (*p == '+') {
      plus = 1;
    }
  }
  if (plus) acc = GOCL_RDWR;
  return acc | extra;
}

static struct GFile *g_ours(FILE *fp) {
  if (!fp) return NULL;
  struct GFile *f = (struct GFile *)fp;
  if (f->magic != kGMagic) return NULL;
  if (f->slot < 3 || f->slot >= kGMax) return NULL;
  if (g_slots[f->slot].file != f) return NULL;
  return f;
}

static void g_fail(int rc) {
  if (rc < 0) errno = -rc;
}

static int g_fill_stat(unsigned char *raw, struct stat *st) {
  unsigned mode = (unsigned)raw[0] | ((unsigned)raw[1] << 8) | ((unsigned)raw[2] << 16) |
                  ((unsigned)raw[3] << 24);
  unsigned long long sz = 0;
  for (int i = 0; i < 8; i++) sz |= (unsigned long long)raw[8 + i] << (8 * i);
  memset(st, 0, sizeof *st);
  st->st_mode = (mode_t)mode;
  st->st_nlink = 1;
  st->st_size = (off_t)sz;
  return 0;
}

static struct GFile *g_wrap(int slot) {
  struct GFile *f = (struct GFile *)calloc(1, sizeof *f);
  if (!f) return NULL;
  f->magic = kGMagic;
  f->slot = slot;
  g_slots[slot].file = f;
  return f;
}

static int g_pipe_read(struct GPipe *p, void *buf, size_t n) {
  size_t avail = p->n - p->r;
  if (avail > n) avail = n;
  if (avail && buf) memcpy(buf, p->b + p->r, avail);
  p->r += avail;
  return (int)avail;
}

static int g_pipe_write(struct GPipe *p, const void *buf, size_t n) {
  if (p->r > 0 && p->r == p->n) {
    p->r = 0;
    p->n = 0;
  }
  if (p->n + n > p->cap) {
    size_t cap = p->cap ? p->cap * 2 : 256;
    while (cap < p->n + n) cap *= 2;
    char *b = (char *)realloc(p->b, cap);
    if (!b) return -1;
    p->b = b;
    p->cap = cap;
  }
  if (n && buf) memcpy(p->b + p->n, buf, n);
  p->n += n;
  return (int)n;
}

FILE *__wrap_fopen(const char *path, const char *mode) {
  if (!path || !path[0]) {
    errno = EINVAL;
    return NULL;
  }
  int flags = g_mode_flags(mode);
  int slot = g_alloc();
  if (slot < 0) {
    errno = EMFILE;
    return NULL;
  }
  int h = goclibc_host_open((int)(uintptr_t)path, (int)strlen(path), flags);
  if (h < 0) {
    g_slots[slot].used = 0;
    if (!g_writing(flags)) {
      FILE *r = __real_fopen(path, mode);
      if (r) return r;
    }
    g_fail(h);
    return NULL;
  }
  g_slots[slot].kind = kGHost;
  g_slots[slot].host = h;
  struct GFile *f = g_wrap(slot);
  if (!f) {
    goclibc_host_close(h);
    g_slots[slot].used = 0;
    errno = ENOMEM;
    return NULL;
  }
  return (FILE *)f;
}

FILE *__wrap_fdopen(int fd, const char *mode) {
  (void)mode;
  if (fd >= 3 && fd < kGMax && g_slots[fd].used) {
    if (g_slots[fd].file) return (FILE *)g_slots[fd].file;
    struct GFile *f = g_wrap(fd);
    if (!f) {
      errno = ENOMEM;
      return NULL;
    }
    return (FILE *)f;
  }
  return __real_fdopen(fd, mode);
}

int __wrap_fclose(FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) return __real_fclose(fp);
  int slot = f->slot;
  if (g_slots[slot].kind == kGHost) goclibc_host_close(g_slots[slot].host);
  if (g_slots[slot].kind == kGPipe && g_slots[slot].pipe) {
    struct GPipe *p = g_slots[slot].pipe;
    if (g_slots[slot].pend == 0) p->readers--;
    else p->writers--;
    if (p->readers <= 0 && p->writers <= 0) {
      free(p->b);
      free(p);
    }
  }
  g_slots[slot].file = NULL;
  g_slots[slot].used = 0;
  free(f);
  return 0;
}

size_t __wrap_fread(void *ptr, size_t size, size_t nmemb, FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) return __real_fread(ptr, size, nmemb, fp);
  size_t want = size * nmemb;
  if (!want) return 0;
  int n = 0;
  struct GSlot *s = &g_slots[f->slot];
  if (s->kind == kGPipe) {
    if (!s->pipe) return 0;
    n = g_pipe_read(s->pipe, ptr, want);
  } else {
    n = goclibc_host_read(s->host, (int)(uintptr_t)ptr, (int)want);
  }
  if (n < 0) {
    g_fail(n);
    f->err = 1;
    return 0;
  }
  if ((size_t)n < want) f->eof = 1;
  if (size == 0) return 0;
  return (size_t)n / size;
}

size_t __wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) return __real_fwrite(ptr, size, nmemb, fp);
  size_t want = size * nmemb;
  if (!want) return 0;
  int n = 0;
  struct GSlot *s = &g_slots[f->slot];
  if (s->kind == kGPipe) {
    if (!s->pipe) return 0;
    n = g_pipe_write(s->pipe, ptr, want);
  } else {
    n = goclibc_host_write(s->host, (int)(uintptr_t)ptr, (int)want);
  }
  if (n < 0) {
    g_fail(n);
    f->err = 1;
    return 0;
  }
  if (size == 0) return 0;
  return (size_t)n / size;
}

int __wrap_fflush(FILE *fp) {
  if (!fp) {
    for (int i = 3; i < kGMax; i++) {
      if (g_slots[i].used && g_slots[i].kind == kGHost) goclibc_host_flush(g_slots[i].host);
    }
    return __real_fflush(NULL);
  }
  struct GFile *f = g_ours(fp);
  if (!f) return __real_fflush(fp);
  if (g_slots[f->slot].kind != kGHost) return 0;
  int rc = goclibc_host_flush(g_slots[f->slot].host);
  if (rc < 0) {
    g_fail(rc);
    f->err = 1;
    return EOF;
  }
  return 0;
}

int __wrap_fseek(FILE *fp, long off, int whence) {
  struct GFile *f = g_ours(fp);
  if (!f) return __real_fseek(fp, off, whence);
  if (g_slots[f->slot].kind != kGHost) {
    errno = ESPIPE;
    return -1;
  }
  int rc = goclibc_host_seek(g_slots[f->slot].host, (int)off, whence);
  if (rc < 0) {
    g_fail(rc);
    f->err = 1;
    return -1;
  }
  f->eof = 0;
  return 0;
}

long __wrap_ftell(FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) return __real_ftell(fp);
  if (g_slots[f->slot].kind != kGHost) return -1;
  int rc = goclibc_host_seek(g_slots[f->slot].host, 0, SEEK_CUR);
  if (rc < 0) {
    g_fail(rc);
    return -1;
  }
  return (long)rc;
}

int __wrap_fileno(FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) return __real_fileno(fp);
  return f->slot;
}

int __wrap_feof(FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) return __real_feof(fp);
  return f->eof;
}

int __wrap_ferror(FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) return __real_ferror(fp);
  return f->err;
}

void __wrap_clearerr(FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) {
    __real_clearerr(fp);
    return;
  }
  f->eof = 0;
  f->err = 0;
}

void __wrap_rewind(FILE *fp) {
  struct GFile *f = g_ours(fp);
  if (!f) {
    __real_rewind(fp);
    return;
  }
  __wrap_fseek(fp, 0, SEEK_SET);
  f->err = 0;
  f->eof = 0;
}

static int g_stat_at(int rc, unsigned char *raw, struct stat *st) {
  if (rc < 0) {
    g_fail(rc);
    return -1;
  }
  return g_fill_stat(raw, st);
}

int __wrap_stat(const char *path, struct stat *st) {
  if (!path || !st) {
    errno = EINVAL;
    return -1;
  }
  unsigned char raw[16];
  int rc = goclibc_host_stat((int)(uintptr_t)path, (int)strlen(path), (int)(uintptr_t)raw);
  if (rc < 0) {
    int r = __real_stat(path, st);
    if (r == 0) return 0;
    g_fail(rc);
    return -1;
  }
  return g_stat_at(rc, raw, st);
}

int __wrap_lstat(const char *path, struct stat *st) { return __wrap_stat(path, st); }

int __wrap_fstat(int fd, struct stat *st) {
  if (fd >= 3 && fd < kGMax && g_slots[fd].used && g_slots[fd].kind == kGHost) {
    unsigned char raw[16];
    int rc = goclibc_host_fstat(g_slots[fd].host, (int)(uintptr_t)raw);
    return g_stat_at(rc, raw, st);
  }
  return __real_fstat(fd, st);
}

static int g_path(const char *path, int (*fn)(int, int)) {
  if (!path) {
    errno = EINVAL;
    return -1;
  }
  int rc = fn((int)(uintptr_t)path, (int)strlen(path));
  if (rc < 0) {
    g_fail(rc);
    return -1;
  }
  return 0;
}

int __wrap_mkdir(const char *path, mode_t mode) {
  (void)mode;
  return g_path(path, goclibc_host_mkdir);
}

int __wrap_unlink(const char *path) { return g_path(path, goclibc_host_unlink); }

int __wrap_remove(const char *path) { return __wrap_unlink(path); }

int __wrap_rmdir(const char *path) { return g_path(path, goclibc_host_rmdir); }

int __wrap_rename(const char *oldp, const char *newp) {
  if (!oldp || !newp) {
    errno = EINVAL;
    return -1;
  }
  int rc = goclibc_host_rename((int)(uintptr_t)oldp, (int)strlen(oldp), (int)(uintptr_t)newp,
                              (int)strlen(newp));
  if (rc < 0) {
    g_fail(rc);
    return -1;
  }
  return 0;
}

int __wrap_access(const char *path, int mode) {
  if (!path) {
    errno = EINVAL;
    return -1;
  }
  int rc = goclibc_host_access((int)(uintptr_t)path, (int)strlen(path));
  if (rc < 0) {
    int r = __real_access(path, mode);
    if (r == 0) return 0;
    g_fail(rc);
    return -1;
  }
  (void)mode;
  return 0;
}

char *__wrap_getcwd(char *buf, size_t cap) {
  if (!buf || cap == 0) {
    errno = EINVAL;
    return NULL;
  }
  int n = goclibc_host_getcwd((int)(uintptr_t)buf, (int)cap);
  if (n < 0) {
    g_fail(n);
    return NULL;
  }
  return buf;
}

int __wrap_chdir(const char *path) { return g_path(path, goclibc_host_chdir); }

DIR *__wrap_opendir(const char *path) {
  if (!path) {
    errno = EINVAL;
    return NULL;
  }
  char *blob = (char *)malloc(8192);
  if (!blob) {
    errno = ENOMEM;
    return NULL;
  }
  int n = goclibc_host_readdir((int)(uintptr_t)path, (int)strlen(path), (int)(uintptr_t)blob, 8192);
  if (n < 0) {
    free(blob);
    DIR *real = __real_opendir(path);
    if (real) return real;
    g_fail(n);
    return NULL;
  }
  struct GDir *d = (struct GDir *)calloc(1, sizeof *d);
  if (!d) {
    free(blob);
    errno = ENOMEM;
    return NULL;
  }
  d->magic = kGMagic;
  d->blob = blob;
  d->len = n;
  d->off = 0;
  d->ent = (struct dirent *)malloc(sizeof(struct dirent) + 256);
  if (!d->ent) {
    free(blob);
    free(d);
    errno = ENOMEM;
    return NULL;
  }
  memset(d->ent, 0, sizeof(struct dirent));
  return (DIR *)d;
}

struct dirent *__wrap_readdir(DIR *dir) {
  struct GDir *d = (struct GDir *)dir;
  if (!d || d->magic != kGMagic) return __real_readdir(dir);
  if (!d->blob) return NULL;
  if (d->off >= d->len) return NULL;
  char kind = d->blob[d->off++];
  char *name = d->blob + d->off;
  size_t n = strlen(name);
  d->off += (int)n + 1;
  if (n > 255) n = 255;
  memset(d->ent, 0, sizeof(struct dirent));
  memcpy(d->ent->d_name, name, n);
  d->ent->d_name[n] = 0;
#ifdef DT_DIR
  d->ent->d_type = (kind == 'd') ? DT_DIR : DT_REG;
#else
  (void)kind;
#endif
  return d->ent;
}

int __wrap_closedir(DIR *dir) {
  struct GDir *d = (struct GDir *)dir;
  if (!d || d->magic != kGMagic) return __real_closedir(dir);
  free(d->blob);
  free(d->ent);
  free(d);
  return 0;
}

int __wrap_pipe(int fds[2]) {
  if (!fds) {
    errno = EINVAL;
    return -1;
  }
  int a = g_alloc();
  int b = g_alloc();
  if (a < 0 || b < 0) {
    if (a >= 0) g_slots[a].used = 0;
    if (b >= 0) g_slots[b].used = 0;
    errno = EMFILE;
    return -1;
  }
  struct GPipe *p = (struct GPipe *)calloc(1, sizeof *p);
  if (!p) {
    g_slots[a].used = 0;
    g_slots[b].used = 0;
    errno = ENOMEM;
    return -1;
  }
  p->readers = 1;
  p->writers = 1;
  g_slots[a].kind = kGPipe;
  g_slots[a].pipe = p;
  g_slots[a].pend = 0;
  g_slots[b].kind = kGPipe;
  g_slots[b].pipe = p;
  g_slots[b].pend = 1;
  fds[0] = a;
  fds[1] = b;
  return 0;
}

int __wrap_dup(int fd) {
  if (fd >= 3 && fd < kGMax && g_slots[fd].used) {
    int n = g_alloc();
    if (n < 0) {
      errno = EMFILE;
      return -1;
    }
    g_slots[n] = g_slots[fd];
    g_slots[n].file = NULL;
    if (g_slots[fd].kind == kGHost) {
      int h = goclibc_host_dup(g_slots[fd].host);
      if (h < 0) {
        g_slots[n].used = 0;
        g_fail(h);
        return -1;
      }
      g_slots[n].host = h;
    } else if (g_slots[fd].pipe) {
      if (g_slots[fd].pend == 0) g_slots[fd].pipe->readers++;
      else g_slots[fd].pipe->writers++;
    }
    return n;
  }
  return __real_dup(fd);
}

int __wrap_dup2(int oldfd, int newfd) {
  if (oldfd == newfd) return newfd;
  int n = __wrap_dup(oldfd);
  if (n < 0) return -1;
  if (n == newfd) return newfd;
  if (newfd >= 3 && newfd < kGMax) {
    if (g_slots[newfd].file) __wrap_fclose((FILE *)g_slots[newfd].file);
    g_slots[newfd] = g_slots[n];
    g_slots[n].used = 0;
    return newfd;
  }
  return n;
}

#ifdef __cplusplus
}
#endif
