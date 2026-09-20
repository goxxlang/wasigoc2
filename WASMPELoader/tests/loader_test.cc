#include "wasmpe/loader.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
  auto dll = wasmpe::minimal_dll();
  wasmpe::Image mapped;
  assert(wasmpe::map(dll.data(), dll.size(), &mapped, nullptr, nullptr));
  assert(mapped.base != nullptr);
  assert(mapped.size >= 0x2000);
  assert(mapped.pe64 == 0);
  assert(mapped.dll == 1);
  assert(mapped.nt_off == 0x40);
  assert(std::memcmp(static_cast<unsigned char*>(mapped.base) + 0x1000, "PEMAP", 5) == 0);
  assert(mapped.exports.count("PeMark") == 1);
  assert(mapped.exports["PeMark"] == 0x1000);
  assert(wasmpe::export_rva(mapped, "PeMark") == 0x1000);
  assert(mapped.sections.size() == 1);
  assert(mapped.dirs_rva[wasmpe::kExport] == 0x1080);
  const unsigned char* nt = wasmpe::rva_ptr(mapped, mapped.nt_off, 4);
  assert(nt && nt[0] == 'P' && nt[1] == 'E');

  unsigned char junk[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  wasmpe::Image bad;
  assert(!wasmpe::map(junk, sizeof(junk), &bad, nullptr, nullptr));
  assert(bad.err == "short" || bad.err == "not MZ");

  wasmpe::unmap(&mapped);
  assert(mapped.base == nullptr);
  std::printf("wasmpe_test ok\n");
  return 0;
}
