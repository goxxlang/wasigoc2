#ifndef WOW_GOCVM_H_
#define WOW_GOCVM_H_

#include "wow/occupancy.h"

#include <string>
#include <string_view>

namespace wow {

// GocVM occupancy bridge. Same Call(topic, payload, reply, err) shape
// as wasigo::gocvm::HostBridge in ~/Go++/src/runtime.hpp — load that
// runtime, do not rewrite it. Go++ compile.bat / wasigocvm (Was2p
// successor) is one VM: occupancy stacks win32.* on the in-module
// machine. It does not exec a second gocvm.exe. Register with
// gocvm::RegisterHostBridge from OccupancyHostBridge when WOW_HAS_GOCVM
// is set.
class OccupancyBridge {
 public:
  explicit OccupancyBridge(Occupancy* occupancy) : occupancy_(occupancy) {}

  Occupancy* occupancy() { return occupancy_; }

  bool Call(const std::string& topic, const std::string& payload,
            std::string* reply_out, std::string* err_out);

  // Topics this bridge owns: win32.* occupancy catalog, plus leftover
  // win32 / wsl / nix wasmwin32_call. skia.* / mojo.* / webgpu.* /
  // os.exec / v8.* / wst.* / tty.* / net.call / vtpm.attest miss so
  // stacked hops can still run. Occupancy does not steal those topics.
  static bool OwnsTopic(std::string_view topic);

 private:
  Occupancy* occupancy_ = nullptr;
};

#if defined(WOW_HAS_GOCVM)
void RegisterOccupancyHostBridge(Occupancy* occupancy);
void UnregisterOccupancyHostBridge();
#endif

}  // namespace wow

#endif  // WOW_GOCVM_H_
