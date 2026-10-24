#include "gocos/jitrt.hpp"
#include "gocos/peinterp.hpp"

#include "reactos/catalog.h"
#include "reactos/dispatch.h"
#include "wasmpe/loader.hpp"
#include "win32/catalog.h"
#include "win32/dispatch.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace wasmgocos {

// FreeLoader and ntoskrnl. The guest name is React. No args boots.
// `api` and the rest are 0x1F fields passed through wasmreact_call.
std::string jit_react(const char* args) {
  std::string spec = args != nullptr ? args : "";
  std::string api = "Boot";
  std::string rest;
  if (!spec.empty()) {
    size_t cut = spec.find('\x1f');
    api = cut == std::string::npos ? spec : spec.substr(0, cut);
    rest = cut == std::string::npos ? std::string() : spec.substr(cut + 1);
  }
  char out[4096];
  out[0] = 0;
  int rc = wasmreact_call(api.c_str(), rest.c_str(), out, sizeof(out));
  if (rc != 0 && out[0] == 0) return "error: react";
  return out;
}

int jit_react_export_index(const char* name) {
  if (name == nullptr || name[0] == 0) return -1;
  int count = 0;
  const WasmReactApi* rows = wasmreact_catalog(&count);
  for (int i = 0; i < count; ++i)
    if (rows[i].name && std::strcmp(rows[i].name, name) == 0) return i;
  return -1;
}

namespace {

// The two hooks WASMWin32's software WHv calls to reach ~/WASMReact for a
// PE import the host catalog does not serve. WASMWin32 must not depend on
// WASMReact, so the kernel installs them here. This is the no-vCPU path:
// LoadLibraryW's k32_pe_resolve binds such imports to the React band, and
// k32_whp_decode_run dispatches them here as it steps the mapped code.
int ReactCallByIndex(unsigned index, const char* args, char* out, unsigned cap) {
  int count = 0;
  const WasmReactApi* rows = wasmreact_catalog(&count);
  if (static_cast<int>(index) >= count) {
    if (out != nullptr && cap != 0) out[0] = 0;
    return -1;
  }
  return wasmreact_call(rows[index].name, args != nullptr ? args : "", out, cap);
}

struct InstallReactHooks {
  InstallReactHooks() {
    k32_set_react_index(jit_react_export_index);
    k32_set_react_call(ReactCallByIndex);
  }
};
InstallReactHooks install_react_hooks;

// Per-load import tally for the reply. A PE that WASMReact fully backs
// has unresolved == 0.
struct PeLink {
  int imports = 0;
  int react = 0;
  int unresolved = 0;
  std::vector<std::string> rows;
};

// WASMPELoader's Resolve callback. Each import is linked to ~/WASMReact's
// kernel32 / ntdll row when it has one; the returned value is the IAT
// entry the vCPU decodes as a catalog OUT into wasmreact_call. An import
// WASMReact does not answer resolves to 0 and is named in the reply.
// kThunkReactBase keeps these off the host thunk band (0x10000, the one
// WASMWin32's k32_pe_resolve uses), so the two never collide in one IAT.
constexpr unsigned long long kThunkReactBase = 0x40000000ull;

unsigned long long ReactResolve(const char* dll, const char* name, void* ctx) {
  auto* link = static_cast<PeLink*>(ctx);
  if (link) ++link->imports;
  std::string row = std::string(dll ? dll : "?") + "!" + (name ? name : "?") + "=";
  int index = jit_react_export_index(name);
  if (index < 0) {
    if (link) {
      ++link->unresolved;
      link->rows.push_back(row + "unresolved");
    }
    return 0;
  }
  int count = 0;
  const WasmReactApi* rows = wasmreact_catalog(&count);
  if (link) {
    ++link->react;
    link->rows.push_back(row + rows[index].lib + ":" + std::to_string(index));
  }
  return kThunkReactBase + static_cast<unsigned long long>(index);
}

// Host-first, React-fallback resolver for the run path: kernel32 the host
// catalog serves binds to the host band (wasmwin32_call, in-process on
// wasm), the rest to ~/WASMReact's band. Bands match peinterp's.
unsigned long long CatalogResolve(const char* dll, const char* name, void*) {
  (void)dll;
  if (name == nullptr) return 0;
  int c = 0;
  const WasmWin32Api* cat = wasmwin32_catalog(&c);
  for (int i = 0; i < c; ++i)
    if (cat[i].name && std::strcmp(cat[i].name, name) == 0)
      return kPeThunkHostBase + static_cast<unsigned long long>(i);
  int idx = jit_react_export_index(name);
  if (idx >= 0) return kPeThunkReactBase + static_cast<unsigned long long>(idx);
  return 0;
}

bool ReadPeBytes(const std::string& path, std::vector<unsigned char>* bytes, std::string* err) {
  if (path.empty() || path == "minimal") {
    *bytes = wasmpe::minimal_dll();
    return true;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    *err = "no file";
    return false;
  }
  bytes->assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes->empty()) {
    *err = "empty file";
    return false;
  }
  return true;
}

