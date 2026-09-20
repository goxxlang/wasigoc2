#ifndef WOW_HARNESS_H_
#define WOW_HARNESS_H_

#include "wow/c/types.h"
#include "wow/catalog.h"
#include "wow/ipc.h"
#include "wow/occupancy.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wow {

struct Shot {
  uint32_t ordinal = 0;
  std::string name;
  std::string iface = "win32";
  std::vector<uint8_t> payload;
};

using FireFn = std::function<WowResult(const Shot&)>;

class Harness {
 public:
  explicit Harness(Catalog catalog = Catalog::Drive());

  Catalog& catalog() { return catalog_; }
  const Catalog& catalog() const { return catalog_; }
  Occupancy& occupancy() { return occupancy_; }
  const Occupancy& occupancy() const { return occupancy_; }

  void set_fire(FireFn fn) { fire_ = std::move(fn); }

  Shot Prepare(std::string_view name,
               std::span<const uint8_t> payload = {}) const;

  WowResult FireOccupyWin32();
  WowResult FireOccupyCatalog();
  WowResult FireThinMap();
  WowResult FireOccupancyStatus();
  WowResult FireOccupyKernel32();
  WowResult FireOccupyNtdll();
  WowResult FireOccupyVmem();
  WowResult FireOccupyPipe();
  WowResult FireOccupySys();
  WowResult FireOccupyDriver();
  WowResult FireOccupyGocvm();
  WowResult FireIpc(const Ipc& ipc);
  WowResult FireIpcJson(std::string_view json);
  WowResult FireName(std::string_view name,
                     std::span<const uint8_t> payload = {});

  const std::vector<uint8_t>& last_payload() const { return last_payload_; }

 private:
  WowResult Send(const Shot& s);

  Catalog catalog_;
  Occupancy occupancy_;
  FireFn fire_;
  std::vector<uint8_t> last_payload_;
};

}  // namespace wow

#endif  // WOW_HARNESS_H_
