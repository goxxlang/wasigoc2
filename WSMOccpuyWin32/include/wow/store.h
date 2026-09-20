#ifndef WOW_STORE_H_
#define WOW_STORE_H_

#include "wow/c/types.h"
#include "wow/surface.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wow {

// One occupancy channel through a Win32 catalog surface. Bytes live in
// the viewer ring — they hop kernel32 / ntdll / HWND / mapped vmem when
// the live wasmwin32_call handles are attached.
struct Channel {
  int id = 0;
  SurfaceKind kind = SurfaceKind::kKernel32;
  Isolation isolation = Isolation::kHost;
  std::string label;
  std::vector<uint8_t> bytes;
  uint64_t writes = 0;
  uint64_t reads = 0;
  uint64_t submits = 0;
  bool mapped = false;
  bool live = true;
  int parent = 0;

  std::string ToJson() const;
};

class OccupancyTable {
 public:
  OccupancyTable();

  Channel Open(SurfaceKind kind, std::string_view label, int parent = 0);
  std::optional<Channel> Find(int id) const;
  Channel* FindMutable(int id);
  std::vector<Channel> All() const;
  std::vector<Channel> OfKind(SurfaceKind kind) const;

  bool Drop(int id);
  void Clear();
  size_t size() const { return channels_.size(); }
  int next_id() const { return next_id_; }

 private:
  std::vector<Channel> channels_;
  int next_id_ = 1;
};

}  // namespace wow

#endif  // WOW_STORE_H_
