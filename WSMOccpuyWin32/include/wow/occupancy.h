#ifndef WOW_OCCUPANCY_H_
#define WOW_OCCUPANCY_H_

#include "wow/c/types.h"
#include "wow/driver.h"
#include "wow/event.h"
#include "wow/posix.h"
#include "wow/store.h"
#include "wow/tty.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wow {

// Viewer-side Win32 occupancy. Every catalog DLL / namespace is a
// channel into kernel32 / ntdll / HWND / mapped vmem / WinHv / WSL.
// Isolation is thin there. Occupancy fires the catalog; a live backend
// may still refuse. Occupancy does not pre-block its own hops.
class Occupancy {
 public:
  Occupancy();
  ~Occupancy() = default;
  Occupancy(const Occupancy&) = delete;
  Occupancy& operator=(const Occupancy&) = delete;

  OccupancyTable& table() { return table_; }
  const OccupancyTable& table() const { return table_; }
  EventRouter& events() { return events_; }
  const EventRouter& events() const { return events_; }

  bool win32() const { return win32_; }
  bool catalog() const { return catalog_; }
  bool kernel32() const { return kernel32_; }
  bool ntdll() const { return ntdll_; }
  bool user32() const { return user32_; }
  bool gdi32() const { return gdi32_; }
  bool ole32() const { return ole32_; }
  bool advapi32() const { return advapi32_; }
  bool sockets() const { return sockets_; }
  bool bcrypt() const { return bcrypt_; }
  bool vmem() const { return vmem_; }
  bool pipe() const { return pipe_; }
  bool pe() const { return pe_; }
  bool hv() const { return hv_; }
  bool wsl() const { return wsl_; }
  bool console() const { return console_; }
  bool hwnd() const { return hwnd_; }
  bool sys() const { return sys_; }
  bool cmd() const { return cmd_; }
  bool linux() const { return linux_; }
  bool wasmtty() const { return wasmtty_; }
  bool gocvm() const { return gocvm_; }
  bool net() const { return net_; }
  int calc_pid() const { return calc_pid_; }
  int depth() const { return depth_; }
  int catalog_dlls() const { return catalog_dlls_; }
  int catalog_rows() const { return catalog_rows_; }
  bool Called(std::string_view api) const;
  const std::vector<std::string>& called() const { return called_; }
  const std::string& privileges() const { return privileges_; }
  const std::string& privilege_lookup() const { return privilege_name_; }

  Driver& win32_driver() { return driver_; }
  const Driver& win32_driver() const { return driver_; }
  PosixModule& posix() { return posix_; }
  const PosixModule& posix() const { return posix_; }

  const std::string& last_error() const { return last_error_; }

  WowResult OccupyWin32(std::string_view label = "win32");
  WowResult OccupyCatalog(std::string_view label = "catalog");
  WowResult OccupyDll(std::string_view dll, std::string_view api = {});
  WowResult OccupyKernel32(std::string_view label = "kernel32");
  WowResult OccupyNtdll(std::string_view label = "ntdll");
  WowResult OccupyUser32(std::string_view label = "user32");
  WowResult OccupyGdi32(std::string_view label = "gdi32");
  WowResult OccupyOle32(std::string_view label = "ole32");
  WowResult OccupyCom(std::string_view label = "ole32");
  WowResult OccupyAdvapi32(std::string_view label = "advapi32");
  WowResult OccupyToken(std::string_view label = "token");
  WowResult OccupyRegistry(std::string_view label = "registry");
  WowResult OccupyScm(std::string_view label = "scm");
  WowResult OccupyDevice(std::string_view label = "WowWin32");
  WowResult OccupySockets(std::string_view label = "ws2_32");
  WowResult OccupyBcrypt(std::string_view label = "bcrypt");
  WowResult OccupyVmem(std::string_view label = "vmem");
  WowResult OccupyPipe(std::string_view label = "pipe");
  WowResult OccupyPe(std::string_view label = "pe");
  WowResult OccupyProcess(std::string_view label = "process");
  WowResult OccupyConsole(std::string_view label = "console");
  WowResult OccupyHwnd(std::string_view label = "hwnd");
  WowResult OccupyHv(std::string_view label = "winhv");
  WowResult OccupyWsl(std::string_view label = "wsl");
  WowResult OccupyNix(std::string_view label = "nix");
  WowResult OccupySys(std::string_view label = "sys");
  WowResult OccupyDriver(std::string_view label = "wowwin32");
  WowResult OccupyLoadDriver(std::string_view label = "NtLoadDriver");
  WowResult OccupyWin32k(std::string_view label = "win32k");
  WowResult OccupyHostNtos(std::string_view label = "ntoskrnl");
  WowResult OccupyCmd(std::string_view label = "cmd.exe");
  WowResult OccupyCalc(std::string_view label = "calc.exe");
  WowResult OccupyLinux(std::string_view label = "linux");
  WowResult OccupySh(std::string_view label = "/bin/sh");
  WowResult OccupyWasmtty(std::string_view label = "wasmtty");
  WowResult OccupyGocvm(std::string_view label = "wasigocvm");
  WowResult OccupyNet(std::string_view label = "wns");
  WowResult TtyWriteHello(std::string* reply = nullptr);
  WowResult TtyPing(std::string* reply = nullptr);
  std::string CommonRoutesJson() const;
  std::string GetDriverInfoJson() const { return driver_.GetDriverInfoJson(); }

