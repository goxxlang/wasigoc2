#include "win32/catalog.h"
#include "win32/dispatch.h"
#include "win32/pe_map.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main() {
  char buf[4096];
  int n = 0;
  const WasmWin32Api* cat = wasmwin32_catalog(&n);
  assert(wasmwin32_call("GetACP", "", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) != 0);

  assert(wasmwin32_call("GetUserNameW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetStdHandle", "-11", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetCurrentProcess", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetSystemInfo", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetSystemTime", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetEnvironmentStringsW", "", buf, sizeof(buf)) == 0);

  assert(n >= 220);
  bool saw_pid = false;
  bool saw_create = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].name, "GetCurrentProcessId") == 0) saw_pid = true;
    if (std::strcmp(cat[i].name, "CreateProcessW") == 0) saw_create = true;
  }
  assert(saw_pid);
  assert(saw_create);

  assert(wasmwin32_call("GetTickCount", "", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) != 0);

  {
#if defined(_WIN32)
    const char* cpargs = "cmd.exe\x1f" "cmd.exe /c exit 0";
#else
    const char* cpargs = "true";
#endif
    assert(wasmwin32_call("CreateProcessW", cpargs, buf, sizeof(buf)) == 0);
    std::string reply = buf;
    auto p1 = reply.find('\x1f');
    assert(p1 != std::string::npos);
    auto p2 = reply.find('\x1f', p1 + 1);
    assert(p2 != std::string::npos);
    std::string ph = reply.substr(p1 + 1, p2 - p1 - 1);
    std::string th = reply.substr(p2 + 1);
    assert(!ph.empty());
    std::string waitarg = ph + "\x1f" "5000";
    assert(wasmwin32_call("WaitForSingleObject", waitarg.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("GetExitCodeProcess", ph.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strtoul(buf, nullptr, 10) == 0);
    assert(wasmwin32_call("CloseHandle", ph.c_str(), buf, sizeof(buf)) == 0);
    if (th != ph) {
      wasmwin32_call("CloseHandle", th.c_str(), buf, sizeof(buf));
    }
  }

  {
    assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
    std::string dir = buf;
    std::string path = dir;
    if (!path.empty() && path.back() != '\\' && path.back() != '/') path += '/';
    path += "wasmwin32_k32_test.txt";
    std::string cf = path + "\x1f" + "1073741824" + "\x1f" "2";  // GENERIC_WRITE, CREATE_ALWAYS
    assert(wasmwin32_call("CreateFileW", cf.c_str(), buf, sizeof(buf)) == 0);
    std::string fh = buf;
    std::string wr = fh + "\x1f" "hello-k32";
    assert(wasmwin32_call("WriteFile", wr.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", fh.c_str(), buf, sizeof(buf)) == 0);
    std::string cfr = path + "\x1f" + "2147483648" + "\x1f" "3";  // GENERIC_READ, OPEN_EXISTING
    assert(wasmwin32_call("CreateFileW", cfr.c_str(), buf, sizeof(buf)) == 0);
    fh = buf;
    std::string rd = fh + "\x1f" "64";
    assert(wasmwin32_call("ReadFile", rd.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "hello-k32") == 0);
    assert(wasmwin32_call("GetFileTime", fh.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("GetFinalPathNameByHandleW", fh.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("CloseHandle", fh.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("DeleteFileW", path.c_str(), buf, sizeof(buf)) == 0);
  }

  assert(wasmwin32_call("HeapAlloc", "32", buf, sizeof(buf)) == 0);
  std::string hp = buf;
  assert(std::strtoull(buf, nullptr, 10) != 0);
  assert(wasmwin32_call("HeapSize", hp.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strtoull(buf, nullptr, 10) >= 32);
  assert(wasmwin32_call("HeapValidate", hp.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("HeapFree", hp.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmwin32_call("GlobalAlloc", "16", buf, sizeof(buf)) == 0);
  std::string gp = buf;
  assert(wasmwin32_call("GlobalSize", gp.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strtoull(buf, nullptr, 10) >= 16);
  assert(wasmwin32_call("GlobalFree", gp.c_str(), buf, sizeof(buf)) == 0);

  {
    assert(wasmwin32_call("CreateEventW", "0\x1f" "0", buf, sizeof(buf)) == 0);
    std::string ev = buf;
    std::string wait0 = ev + "\x1f" "0";
    assert(wasmwin32_call("WaitForSingleObject", wait0.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "258") == 0);
    assert(wasmwin32_call("SetEvent", ev.c_str(), buf, sizeof(buf)) == 0);
    std::string wait1 = ev + "\x1f" "1000";
    assert(wasmwin32_call("WaitForSingleObject", wait1.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("CloseHandle", ev.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("CreateEventW", "1\x1f" "0", buf, sizeof(buf)) == 0);
    std::string e1 = buf;
    assert(wasmwin32_call("CreateEventW", "1\x1f" "0", buf, sizeof(buf)) == 0);
    std::string e2 = buf;
    std::string wmo = std::string("2\x1f") + e1 + "\x1f" + e2 + "\x1f" "0\x1f" "0";
    assert(wasmwin32_call("WaitForMultipleObjects", wmo.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "258") == 0);
    assert(wasmwin32_call("SetEvent", e2.c_str(), buf, sizeof(buf)) == 0);
    wmo = std::string("2\x1f") + e1 + "\x1f" + e2 + "\x1f" "1000\x1f" "0";
    assert(wasmwin32_call("WaitForMultipleObjects", wmo.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0);
    assert(wasmwin32_call("CloseHandle", e1.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", e2.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("CreatePipe", "", buf, sizeof(buf)) == 0);
    std::string pr = buf;
    auto sp = pr.find('\x1f');
    assert(sp != std::string::npos);
    std::string rh = pr.substr(0, sp);
    std::string wh = pr.substr(sp + 1);
    std::string wr = wh + "\x1f" "pipe-k32";
    assert(wasmwin32_call("WriteFile", wr.c_str(), buf, sizeof(buf)) == 0);
    std::string rd = rh + "\x1f" "32";
    assert(wasmwin32_call("ReadFile", rd.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "pipe-k32") == 0);
    assert(wasmwin32_call("CloseHandle", rh.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", wh.c_str(), buf, sizeof(buf)) == 0);
  }

  assert(wasmwin32_call("SearchPathW", "kernel32.dll", buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "kernel32") != nullptr || std::strstr(buf, "KERNEL32") != nullptr);

  assert(wasmwin32_call("GetDriveTypeW", "C:\\", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) == 3);

  assert(wasmwin32_call("CharUpperW", "k32", buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "K32") == 0);

  assert(wasmwin32_call("GetSystemTimeAsFileTime", "", buf, sizeof(buf)) == 0);
  std::string ftnow = buf;
  assert(wasmwin32_call("FileTimeToSystemTime", ftnow.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 8);

  {
    assert(wasmwin32_call("CreateFileMappingW", "0\x1f" "4\x1f" "4096", buf, sizeof(buf)) == 0);
    std::string mh = buf;
    std::string maparg = mh + "\x1f" "0";
    assert(wasmwin32_call("MapViewOfFile", maparg.c_str(), buf, sizeof(buf)) == 0);
    std::string view = buf;
    assert(std::strtoull(view.c_str(), nullptr, 10) != 0);
    assert(wasmwin32_call("UnmapViewOfFile", view.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", mh.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("InitializeCriticalSection", "", buf, sizeof(buf)) == 0);
    std::string cs = buf;
    assert(wasmwin32_call("EnterCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("LeaveCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("DeleteCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
  }

  assert(wasmwin32_call("MulDiv", "10\x1f" "20\x1f" "5", buf, sizeof(buf)) == 0);
  assert(std::strtol(buf, nullptr, 10) == 40);

  {
    assert(wasmwin32_call("AddAtomW", "wasmwin32-k32", buf, sizeof(buf)) == 0);
    std::string atom = buf;
    assert(wasmwin32_call("FindAtomW", "wasmwin32-k32", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, atom.c_str()) == 0);
    assert(wasmwin32_call("DeleteAtom", atom.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("CreateIoCompletionPort", "", buf, sizeof(buf)) == 0);
    std::string iocp = buf;
    std::string post = iocp + "\x1f" "8\x1f" "99";
    assert(wasmwin32_call("PostQueuedCompletionStatus", post.c_str(), buf, sizeof(buf)) == 0);
    std::string get = iocp + "\x1f" "1000";
    assert(wasmwin32_call("GetQueuedCompletionStatus", get.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "8") != nullptr);
    assert(wasmwin32_call("CloseHandle", iocp.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
    std::string dir = buf;
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += '\\';
    std::string ini = dir + "wasmwin32_k32.ini";
    std::string wrini = std::string("sec\x1f" "key\x1f" "val42\x1f") + ini;
    assert(wasmwin32_call("WritePrivateProfileStringW", wrini.c_str(), buf, sizeof(buf)) == 0);
    std::string rdini = std::string("sec\x1f" "key\x1f" "missing\x1f") + ini;
    assert(wasmwin32_call("GetPrivateProfileStringW", rdini.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "val42") == 0);
    wasmwin32_call("DeleteFileW", ini.c_str(), buf, sizeof(buf));
  }

  {
    assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
    std::string dir = buf;
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += '\\';
    std::string src = dir + "wasmwin32_k32_hl.txt";
    std::string dst = dir + "wasmwin32_k32_hl2.txt";
    std::string cf = src + "\x1f" "1073741824\x1f" "2";
    assert(wasmwin32_call("CreateFileW", cf.c_str(), buf, sizeof(buf)) == 0);
    std::string fh = buf;
    assert(wasmwin32_call("CloseHandle", fh.c_str(), buf, sizeof(buf)) == 0);
    std::string hl = dst + "\x1f" + src;
    assert(wasmwin32_call("CreateHardLinkW", hl.c_str(), buf, sizeof(buf)) == 0);
    wasmwin32_call("DeleteFileW", src.c_str(), buf, sizeof(buf));
    wasmwin32_call("DeleteFileW", dst.c_str(), buf, sizeof(buf));
  }

  {
    assert(wasmwin32_call("CreateToolhelp32Snapshot", "2", buf, sizeof(buf)) == 0);
    std::string snap = buf;
    assert(wasmwin32_call("Process32FirstW", snap.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("CloseHandle", snap.c_str(), buf, sizeof(buf)) == 0);
  }

  assert(wasmwin32_call("GetCPInfo", "65001", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);
  assert(wasmwin32_call("IsWow64Process", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetDateFormatW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);
  assert(wasmwin32_call("lstrcatW", "go\x1f" "++", buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "go++") == 0);

  {
    assert(wasmwin32_call("CreateWaitableTimerW", "1", buf, sizeof(buf)) == 0);
    std::string tm = buf;
    std::string set = tm + "\x1f" "30";
    assert(wasmwin32_call("SetWaitableTimer", set.c_str(), buf, sizeof(buf)) == 0);
    std::string wait = tm + "\x1f" "2000";
    assert(wasmwin32_call("WaitForSingleObject", wait.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("CloseHandle", tm.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("CreateJobObjectW", "", buf, sizeof(buf)) == 0);
    std::string job = buf;
    assert(wasmwin32_call("QueryInformationJobObject", job.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("IsProcessInJob", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", job.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("InitializeSRWLock", "", buf, sizeof(buf)) == 0);
    std::string srw = buf;
    assert(wasmwin32_call("TryAcquireSRWLockExclusive", srw.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0);
    assert(wasmwin32_call("ReleaseSRWLockExclusive", srw.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("InitializeCriticalSection", "", buf, sizeof(buf)) == 0);
    std::string cs = buf;
    assert(wasmwin32_call("InitializeConditionVariable", "", buf, sizeof(buf)) == 0);
    std::string cv = buf;
    assert(wasmwin32_call("EnterCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
    std::string sl = cv + "\x1f" + cs + "\x1f" "1";
    assert(wasmwin32_call("SleepConditionVariableCS", sl.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("LeaveCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("DeleteCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("InitOnceInitialize", "", buf, sizeof(buf)) == 0);
    std::string once = buf;
    assert(wasmwin32_call("InitOnceExecuteOnce", once.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("InitOnceExecuteOnce", once.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("CreateEventW", "1\x1f" "0", buf, sizeof(buf)) == 0);
    std::string e1 = buf;
    assert(wasmwin32_call("CreateEventW", "1\x1f" "1", buf, sizeof(buf)) == 0);
    std::string e2 = buf;
    std::string so = e1 + "\x1f" + e2 + "\x1f" "1000";
    assert(wasmwin32_call("SignalObjectAndWait", so.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("CloseHandle", e1.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", e2.c_str(), buf, sizeof(buf)) == 0);
  }

  assert(wasmwin32_call("GetCurrentThreadId", "", buf, sizeof(buf)) == 0);
  std::string tid = buf;
  assert(wasmwin32_call("OpenThread", tid.c_str(), buf, sizeof(buf)) == 0);
  std::string th = buf;
  assert(wasmwin32_call("GetThreadTimes", th.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);
  assert(wasmwin32_call("GetProcessIdOfThread", th.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) != 0);
  assert(wasmwin32_call("CloseHandle", th.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmwin32_call("GetProcessTimes", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("FlushProcessWriteBuffers", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("QueueUserWorkItem", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetLargePageMinimum", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetPhysicallyInstalledSystemMemory", "", buf, sizeof(buf)) == 0);
  assert(std::strtoull(buf, nullptr, 10) != 0);
  assert(wasmwin32_call("GetProcessHeaps", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetSystemPowerStatus", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetCurrentProcessorNumberEx", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetNumaHighestNodeNumber", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetProcessAffinityMask", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetActiveProcessorCount", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("CompareStringOrdinal", "abc\x1f" "abc", buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "2") == 0);
  assert(wasmwin32_call("LocaleNameToLCID", "en-US", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) == 1033);
  assert(wasmwin32_call("LCIDToLocaleName", "1033", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);
  assert(wasmwin32_call("IsValidLocaleName", "en-US", buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "1") == 0);
  assert(wasmwin32_call("GetCalendarInfoW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);
  assert(wasmwin32_call("GetUserPreferredUILanguages", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetProcessWorkingSetSize", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetProcessIoCounters", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("CaptureStackBackTrace", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("NeedCurrentDirectoryForExePathW", "cmd.exe", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("QueryDosDeviceW", "C:", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  {
    assert(wasmwin32_call("FindFirstVolumeW", "", buf, sizeof(buf)) == 0);
    std::string vol = buf;
    auto sp = vol.find('\x1f');
    assert(sp != std::string::npos);
    std::string vh = vol.substr(0, sp);
    std::string vname = vol.substr(sp + 1);
    assert(!vname.empty());
    wasmwin32_call("GetVolumePathNamesForVolumeNameW", vname.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("FindVolumeClose", vh.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GetVolumeNameForVolumeMountPointW", "C:\\", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("GetVolumePathNameW", ".", buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
    std::string dir = buf;
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += '\\';
    std::string nd = dir + "wasmwin32_k32_exdir";
    std::string arg = "\x1f" + nd;
    wasmwin32_call("RemoveDirectoryW", nd.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("CreateDirectoryExW", arg.c_str(), buf, sizeof(buf)) == 0);
    std::string streamf = nd + "\\s.txt";
    std::string cf = streamf + "\x1f" "1073741824\x1f" "2";
    assert(wasmwin32_call("CreateFileW", cf.c_str(), buf, sizeof(buf)) == 0);
    std::string fh = buf;
    assert(wasmwin32_call("CloseHandle", fh.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("FindFirstStreamW", streamf.c_str(), buf, sizeof(buf)) == 0);
    std::string st = buf;
    auto sp = st.find('\x1f');
    if (sp != std::string::npos) {
      std::string sh = st.substr(0, sp);
      wasmwin32_call("FindClose", sh.c_str(), buf, sizeof(buf));
    }
    wasmwin32_call("DeleteFileW", streamf.c_str(), buf, sizeof(buf));
    wasmwin32_call("RemoveDirectoryW", nd.c_str(), buf, sizeof(buf));
  }

  if (wasmwin32_call("Wow64DisableWow64FsRedirection", "", buf, sizeof(buf)) == 0) {
    std::string cookie = buf;
    wasmwin32_call("Wow64RevertWow64FsRedirection", cookie.c_str(), buf, sizeof(buf));
  }

  assert(wasmwin32_call("GetConsoleWindow", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetConsoleOriginalTitleW", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetConsoleProcessList", "", buf, sizeof(buf)) == 0);

  assert(wasmwin32_call("IsProcessorFeaturePresent", "8", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("ProcessIdToSessionId", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("WTSGetActiveConsoleSessionId", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("K32GetProcessMemoryInfo", "", buf, sizeof(buf)) == 0);
  assert(std::strtoull(buf, nullptr, 10) != 0);
  assert(wasmwin32_call("K32EnumProcesses", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("K32GetModuleFileNameExW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);
  assert(wasmwin32_call("EncodePointer", "42", buf, sizeof(buf)) == 0);
  std::string enc = buf;
  assert(wasmwin32_call("DecodePointer", enc.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strtoull(buf, nullptr, 10) == 42);
  assert(wasmwin32_call("GetFirmwareType", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetDynamicTimeZoneInformation", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetUserDefaultLocaleName", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);
  assert(wasmwin32_call("ResolveLocaleName", "en-US", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetLocaleInfoEx", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetThreadUILanguage", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetGeoInfoW", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("IdnToAscii", "example.com", buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "example.com") != nullptr);
  assert(wasmwin32_call("FindStringOrdinal", "hello\x1f" "ell", buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "1") == 0);
  assert(wasmwin32_call("GetTempPath2W", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);
  assert(wasmwin32_call("GetSystemTimeAdjustment", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetConsoleOutputCP", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("VerSetConditionMask", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetProcessMitigationPolicy", "", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("DnsHostnameToComputerNameW", "", buf, sizeof(buf)) == 0);

  {
    assert(wasmwin32_call("CreateEventW", "1\x1f" "0\x1f" "Local\\wasmwin32_k32_evt", buf, sizeof(buf)) == 0);
    std::string ev = buf;
    assert(wasmwin32_call("OpenEventW", "Local\\wasmwin32_k32_evt", buf, sizeof(buf)) == 0);
    std::string ev2 = buf;
    assert(wasmwin32_call("CloseHandle", ev2.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", ev.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("HeapAlloc", "4", buf, sizeof(buf)) == 0);
    std::string p = buf;
    std::string wa = p + "\x1f" "1" "\x1f" "10";
    assert(wasmwin32_call("WaitOnAddress", wa.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0);
    assert(wasmwin32_call("HeapFree", p.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("CreateEventW", "1\x1f" "0", buf, sizeof(buf)) == 0);
    std::string ev = buf;
    std::string rw = ev + "\x1f" "2000";
    assert(wasmwin32_call("RegisterWaitForSingleObject", rw.c_str(), buf, sizeof(buf)) == 0);
    std::string wh = buf;
    assert(wasmwin32_call("SetEvent", ev.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("UnregisterWait", wh.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", ev.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("CreateTimerQueue", "", buf, sizeof(buf)) == 0);
    std::string q = buf;
    std::string ct = q + "\x1f" "10";
    assert(wasmwin32_call("CreateTimerQueueTimer", ct.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("Sleep", "30", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("DeleteTimerQueueEx", q.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("CreateThreadpoolWork", "", buf, sizeof(buf)) == 0);
    std::string work = buf;
    assert(wasmwin32_call("SubmitThreadpoolWork", work.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("WaitForThreadpoolWorkCallbacks", work.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseThreadpoolWork", work.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("TrySubmitThreadpoolCallback", "", buf, sizeof(buf)) == 0);
  }

  {
    if (wasmwin32_call("ConvertThreadToFiber", "", buf, sizeof(buf)) == 0) {
      std::string self = buf;
      assert(wasmwin32_call("IsThreadAFiber", "", buf, sizeof(buf)) == 0);
      assert(std::strcmp(buf, "1") == 0);
      if (wasmwin32_call("CreateFiber", "", buf, sizeof(buf)) == 0) {
        std::string fib = buf;
        assert(wasmwin32_call("DeleteFiber", fib.c_str(), buf, sizeof(buf)) == 0);
      }
      assert(wasmwin32_call("ConvertFiberToThread", "", buf, sizeof(buf)) == 0);
      (void)self;
    }
  }

  {
    assert(wasmwin32_call("InitializeSynchronizationBarrier", "1", buf, sizeof(buf)) == 0);
    std::string br = buf;
    assert(wasmwin32_call("EnterSynchronizationBarrier", br.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("DeleteSynchronizationBarrier", br.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("GlobalAlloc", "16", buf, sizeof(buf)) == 0);
    std::string g = buf;
    assert(wasmwin32_call("GlobalLock", g.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GlobalUnlock", g.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GlobalFree", g.c_str(), buf, sizeof(buf)) == 0);
  }

  if (wasmwin32_call("CreateMailslotW", "\\\\.\\mailslot\\wasmwin32_k32", buf, sizeof(buf)) == 0) {
    std::string ms = buf;
    wasmwin32_call("GetMailslotInfo", ms.c_str(), buf, sizeof(buf));
    wasmwin32_call("CloseHandle", ms.c_str(), buf, sizeof(buf));
  }

  assert(wasmwin32_call("GetCurrentProcessId", "", buf, sizeof(buf)) == 0);
  unsigned long pid = std::strtoul(buf, nullptr, 10);
  assert(pid != 0);

  assert(wasmwin32_call("GetCurrentThreadId", "", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) != 0);

  assert(wasmwin32_call("GetTickCount64", "", buf, sizeof(buf)) == 0);
  unsigned long long t0 = std::strtoull(buf, nullptr, 10);
  assert(wasmwin32_call("Sleep", "15", buf, sizeof(buf)) == 0);
  assert(wasmwin32_call("GetTickCount64", "", buf, sizeof(buf)) == 0);
  unsigned long long t1 = std::strtoull(buf, nullptr, 10);
  assert(t1 >= t0);

  assert(wasmwin32_call("GetComputerNameW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetWindowsDirectoryW", "", buf, sizeof(buf)) == 0);
#if defined(_WIN32)
  assert(std::strchr(buf, ':') != nullptr || std::strstr(buf, "Windows") != nullptr);
#else
  assert(std::strlen(buf) > 0);
#endif

  assert(wasmwin32_call("GetSystemDirectoryW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetCurrentDirectoryW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("GetModuleFileNameW", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmwin32_call("QueryPerformanceFrequency", "", buf, sizeof(buf)) == 0);
  assert(std::strtoull(buf, nullptr, 10) != 0);

  assert(wasmwin32_call("QueryPerformanceCounter", "", buf, sizeof(buf)) == 0);
  assert(std::strtoull(buf, nullptr, 10) != 0);

  {
    assert(wasmwin32_call("RegCreateKeyExW", "HKCU\x1f" "Software\\WASMWin32K32", buf,
                          sizeof(buf)) == 0);
    std::string reply = buf;
    auto sp = reply.find('\x1f');
    std::string key = sp == std::string::npos ? reply : reply.substr(0, sp);
    std::string setv = key + "\x1f" "Greeting" "\x1f" "1" "\x1f" "hello-reg";
    assert(wasmwin32_call("RegSetValueExW", setv.c_str(), buf, sizeof(buf)) == 0);
    std::string qv = key + "\x1f" "Greeting";
    assert(wasmwin32_call("RegQueryValueExW", qv.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "hello-reg") != nullptr);
    std::string dw = key + "\x1f" "Count" "\x1f" "4" "\x1f" "42";
    assert(wasmwin32_call("RegSetValueExW", dw.c_str(), buf, sizeof(buf)) == 0);
    std::string qd = key + "\x1f" "Count";
    assert(wasmwin32_call("RegQueryValueExW", qd.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "42") != nullptr);
    assert(wasmwin32_call("RegQueryInfoKeyW", key.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RegEnumValueW", (key + "\x1f" "0").c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RegFlushKey", key.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RegGetValueW", "HKCU\x1f" "Software\\WASMWin32K32" "\x1f" "Greeting",
                          buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "hello-reg") != nullptr);
    assert(wasmwin32_call("RegDeleteValueW", qv.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RegDeleteValueW", qd.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RegCloseKey", key.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RegDeleteKeyW", "HKCU\x1f" "Software\\WASMWin32K32", buf,
                          sizeof(buf)) == 0);
    assert(wasmwin32_call("RegOpenCurrentUser", "", buf, sizeof(buf)) == 0);
    std::string cu = buf;
    assert(wasmwin32_call("RegCloseKey", cu.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("GetCurrentProcess", "", buf, sizeof(buf)) == 0);
    std::string proc = buf;
    assert(wasmwin32_call("OpenProcessToken", proc.c_str(), buf, sizeof(buf)) == 0);
    std::string tok = buf;
    assert(!tok.empty());
    assert(wasmwin32_call("GetTokenInformation", (tok + "\x1f" "1").c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("GetTokenInformation", (tok + "\x1f" "8").c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0 || std::strcmp(buf, "2") == 0);
    assert(wasmwin32_call("LookupAccountSidW", "S-1-5-18", buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "SYSTEM") != nullptr);
    assert(wasmwin32_call("AllocateAndInitializeSid", "1\x1f" "5" "\x1f" "18", buf, sizeof(buf)) ==
           0);
    std::string sidh = buf;
    assert(wasmwin32_call("LookupAccountSidW", sidh.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "SYSTEM") != nullptr);
    assert(wasmwin32_call("CheckTokenMembership", (tok + "\x1f" "S-1-1-0").c_str(), buf,
                          sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0 || std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("DuplicateTokenEx", tok.c_str(), buf, sizeof(buf)) == 0);
    std::string dup = buf;
    wasmwin32_call("AdjustTokenPrivileges", (tok + "\x1f" "SeLoadDriverPrivilege").c_str(), buf,
                   sizeof(buf));
    wasmwin32_call("NtAdjustPrivilegesToken", (dup + "\x1f" "SeLoadDriverPrivilege").c_str(), buf,
                   sizeof(buf));
    assert(wasmwin32_call("GetTokenInformation", (dup + "\x1f" "3").c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("ImpersonateLoggedOnUser", dup.c_str(), buf, sizeof(buf)) == 0);
    wasmwin32_call("RtlAdjustPrivilege", "10" "\x1f" "1" "\x1f" "1", buf, sizeof(buf));
    assert(wasmwin32_call("OpenThreadToken", "-2", buf, sizeof(buf)) == 0);
    std::string ttok = buf;
    wasmwin32_call("AdjustTokenPrivileges", (ttok + "\x1f" "SeLoadDriverPrivilege").c_str(), buf,
                   sizeof(buf));
    assert(wasmwin32_call("GetTokenInformation", (ttok + "\x1f" "3").c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", ttok.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("SetThreadToken", (std::string("-2\x1f") + dup).c_str(), buf,
                          sizeof(buf)) == 0);
    // Stay impersonated: SC_MANAGER_CONNECT (0x1), not ALL_ACCESS (0xF003F).
    assert(wasmwin32_call("OpenSCManagerW", "\x1f" "\x1f" "0x1", buf, sizeof(buf)) == 0);
    std::string iscm = buf;
    // CONNECT | ENUMERATE_SERVICE | QUERY_LOCK_STATUS
    assert(wasmwin32_call("OpenSCManagerW", "\x1f" "\x1f" "0x15", buf, sizeof(buf)) == 0);
    std::string iscm2 = buf;
    // SERVICE_QUERY_STATUS | SERVICE_START (0x14). START may still be 5 on
    // EventLog; QUERY_STATUS (0x4) is the Authenticated Users query bit.
    std::string iopen = iscm + "\x1f" "EventLog" "\x1f" "0x14";
    if (wasmwin32_call("OpenServiceW", iopen.c_str(), buf, sizeof(buf)) != 0) {
      iopen = iscm + "\x1f" "EventLog" "\x1f" "0x4";
      assert(wasmwin32_call("OpenServiceW", iopen.c_str(), buf, sizeof(buf)) == 0);
    }
    std::string isvc = buf;
    assert(wasmwin32_call("QueryServiceStatus", isvc.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseServiceHandle", isvc.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseServiceHandle", iscm.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseServiceHandle", iscm2.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RevertToSelf", "", buf, sizeof(buf)) == 0);
    wasmwin32_call("SetTokenInformation", (tok + "\x1f" "3" "\x1f" "SeDebugPrivilege").c_str(), buf,
                   sizeof(buf));
    assert(wasmwin32_call("LookupPrivilegeValueW", "SeLoadDriverPrivilege", buf, sizeof(buf)) == 0);
    assert(std::strtoull(buf, nullptr, 10) != 0);
    std::string luid = buf;
    assert(wasmwin32_call("LookupPrivilegeNameW", luid.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "SeLoadDriverPrivilege") != nullptr);
    wasmwin32_call("AdjustTokenPrivileges", (tok + "\x1f" "SeLoadDriverPrivilege").c_str(), buf,
                   sizeof(buf));
    assert(wasmwin32_call("GetTokenInformation", (tok + "\x1f" "3").c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", tok.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", dup.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("OpenSCManagerW", "", buf, sizeof(buf)) == 0);
    std::string scm = buf;
    assert(!scm.empty());
    std::string open = scm + "\x1f" "EventLog";
    assert(wasmwin32_call("OpenServiceW", open.c_str(), buf, sizeof(buf)) == 0);
    std::string svc = buf;
    assert(wasmwin32_call("QueryServiceStatus", svc.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strtoul(buf, nullptr, 10) != 0);
    wasmwin32_call("QueryServiceConfigW", svc.c_str(), buf, sizeof(buf));
    wasmwin32_call("EnumServicesStatusW", scm.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("CloseServiceHandle", svc.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("StartServiceCtrlDispatcherW", "EventLog", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RegisterServiceCtrlHandlerW", "EventLog", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseServiceHandle", scm.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("GetCurrentProcess", "", buf, sizeof(buf)) == 0);
    std::string proc = buf;
    std::string alloc = proc + "\x1f" "64";
    assert(wasmwin32_call("VirtualAllocEx", alloc.c_str(), buf, sizeof(buf)) == 0);
    std::string ptr = buf;
    assert(std::strtoull(ptr.c_str(), nullptr, 10) != 0);
    std::string wr = proc + "\x1f" + ptr + "\x1f" "hello-vm";
    assert(wasmwin32_call("WriteProcessMemory", wr.c_str(), buf, sizeof(buf)) == 0);
    std::string rd = proc + "\x1f" + ptr + "\x1f" "8";
    assert(wasmwin32_call("ReadProcessMemory", rd.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strncmp(buf, "hello-vm", 8) == 0);
    std::string prot = proc + "\x1f" + ptr + "\x1f" "64";
    assert(wasmwin32_call("VirtualProtectEx", prot.c_str(), buf, sizeof(buf)) == 0);
    std::string q = proc + "\x1f" + ptr;
    assert(wasmwin32_call("VirtualQueryEx", q.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    std::string fr = proc + "\x1f" + ptr;
    assert(wasmwin32_call("VirtualFreeEx", fr.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("DeviceIoControl", "0" "\x1f" "0x70000", buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "512") != nullptr);
    assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
    std::string dir = buf;
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += '/';
    std::string path = dir + "wasmwin32_ioctl.tmp";
    std::string cf = path + "\x1f" "1073741824" "\x1f" "2";
    if (wasmwin32_call("CreateFileW", cf.c_str(), buf, sizeof(buf)) == 0) {
      std::string fh = buf;
      wasmwin32_call("SetFileCompletionNotificationModes", (fh + "\x1f" "1").c_str(), buf,
                     sizeof(buf));
      assert(wasmwin32_call("WriteFile", (fh + "\x1f" "abc").c_str(), buf, sizeof(buf)) == 0);
      wasmwin32_call("GetOverlappedResult", fh.c_str(), buf, sizeof(buf));
      wasmwin32_call("SetFilePointerEx", (fh + "\x1f" "0" "\x1f" "0").c_str(), buf, sizeof(buf));
      wasmwin32_call("CancelIoEx", fh.c_str(), buf, sizeof(buf));
      wasmwin32_call("CloseHandle", fh.c_str(), buf, sizeof(buf));
      wasmwin32_call("DeleteFileW", path.c_str(), buf, sizeof(buf));
    }
  }

  {
    assert(wasmwin32_call("WSAStartup", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("WSASocketW", "", buf, sizeof(buf)) == 0);
    std::string s1 = buf;
    assert(wasmwin32_call("bind", (s1 + "\x1f" "inproc:wasmwin32-wsa").c_str(), buf, sizeof(buf)) ==
           0);
    assert(wasmwin32_call("listen", s1.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("WSASocketW", "", buf, sizeof(buf)) == 0);
    std::string s2 = buf;
    assert(wasmwin32_call("connect", (s2 + "\x1f" "inproc:wasmwin32-wsa").c_str(), buf,
                          sizeof(buf)) == 0);
    assert(wasmwin32_call("accept", s1.c_str(), buf, sizeof(buf)) == 0);
    std::string s3 = buf;
    assert(wasmwin32_call("WSASend", (s2 + "\x1f" "ping").c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("WSARecv", (s3 + "\x1f" "16").c_str(), buf, sizeof(buf)) == 0);
    assert(std::strncmp(buf, "ping", 4) == 0);
    wasmwin32_call("ioctlsocket", (s2 + "\x1f" "FIONBIO" "\x1f" "1").c_str(), buf, sizeof(buf));
    wasmwin32_call("select", (s3 + "\x1f" "0").c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("closesocket", s3.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("closesocket", s2.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("closesocket", s1.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("WSACleanup", "", buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
    std::string dir = buf;
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += '/';
    std::string watch = dir + "wasmwin32_notify";
    wasmwin32_call("CreateDirectoryW", watch.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("FindFirstChangeNotificationW", watch.c_str(), buf, sizeof(buf)) == 0);
    std::string nh = buf;
    assert(!nh.empty());
    wasmwin32_call("ReadDirectoryChangesW", nh.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("FindCloseChangeNotification", nh.c_str(), buf, sizeof(buf)) == 0);
    wasmwin32_call("RemoveDirectoryW", watch.c_str(), buf, sizeof(buf));
  }

  {
    assert(wasmwin32_call("CreatePipe", "", buf, sizeof(buf)) == 0);
    std::string reply = buf;
    auto sp = reply.find('\x1f');
    assert(sp != std::string::npos);
    std::string rd = reply.substr(0, sp);
    std::string wr = reply.substr(sp + 1);
    assert(wasmwin32_call("GetNamedPipeInfo", rd.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    wasmwin32_call("GetNamedPipeClientProcessId", rd.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("GetHandleInformation", wr.c_str(), buf, sizeof(buf)) == 0);
    std::string setf = wr + "\x1f" "1" "\x1f" "0";
    assert(wasmwin32_call("SetHandleInformation", setf.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", rd.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CloseHandle", wr.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("SetThreadDescription", "-2" "\x1f" "wasmwin32-k32", buf, sizeof(buf)) ==
           0);
    assert(wasmwin32_call("GetThreadDescription", "-2", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GetCurrentThreadStackLimits", "", buf, sizeof(buf)) == 0);
    assert(std::strchr(buf, '\x1f') != nullptr);
    assert(wasmwin32_call("InitializeProcThreadAttributeList", "1", buf, sizeof(buf)) == 0);
    std::string attr = buf;
    assert(wasmwin32_call("DeleteProcThreadAttributeList", attr.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("GetSystemTimeAsFileTime", "", buf, sizeof(buf)) == 0);
    std::string ft = buf;
    assert(wasmwin32_call("FileTimeToDosDateTime", ft.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strchr(buf, '\x1f') != nullptr);
    std::string dos = buf;
    assert(wasmwin32_call("DosDateTimeToFileTime", dos.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strtoull(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("IsDBCSLeadByte", "65", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    wasmwin32_call("GetConsoleCursorInfo", "", buf, sizeof(buf));
    wasmwin32_call("PeekConsoleInputW", "", buf, sizeof(buf));
  }

  {
    assert(wasmwin32_call("NtQueryInformationProcess", "-1" "\x1f" "0", buf, sizeof(buf)) == 0);
    std::string pbi = buf;
    auto sp = pbi.find('\x1f');
    assert(sp != std::string::npos);
    unsigned long long peb = std::strtoull(pbi.c_str(), nullptr, 10);
    assert(peb != 0);
    std::string rpm = std::string("-1") + "\x1f" + std::to_string(peb + 2) + "\x1f" + "1";
    assert(wasmwin32_call("ReadProcessMemory", rpm.c_str(), buf, sizeof(buf)) == 0);
    assert((unsigned char)buf[0] == 0 || (unsigned char)buf[0] == 1);
    assert(wasmwin32_call("NtQueryInformationProcess", "-1" "\x1f" "7", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("RtlGetCurrentPeb", "", buf, sizeof(buf)) == 0);
    assert(std::strtoull(buf, nullptr, 10) != 0);
  }

  {
    assert(wasmwin32_call("NtCreateSection", "4096", buf, sizeof(buf)) == 0);
    std::string sec = buf;
    assert(wasmwin32_call("NtMapViewOfSection", sec.c_str(), buf, sizeof(buf)) == 0);
    std::string view = buf;
    assert(std::strtoull(view.c_str(), nullptr, 10) != 0);
    std::string wpm = std::string("-1") + "\x1f" + view + "\x1f" + "ntdll-map";
    assert(wasmwin32_call("WriteProcessMemory", wpm.c_str(), buf, sizeof(buf)) == 0);
    std::string rpm = std::string("-1") + "\x1f" + view + "\x1f" + "9";
    assert(wasmwin32_call("ReadProcessMemory", rpm.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strncmp(buf, "ntdll-map", 9) == 0);
    wasmwin32_call("NtUnmapViewOfSection", view.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("NtClose", sec.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    auto dll = pe_minimal_dll();
    PeMap mapped;
    assert(pe_map_image(dll.data(), dll.size(), &mapped, nullptr, nullptr));
    assert(mapped.base != nullptr);
    assert(mapped.size >= 0x2000);
    assert(std::memcmp(static_cast<unsigned char*>(mapped.base) + 0x1000, "PEMAP", 5) == 0);
    assert(mapped.exports.count("PeMark") == 1);
    assert(mapped.exports["PeMark"] == 0x1000);
    assert(mapped.nt_off == 0x40);
    pe_unmap(&mapped);
  }

  {
    assert(wasmwin32_call("NtAllocateVirtualMemory", "-1" "\x1f" "4096", buf, sizeof(buf)) == 0);
    std::string va = buf;
    assert(std::strtoull(va.c_str(), nullptr, 10) != 0);
    std::string wpm = std::string("-1") + "\x1f" + va + "\x1f" + "nt-alloc";
    assert(wasmwin32_call("WriteProcessMemory", wpm.c_str(), buf, sizeof(buf)) == 0);
    std::string rpm = std::string("-1") + "\x1f" + va + "\x1f" + "8";
    assert(wasmwin32_call("ReadProcessMemory", rpm.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strncmp(buf, "nt-alloc", 8) == 0);
    std::string fr = std::string("-1") + "\x1f" + va;
    assert(wasmwin32_call("NtFreeVirtualMemory", fr.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtQuerySystemTime", "", buf, sizeof(buf)) == 0);
    assert(std::strtoull(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("NtCreateEvent", "", buf, sizeof(buf)) == 0);
    std::string ev = buf;
    assert(wasmwin32_call("NtSetEvent", ev.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtClose", ev.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
    std::string tmp = buf;
    if (!tmp.empty() && tmp.back() != '\\' && tmp.back() != '/') tmp.push_back('\\');
    std::string ntfile = tmp + "wasmwin32-nt.bin";
    assert(wasmwin32_call("NtCreateFile", ntfile.c_str(), buf, sizeof(buf)) == 0);
    std::string ntf = buf;
    std::string wr = ntf + "\x1f" "nt-file";
    assert(wasmwin32_call("NtWriteFile", wr.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtClose", ntf.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtOpenFile", ntfile.c_str(), buf, sizeof(buf)) == 0);
    ntf = buf;
    std::string rd = ntf + "\x1f" "16";
    assert(wasmwin32_call("NtReadFile", rd.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strncmp(buf, "nt-file", 7) == 0);
    assert(wasmwin32_call("NtClose", ntf.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("AddVectoredExceptionHandler", "", buf, sizeof(buf)) == 0);
    std::string veh = buf;
    assert(!veh.empty());
    assert(wasmwin32_call("RaiseException", "0xE0000001", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "continue") == 0);
    assert(wasmwin32_call("RemoveVectoredExceptionHandler", veh.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("BCryptOpenAlgorithmProvider", "SHA256", buf, sizeof(buf)) == 0);
    std::string alg = buf;
    assert(wasmwin32_call("BCryptCreateHash", alg.c_str(), buf, sizeof(buf)) == 0);
    std::string hh = buf;
    std::string hd = hh + "\x1f" "abc";
    assert(wasmwin32_call("BCryptHashData", hd.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("BCryptFinishHash", hh.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") ==
           0);
    assert(wasmwin32_call("BCryptDestroyHash", hh.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("BCryptCloseAlgorithmProvider", alg.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("BCryptGenRandom", "16", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) == 32);
    assert(wasmwin32_call("NCryptOpenStorageProvider", "", buf, sizeof(buf)) == 0);
    std::string prov = buf;
    assert(!prov.empty());
    std::string ncr = prov + "\x1f" "16";
    assert(wasmwin32_call("NCryptGenRandom", ncr.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) == 32);
    assert(wasmwin32_call("NCryptFreeObject", prov.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("LdrLoadDll", "kernel32", buf, sizeof(buf)) == 0);
    std::string k32 = buf;
    std::string gpa = k32 + "\x1f" "GetCurrentProcessId";
    assert(wasmwin32_call("LdrGetProcedureAddress", gpa.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strtoull(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("RtlImageNtHeader", k32.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("LdrAddRefDll", k32.c_str(), buf, sizeof(buf)) == 0);
    std::string fr = k32 + "\x1f" "16" "\x1f" "1";
    if (wasmwin32_call("FindResourceW", fr.c_str(), buf, sizeof(buf)) == 0) {
      std::string rsrc = buf;
      std::string lr = k32 + "\x1f" + rsrc;
      assert(wasmwin32_call("SizeofResource", lr.c_str(), buf, sizeof(buf)) == 0);
      assert(std::strtoul(buf, nullptr, 10) != 0);
      wasmwin32_call("LoadResource", lr.c_str(), buf, sizeof(buf));
    }
    assert(wasmwin32_call("RtlNtStatusToDosError", "0", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("RtlGetVersion", "", buf, sizeof(buf)) == 0);
    assert(std::strchr(buf, '.') != nullptr);
    assert(wasmwin32_call("RtlGetNtVersionNumbers", "", buf, sizeof(buf)) == 0);
    assert(std::strchr(buf, '.') != nullptr);
    assert(wasmwin32_call("RtlGetNtProductType", "", buf, sizeof(buf)) == 0);
    assert(std::strtol(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("RtlAllocateHeap", "32", buf, sizeof(buf)) == 0);
    std::string hp = buf;
    assert(std::strtoull(hp.c_str(), nullptr, 10) != 0);
    std::string z = hp + "\x1f" "32";
    assert(wasmwin32_call("RtlZeroMemory", z.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RtlSizeHeap", hp.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RtlFreeHeap", hp.c_str(), buf, sizeof(buf)) == 0);
    std::string eq = std::string("abc") + "\x1f" "ABC" + "\x1f" "1";
    assert(wasmwin32_call("RtlEqualUnicodeString", eq.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0);
    assert(wasmwin32_call("RtlIntegerToUnicodeString", "255", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "255") == 0);
    assert(wasmwin32_call("RtlUnicodeStringToInteger", "42", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "42") == 0);
    assert(wasmwin32_call("RtlUpcaseUnicodeChar", "a", buf, sizeof(buf)) == 0);
    assert(std::strtoul(buf, nullptr, 10) == 65);
    assert(wasmwin32_call("RtlRandomEx", "1", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("RtlSetLastWin32Error", "5", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RtlGetLastWin32Error", "", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "5") == 0);
    assert(wasmwin32_call("RtlHashUnicodeString", "hello", buf, sizeof(buf)) == 0);
    assert(std::strtoul(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("NtCreateSemaphore", "0\x1f" "1", buf, sizeof(buf)) == 0);
    std::string sem = buf;
    assert(wasmwin32_call("NtReleaseSemaphore", sem.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtClose", sem.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtCreateEvent", "", buf, sizeof(buf)) == 0);
    std::string ev2 = buf;
    assert(wasmwin32_call("NtPulseEvent", ev2.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtQueryEvent", ev2.c_str(), buf, sizeof(buf)) == 0);
    assert(buf[0] == '0' || buf[0] == '1');
    assert(wasmwin32_call("NtClearEvent", ev2.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtClose", ev2.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtQueryTimerResolution", "", buf, sizeof(buf)) == 0);
    assert(std::strchr(buf, '\x1f') != nullptr);
    assert(wasmwin32_call("NtQueryDefaultLocale", "", buf, sizeof(buf)) == 0);
    assert(std::strtoul(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("NtIsProcessInJob", "", buf, sizeof(buf)) == 0);
    assert(buf[0] == '0' || buf[0] == '1');
    assert(wasmwin32_call("NtQueryDebugFilterState", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GetTempPathW", "", buf, sizeof(buf)) == 0);
    std::string tmpdir = buf;
    assert(wasmwin32_call("NtQueryAttributesFile", tmpdir.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtQueryVolumeInformationFile", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtOpenProcessToken", "", buf, sizeof(buf)) == 0);
    std::string tok = buf;
    assert(wasmwin32_call("NtDuplicateToken", tok.c_str(), buf, sizeof(buf)) == 0);
    wasmwin32_call("NtClose", buf, buf, sizeof(buf));
    wasmwin32_call("NtClose", tok.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("NtCreateSection", "4096", buf, sizeof(buf)) == 0);
    std::string sec2 = buf;
    assert(wasmwin32_call("NtQuerySection", sec2.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strtoull(buf, nullptr, 10) != 0);
    wasmwin32_call("NtClose", sec2.c_str(), buf, sizeof(buf));
    wasmwin32_call("RtlPcToFileHeader", k32.c_str(), buf, sizeof(buf));

    bool saw_nt_user = false, saw_zw_create = false, saw_rtl_cs = false, saw_tp = false;
    int ntdll_n = 0, k32_n = 0;
    for (int i = 0; i < n; i++) {
      if (cat[i].dll && std::strcmp(cat[i].dll, "ntdll") == 0) ntdll_n++;
      if (cat[i].dll && std::strcmp(cat[i].dll, "kernel32") == 0) k32_n++;
      if (std::strcmp(cat[i].name, "NtCreateUserProcess") == 0) saw_nt_user = true;
      if (std::strcmp(cat[i].name, "ZwCreateFile") == 0) saw_zw_create = true;
      if (std::strcmp(cat[i].name, "RtlInitializeCriticalSection") == 0) saw_rtl_cs = true;
      if (std::strcmp(cat[i].name, "TpAllocPool") == 0) saw_tp = true;
    }
    assert(saw_nt_user);
    assert(saw_zw_create);
    assert(saw_rtl_cs);
    assert(saw_tp);
    assert(ntdll_n >= k32_n);

    assert(wasmwin32_call("ZwQuerySystemTime", "", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("NtGetTickCount", "", buf, sizeof(buf)) == 0);
    assert(std::strtoul(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("RtlIsProcessorFeaturePresent", "0", buf, sizeof(buf)) == 0);
    assert(buf[0] == '0' || buf[0] == '1');
    assert(wasmwin32_call("RtlDosPathNameToNtPathName_U", "C:\\Windows", buf, sizeof(buf)) == 0);
    assert(std::strncmp(buf, "\\??\\C:\\Windows", 14) == 0);
    assert(wasmwin32_call("RtlDetermineDosPathNameType_U", "C:\\Windows", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "2") == 0);
    assert(wasmwin32_call("RtlQueryEnvironmentVariable_U", "PATH", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("RtlInitializeCriticalSection", "", buf, sizeof(buf)) == 0);
    std::string cs = buf;
    assert(wasmwin32_call("RtlEnterCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RtlLeaveCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RtlDeleteCriticalSection", cs.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RtlCreateHeap", "", buf, sizeof(buf)) == 0);
    std::string heap = buf;
    assert(std::strtoull(heap.c_str(), nullptr, 10) != 0);
    assert(wasmwin32_call("GetProcessHeap", "", buf, sizeof(buf)) == 0);
    assert(std::strtoull(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("ExAllocatePoolWithTag", "0" "\x1f" "64" "\x1f" "Wasm", buf, sizeof(buf)) ==
           0);
    std::string poolp = buf;
    assert(std::strtoull(poolp.c_str(), nullptr, 10) != 0);
    assert(wasmwin32_call("ExFreePoolWithTag", poolp.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("RtlDestroyHeap", heap.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("NtCreateTimer", "", buf, sizeof(buf)) == 0);
    std::string tm = buf;
    assert(wasmwin32_call("NtCancelTimer", tm.c_str(), buf, sizeof(buf)) == 0);
    wasmwin32_call("NtClose", tm.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("NtCreateJobObject", "", buf, sizeof(buf)) == 0);
    std::string job = buf;
    wasmwin32_call("NtClose", job.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("NtCreateIoCompletion", "", buf, sizeof(buf)) == 0);
    std::string iocp = buf;
    std::string post = iocp + "\x1f" "4" "\x1f" "9";
    assert(wasmwin32_call("NtSetIoCompletion", post.c_str(), buf, sizeof(buf)) == 0);
    std::string wait = iocp + "\x1f" "1000";
    assert(wasmwin32_call("NtRemoveIoCompletion", wait.c_str(), buf, sizeof(buf)) == 0);
    wasmwin32_call("NtClose", iocp.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("RtlInitializeSid", "", buf, sizeof(buf)) == 0);
    std::string sid = buf;
    assert(wasmwin32_call("RtlValidSid", sid.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0);
    assert(wasmwin32_call("RtlLengthSid", sid.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strtoul(buf, nullptr, 10) != 0);
    wasmwin32_call("RtlFreeSid", sid.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("TpAllocPool", "", buf, sizeof(buf)) == 0);
    std::string pool = buf;
    assert(wasmwin32_call("TpReleasePool", pool.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("DbgPrint", "ntdll-complete", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("DbgBreakPoint", "", buf, sizeof(buf)) != 0);
    assert(wasmwin32_call("NtShutdownSystem", "", buf, sizeof(buf)) != 0);
    assert(wasmwin32_call("NtCreateUserProcess", "", buf, sizeof(buf)) != 0);
  }

  {
    assert(wasmwin32_call("CryptProtectData", "hello-dpapi", buf, sizeof(buf)) == 0);
    std::string prot = buf;
    assert(!prot.empty());
    assert(wasmwin32_call("CryptUnprotectData", prot.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "hello-dpapi") == 0);
    std::string b64 = std::string("abc") + "\x1f" "1";
    assert(wasmwin32_call("CryptBinaryToStringW", b64.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strncmp(buf, "YWJj", 4) == 0);
    std::string back = std::string(buf) + "\x1f" "1";
    assert(wasmwin32_call("CryptStringToBinaryW", back.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strncmp(buf, "abc", 3) == 0);
    assert(wasmwin32_call("CertOpenStore", "MEMORY", buf, sizeof(buf)) == 0);
    std::string store = buf;
    assert(!store.empty());
    wasmwin32_call("CertEnumCertificatesInStore", store.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("CertCloseStore", store.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("SHGetFolderPathW", "40", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    std::string profile = buf;
    assert(wasmwin32_call("PathFileExistsW", profile.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0);
    assert(wasmwin32_call("SHGetKnownFolderPath", "Profile", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    std::string comb = profile + "\x1f" "wasmwin32-probe";
    assert(wasmwin32_call("PathCombineW", comb.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "wasmwin32-probe") != nullptr);
    assert(wasmwin32_call("CommandLineToArgvW", "a b c", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "a" "\x1f" "b" "\x1f" "c") == 0);
  }

  {
    assert(wasmwin32_call("WinHttpCrackUrl", "http://127.0.0.1/index", buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "http") != nullptr);
    assert(std::strstr(buf, "127.0.0.1") != nullptr);
    assert(wasmwin32_call("WinHttpOpen", "wasmwin32", buf, sizeof(buf)) == 0);
    std::string sess = buf;
    assert(wasmwin32_call("WinHttpCloseHandle", sess.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GetAdaptersAddresses", "", buf, sizeof(buf)) == 0);
    assert(std::strchr(buf, '\x1f') != nullptr);
    assert(wasmwin32_call("GetNetworkParams", "", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("GetFileVersionInfoSizeW", "kernel32.dll", buf, sizeof(buf)) == 0);
    assert(std::strtoul(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("GetFileVersionInfoW", "kernel32.dll", buf, sizeof(buf)) == 0);
    assert(std::strchr(buf, '.') != nullptr);
    assert(wasmwin32_call("InitCommonControlsEx", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("ImageList_Create", "16" "\x1f" "16", buf, sizeof(buf)) == 0);
    std::string iml = buf;
    assert(wasmwin32_call("ImageList_GetImageCount", iml.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("ImageList_Destroy", iml.c_str(), buf, sizeof(buf)) == 0);
  }

  {
    assert(wasmwin32_call("InternetOpenW", "wasmwin32", buf, sizeof(buf)) == 0);
    std::string inet = buf;
    assert(wasmwin32_call("InternetCloseHandle", inet.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("InternetGetConnectedState", "", buf, sizeof(buf)) == 0);
    assert(buf[0] == '0' || buf[0] == '1');
    std::string dns = std::string("localhost") + "\x1f" "LOCALHOST";
    assert(wasmwin32_call("DnsNameCompare_A", dns.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0);
    assert(wasmwin32_call("GetUserNameExW", "", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("SymInitialize", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("ImageNtHeader", "", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("SymCleanup", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("IsThemeActive", "", buf, sizeof(buf)) == 0);
    assert(buf[0] == '0' || buf[0] == '1');
    assert(wasmwin32_call("DwmIsCompositionEnabled", "", buf, sizeof(buf)) == 0);
    assert(buf[0] == '0' || buf[0] == '1');
    assert(wasmwin32_call("UuidCreate", "", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) == 32);
    assert(wasmwin32_call("SetupDiGetClassDevsW", "", buf, sizeof(buf)) == 0);
    std::string devs = buf;
    assert(!devs.empty());
    std::string en = devs + "\x1f" "0";
    assert(wasmwin32_call("SetupDiEnumDeviceInfo", en.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    wasmwin32_call("SetupDiGetDeviceRegistryPropertyW", en.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("SetupDiDestroyDeviceInfoList", devs.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CM_Locate_DevNodeW", "", buf, sizeof(buf)) == 0);
    std::string inst = buf;
    assert(wasmwin32_call("CM_Get_Device_IDW", inst.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("NetGetJoinInformation", "", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("NetWkstaGetInfo", "", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmwin32_call("NetApiBufferFree", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("PdhOpenQueryW", "", buf, sizeof(buf)) == 0);
    std::string qh = buf;
    std::string add = qh + "\x1f" "\\Processor(_Total)\\% Processor Time";
    if (wasmwin32_call("PdhAddEnglishCounterW", add.c_str(), buf, sizeof(buf)) == 0 ||
        wasmwin32_call("PdhAddCounterW", add.c_str(), buf, sizeof(buf)) == 0) {
      wasmwin32_call("PdhCollectQueryData", qh.c_str(), buf, sizeof(buf));
    }
    assert(wasmwin32_call("PdhCloseQuery", qh.c_str(), buf, sizeof(buf)) == 0);
    if (wasmwin32_call("EvtQuery", "Application\x1f*", buf, sizeof(buf)) == 0) {
      std::string eh = buf;
      wasmwin32_call("EvtNext", eh.c_str(), buf, sizeof(buf));
      assert(wasmwin32_call("EvtClose", eh.c_str(), buf, sizeof(buf)) == 0);
    }
    assert(wasmwin32_call("PathIsRelativeW", "foo", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "1") == 0);
    assert(wasmwin32_call("PathIsRelativeW", "C:\\Windows", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("PathCanonicalizeW", "C:\\foo\\.\\bar", buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "bar") != nullptr);
    std::string ap = std::string("C:\\Windows") + "\x1f" "System32";
    assert(wasmwin32_call("PathAppendW", ap.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "System32") != nullptr);
    assert(wasmwin32_call("PathRemoveFileSpecW", "C:\\Windows\\System32", buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "Windows") != nullptr);
    std::string cmp = std::string("AbC") + "\x1f" "abc";
    assert(wasmwin32_call("StrCmpIW", cmp.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
    assert(wasmwin32_call("GetFileTitleW", "C:\\Windows\\notepad.exe", buf, sizeof(buf)) == 0);
    assert(std::strstr(buf, "notepad") != nullptr);
    assert(wasmwin32_call("CommDlgExtendedError", "", buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "0") == 0);
  }

  {
    assert(wasmwin32_call("BCryptOpenAlgorithmProvider", "AES", buf, sizeof(buf)) == 0);
    std::string alg = buf;
    std::string gk = alg + "\x1f" "0123456789abcdef";
    if (wasmwin32_call("BCryptGenerateSymmetricKey", gk.c_str(), buf, sizeof(buf)) == 0) {
      std::string kh = buf;
      std::string enc = kh + "\x1f" "hello-bcrypt-aes";
      if (wasmwin32_call("BCryptEncrypt", enc.c_str(), buf, sizeof(buf)) == 0) {
        std::string ct = buf;
        std::string dec = kh + "\x1f" + ct;
        assert(wasmwin32_call("BCryptDecrypt", dec.c_str(), buf, sizeof(buf)) == 0);
        assert(std::strncmp(buf, "hello-bcrypt-aes", 16) == 0);
      }
      wasmwin32_call("BCryptDestroyKey", kh.c_str(), buf, sizeof(buf));
    }
    wasmwin32_call("BCryptCloseAlgorithmProvider", alg.c_str(), buf, sizeof(buf));
  }

  {
    assert(wasmwin32_call("LoadLibraryW", "user32", buf, sizeof(buf)) == 0);
    std::string u32 = buf;
    std::string gpa = u32 + "\x1f" "CreateWindowExW";
    assert(wasmwin32_call("GetProcAddress", gpa.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strtoull(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("FreeLibrary", u32.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GetModuleHandleW", "kernel32", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
  }

  {
    assert(wasmwin32_call("RegisterClassW", "WASMWin32", buf, sizeof(buf)) == 0);
    std::string cw = std::string("WASMWin32") + "\x1f" "wasmwin32-wnd" + "\x1f" "0";
    assert(wasmwin32_call("CreateWindowExW", cw.c_str(), buf, sizeof(buf)) == 0);
    std::string hwnd = buf;
    assert(!hwnd.empty());
    assert(wasmwin32_call("GetClientRect", hwnd.c_str(), buf, sizeof(buf)) == 0);
    std::string set = hwnd + "\x1f" "wasmwin32-title";
    assert(wasmwin32_call("SetWindowTextW", set.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GetWindowTextW", hwnd.c_str(), buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "wasmwin32-title") == 0);
    std::string pm = hwnd + "\x1f" "1024" "\x1f" "1" "\x1f" "2";
    assert(wasmwin32_call("PostMessageW", pm.c_str(), buf, sizeof(buf)) == 0);
    wasmwin32_call("PeekMessageW", hwnd.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("GetDC", hwnd.c_str(), buf, sizeof(buf)) == 0);
    std::string dc = buf;
    assert(wasmwin32_call("CreateSolidBrush", "255", buf, sizeof(buf)) == 0);
    std::string br = buf;
    std::string sel = dc + "\x1f" + br;
    assert(wasmwin32_call("SelectObject", sel.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("DeleteObject", br.c_str(), buf, sizeof(buf)) == 0);
    std::string rel = hwnd + "\x1f" + dc;
    wasmwin32_call("ReleaseDC", rel.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("DestroyWindow", hwnd.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("GetSystemMetrics", "0", buf, sizeof(buf)) == 0);
    assert(std::strtol(buf, nullptr, 10) != 0);
    assert(wasmwin32_call("GetCursorPos", "", buf, sizeof(buf)) == 0);
    assert(std::strchr(buf, '\x1f') != nullptr);
  }

  {
    assert(wasmwin32_call("CoInitializeEx", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("CoCreateGuid", "", buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) == 32);
    assert(wasmwin32_call("CoTaskMemAlloc", "32", buf, sizeof(buf)) == 0);
    std::string mem = buf;
    assert(std::strtoull(mem.c_str(), nullptr, 10) != 0);
    assert(wasmwin32_call("CoTaskMemFree", mem.c_str(), buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("SysAllocString", "hello-bstr", buf, sizeof(buf)) == 0);
    std::string bstr = buf;
    wasmwin32_call("SysStringLen", bstr.c_str(), buf, sizeof(buf));
    wasmwin32_call("SysFreeString", bstr.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("CoUninitialize", "", buf, sizeof(buf)) == 0);
  }

  {
    int cap = wasmwin32_call("WHvGetCapability", "", buf, sizeof(buf));
    if (cap == 0) {
      assert(buf[0] == '0' || buf[0] == '1');
      if (buf[0] == '1') {
        if (wasmwin32_call("WHvCreatePartition", "", buf, sizeof(buf)) == 0) {
          std::string part = buf;
          std::string prop = part + "\x1f" "ProcessorCount" "\x1f" "1";
          assert(wasmwin32_call("WHvSetPartitionProperty", prop.c_str(), buf, sizeof(buf)) == 0);
          std::string gp = part + "\x1f" "ProcessorCount";
          assert(wasmwin32_call("WHvGetPartitionProperty", gp.c_str(), buf, sizeof(buf)) == 0);
          assert(std::strtoul(buf, nullptr, 10) == 1);
          assert(wasmwin32_call("WHvSetupPartition", part.c_str(), buf, sizeof(buf)) == 0);
          assert(wasmwin32_call("GetProcessHeap", "", buf, sizeof(buf)) == 0);
          std::string ph = buf;
          std::string hostmap = part + "\x1f" "33554432" "\x1f" "4096" "\x1f" "7" "\x1f" + ph;
          assert(wasmwin32_call("WHvMapGpaRange", hostmap.c_str(), buf, sizeof(buf)) == 0);
          std::string map = part + "\x1f" "0" "\x1f" "4096";
          assert(wasmwin32_call("WHvMapGpaRange", map.c_str(), buf, sizeof(buf)) == 0);
          std::string wr = part + "\x1f" "0" "\x1f" "guest";
          assert(wasmwin32_call("WHvWriteGpaRange", wr.c_str(), buf, sizeof(buf)) == 0);
          std::string rd = part + "\x1f" "0" "\x1f" "5";
          assert(wasmwin32_call("WHvReadGpaRange", rd.c_str(), buf, sizeof(buf)) == 0);
          assert(std::strncmp(buf, "guest", 5) == 0);
          std::string vp = part + "\x1f" "0";
          assert(wasmwin32_call("WHvCreateVirtualProcessor", vp.c_str(), buf, sizeof(buf)) == 0);
          std::string set = part + "\x1f" "0" "\x1f" "Rip" "\x1f" "0";
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", set.c_str(), buf, sizeof(buf)) ==
                 0);
          std::string getr = part + "\x1f" "0" "\x1f" "Rip";
          assert(wasmwin32_call("WHvGetVirtualProcessorRegisters", getr.c_str(), buf, sizeof(buf)) ==
                 0);
          assert(std::strtoull(buf, nullptr, 10) == 0);
          std::string map2 = part + "\x1f" "4096" "\x1f" "4096";
          assert(wasmwin32_call("WHvMapGpaRange", map2.c_str(), buf, sizeof(buf)) == 0);
          std::string img = part + "\x1f" "4096" "\x1f" "\xf4";
          assert(wasmwin32_call("WHvWriteGpaRange", img.c_str(), buf, sizeof(buf)) == 0);
          std::string cs = part + "\x1f" "0" "\x1f" "Cs" "\x1f" "4096" "\x1f" "4095" "\x1f" "0" "\x1f"
                                 "0x9b";
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", cs.c_str(), buf, sizeof(buf)) == 0);
          std::string ss = part + "\x1f" "0" "\x1f" "Ss" "\x1f" "0" "\x1f" "65535" "\x1f" "0" "\x1f"
                                 "0x93";
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", ss.c_str(), buf, sizeof(buf)) == 0);
          std::string ds = part + "\x1f" "0" "\x1f" "Ds" "\x1f" "0" "\x1f" "65535" "\x1f" "0" "\x1f"
                                 "0x93";
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", ds.c_str(), buf, sizeof(buf)) == 0);
          std::string gdtr = part + "\x1f" "0" "\x1f" "Gdtr" "\x1f" "0" "\x1f" "0";
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", gdtr.c_str(), buf, sizeof(buf)) ==
                 0);
          std::string rf = part + "\x1f" "0" "\x1f" "Rflags" "\x1f" "2";
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", rf.c_str(), buf, sizeof(buf)) == 0);
          std::string setrip = part + "\x1f" "0" "\x1f" "Rip" "\x1f" "0";
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", setrip.c_str(), buf, sizeof(buf)) ==
                 0);
          std::string getcs = part + "\x1f" "0" "\x1f" "Cs";
          assert(wasmwin32_call("WHvGetVirtualProcessorRegisters", getcs.c_str(), buf, sizeof(buf)) ==
                 0);
          assert(std::strtoull(buf, nullptr, 10) == 4096);
          std::string getgd = part + "\x1f" "0" "\x1f" "Gdtr";
          assert(wasmwin32_call("WHvGetVirtualProcessorRegisters", getgd.c_str(), buf, sizeof(buf)) ==
                 0);
          assert(std::strtoull(buf, nullptr, 10) == 0);
          int emu_ok = wasmwin32_call("WHvEmulatorCreateEmulator", part.c_str(), buf, sizeof(buf)) == 0;
          std::string emu = emu_ok ? std::string(buf) : "";
          std::string run = part + "\x1f" "0";
          assert(wasmwin32_call("WHvRunVirtualProcessor", run.c_str(), buf, sizeof(buf)) == 0);
          unsigned reason = (unsigned)std::strtoul(buf, nullptr, 10);
          assert(reason == 8);
          assert(std::strstr(buf, "halt") != nullptr);
          std::string outb = part + "\x1f" "4096" "\x1f" "\xe6\x10";
          assert(wasmwin32_call("WHvWriteGpaRange", outb.c_str(), buf, sizeof(buf)) == 0);
          std::string rax = part + "\x1f" "0" "\x1f" "Rax" "\x1f" "65";
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", rax.c_str(), buf, sizeof(buf)) ==
                 0);
          assert(wasmwin32_call("WHvSetVirtualProcessorRegisters", setrip.c_str(), buf, sizeof(buf)) ==
                 0);
          assert(wasmwin32_call("WHvRunVirtualProcessor", run.c_str(), buf, sizeof(buf)) == 0);
          reason = (unsigned)std::strtoul(buf, nullptr, 10);
          assert(reason == 2);
          if (emu_ok) {
            std::string tryio = emu + "\x1f" + part + "\x1f" "0";
            assert(wasmwin32_call("WHvEmulatorTryIoEmulation", tryio.c_str(), buf, sizeof(buf)) == 0);
            assert(buf[0] == '1');
            wasmwin32_call("WHvEmulatorDestroyEmulator", emu.c_str(), buf, sizeof(buf));
          }
          assert(wasmwin32_call("WHvDeletePartition", part.c_str(), buf, sizeof(buf)) == 0);
        }
      }
    }
    assert(wasmwin32_call("DllMain", "1" "\x1f" "1", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("LoadLibraryW", "kernel32", buf, sizeof(buf)) == 0);
    std::string k32 = buf;
    std::string dm = k32 + "\x1f" "1";
    assert(wasmwin32_call("DllMain", dm.c_str(), buf, sizeof(buf)) == 0);
    wasmwin32_call("FreeLibrary", k32.c_str(), buf, sizeof(buf));
    assert(wasmwin32_call("NtLoadDriver", "", buf, sizeof(buf)) == 0);
    assert(wasmwin32_call("ZwLoadDriver", "", buf, sizeof(buf)) == 0);
  }

  std::printf("win32_test ok pid=%lu\n", pid);
  return 0;
}
