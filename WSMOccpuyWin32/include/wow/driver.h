#ifndef WOW_DRIVER_H_
#define WOW_DRIVER_H_

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

// Windows Win32 occupancy .sys (wowwin32.sys).
// Viewer occupies DRIVER_OBJECT, DEVICE_OBJECT, and IRP_MJ dispatch.
// Host ntoskrnl.exe / win32k.sys stay. Occupancy Calls NtLoadDriver
// and NtDeviceIoControlFile through wasmwin32_call when those names
// are reachable. A live backend may refuse. Occupancy does not
// pre-block those hops.

enum class DriverKind {
  kOccupancy = WOW_DRIVER_OCCUPANCY,
  kWin32k = WOW_DRIVER_WIN32K,
  kNtoskrnl = WOW_DRIVER_NTOSKRNL,
};

struct SysImage {
  int id = 0;
  DriverKind kind = DriverKind::kOccupancy;
  Isolation isolation = Isolation::kSys;
  std::string image;
  std::string nt_path;
  std::string nt_device;
  bool loaded = false;
  bool attached = false;
  bool started = false;
  bool mapped = false;
  uint64_t ioctls = 0;
  int channel_id = 0;

  const char* KindName() const;
  std::string ToJson() const;
};

class DriverTable {
 public:
  SysImage Load(DriverKind kind, std::string_view image,
                std::string_view nt_device);
  std::optional<SysImage> Find(int id) const;
  SysImage* FindMutable(int id);
  SysImage* OfKind(DriverKind kind);
  const SysImage* OfKind(DriverKind kind) const;
  std::vector<SysImage> All() const;
  bool Drop(int id);
  void Clear();
  size_t size() const { return images_.size(); }

 private:
  std::vector<SysImage> images_;
  int next_id_ = 1;
};

struct DeviceObject {
  std::string nt_name = "\\Device\\WowWin32";
  std::string dos_link = "\\DosDevices\\WowWin32";
  uint32_t device_type = WOW_FILE_DEVICE_UNKNOWN;
  uint32_t flags = 0;
  bool attached = false;
  bool started = false;
  uint64_t irps = 0;
  uint64_t ioctls = 0;

  std::string ToJson() const;
};

struct DriverObject {
  std::string name = "\\Driver\\WowWin32";
  std::string image = "wowwin32.sys";
  std::string registry_path =
      "\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WowWin32";
  bool major_function[WOW_IRP_MJ_COUNT] = {};
  bool entered = false;
  bool unload_set = false;
  uint32_t service_type = WOW_SERVICE_KERNEL_DRIVER;
  DeviceObject device;

  std::string ToJson() const;
};

struct Irp {
  uint8_t major = 0;
  uint8_t minor = 0;
  uint32_t ioctl = 0;
  uint32_t status = 0;
  uint64_t information = 0;
};

class Driver {
 public:
  DriverTable& table() { return table_; }
  const DriverTable& table() const { return table_; }
  EventRouter& events() { return events_; }
  const EventRouter& events() const { return events_; }
  const DriverObject& object() const { return object_; }
  const DeviceObject& device() const { return object_.device; }
  const Irp& last_irp() const { return last_irp_; }

  bool replaces_host_ntos() const { return false; }
  bool loaded() const { return object_.entered; }
  bool started() const { return object_.device.started; }

  const std::string& last_error() const { return last_error_; }

  WowResult DriverEntry(std::string_view label = "wowwin32");
  WowResult LoadImage(DriverKind kind, std::string_view image,
                      std::string_view nt_device);
  WowResult AddDevice();
  WowResult StartDevice();
  WowResult DeviceControl(uint32_t ioctl,
                          std::span<const uint8_t> bytes = {});
  WowResult Unload(int id);
  WowResult GetDriverInfo(std::vector<SysImage>* out) const;
  std::string GetDriverInfoJson() const;
  void Clear();

 private:
  WowResult DispatchIrp(uint8_t major, uint8_t minor, uint32_t ioctl,
                        std::span<const uint8_t> bytes);
  void OccupyMajor(uint8_t mj);
  void NoteLoadImage(const SysImage& image);
  void NoteUnload(const SysImage& image);

  DriverTable table_;
  DriverObject object_;
  EventRouter events_;
  Irp last_irp_;
  std::string last_error_;
};

const char* DriverKindName(DriverKind k);

}  // namespace wow

#endif  // WOW_DRIVER_H_
