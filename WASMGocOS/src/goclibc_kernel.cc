// The goclibc calls of the module that links this kernel. go++'s goclibc
// built with GOCLIBC_IN_KERNEL calls these instead of importing them, so
// that module's own libc (fopen, mkdir, opendir, ...) lands on the
// kernel's files: the same WasiHost::Goclibc a guest's imports reach, on
// the session's namespace. Paths are DOS paths too: C:\ is /mnt/c, and
// the process starts there.

#include "gocos/files.hpp"
#include "gocos/wasi.hpp"

#include <cstdint>

namespace {

wasmgocos::WasiHost& Self() {
  static wasmgocos::WasiHost* host = [] {
    // The drive is a directory before anything is made on it, and the
    // process starts there.
    wasmgocos::files::MakeDir(wasmgocos::files::Walk("/mnt"));
    wasmgocos::files::MakeDir(wasmgocos::files::Walk("/mnt/c"));
    auto* h = new wasmgocos::WasiHost({}, {});
    h->set_dos(true);
    h->set_cwd("/mnt/c");
    return h;
  }();
  return *host;
}

uint64_t U(int v) { return static_cast<uint32_t>(v); }

}  // namespace

extern "C" {

int goclibc_host_open(int path, int path_len, int flags) {
  const uint64_t a[] = {U(path), U(path_len), U(flags)};
  return Self().GoclibcLocal("open", a);
}
int goclibc_host_read(int fd, int ptr, int n) {
  const uint64_t a[] = {U(fd), U(ptr), U(n)};
  return Self().GoclibcLocal("read", a);
}
int goclibc_host_write(int fd, int ptr, int n) {
  const uint64_t a[] = {U(fd), U(ptr), U(n)};
  return Self().GoclibcLocal("write", a);
}
int goclibc_host_close(int fd) {
  const uint64_t a[] = {U(fd)};
  return Self().GoclibcLocal("close", a);
}
int goclibc_host_seek(int fd, int off, int whence) {
  const uint64_t a[] = {U(fd), U(off), U(whence)};
  return Self().GoclibcLocal("seek", a);
}
int goclibc_host_flush(int fd) {
  const uint64_t a[] = {U(fd)};
  return Self().GoclibcLocal("flush", a);
}
int goclibc_host_stat(int path, int path_len, int out) {
  const uint64_t a[] = {U(path), U(path_len), U(out)};
  return Self().GoclibcLocal("stat2", a);
}
int goclibc_host_fstat(int fd, int out) {
  const uint64_t a[] = {U(fd), U(out)};
  return Self().GoclibcLocal("fstat2", a);
}
int goclibc_host_mkdir(int path, int path_len) {
  const uint64_t a[] = {U(path), U(path_len)};
  return Self().GoclibcLocal("mkdir", a);
}
int goclibc_host_unlink(int path, int path_len) {
  const uint64_t a[] = {U(path), U(path_len)};
  return Self().GoclibcLocal("unlink", a);
}
int goclibc_host_rmdir(int path, int path_len) {
  const uint64_t a[] = {U(path), U(path_len)};
  return Self().GoclibcLocal("rmdir", a);
}
int goclibc_host_rename(int oldp, int oldn, int newp, int newn) {
  const uint64_t a[] = {U(oldp), U(oldn), U(newp), U(newn)};
  return Self().GoclibcLocal("rename", a);
}
int goclibc_host_access(int path, int path_len) {
  const uint64_t a[] = {U(path), U(path_len)};
  return Self().GoclibcLocal("access", a);
}
int goclibc_host_getcwd(int buf, int cap) {
  const uint64_t a[] = {U(buf), U(cap)};
  return Self().GoclibcLocal("getcwd", a);
}
int goclibc_host_chdir(int path, int path_len) {
  const uint64_t a[] = {U(path), U(path_len)};
  return Self().GoclibcLocal("chdir", a);
}
int goclibc_host_readdir(int path, int path_len, int buf, int cap) {
  const uint64_t a[] = {U(path), U(path_len), U(buf), U(cap)};
  return Self().GoclibcLocal("readdir", a);
}
int goclibc_host_dup(int fd) {
  const uint64_t a[] = {U(fd)};
  return Self().GoclibcLocal("dup", a);
}

}  // extern "C"
