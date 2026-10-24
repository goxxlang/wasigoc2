#include "gocos/files.hpp"

#include "gocos/jitrt.hpp"
#include "src/runtime/vfs.h"

namespace wasmgocos {
namespace files {
namespace {

wasmturbo::Vfs& Origin() { return wasmturbo::Vfs::Origin(); }
uint32_t Pid() { return KernelPid(); }

}  // namespace

std::string Walk(const std::string& absolute) { return jit_walk(absolute); }

bool IsDevice(const std::string& walked) { return !walked.empty() && walked[0] == '#'; }

bool DeviceWritable(const std::string& walked) {
  return walked == "#c/cons" || walked == "#n/nul" ||
         (walked.rfind("#e/", 0) == 0 && walked.size() > 3 && walked.find('/', 3) == std::string::npos) ||
         walked == "#g/ctl" || walked == "#g/prog" || (walked.rfind("#g/buf/", 0) == 0 && walked.size() > 7);
}

Stat StatOf(const std::string& walked) {
  Stat st;
  if (IsDevice(walked)) {
    std::string body;
    if (jit_dev_read(walked, &body)) {
      st.kind = Kind::kDevice;
      st.size = body.size();
    }
    return st;
  }
  uint64_t len = 0;
  switch (Origin().Stat(ProcessRuntime(), Pid(), walked, &len, &st.mtime)) {
    case wasmturbo::Vfs::Kind::kFile:
      st.kind = Kind::kFile;
      st.size = len;
      break;
    case wasmturbo::Vfs::Kind::kDir:
      st.kind = Kind::kDir;
      break;
    case wasmturbo::Vfs::Kind::kNone:
      break;
  }
  return st;
}

bool Read(const std::string& walked, std::string* out) {
  if (IsDevice(walked)) return jit_dev_read(walked, out);
  return Origin().ReadFile(ProcessRuntime(), Pid(), walked, out);
}

bool Write(const std::string& walked, const std::string& bytes) {
  if (IsDevice(walked)) return DeviceWritable(walked) && jit_dev_write(walked, bytes);
  // A container region of a different length grows that container.
  if (!bytes.empty()) {
    std::string grew = jit_ctr_grow(walked.c_str(), bytes.c_str());
    if (grew.rfind("write ", 0) == 0) return true;
    if (!grew.empty()) return false;
  }
  return Origin().WriteFile(ProcessRuntime(), Pid(), walked, bytes);
}

bool List(const std::string& walked, std::vector<std::pair<std::string, bool>>* entries) {
  if (IsDevice(walked)) return false;
  return Origin().List(ProcessRuntime(), Pid(), walked, entries);
}

bool MakeDir(const std::string& walked) {
  return !IsDevice(walked) && Origin().MakeDir(ProcessRuntime(), Pid(), walked);
}

bool Remove(const std::string& walked, bool dir) {
  return !IsDevice(walked) && Origin().Remove(ProcessRuntime(), Pid(), walked, dir);
}

bool Rename(const std::string& from, const std::string& to) {
  if (IsDevice(from) || IsDevice(to)) return false;
  return Origin().Rename(ProcessRuntime(), Pid(), from, to);
}

bool SetTime(const std::string& walked, int64_t mtime) {
  return !IsDevice(walked) && Origin().SetTime(ProcessRuntime(), Pid(), walked, mtime);
}

}  // namespace files
}  // namespace wasmgocos
