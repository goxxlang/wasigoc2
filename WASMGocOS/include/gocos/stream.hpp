#ifndef WASMGOCOS_INCLUDE_GOCOS_STREAM_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_STREAM_HPP_

// A byte stream between two stages of a pipeline that run at once
// (src/sh.cc): one stage's stdout, the next one's stdin. It holds at most
// `cap` bytes. The writer waits while it is full and the reader while it
// is empty, each with the kernel lock given up (include/gocos/klock.hpp),
// so the stage on the other end gets to run.
//
// Each end closes when its stage ends. With the write end closed, a read
// drains what is left, then reads end of stream. With the read end
// closed, a write fails: a broken pipe. The WASI host ends a guest that
// writes to one with status 141, as SIGPIPE ends a Unix writer.
//
// A thread whose job is killed (kernel_kill_asked) stops waiting: its
// write fails and its read ends the stream, and the kernel lock, taken
// back, ends its guest.

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <string>

namespace wasmgocos {

class Stream {
 public:
  explicit Stream(size_t cap = 64 * 1024) : cap_(cap) {}
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;

  // Appends all n bytes, waiting for room. False on a broken pipe; some
  // of the bytes may have gone before the reader closed.
  bool Write(const char* bytes, size_t n);
  bool Write(const std::string& bytes) { return Write(bytes.data(), bytes.size()); }
  // Up to `max` bytes, waiting until there is at least one. Empty at the
  // end of the stream, or when max is 0.
  std::string Read(size_t max);
  // Everything up to the end of the stream.
  std::string ReadAll();
  void CloseWrite();
  void CloseRead();

 private:
  std::mutex m_;
  std::condition_variable cv_;
  std::string buf_;
  size_t cap_;
  bool write_closed_ = false;
  bool read_closed_ = false;
};

}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_STREAM_HPP_
