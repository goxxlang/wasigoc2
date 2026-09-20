#ifndef WOW_CATALOG_H_
#define WOW_CATALOG_H_

#include "wow/c/types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wow {

inline constexpr uint32_t kMaxWin32Ordinal = 0x7fffffffu;

inline constexpr uint32_t Fnv1a31(std::string_view s) {
  uint32_t h = 2166136261u;
  for (unsigned char c : s) {
    h ^= c;
    h *= 16777619u;
  }
  return h & 0x7fffffffu;
}

inline constexpr uint32_t Fnv131(std::string_view s) {
  uint32_t h = 2166136261u;
  for (unsigned char c : s) {
    h *= 16777619u;
    h ^= c;
  }
  return h & 0x7fffffffu;
}

inline bool IsWin32Ordinal(uint64_t token) { return token <= kMaxWin32Ordinal; }

enum class Pack {
  kDrive = WOW_PACK_DRIVE,
  kDagger = WOW_PACK_DAGGER,
};

struct Method {
  std::string name;
  uint32_t ordinal = 0;
  std::string iface;
  bool fire = false;
};

class Catalog {
 public:
  static Catalog Drive();
  static Catalog Dagger();

  void Set(std::string name, uint32_t ordinal, std::string iface = {},
           bool fire = false);
  bool Lookup(std::string_view name, uint32_t* ordinal,
              std::string* iface = nullptr) const;
  const char* NameOf(uint32_t ordinal) const;
  bool Owns(uint32_t ordinal) const;
  bool MayFire(uint32_t ordinal) const;

  WowResult LoadJson(std::string_view json);

  const std::vector<Method>& methods() const { return methods_; }

 private:
  std::vector<Method> methods_;
  std::unordered_map<std::string, uint32_t> named_;
  std::unordered_map<uint32_t, std::string> by_ord_;
  std::unordered_map<uint32_t, std::string> iface_;
  std::unordered_map<uint32_t, bool> fire_;
};

Catalog& DefaultCatalog();
void SetDefaultPack(Pack pack);

}  // namespace wow

#endif  // WOW_CATALOG_H_
