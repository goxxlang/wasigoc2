// Guest gocvm win32 topic bridge.
//
// A guest program running on the in-module VM reaches the host Win32
// layer with gocvm.Call("win32", "<api>\x1f<args>"), gocvm.Call("wsl",
// ...) or gocvm.Call("nix", ...). This registers a wasigo::gocvm async
// host bridge that owns exactly those three topics and forwards each to
// wasmwin32_call() (win32/dispatch.h) -- the same catalog entry the shell
// and the wsl_test use. Every other topic falls through to whatever async
// bridge was already installed (the net bridge), so this composes with it
// rather than replacing it.
//
// Only the topic->wasmwin32_call routing is host-side state here; the
// bridge is otherwise stateless, forwarding each owned topic to the
// WASMWin32 catalog entry point.
#ifndef WASMWIN32_WIN32_GOCVM_BRIDGE_HPP
#define WASMWIN32_WIN32_GOCVM_BRIDGE_HPP

#include <cstring>
#include <deque>
#include <string>

#include "runtime.hpp"
#include "win32/dispatch.h"

namespace win32 {

// Owns "win32" / "wsl" / "nix"; chains everything else to next_.
class Win32HostBridge final : public wasigo::gocvm::AsyncHostBridge {
 public:
  explicit Win32HostBridge(wasigo::gocvm::AsyncHostBridge* next) : next_(next) {}

  wasigo::gocvm::AsyncHostBridge* next() const { return next_; }

  static bool Owns(const std::string& topic) {
    return topic == "win32" || topic == "wsl" || topic == "nix";
  }

  uint64_t Submit(const std::string& topic, const std::string& payload) override {
    if (!Owns(topic) && next_) {
      return next_->Submit(topic, payload);
    }
    uint64_t id = next_id_++;
    Completion c;
    c.id = id;

    std::string api;
    std::string args;
    if (topic == "win32") {
      auto p = payload.find('\x1f');
      if (p == std::string::npos) {
        api = payload;
      } else {
        api = payload.substr(0, p);
        args = payload.substr(p + 1);
      }
    } else if (topic == "wsl") {
      if (payload.empty() || payload == "list") {
        api = "WslList";
      } else {
        api = "WslExec";
        args = payload;
      }
    } else {  // "nix"
      api = payload.rfind("run", 0) == 0 ? "NixRun" : "NixVersion";
      args = payload;
    }

    char buf[4096] = {};
    int rc = wasmwin32_call(api.c_str(), args.c_str(), buf, sizeof(buf));
    if (rc != 0 || std::strncmp(buf, "error:", 6) == 0) {
      c.ok = false;
      c.err = buf[0] ? buf : "wasmwin32_call failed";
      c.reply = buf;
    } else {
      c.ok = true;
      c.reply = buf;
    }
    ready_.push_back(std::move(c));
    return id;
  }

  bool PollOne(Completion* out) override {
    if (!out) return false;
    if (!ready_.empty()) {
      *out = std::move(ready_.front());
      ready_.pop_front();
      return true;
    }
    return next_ ? next_->PollOne(out) : false;
  }

  void WaitOne(Completion* out) override {
    if (PollOne(out)) return;
    if (next_) {
      next_->WaitOne(out);
      return;
    }
    if (out) {
      out->ok = false;
      out->err = "win32 async wait with no pending";
    }
  }

 private:
  wasigo::gocvm::AsyncHostBridge* next_ = nullptr;
  uint64_t next_id_ = 1;
  std::deque<Completion> ready_;
};

// Install on top of whatever async bridge is already registered (the net
// bridge). Captures that predecessor once as next_, then registers self.
inline void install_wasigocvm_win32_bridge() {
  static Win32HostBridge bridge(wasigo::gocvm::detail::async_bridge_slot());
  wasigo::gocvm::RegisterAsyncHostBridge(&bridge);
}

}  // namespace win32

#endif  // WASMWIN32_WIN32_GOCVM_BRIDGE_HPP