  WowResult Open(SurfaceKind kind, std::string_view label, int parent,
                 int* out_id);
  WowResult Write(int id, std::span<const uint8_t> bytes);
  WowResult Read(int id, std::vector<uint8_t>* out);
  WowResult Map(int id);
  WowResult Unmap(int id);

  std::string StatusJson() const;
  std::string ThinMapJson() const { return wow::ThinMapJson(); }

  bool DispatchTopic(std::string_view topic, std::string_view payload,
                     std::string* reply_out, std::string* err_out);

  bool CallWin32(std::string_view api, std::string_view args,
                 std::string* reply_out, std::string* err_out);

 private:
  WowResult NeedLive(int id, Channel** ch);
  Channel* Stamp(SurfaceKind kind, std::string_view label, int parent = 0);
  Channel* EnsureKind(SurfaceKind kind, std::string_view label);
  WowResult Probe(SurfaceKind kind, std::string_view label, const char* api,
                  const char* args, bool mapped);
  void ProbeImpersonatedScm(std::string_view tag);
  void ProbeEnableSeLoadDriver(std::string_view tag);
  void NoteCall(const char* api);
  WowResult EnsureDriverPermission();
  WowResult EnsureLinuxPermission();
  WowResult OccupyShellImage(SurfaceKind kind, std::string_view image);
  WowResult EnsureGocvmToolkit();
  WowResult LaunchWin32(std::string_view image);
  void WireTty();
  std::string TtyHelloJson() const;
  std::string ChannelReply(const Channel& ch) const;

  OccupancyTable table_;
  EventRouter events_;
  Driver driver_;
  PosixModule posix_;
  TtySession tty_;
  std::vector<uint8_t> tty_out_;
  std::string last_error_;
  bool win32_ = false;
  bool catalog_ = false;
  bool kernel32_ = false;
  bool ntdll_ = false;
  bool user32_ = false;
  bool gdi32_ = false;
  bool ole32_ = false;
  bool advapi32_ = false;
  bool sockets_ = false;
  bool bcrypt_ = false;
  bool vmem_ = false;
  bool pipe_ = false;
  bool pe_ = false;
  bool hv_ = false;
  bool wsl_ = false;
  bool console_ = false;
  bool hwnd_ = false;
  bool sys_ = false;
  bool win32k_ = false;
  bool host_ntos_ = false;
  bool cmd_ = false;
  bool sh_ = false;
  bool linux_ = false;
  bool wasmtty_ = false;
  bool gocvm_ = false;
  bool conpty_ = false;
  bool pts_ = false;
  bool net_ = false;
  int calc_pid_ = 0;
  int depth_ = 0;
  int catalog_dlls_ = 0;
  int catalog_rows_ = 0;
  std::vector<std::string> called_;
  std::string last_reply_;
  std::string privileges_;
  std::string privilege_name_;
  std::string registry_services_;
  std::string registry_wow_;
  std::string registry_create_;
  std::string device_;
  std::string scm_;
  std::string scm_connect_;
  std::string scm_enum_;
  std::string scm_all_;
  std::string scm_eventlog_;
  std::string scm_wow_;
  std::string scm_create_;
  std::string scm_status_;
  std::string token_handle_;
  std::string token_dup_;
  std::string token_elevation_type_;
  std::string token_linked_;
  std::string token_elevation_;
  std::string token_admin_member_;
  std::string impersonate_;
  std::string thread_token_;
  std::string set_thread_token_;
  std::string revert_;
  std::string adjust_process_;
  std::string adjust_dup_;
  std::string nt_adjust_;
  std::string adjust_thread_;
  std::string rtl_thread_;
  std::string com_instance_;
  std::string com_scm_;
  std::string anonymous_;
  std::string admin_impersonate_;
  std::string admin_member_;
  std::string admin_privileges_;
};

}  // namespace wow

#endif  // WOW_OCCUPANCY_H_
