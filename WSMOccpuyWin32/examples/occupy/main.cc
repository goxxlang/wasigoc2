// Encode / run viewer-side Win32 occupancy. Isolation is thin at
// kernel32 host / ntdll sys / HWND compositor / mapped vmem / WinHv.
// Occupancy fires the catalog; a live backend may still refuse.
//
//   wow_occupy --occupy --thin
//   wow_occupy --occupy --catalog --vmem --pipe --hv

#include "wow/c/system.h"
#include "wow/catalog.h"
#include "wow/harness.h"
#include "wow/ipc.h"

#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
  WowInit();
  bool occupy = false;
  bool thin = false;
  bool status = false;
  bool dagger = false;
  bool catalog = false;
  bool kernel32 = false;
  bool ntdll = false;
  bool vmem = false;
  bool pipe = false;
  bool hv = false;
  std::string ipc_json;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--occupy" || a == "--occupy-win32") {
      occupy = true;
    } else if (a == "--thin" || a == "--thin-map") {
      thin = true;
    } else if (a == "--status" || a == "--info") {
      status = true;
    } else if (a == "--dagger") {
      dagger = true;
    } else if (a == "--catalog") {
      catalog = true;
    } else if (a == "--kernel32") {
      kernel32 = true;
    } else if (a == "--ntdll") {
      ntdll = true;
    } else if (a == "--vmem") {
      vmem = true;
    } else if (a == "--pipe") {
      pipe = true;
    } else if (a == "--hv" || a == "--winhv") {
      hv = true;
    } else if (a == "--ipc" && i + 1 < argc) {
      ipc_json = argv[++i];
    } else if (a == "-h" || a == "--help") {
      std::fprintf(
          stderr,
          "wow_occupy — viewer Win32 occupancy (thin isolation hop)\n"
          "  --occupy               live catalog walk + named hops\n"
          "  --catalog              every unique DLL in wasmwin32_catalog\n"
          "  --kernel32             GetCurrentProcessId\n"
          "  --ntdll                RtlGetCurrentPeb\n"
          "  --vmem                 VirtualAlloc host pages\n"
          "  --pipe                 CreatePipe ipc\n"
          "  --hv                   WHvGetCapability\n"
          "  --thin                 dump isolation-thin surface map\n"
          "  --status               occupancyStatus JSON\n"
          "  --ipc [JSON]           gocvm invoke\n"
          "  --dagger               use dagger pack ordinals\n");
      return 0;
    }
  }
  if (dagger) {
    WowSetPack(WOW_PACK_DAGGER);
  }
  wow::Harness h(dagger ? wow::Catalog::Dagger() : wow::Catalog::Drive());
  std::printf("win32=%d catalog=%d kernel32=%d vmem=%d hv=%d\n",
              h.occupancy().win32() ? 1 : 0, h.occupancy().catalog() ? 1 : 0,
              h.occupancy().kernel32() ? 1 : 0, h.occupancy().vmem() ? 1 : 0,
              h.occupancy().hv() ? 1 : 0);

  bool any = occupy || thin || status || catalog || kernel32 || ntdll || vmem ||
             pipe || hv || !ipc_json.empty();
  if (!any) {
    occupy = true;
    thin = true;
    status = true;
  }

  auto dump = [&](const char* name, WowResult r) {
    auto payload = h.last_payload();
    wow::Shot s = h.Prepare(name);
    std::printf("%s ordinal=%u -> %u %.*s\n", name, s.ordinal, r,
                static_cast<int>(payload.size()),
                payload.empty() ? ""
                                : reinterpret_cast<const char*>(payload.data()));
  };

  if (occupy) {
    dump("occupyWin32", h.FireOccupyWin32());
  }
  if (catalog) {
    dump("occupyCatalog", h.FireOccupyCatalog());
  }
  if (kernel32) {
    dump("occupyKernel32", h.FireOccupyKernel32());
  }
  if (ntdll) {
    dump("occupyNtdll", h.FireOccupyNtdll());
  }
  if (vmem) {
    dump("occupyVmem", h.FireOccupyVmem());
  }
  if (pipe) {
    dump("occupyPipe", h.FireOccupyPipe());
  }
  if (hv) {
    dump("occupyHv", h.FireName("occupyHv"));
  }
  if (thin) {
    dump("thinMap", h.FireThinMap());
  }
  if (status) {
    dump("occupancyStatus", h.FireOccupancyStatus());
  }
  if (!ipc_json.empty()) {
    dump("ipc", h.FireIpcJson(ipc_json));
  }

  WowShutdown();
  return 0;
}