// Map a PE at the interpreter's fixed VA, then execute its `export` (or the
// entry) on the portable software interpreter — no vCPU, so it runs on
// Android. Imports are linked host-first with a ~/WASMReact fallback.
std::string RunDll(const std::string& rest) {
  std::string path, exportName;
  size_t c1 = rest.find('\x1f');
  path = c1 == std::string::npos ? rest : rest.substr(0, c1);
  if (c1 != std::string::npos) {
    std::string r = rest.substr(c1 + 1);
    size_t c2 = r.find('\x1f');
    exportName = c2 == std::string::npos ? r : r.substr(0, c2);
  }

  std::vector<unsigned char> bytes;
  std::string err;
  if (!ReadPeBytes(path, &bytes, &err)) return "error: jit_dll run: " + err;

  wasmpe::Image image{};
  if (!wasmpe::map(bytes.data(), bytes.size(), &image, CatalogResolve, nullptr, kPeImageBase) ||
      !image.base)
    return "error: jit_dll run: not a PE image";

  unsigned rva = image.entry_rva;
  if (!exportName.empty()) {
    auto it = image.exports.find(exportName);
    if (it == image.exports.end()) {
      wasmpe::unmap(&image);
      return "error: jit_dll run: no export " + exportName;
    }
    rva = it->second;
  }

  PeInterpResult r = pe_interp_run(static_cast<const uint8_t*>(image.base), image.size, rva, nullptr,
                                   500000);
  wasmpe::unmap(&image);

  char head[256];
  std::snprintf(head, sizeof(head), "backend=react run=%s eax=%u steps=%ld fault=%d",
                exportName.empty() ? "<entry>" : exportName.c_str(), r.eax, r.steps, r.fault ? 1 : 0);
  std::string reply = head;
  if (!r.last_reply.empty()) {
    reply += "\x1f" "reply=";
    reply += r.last_reply;
  }
  return reply;
}

}  // namespace

std::string jit_dll(const char* args) {
  std::string spec = args != nullptr ? args : "";
  size_t cut = spec.find('\x1f');
  std::string first = cut == std::string::npos ? spec : spec.substr(0, cut);
  std::string rest = cut == std::string::npos ? std::string() : spec.substr(cut + 1);
  if (first == "run") return RunDll(rest);

  std::string path = first;  // map + link + report
  std::vector<unsigned char> bytes;
  std::string err;
  if (!ReadPeBytes(path, &bytes, &err)) return "error: jit_dll: " + err;

  PeLink link;
  wasmpe::Image image{};
  if (!wasmpe::map(bytes.data(), bytes.size(), &image, ReactResolve, &link) || !image.base)
    return "error: jit_dll: not a PE image";

  char head[256];
  std::snprintf(head, sizeof(head),
                "backend=react base=%llu size=%zu entry=%u machine=0x%x exports=%zu imports=%d "
                "react=%d unresolved=%d",
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(image.base)), image.size,
                image.entry_rva, image.machine, image.exports.size(), link.imports, link.react,
                link.unresolved);
  std::string reply = head;
  for (const std::string& row : link.rows) {
    reply += '\x1f';
    reply += row;
  }
  wasmpe::unmap(&image);
  return reply;
}

}  // namespace wasmgocos
