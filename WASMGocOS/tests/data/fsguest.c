// A C guest on wasi:filesystem: open, read, write, lseek, fstat, stat,
// mkdir, opendir, rename, unlink, rmdir, through wasi-libc's preopen of
// `/`. The kernel's files are what it sees (include/gocos/files.hpp).
// Built by the toolchain's clang (CMakeLists.txt); guest_test runs it.
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int failed = 0;

static void check(int ok, const char* what) {
  printf("%s %s\n", ok ? "ok" : "FAIL", what);
  if (!ok) failed = 1;
}

static int put(const char* path, const char* text, int flags) {
  int fd = open(path, flags, 0644);
  if (fd < 0) return -1;
  ssize_t n = write(fd, text, strlen(text));
  close(fd);
  return (int)n;
}

static long long ns(struct timespec t) { return (long long)t.tv_sec * 1000000000LL + t.tv_nsec; }

static int get(const char* path, char* buf, size_t cap) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return -1;
  ssize_t n = read(fd, buf, cap - 1);
  close(fd);
  if (n < 0) return -1;
  buf[n] = 0;
  return (int)n;
}

int main(void) {
  char buf[256];
  check(mkdir("/fsguest", 0755) == 0, "mkdir");
  check(mkdir("/fsguest", 0755) != 0 && errno == EEXIST, "mkdir again is EEXIST");
  check(put("/fsguest/a.txt", "hello", O_CREAT | O_WRONLY | O_TRUNC) == 5, "create and write");
  check(get("/fsguest/a.txt", buf, sizeof buf) == 5 && strcmp(buf, "hello") == 0, "read back");
  check(put("/fsguest/a.txt", " world", O_WRONLY | O_APPEND) == 6, "append");
  check(get("/fsguest/a.txt", buf, sizeof buf) == 11 && strcmp(buf, "hello world") == 0, "read appended");

  int fd = open("/fsguest/a.txt", O_RDWR);
  check(fd >= 0, "open read-write");
  check(lseek(fd, 6, SEEK_SET) == 6 && write(fd, "WORLD", 5) == 5, "seek and overwrite");
  struct stat st;
  check(fstat(fd, &st) == 0 && st.st_size == 11 && S_ISREG(st.st_mode), "fstat");
  check(ftruncate(fd, 5) == 0, "ftruncate");
  close(fd);
  check(get("/fsguest/a.txt", buf, sizeof buf) == 5 && strcmp(buf, "hello") == 0, "truncated");

  check(stat("/fsguest", &st) == 0 && S_ISDIR(st.st_mode), "stat a directory");

  // Times: a write stamps the file, and a new entry its directory; both
  // are between the guest's clock before and after.
  struct timespec before, after;
  clock_gettime(CLOCK_REALTIME, &before);
  usleep(2000);
  check(put("/fsguest/t.txt", "12345", O_CREAT | O_WRONLY | O_TRUNC) == 5, "a file to time");
  struct stat ft, dt;
  check(stat("/fsguest/t.txt", &ft) == 0 && stat("/fsguest", &dt) == 0, "stat for times");
  clock_gettime(CLOCK_REALTIME, &after);
  check(ns(ft.st_mtim) > ns(before) && ns(ft.st_mtim) <= ns(after), "a write stamps the file");
  check(ns(dt.st_mtim) > ns(before) && ns(dt.st_mtim) <= ns(after), "a new entry stamps its directory");
  usleep(2000);
  check(put("/fsguest/t.txt", "54321", O_WRONLY) == 5, "rewrite in place");
  struct stat ft2;
  check(stat("/fsguest/t.txt", &ft2) == 0 && ns(ft2.st_mtim) > ns(ft.st_mtim), "a rewrite stamps it again");
  // utimensat: a time set, not stamped; UTIME_OMIT leaves it; futimens
  // on an open file; UTIME_NOW is now again.
  struct timespec set[2] = {{0, UTIME_OMIT}, {1000000000, 5}};
  check(utimensat(AT_FDCWD, "/fsguest/t.txt", set, 0) == 0, "utimensat");
  check(stat("/fsguest/t.txt", &ft) == 0 && ft.st_mtim.tv_sec == 1000000000 && ft.st_mtim.tv_nsec == 5,
        "the time is the one set");
  struct timespec omit[2] = {{0, UTIME_OMIT}, {0, UTIME_OMIT}};
  check(utimensat(AT_FDCWD, "/fsguest/t.txt", omit, 0) == 0 && stat("/fsguest/t.txt", &ft) == 0 &&
            ft.st_mtim.tv_sec == 1000000000,
        "UTIME_OMIT keeps it");
  int tfd = open("/fsguest/t.txt", O_RDWR);
  struct timespec later[2] = {{0, UTIME_OMIT}, {1100000000, 0}};
  // Written first: the write goes back before the time is set, not after.
  check(tfd >= 0 && write(tfd, "x", 1) == 1 && futimens(tfd, later) == 0 && fstat(tfd, &ft) == 0 &&
            ft.st_mtim.tv_sec == 1100000000,
        "futimens");
  close(tfd);
  struct timespec now2[2] = {{0, UTIME_OMIT}, {0, UTIME_NOW}};
  check(utimensat(AT_FDCWD, "/fsguest/t.txt", now2, 0) == 0 && stat("/fsguest/t.txt", &ft) == 0 &&
            ft.st_mtim.tv_sec >= after.tv_sec,
        "UTIME_NOW");
  check(utimensat(AT_FDCWD, "/fsguest/none.txt", set, 0) != 0 && errno == ENOENT, "utimensat on nothing");
  check(unlink("/fsguest/t.txt") == 0, "remove the timed file");
  check(open("/fsguest/none.txt", O_RDONLY) < 0 && errno == ENOENT, "missing is ENOENT");
  check(open("/fsguest/a.txt", O_CREAT | O_EXCL | O_WRONLY, 0644) < 0 && errno == EEXIST, "O_EXCL is EEXIST");

  check(mkdir("/fsguest/sub", 0755) == 0 && put("/fsguest/sub/b.txt", "b", O_CREAT | O_WRONLY) == 1,
        "a file in a subdirectory");
  check(rename("/fsguest/sub/b.txt", "/fsguest/c.txt") == 0, "rename");
  check(get("/fsguest/c.txt", buf, sizeof buf) == 1, "renamed file reads");

  DIR* dir = opendir("/fsguest");
  char names[128] = "";
  if (dir != NULL) {
    struct dirent* e;
    while ((e = readdir(dir)) != NULL) {
      if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
      strcat(names, e->d_name);
      strcat(names, e->d_type == DT_DIR ? "/," : ",");
    }
    closedir(dir);
  }
  printf("ls %s\n", names);
  check(strcmp(names, "a.txt,c.txt,sub/,") == 0, "readdir");

  check(rmdir("/fsguest") != 0 && errno == ENOTEMPTY, "rmdir of a full directory is ENOTEMPTY");
  check(unlink("/fsguest/c.txt") == 0 && rmdir("/fsguest/sub") == 0, "unlink and rmdir");
  check(access("/fsguest/c.txt", F_OK) != 0, "gone");
  check(put("/fsguest/kept.txt", "from C", O_CREAT | O_WRONLY | O_TRUNC) == 6, "a file the kernel reads");

  printf(failed ? "fsguest FAILED\n" : "fsguest ok\n");
  return failed;
}
