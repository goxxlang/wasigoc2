#ifndef WASMGOCOS_INCLUDE_GOCOS_FILES_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_FILES_HPP_

// The kernel's files as a guest sees them. A path is walked the way the
// session walks it -- binds, union mounts, DOS names, devices -- and lands
// on the origin filesystem or on a device. Origin files are whole-file,
// binary-safe reads and writes (wasmturbo::Vfs); a path under a
// container, `/ctr/<name>/<region>`, grows that container's object the
// way a session write does. A device is text: `#e`, `#p`, `#s`, `#u`,
// `#k`, `#g`, and `#t` read as a snapshot, and only `#c/cons`, `#e/<name>`,
// `#n/nul`, and the vGPU's `#g/ctl`, `#g/prog`, and `#g/buf/<name>` take
// writes.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace wasmgocos {
namespace files {

enum class Kind { kNone, kFile, kDir, kDevice };

struct Stat {
  Kind kind = Kind::kNone;
  uint64_t size = 0;
  int64_t mtime = 0;  // ns since the Unix epoch; 0 when there is none
};

// `absolute` must start with `/` or `#`. The result is an origin path or a
// device path.
std::string Walk(const std::string& absolute);
Stat StatOf(const std::string& walked);
bool Read(const std::string& walked, std::string* out);
bool Write(const std::string& walked, const std::string& bytes);
bool List(const std::string& walked, std::vector<std::pair<std::string, bool>>* entries);
bool MakeDir(const std::string& walked);
bool Remove(const std::string& walked, bool dir);
bool Rename(const std::string& from, const std::string& to);
// An origin entry's time, set (ns since the Unix epoch); false for a
// device or a path with no entry.
bool SetTime(const std::string& walked, int64_t mtime);
bool IsDevice(const std::string& walked);
bool DeviceWritable(const std::string& walked);

}  // namespace files
}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_FILES_HPP_
