#ifndef WASMWIN32_INCLUDE_WIN32_PE_MAP_HPP_
#define WASMWIN32_INCLUDE_WIN32_PE_MAP_HPP_

// WASMWin32 include name for ~/WASMPELoader. Load and call that hop.
// Do not keep a second mapper here. MainDLL is the LoadLibrary hop.

#include "wasmpe/loader.hpp"

using PeRes = wasmpe::Res;
using PeMap = wasmpe::Image;
using PeResolve = wasmpe::Resolve;

inline uint16_t pe_u16(const unsigned char* p) { return wasmpe::u16(p); }
inline uint32_t pe_u32(const unsigned char* p) { return wasmpe::u32(p); }
inline uint64_t pe_u64(const unsigned char* p) { return wasmpe::u64(p); }
inline void pe_w32(unsigned char* p, uint32_t v) { wasmpe::w32(p, v); }
inline void pe_w64(unsigned char* p, uint64_t v) { wasmpe::w64(p, v); }

inline void pe_unmap(PeMap* m) { wasmpe::unmap(m); }

inline bool pe_map_image(const void* file, size_t n, PeMap* out, PeResolve resolve, void* ctx) {
  return wasmpe::map(file, n, out, resolve, ctx);
}

inline std::vector<unsigned char> pe_minimal_dll() { return wasmpe::minimal_dll(); }

inline std::string pe_file_version(const PeMap& m) { return wasmpe::file_version(m); }

#endif  // WASMWIN32_INCLUDE_WIN32_PE_MAP_HPP_
