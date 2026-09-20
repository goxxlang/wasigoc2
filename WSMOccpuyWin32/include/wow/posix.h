#ifndef WOW_POSIX_H_
#define WOW_POSIX_H_

#include "wow/c/types.h"
#include "wow/event.h"
#include "wow/surface.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wow {

// Linux occupancy .ko (wowwin32.ko).
// Viewer occupies module_init, file_operations, unlocked_ioctl.
// Host ntfs.ko stays. Occupancy never insmod / ioctl the host.

enum class KoKind {
  kOccupancy = WOW_KO_OCCUPANCY,
  kNtfs = WOW_KO_NTFS,
};

struct KoImage {
  int id = 0;
  KoKind kind = KoKind::kOccupancy;
  Isolation isolation = Isolation::kSys;
  std::string image;
  std::string devnode;
  bool loaded = false;
  bool attached = false;
  uint64_t ioctls = 0;
  int channel_id = 0;

  const char* KindName() const;
  std::string ToJson() const;
};

class KoTable {
 public:
  KoImage Load(KoKind kind, std::string_view image, std::string_view devnode);
  std::optional<KoImage> Find(int id) const;
  KoImage* FindMutable(int id);
  KoImage* OfKind(KoKind kind);
  const KoImage* OfKind(KoKind kind) const;
  std::vector<KoImage> All() const;
  bool Drop(int id);
  void Clear();
  size_t size() const { return images_.size(); }

 private:
  std::vector<KoImage> images_;
  int next_id_ = 1;
};

class PosixModule {
 public:
  KoTable& table() { return table_; }
  const KoTable& table() const { return table_; }

  bool replaces_host_ntfs() const { return false; }
  bool loaded() const { return entered_; }
  const std::string& last_error() const { return last_error_; }
  const std::string& devnode() const { return devnode_; }

  WowResult ModuleInit(std::string_view label = "wowwin32");
  WowResult AddDevnode();
  WowResult Ioctl(uint32_t request, std::span<const uint8_t> bytes = {});
  WowResult Unload(int id);
  std::string GetInfoJson() const;
  void Clear();

 private:
  WowResult LoadImage(KoKind kind, std::string_view image,
                      std::string_view devnode);

  KoTable table_;
  std::string last_error_;
  std::string devnode_ = "/dev/wowwin32";
  bool entered_ = false;
};

const char* KoKindName(KoKind k);

}  // namespace wow

#endif  // WOW_POSIX_H_
