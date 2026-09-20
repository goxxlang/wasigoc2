#include "test.h"

#include "wow/occupancy.h"

#include <string>
#include <vector>

TEST(OccupyWin32BootsThinHop) {
  wow::Occupancy o;
  EXPECT(!o.win32());
  EXPECT_EQ(o.OccupyWin32("win32"), WOW_RESULT_OK);
  EXPECT(o.win32());
  EXPECT(o.catalog());
  EXPECT(o.kernel32());
  EXPECT(o.ntdll());
  EXPECT(o.user32());
  EXPECT(o.gdi32());
  EXPECT(o.ole32());
  EXPECT(o.advapi32());
  EXPECT(o.sockets());
  EXPECT(o.bcrypt());
  EXPECT(o.vmem());
  EXPECT(o.pipe());
  EXPECT(o.pe());
  EXPECT(o.hv());
  EXPECT(o.wsl());
  EXPECT(o.console());
  EXPECT(o.hwnd());
  EXPECT(o.catalog_dlls() > 1);
  EXPECT(o.catalog_rows() > 1);
  auto json = o.StatusJson();
  EXPECT(json.find("\"thin\":true") != std::string::npos);
  EXPECT(json.find("\"win32\":true") != std::string::npos);
}

TEST(DispatchWin32Topics) {
  wow::Occupancy o;
  std::string reply;
  std::string err;
  EXPECT(o.DispatchTopic("win32.occupyKernel32", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"host\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyNtdll", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"sys\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyVmem", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"host\"") != std::string::npos);
  EXPECT(reply.find("\"mapped\":true") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyPipe", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"ipc\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyUser32", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"compositor\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyHv", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"hv\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyNtdll", "", &reply, &err));
  EXPECT(o.Called("NtCreateFile"));
  EXPECT(o.Called("NtCreateSection"));
  EXPECT(o.Called("NtMapViewOfSection"));
  EXPECT(o.DispatchTopic("win32.occupyHwnd", "", &reply, &err));
  EXPECT(o.Called("CreateWindowExW"));
  EXPECT(o.DispatchTopic("win32.occupyVmem", "", &reply, &err));
  EXPECT(o.Called("VirtualAlloc"));
  EXPECT(o.Called("VirtualProtect"));
  EXPECT(o.DispatchTopic("win32.occupyToken", "", &reply, &err));
  EXPECT(o.Called("LookupPrivilegeValueW"));
  EXPECT(o.Called("LookupPrivilegeNameW"));
  EXPECT(o.Called("AdjustTokenPrivileges"));
  EXPECT(o.Called("RtlAdjustPrivilege"));
  EXPECT(o.Called("NtAdjustPrivilegesToken"));
  EXPECT(o.Called("GetTokenInformation"));
  EXPECT(reply.find("SeLoadDriverPrivilege") != std::string::npos);
  EXPECT(reply.find("SeChangeNotifyPrivilege") != std::string::npos);
  EXPECT(o.Called("DuplicateTokenEx"));
  EXPECT(o.Called("ImpersonateLoggedOnUser"));
  EXPECT(o.Called("OpenThreadToken"));
  EXPECT(o.Called("SetThreadToken"));
  EXPECT(o.Called("RevertToSelf"));
  EXPECT(o.Called("OpenSCManagerW"));
  EXPECT(o.Called("OpenServiceW"));
  EXPECT(o.Called("QueryServiceStatus"));
  EXPECT(reply.find("unknown api ImpersonateLoggedOnUser") == std::string::npos);
  EXPECT(reply.find("unknown api SetThreadToken") == std::string::npos);
  EXPECT(reply.find("unknown api RevertToSelf") == std::string::npos);
  EXPECT(reply.find("unknown api OpenSCManagerW") == std::string::npos);
  EXPECT(reply.find("scm_connect") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyCom", "", &reply, &err));
  EXPECT(o.Called("CoInitializeEx"));
  EXPECT(o.Called("CoCreateInstance"));
  EXPECT(o.Called("NtImpersonateAnonymousToken"));
  EXPECT(o.Called("ImpersonateLoggedOnUser"));
  EXPECT(o.Called("SetThreadToken"));
  EXPECT(o.Called("CheckTokenMembership"));
  EXPECT(reply.find("unknown api CoCreateInstance") == std::string::npos);
  EXPECT(reply.find("DuplicateTokenEx") != std::string::npos);
  EXPECT(reply.find("admin_impersonate") != std::string::npos);
  EXPECT(reply.find("S-1-5-32-544") != std::string::npos ||
         reply.find("admin_member") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyRegistry", "", &reply, &err));
  EXPECT(o.Called("RegOpenKeyExW"));
  EXPECT(o.Called("RegCreateKeyExW"));
  EXPECT(o.Called("NtOpenKey"));
  EXPECT(o.Called("NtCreateKey"));
  EXPECT(reply.find("CurrentControlSet") != std::string::npos);
  EXPECT(reply.find("WowWin32") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyScm", "", &reply, &err));
  EXPECT(o.Called("OpenSCManagerW"));
  EXPECT(o.Called("OpenServiceW"));
  EXPECT(o.Called("QueryServiceStatus"));
  EXPECT(o.Called("CreateServiceW"));
  EXPECT(reply.find("unknown api OpenSCManagerW") == std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyDevice", "", &reply, &err));
  EXPECT(o.Called("CreateFileW"));
  EXPECT(o.Called("NtOpenFile"));
  EXPECT(reply.find("WowWin32") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyLoadDriver", "", &reply, &err));
  EXPECT(o.Called("NtLoadDriver"));
  EXPECT(o.Called("ZwLoadDriver"));
  EXPECT(o.Called("GetProcAddress"));
  EXPECT(reply.find("NtLoadDriver") != std::string::npos);
  EXPECT(reply.find("unknown api NtLoadDriver") == std::string::npos);
  EXPECT(reply.find("RegCreateKeyExW") != std::string::npos);
  EXPECT(reply.find("CreateFileW") != std::string::npos);
#if defined(WOW_HAS_WIN32)
  std::string live;
  std::string live_err;
  EXPECT(o.CallWin32("NtLoadDriver", "", &live, &live_err));
  EXPECT(live.find("unknown api") == std::string::npos);
  EXPECT(o.CallWin32("ZwLoadDriver", "", &live, &live_err));
  EXPECT(live.find("unknown api") == std::string::npos);
  EXPECT(o.CallWin32("OpenSCManagerW", "", &live, &live_err));
  EXPECT(live.find("unknown api") == std::string::npos);
  EXPECT(o.CallWin32("OpenSCManagerW", "\x1f" "\x1f" "0x1", &live, &live_err));
  EXPECT(live.find("unknown api") == std::string::npos);
  EXPECT(o.CallWin32("OpenSCManagerW", "\x1f" "\x1f" "0x15", &live, &live_err));
  EXPECT(live.find("unknown api") == std::string::npos);
  o.CallWin32("OpenSCManagerW", "\x1f" "\x1f" "0xF003F", &live, &live_err);
  EXPECT(live.find("unknown api") == std::string::npos);
  o.CallWin32("ImpersonateLoggedOnUser", "", &live, &live_err);
  EXPECT(live.find("unknown api") == std::string::npos);
  EXPECT(o.CallWin32("SetThreadToken", "-2", &live, &live_err));
  EXPECT(live.find("unknown api") == std::string::npos);
  EXPECT(o.CallWin32("RevertToSelf", "", &live, &live_err));
  EXPECT(live.find("unknown api") == std::string::npos);
  o.CallWin32("OpenThreadToken", "-2", &live, &live_err);
  EXPECT(live.find("unknown api") == std::string::npos);
  EXPECT(o.CallWin32("CoCreateInstance",
                     "{00000320-0000-0000-C000-000000000046}", &live,
                     &live_err));
  EXPECT(live.find("unknown api") == std::string::npos);
#endif
  EXPECT(o.DispatchTopic("win32.occupySys", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"sys\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyCmd", "", &reply, &err));
  EXPECT(reply.find("\"permission\":\"sys\"") != std::string::npos);
  EXPECT(reply.find("cmd.exe") != std::string::npos);
  EXPECT(reply.find("\"isolation\":\"shell\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyGocvm", "", &reply, &err));
  EXPECT(reply.find("\"gocvm\":true") != std::string::npos);
  EXPECT(reply.find("\"toolkit\":\"gocvm\"") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.occupyCalc", "", &reply, &err));
  EXPECT(reply.find("calc.exe") != std::string::npos);
  EXPECT(reply.find("\"isolation\":\"shell\"") != std::string::npos);
  EXPECT(reply.find("\"win32\":\"CreateProcessW\"") != std::string::npos);
  EXPECT(reply.find("\"pid\":") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.thinMap", "", &reply, &err));
  EXPECT(reply.find("host") != std::string::npos);
  EXPECT(o.DispatchTopic("win32.commonRoutes", "", &reply, &err));
  EXPECT(reply.find("VirtualAlloc") != std::string::npos);
  EXPECT(reply.find("CreateProcessW") != std::string::npos);
  EXPECT(reply.find("NtLoadDriver") != std::string::npos);
  EXPECT(reply.find("OpenSCManagerW") != std::string::npos);
  EXPECT(reply.find("CoCreateInstance") != std::string::npos);
  EXPECT(reply.find("DuplicateTokenEx") != std::string::npos);
}

TEST(DoesNotStealSkiaMojoExec) {
  wow::Occupancy o;
  std::string reply;
  std::string err;
  EXPECT(!o.DispatchTopic("skia.occupySkia", "", &reply, &err));
  EXPECT(!o.DispatchTopic("mojo.occupyMojo", "", &reply, &err));
  EXPECT(!o.DispatchTopic("webgpu.occupyGpu", "", &reply, &err));
  EXPECT(!o.DispatchTopic("os.exec", "cmd.exe", &reply, &err));
  EXPECT(!o.DispatchTopic("v8.isolate", "", &reply, &err));
  EXPECT(!o.DispatchTopic("wst.createMessagePipe", "", &reply, &err));
  EXPECT(!o.DispatchTopic("tty.hello", "", &reply, &err));
  EXPECT(!o.DispatchTopic("net.call", "", &reply, &err));
  EXPECT(!o.DispatchTopic("vtpm.attest", "", &reply, &err));
}
