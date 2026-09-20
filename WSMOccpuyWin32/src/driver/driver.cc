#include "wow/driver.h"

namespace wow {
namespace {

const char* NameOf(DriverKind k) {
  switch (k) {
    case DriverKind::kOccupancy:
      return "occupancy";
    case DriverKind::kWin32k:
      return "win32k";
    case DriverKind::kNtoskrnl:
      return "ntoskrnl";
  }
  return "other";
}

std::string DriversDir(std::string_view image) {
  std::string o = "\\SystemRoot\\System32\\drivers\\";
  o += image;
  return o;
}

}  // namespace

const char* DriverKindName(DriverKind k) { return NameOf(k); }

const char* SysImage::KindName() const { return NameOf(kind); }

std::string SysImage::ToJson() const {
  std::string o = "{\"id\":";
  o += std::to_string(id);
  o += ",\"kind\":";
  o += JsonEscape(KindName());
  o += ",\"image\":";
  o += JsonEscape(image);
  o += ",\"nt_path\":";
  o += JsonEscape(nt_path);
  o += ",\"nt_device\":";
  o += JsonEscape(nt_device);
  o += ",\"isolation\":";
  o += JsonEscape(IsolationToString(isolation));
  o += ",\"loaded\":";
  o += loaded ? "true" : "false";
  o += ",\"attached\":";
  o += attached ? "true" : "false";
  o += ",\"started\":";
  o += started ? "true" : "false";
  o += ",\"mapped\":";
  o += mapped ? "true" : "false";
  o += ",\"ioctls\":";
  o += std::to_string(ioctls);
  o += ",\"thin\":true}";
  return o;
}

std::string DeviceObject::ToJson() const {
  std::string o = "{\"nt_name\":";
  o += JsonEscape(nt_name);
  o += ",\"dos_link\":";
  o += JsonEscape(dos_link);
  o += ",\"device_type\":";
  o += std::to_string(device_type);
  o += ",\"flags\":";
  o += std::to_string(flags);
  o += ",\"attached\":";
  o += attached ? "true" : "false";
  o += ",\"started\":";
  o += started ? "true" : "false";
  o += ",\"irps\":";
  o += std::to_string(irps);
  o += ",\"ioctls\":";
  o += std::to_string(ioctls);
  o += ",\"isolation\":\"sys\"}";
  return o;
}

std::string DriverObject::ToJson() const {
  std::string o = "{\"name\":";
  o += JsonEscape(name);
  o += ",\"image\":";
  o += JsonEscape(image);
  o += ",\"registry_path\":";
  o += JsonEscape(registry_path);
  o += ",\"service_type\":";
  o += std::to_string(service_type);
  o += ",\"entered\":";
  o += entered ? "true" : "false";
  o += ",\"unload_set\":";
  o += unload_set ? "true" : "false";
  o += ",\"major_function\":{";
  o += "\"irp_mj_create\":";
  o += major_function[WOW_IRP_MJ_CREATE] ? "true" : "false";
  o += ",\"irp_mj_close\":";
  o += major_function[WOW_IRP_MJ_CLOSE] ? "true" : "false";
  o += ",\"irp_mj_device_control\":";
  o += major_function[WOW_IRP_MJ_DEVICE_CONTROL] ? "true" : "false";
  o += ",\"irp_mj_internal_device_control\":";
  o += major_function[WOW_IRP_MJ_INTERNAL_DEVICE_CONTROL] ? "true" : "false";
  o += ",\"irp_mj_pnp\":";
  o += major_function[WOW_IRP_MJ_PNP] ? "true" : "false";
  o += "},\"device\":";
  o += device.ToJson();
  o += '}';
  return o;
}

SysImage DriverTable::Load(DriverKind kind, std::string_view image,
                           std::string_view nt_device) {
  SysImage g;
  g.id = next_id_++;
  g.kind = kind;
  g.isolation =
      kind == DriverKind::kOccupancy ? Isolation::kSys : Isolation::kHost;
  g.image =
      image.empty() ? std::string(NameOf(kind)) + ".sys" : std::string(image);
  g.nt_path = DriversDir(g.image);
  g.nt_device =
      nt_device.empty() ? "\\Device\\WowWin32" : std::string(nt_device);
  g.loaded = true;
  images_.push_back(g);
  return images_.back();
}

std::optional<SysImage> DriverTable::Find(int id) const {
  for (const auto& g : images_) {
    if (g.id == id) {
      return g;
    }
  }
  return std::nullopt;
}

SysImage* DriverTable::FindMutable(int id) {
  for (auto& g : images_) {
    if (g.id == id) {
      return &g;
    }
  }
  return nullptr;
}

SysImage* DriverTable::OfKind(DriverKind kind) {
  for (auto& g : images_) {
    if (g.kind == kind) {
      return &g;
    }
  }
  return nullptr;
}

const SysImage* DriverTable::OfKind(DriverKind kind) const {
  for (const auto& g : images_) {
    if (g.kind == kind) {
      return &g;
    }
  }
  return nullptr;
}

std::vector<SysImage> DriverTable::All() const { return images_; }

bool DriverTable::Drop(int id) {
  for (auto it = images_.begin(); it != images_.end(); ++it) {
    if (it->id == id) {
      images_.erase(it);
      return true;
    }
  }
  return false;
}

void DriverTable::Clear() {
  images_.clear();
  next_id_ = 1;
}

void Driver::OccupyMajor(uint8_t mj) {
  if (mj < WOW_IRP_MJ_COUNT) {
    object_.major_function[mj] = true;
  }
}

void Driver::NoteLoadImage(const SysImage& image) {
  events_.FireOnLoadImage(image.id, image.ToJson());
}

void Driver::NoteUnload(const SysImage& image) {
  events_.FireOnDriverUnload(image.id, image.ToJson());
}

void Driver::Clear() {
  table_.Clear();
  object_ = DriverObject{};
  events_.Clear();
  last_irp_ = Irp{};
  last_error_.clear();
}

WowResult Driver::LoadImage(DriverKind kind, std::string_view image,
                            std::string_view nt_device) {
  SysImage g = table_.Load(kind, image, nt_device);
  NoteLoadImage(g);
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Driver::DriverEntry(std::string_view label) {
  Clear();
  std::string occ = label.empty() ? "wowwin32.sys" : std::string(label);
  if (occ.find(".sys") == std::string::npos &&
      occ.find(".exe") == std::string::npos) {
    occ += ".sys";
  }
  object_.image = occ;
  object_.name = "\\Driver\\WowWin32";
  object_.registry_path =
      "\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WowWin32";
  object_.service_type = WOW_SERVICE_KERNEL_DRIVER;
  object_.unload_set = true;
  OccupyMajor(WOW_IRP_MJ_CREATE);
  OccupyMajor(WOW_IRP_MJ_CLOSE);
  OccupyMajor(WOW_IRP_MJ_DEVICE_CONTROL);
  OccupyMajor(WOW_IRP_MJ_INTERNAL_DEVICE_CONTROL);
  OccupyMajor(WOW_IRP_MJ_PNP);
  LoadImage(DriverKind::kOccupancy, occ, "\\Device\\WowWin32");
  LoadImage(DriverKind::kWin32k, "win32k.sys", "\\Device\\Win32k");
  LoadImage(DriverKind::kNtoskrnl, "ntoskrnl.exe", "\\Device\\Ntoskrnl");
  object_.entered = true;
  events_.FireOnDriverEntry(0, object_.ToJson());
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Driver::AddDevice() {
  if (!object_.entered) {
    WowResult r = DriverEntry("wowwin32");
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  object_.device.nt_name = "\\Device\\WowWin32";
  object_.device.dos_link = "\\DosDevices\\WowWin32";
  object_.device.device_type = WOW_FILE_DEVICE_UNKNOWN;
  object_.device.flags = WOW_DO_BUFFERED_IO;
  object_.device.attached = true;
  if (SysImage* occ = table_.OfKind(DriverKind::kOccupancy)) {
    occ->attached = true;
    occ->nt_device = object_.device.nt_name;
  }
  events_.FireOnAddDevice(0, object_.device.ToJson());
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Driver::StartDevice() {
  if (!object_.device.attached) {
    WowResult r = AddDevice();
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  return DispatchIrp(WOW_IRP_MJ_PNP, WOW_IRP_MN_START_DEVICE, 0, {});
}

WowResult Driver::DeviceControl(uint32_t ioctl, std::span<const uint8_t> bytes) {
  if (!object_.device.started) {
    WowResult r = StartDevice();
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  return DispatchIrp(WOW_IRP_MJ_DEVICE_CONTROL, 0, ioctl, bytes);
}

WowResult Driver::DispatchIrp(uint8_t major, uint8_t minor, uint32_t ioctl,
                              std::span<const uint8_t> bytes) {
  if (major >= WOW_IRP_MJ_COUNT || !object_.major_function[major]) {
    last_error_ = "IRP_MJ not occupied";
    return WOW_RESULT_NOT_FOUND;
  }
  if (!object_.device.attached && major != WOW_IRP_MJ_PNP) {
    last_error_ = "DEVICE_OBJECT missing";
    return WOW_RESULT_FAILED_PRECONDITION;
  }
  last_irp_.major = major;
  last_irp_.minor = minor;
  last_irp_.ioctl = ioctl;
  last_irp_.status = 0;
  last_irp_.information = bytes.size();
  object_.device.irps += 1;

  if (major == WOW_IRP_MJ_PNP && minor == WOW_IRP_MN_START_DEVICE) {
    object_.device.started = true;
    if (SysImage* occ = table_.OfKind(DriverKind::kOccupancy)) {
      occ->attached = true;
      occ->started = true;
    }
    events_.FireOnStartDevice(0, object_.device.ToJson());
  }

  if (major == WOW_IRP_MJ_DEVICE_CONTROL) {
    SysImage* occ = table_.OfKind(DriverKind::kOccupancy);
    if (!occ) {
      last_error_ = "occupancy .sys not loaded";
      return WOW_RESULT_NOT_FOUND;
    }
    object_.device.ioctls += 1;
    occ->ioctls += 1;
    if (ioctl == WOW_IOCTL_MAP || ioctl == WOW_IOCTL_WIN32 || ioctl == 0) {
      occ->mapped = true;
      if (SysImage* ntos = table_.OfKind(DriverKind::kNtoskrnl)) {
        ntos->mapped = true;
        ntos->ioctls += 1;
      }
    }
    events_.FireOnDeviceControl(occ->id, occ->ToJson());
  }

  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Driver::Unload(int id) {
  SysImage* g = table_.FindMutable(id);
  if (!g) {
    last_error_ = "driver image not found";
    return WOW_RESULT_NOT_FOUND;
  }
  if (g->kind == DriverKind::kWin32k || g->kind == DriverKind::kNtoskrnl) {
    last_error_ = "host ntoskrnl stays";
    return WOW_RESULT_FAILED_PRECONDITION;
  }
  SysImage copy = *g;
  copy.loaded = false;
  copy.attached = false;
  copy.started = false;
  NoteUnload(copy);
  table_.Drop(id);
  if (!table_.OfKind(DriverKind::kOccupancy)) {
    object_.entered = false;
    object_.unload_set = false;
    object_.device = DeviceObject{};
    for (auto& slot : object_.major_function) {
      slot = false;
    }
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Driver::GetDriverInfo(std::vector<SysImage>* out) const {
  if (out) {
    *out = table_.All();
  }
  return WOW_RESULT_OK;
}

std::string Driver::GetDriverInfoJson() const {
  auto all = table_.All();
  std::string o = "{\"replaces_host_ntos\":false,\"sys\":true,";
  o += "\"service_type\":";
  o += std::to_string(WOW_SERVICE_KERNEL_DRIVER);
  o += ",\"loaded\":";
  o += object_.entered ? "true" : "false";
  o += ",\"started\":";
  o += object_.device.started ? "true" : "false";
  o += ",\"isolation\":\"sys\",\"driver_object\":";
  o += object_.ToJson();
  o += ",\"device_object\":";
  o += object_.device.ToJson();
  o += ",\"images\":[";
  bool first = true;
  for (const auto& g : all) {
    if (!first) {
      o += ',';
    }
    first = false;
    o += g.ToJson();
  }
  o += "]}";
  return o;
}

}  // namespace wow
