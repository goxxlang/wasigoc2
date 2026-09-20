#include "wow/gocvm.h"

#include "wow/catalog.h"

#include <cstring>

#if defined(WOW_HAS_WIN32)
#include "win32/dispatch.h"
#endif
#if defined(WOW_HAS_GOCVM)
#include "runtime.hpp"
#include <deque>
#endif

namespace wow {
namespace {

std::string Strip(std::string_view topic) {
  std::string t(topic);
  if (t.rfind("win32.", 0) == 0) {
    t = t.substr(6);
  }
  return t;
}

bool IsLeftover(std::string_view topic) {
  return topic == "win32" || topic == "wsl" || topic == "nix";
}

}  // namespace

bool OccupancyBridge::OwnsTopic(std::string_view topic) {
  if (IsLeftover(topic)) {
    return true;
  }
  std::string t = Strip(topic);
  uint32_t ord = 0;
  return DefaultCatalog().Lookup(t, &ord, nullptr) ||
         DefaultCatalog().Lookup(std::string("win32.") + t, &ord, nullptr);
}

bool OccupancyBridge::Call(const std::string& topic, const std::string& payload,
                           std::string* reply_out, std::string* err_out) {
  if (!occupancy_) {
    if (err_out) {
      *err_out = "no occupancy";
    }
    return false;
  }
  if (IsLeftover(topic)) {
#if defined(WOW_HAS_WIN32)
    if (!occupancy_->gocvm()) {
      if (occupancy_->OccupyGocvm("wasigocvm") != WOW_RESULT_OK) {
        if (err_out) {
          *err_out = occupancy_->last_error().empty()
                         ? "gocvm toolkit"
                         : occupancy_->last_error();
        }
        return false;
      }
    }
    std::string api = payload;
    std::string args;
    if (topic == "win32") {
      auto p = payload.find('\x1f');
      if (p != std::string::npos) {
        api = payload.substr(0, p);
        args = payload.substr(p + 1);
      }
    } else if (topic == "wsl") {
      if (payload == "list" || payload.empty()) {
        api = "WslList";
        args.clear();
      } else {
        api = "WslExec";
        args = payload;
      }
    } else {
      api = payload.rfind("run", 0) == 0 ? "NixRun" : "NixVersion";
      args = payload;
    }
    return occupancy_->CallWin32(api, args, reply_out, err_out);
#else
    if (err_out) {
      *err_out = "WASMWin32 missing";
    }
    return false;
#endif
  }
  if (OwnsTopic(topic)) {
    return occupancy_->DispatchTopic(topic, payload, reply_out, err_out);
  }
  if (err_out) {
    *err_out = "unknown win32 topic";
  }
  return false;
}

#if defined(WOW_HAS_GOCVM)

namespace {

class OccupancyHostBridge final : public wasigo::gocvm::HostBridge {
 public:
  explicit OccupancyHostBridge(Occupancy* occupancy) : inner_(occupancy) {}

  bool Call(const std::string& topic, const std::string& payload,
            std::string* reply_out, std::string* err_out) override {
    return inner_.Call(topic, payload, reply_out, err_out);
  }

  OccupancyBridge& inner() { return inner_; }

 private:
  OccupancyBridge inner_;
};

class OccupancyAsyncHostBridge final : public wasigo::gocvm::AsyncHostBridge {
 public:
  OccupancyAsyncHostBridge(OccupancyBridge* inner,
                           wasigo::gocvm::AsyncHostBridge* next)
      : inner_(inner), next_(next) {}

  wasigo::gocvm::AsyncHostBridge* next() const { return next_; }

  uint64_t Submit(const std::string& topic, const std::string& payload) override {
    if (next_ && inner_ && !inner_->OwnsTopic(topic)) {
      return next_->Submit(topic, payload);
    }
    uint64_t id = next_id_++;
    Completion c;
    c.id = id;
    std::string reply;
    std::string err;
    c.ok = inner_->Call(topic, payload, &reply, &err);
    c.reply = std::move(reply);
    c.err = std::move(err);
    ready_.push_back(std::move(c));
    return id;
  }

  bool PollOne(Completion* out) override {
    if (!out) {
      return false;
    }
    if (!ready_.empty()) {
      *out = std::move(ready_.front());
      ready_.pop_front();
      return true;
    }
    return next_ ? next_->PollOne(out) : false;
  }

  void WaitOne(Completion* out) override {
    if (PollOne(out)) {
      return;
    }
    if (next_) {
      next_->WaitOne(out);
      return;
    }
    if (out) {
      out->ok = false;
      out->err = "occupancy async wait with no pending";
    }
  }

 private:
  OccupancyBridge* inner_ = nullptr;
  wasigo::gocvm::AsyncHostBridge* next_ = nullptr;
  uint64_t next_id_ = 1;
  std::deque<Completion> ready_;
};

OccupancyHostBridge*& Slot() {
  static OccupancyHostBridge* b = nullptr;
  return b;
}

OccupancyAsyncHostBridge*& AsyncSlot() {
  static OccupancyAsyncHostBridge* b = nullptr;
  return b;
}

}  // namespace

void RegisterOccupancyHostBridge(Occupancy* occupancy) {
  wasigo::gocvm::AsyncHostBridge* next =
      wasigo::gocvm::detail::async_bridge_slot();
  if (next == AsyncSlot()) {
    next = AsyncSlot() ? AsyncSlot()->next() : nullptr;
  }
  delete AsyncSlot();
  delete Slot();
  Slot() = new OccupancyHostBridge(occupancy);
  AsyncSlot() = new OccupancyAsyncHostBridge(&Slot()->inner(), next);
  wasigo::gocvm::RegisterHostBridge(Slot());
  wasigo::gocvm::RegisterAsyncHostBridge(AsyncSlot());
}

void UnregisterOccupancyHostBridge() {
  wasigo::gocvm::AsyncHostBridge* next =
      AsyncSlot() ? AsyncSlot()->next() : nullptr;
  wasigo::gocvm::RegisterAsyncHostBridge(next);
  wasigo::gocvm::RegisterHostBridge(nullptr);
  delete AsyncSlot();
  AsyncSlot() = nullptr;
  delete Slot();
  Slot() = nullptr;
}

#endif  // WOW_HAS_GOCVM

}  // namespace wow
