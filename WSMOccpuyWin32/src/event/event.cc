#include "wow/event.h"

namespace wow {

std::string ChangeInfo::ToJson() const {
  std::string o = "{\"name\":";
  o += JsonEscape(name);
  o += ",\"channel\":";
  o += std::to_string(channel_id);
  o += ",\"body\":";
  o += json.empty() ? std::string("{}") : json;
  o += '}';
  return o;
}

void EventRouter::Emit(ChangeInfo ev) {
  history_.push_back(ev);
  if (sink_) {
    sink_(history_.back());
  }
}

void EventRouter::FireMapped(const Channel& buffer) {
  ChangeInfo ev;
  ev.kind = EventKind::kMapped;
  ev.name = kOnMapped;
  ev.channel_id = buffer.id;
  ev.json = buffer.ToJson();
  Emit(std::move(ev));
}

void EventRouter::FirePipe(const Channel& pipe) {
  ChangeInfo ev;
  ev.kind = EventKind::kPipe;
  ev.name = kOnPipe;
  ev.channel_id = pipe.id;
  ev.json = pipe.ToJson();
  Emit(std::move(ev));
}

void EventRouter::FireHwnd(const Channel& hwnd) {
  ChangeInfo ev;
  ev.kind = EventKind::kHwnd;
  ev.name = kOnHwnd;
  ev.channel_id = hwnd.id;
  ev.json = hwnd.ToJson();
  Emit(std::move(ev));
}

void EventRouter::FireOnLoadImage(int id, std::string json) {
  ChangeInfo ev;
  ev.kind = EventKind::kLoadImage;
  ev.name = kOnLoadImage;
  ev.channel_id = id;
  ev.json = std::move(json);
  Emit(std::move(ev));
}

void EventRouter::FireOnDriverEntry(int id, std::string json) {
  ChangeInfo ev;
  ev.kind = EventKind::kDriverEntry;
  ev.name = kOnDriverEntry;
  ev.channel_id = id;
  ev.json = std::move(json);
  Emit(std::move(ev));
}

void EventRouter::FireOnAddDevice(int id, std::string json) {
  ChangeInfo ev;
  ev.kind = EventKind::kAddDevice;
  ev.name = kOnAddDevice;
  ev.channel_id = id;
  ev.json = std::move(json);
  Emit(std::move(ev));
}

void EventRouter::FireOnStartDevice(int id, std::string json) {
  ChangeInfo ev;
  ev.kind = EventKind::kStartDevice;
  ev.name = kOnStartDevice;
  ev.channel_id = id;
  ev.json = std::move(json);
  Emit(std::move(ev));
}

void EventRouter::FireOnDeviceControl(int id, std::string json) {
  ChangeInfo ev;
  ev.kind = EventKind::kDeviceControl;
  ev.name = kOnDeviceControl;
  ev.channel_id = id;
  ev.json = std::move(json);
  Emit(std::move(ev));
}

void EventRouter::FireOnDriverUnload(int id, std::string json) {
  ChangeInfo ev;
  ev.kind = EventKind::kDriverUnload;
  ev.name = kOnDriverUnload;
  ev.channel_id = id;
  ev.json = std::move(json);
  Emit(std::move(ev));
}

std::string EventRouter::LastJson() const {
  if (history_.empty()) {
    return "{}";
  }
  return history_.back().ToJson();
}

void EventRouter::Clear() { history_.clear(); }

}  // namespace wow
