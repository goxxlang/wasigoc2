#include "wow/gocvm.h"

#include "wow/occupancy.h"

#if defined(WOW_HAS_GOCVM) && defined(WASIGO_GOCVM) && WASIGO_GOCVM
#include "runtime.hpp"
#endif

// WASIGO_GOCVM_BRIDGE hook. Generated wasigoc main calls
// wasigo::set_os_args → wasigo_gocvm_install_bridge(). Occupancy is the
// hop; this file does not own the guest payload.
// Go++ compile.bat / wasigocvm is one VM: install the in-module machine
// first, then stack occupancy win32.* on it. Do not exec a second gocvm.
namespace {
wow::Occupancy* g_occ = nullptr;
}

extern "C" void wasigo_gocvm_install_bridge() {
#if defined(WOW_HAS_GOCVM)
  if (!g_occ) {
    g_occ = new wow::Occupancy();
  }
#if defined(WASIGO_GOCVM) && WASIGO_GOCVM
  wasigo::gocvm::install_wasigocvm_net_bridge();
#endif
  wow::RegisterOccupancyHostBridge(g_occ);
#endif
}
