#ifndef WOW_EVENT_H_
#define WOW_EVENT_H_

#include "wow/store.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace wow {

inline constexpr char kOnMapped[] = "win32.occupyVmem";
inline constexpr char kOnPipe[] = "win32.occupyPipe";
inline constexpr char kOnHwnd[] = "win32.occupyHwnd";
inline constexpr char kOnLoadImage[] = "win32.loadImage";
inline constexpr char kOnDriverEntry[] = "win32.driverEntry";
inline constexpr char kOnAddDevice[] = "win32.addDevice";
inline constexpr char kOnStartDevice[] = "win32.startDevice";
inline constexpr char kOnDeviceControl[] = "win32.deviceControl";
inline constexpr char kOnDriverUnload[] = "win32.driverUnload";

enum class EventKind {
  kMapped,
  kPipe,
  kHwnd,
  kLoadImage,
  kDriverEntry,
  kAddDevice,
  kStartDevice,
  kDeviceControl,
  kDriverUnload,
};

struct ChangeInfo {
  EventKind kind = EventKind::kMapped;
  std::string name = kOnMapped;
  std::string json = "{}";
  int channel_id = 0;
  std::string ToJson() const;
};

using EventSink = std::function<void(const ChangeInfo&)>;

class EventRouter {
 public:
  void set_sink(EventSink sink) { sink_ = std::move(sink); }

  void FireMapped(const Channel& buffer);
  void FirePipe(const Channel& pipe);
  void FireHwnd(const Channel& hwnd);
  void FireOnLoadImage(int id, std::string json);
  void FireOnDriverEntry(int id, std::string json);
  void FireOnAddDevice(int id, std::string json);
  void FireOnStartDevice(int id, std::string json);
  void FireOnDeviceControl(int id, std::string json);
  void FireOnDriverUnload(int id, std::string json);

  const std::vector<ChangeInfo>& history() const { return history_; }
  std::string LastJson() const;
  void Clear();

 private:
  void Emit(ChangeInfo ev);

  EventSink sink_;
  std::vector<ChangeInfo> history_;
};

}  // namespace wow

#endif  // WOW_EVENT_H_
