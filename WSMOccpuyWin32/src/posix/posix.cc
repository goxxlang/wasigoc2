#include "wow/posix.h"

namespace wow {
namespace {

const char* NameOf(KoKind k) {
  switch (k) {
    case KoKind::kOccupancy:
      return "occupancy";
    case KoKind::kNtfs:
      return "ntfs";
  }
  return "other";
}

}  // namespace

const char* KoKindName(KoKind k) { return NameOf(k); }

const char* KoImage::KindName() const { return NameOf(kind); }

std::string KoImage::ToJson() const {
  std::string o = "{\"id\":";
  o += std::to_string(id);
  o += ",\"kind\":";
  o += JsonEscape(KindName());
  o += ",\"image\":";
  o += JsonEscape(image);
  o += ",\"devnode\":";
  o += JsonEscape(devnode);
  o += ",\"isolation\":";
  o += JsonEscape(IsolationToString(isolation));
  o += ",\"loaded\":";
  o += loaded ? "true" : "false";
  o += ",\"attached\":";
  o += attached ? "true" : "false";
  o += ",\"ioctls\":";
  o += std::to_string(ioctls);
  o += ",\"thin\":true}";
  return o;
}

KoImage KoTable::Load(KoKind kind, std::string_view image,
                      std::string_view devnode) {
  KoImage g;
  g.id = next_id_++;
  g.kind = kind;
  g.isolation = Isolation::kSys;
  g.image =
      image.empty() ? std::string(NameOf(kind)) + ".ko" : std::string(image);
  g.devnode = devnode.empty() ? "/dev/wowwin32" : std::string(devnode);
  g.loaded = true;
  images_.push_back(g);
  return images_.back();
}

std::optional<KoImage> KoTable::Find(int id) const {
  for (const auto& g : images_) {
    if (g.id == id) {
      return g;
    }
  }
  return std::nullopt;
}

KoImage* KoTable::FindMutable(int id) {
  for (auto& g : images_) {
    if (g.id == id) {
      return &g;
    }
  }
  return nullptr;
}

KoImage* KoTable::OfKind(KoKind kind) {
  for (auto& g : images_) {
    if (g.kind == kind) {
      return &g;
    }
  }
  return nullptr;
}

const KoImage* KoTable::OfKind(KoKind kind) const {
  for (const auto& g : images_) {
    if (g.kind == kind) {
      return &g;
    }
  }
  return nullptr;
}

std::vector<KoImage> KoTable::All() const { return images_; }

bool KoTable::Drop(int id) {
  for (auto it = images_.begin(); it != images_.end(); ++it) {
    if (it->id == id) {
      images_.erase(it);
      return true;
    }
  }
  return false;
}

void KoTable::Clear() {
  images_.clear();
  next_id_ = 1;
}

void PosixModule::Clear() {
  table_.Clear();
  entered_ = false;
  devnode_ = "/dev/wowwin32";
  last_error_.clear();
}

WowResult PosixModule::LoadImage(KoKind kind, std::string_view image,
                                 std::string_view devnode) {
  table_.Load(kind, image, devnode);
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult PosixModule::ModuleInit(std::string_view label) {
  Clear();
  std::string occ = label.empty() ? "wowwin32.ko" : std::string(label);
  if (occ.find(".ko") == std::string::npos &&
      occ.find(".sys") == std::string::npos) {
    occ += ".ko";
  }
  if (occ.find(".sys") != std::string::npos) {
    auto p = occ.find(".sys");
    occ.replace(p, 4, ".ko");
  }
  LoadImage(KoKind::kOccupancy, occ, "/dev/wowwin32");
  LoadImage(KoKind::kNtfs, "ntfs.ko", "/dev/ntfs");
  entered_ = true;
  last_error_.clear();
  return AddDevnode();
}

WowResult PosixModule::AddDevnode() {
  if (!entered_) {
    WowResult r = ModuleInit("wowwin32");
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  if (KoImage* occ = table_.OfKind(KoKind::kOccupancy)) {
    occ->attached = true;
    occ->devnode = devnode_;
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult PosixModule::Ioctl(uint32_t request, std::span<const uint8_t> bytes) {
  if (!entered_) {
    WowResult r = AddDevnode();
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  KoImage* occ = table_.OfKind(KoKind::kOccupancy);
  if (!occ) {
    last_error_ = "occupancy .ko not loaded";
    return WOW_RESULT_NOT_FOUND;
  }
  occ->ioctls += 1;
  occ->attached = true;
  if (request == WOW_IOCTL_EXEC || request == WOW_IOCTL_TTY ||
      request == WOW_IOCTL_MAP || request == 0) {
    if (KoImage* ntfs = table_.OfKind(KoKind::kNtfs)) {
      ntfs->ioctls += 1;
    }
  }
  (void)bytes;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult PosixModule::Unload(int id) {
  KoImage* g = table_.FindMutable(id);
  if (!g) {
    last_error_ = "ko image not found";
    return WOW_RESULT_NOT_FOUND;
  }
  if (g->kind == KoKind::kNtfs) {
    last_error_ = "host ntfs.ko stays";
    return WOW_RESULT_FAILED_PRECONDITION;
  }
  table_.Drop(id);
  if (!table_.OfKind(KoKind::kOccupancy)) {
    entered_ = false;
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

std::string PosixModule::GetInfoJson() const {
  auto all = table_.All();
  std::string o = "{\"replaces_host_ntfs\":false,\"ko\":true,\"loaded\":";
  o += entered_ ? "true" : "false";
  o += ",\"devnode\":";
  o += JsonEscape(devnode_);
  o += ",\"isolation\":\"sys\",\"images\":[";
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
