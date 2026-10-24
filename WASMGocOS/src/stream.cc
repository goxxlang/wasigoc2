#include "gocos/stream.hpp"

#include "gocos/klock.hpp"

#include <algorithm>
#include <chrono>

namespace wasmgocos {
namespace {

// Waits for `ready`, or for this thread's job to be killed: a kill does
// not notify the stream, so the wait looks now and then.
template <typename Ready>
bool WaitFor(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, Ready ready) {
  while (!ready()) {
    if (kernel_kill_asked()) return false;
    cv.wait_for(lock, std::chrono::milliseconds(20));
  }
  return true;
}

}  // namespace

bool Stream::Write(const char* bytes, size_t n) {
  size_t sent = 0;
  while (sent < n) {
    bool broken = false;
    kernel_unlocked([&] {
      std::unique_lock<std::mutex> lock(m_);
      if (!WaitFor(cv_, lock, [&] { return read_closed_ || buf_.size() < cap_; }) || read_closed_) {
        broken = true;
        return;
      }
      size_t room = std::min(cap_ - buf_.size(), n - sent);
      buf_.append(bytes + sent, room);
      sent += room;
      cv_.notify_all();
    });
    if (broken) return false;
  }
  return true;
}

std::string Stream::Read(size_t max) {
  std::string got;
  if (max == 0) return got;
  kernel_unlocked([&] {
    std::unique_lock<std::mutex> lock(m_);
    if (!WaitFor(cv_, lock, [&] { return write_closed_ || !buf_.empty(); })) return;
    size_t n = std::min(max, buf_.size());
    got = buf_.substr(0, n);
    buf_.erase(0, n);
    cv_.notify_all();
  });
  return got;
}

std::string Stream::ReadAll() {
  std::string all;
  for (;;) {
    std::string got = Read(cap_);
    if (got.empty()) return all;
    all += got;
  }
}

void Stream::CloseWrite() {
  std::lock_guard<std::mutex> lock(m_);
  write_closed_ = true;
  cv_.notify_all();
}

void Stream::CloseRead() {
  std::lock_guard<std::mutex> lock(m_);
  read_closed_ = true;
  buf_.clear();
  cv_.notify_all();
}

}  // namespace wasmgocos
