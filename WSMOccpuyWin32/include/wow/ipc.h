#ifndef WOW_IPC_H_
#define WOW_IPC_H_

#include "wow/c/types.h"
#include "wow/catalog.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace wow {

// Cadmium / gocvm envelope:
//
//   { action: "invoke", ordinal, name, args }
//
// Empty / missing args → occupancyStatus.
struct Ipc {
  std::string action = "invoke";
  uint32_t ordinal = 0;
  std::string name;
  std::string args_json = "[]";
  std::string sid;
  std::string data;
  int id = 0;
  int src = 0;
  int dst = 0;

  static Ipc Invoke(std::string_view name, uint32_t ordinal = 0);
  static Ipc OccupyWin32(const Catalog& cat = Catalog::Drive());
  static Ipc ThinMap(const Catalog& cat = Catalog::Drive());
  static Ipc OccupancyStatus(const Catalog& cat = Catalog::Drive());

  std::string ToJson() const;
  static bool FromJson(std::string_view json, Ipc* out);

  bool is_win32() const;
};

}  // namespace wow

#endif  // WOW_IPC_H_
