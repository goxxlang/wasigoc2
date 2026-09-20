// Package win32 is the wasigocvm guest side of ~/WASMWin32: a
// win32metadata-named projection (Windows.Win32.*) plus WSL/Nix.
// The session sits on CHPT (token/SID/PEB/TEB/HWND/GDI/COM), process/thread
// on TPT, catalog/vmem/sock/cng/modules/heaps on EPT. Query APIs and WslExec names
// the libc host implements (uname, echo, true, …) run through
// gocvm.Call("win32"|"wsl"|"nix", …) — win32 is WASMWin32 wasi_call;
// wsl/nix are ~/WASMNix posix_call (same topics os/exec and linux use).
// CreateProcessW is a std::thread child on that hop (kernel32
// CreateProcessW on native). LoadLibrary is the same hop as the rest of
// kernel32 (PE maps through ~/WASMPELoader).
// MainDLL (DllMain / TLS) runs on that hop via WHvRunVirtualProcessor.
// Nix and unknown commands stay honest errors. One occupancy path:
// wasi_host.hpp in this module.
package win32

import (
	"errors"
	"gocvm"
	"strconv"
	"strings"
)

func isRealError(reply string) bool {
	return strings.HasPrefix(reply, "error:")
}

func call(topic string, payload string) (string, error) {
	reply, err := gocvm.Call(topic, payload)
	if err != nil {
		return "", err
	}
	if isRealError(reply) {
		return "", errors.New(reply)
	}
	return reply, nil
}

func Call(api string, arg string) (string, error) {
	payload := api
	if arg != "" {
		payload = api + "\x1f" + arg
	}
	return call("win32", payload)
}

func GetCurrentProcessId() (int, error) {
	s, err := Call("GetCurrentProcessId", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func GetCurrentThreadId() (int, error) {
	s, err := Call("GetCurrentThreadId", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func GetTickCount64() (int64, error) {
	s, err := Call("GetTickCount64", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.ParseInt(s, 10, 64)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func GetComputerName() (string, error) {
	return Call("GetComputerNameW", "")
}

func GetWindowsDirectory() (string, error) {
	return Call("GetWindowsDirectoryW", "")
}

func GetSystemDirectory() (string, error) {
	return Call("GetSystemDirectoryW", "")
}

func GetCurrentDirectory() (string, error) {
	return Call("GetCurrentDirectoryW", "")
}

func GetEnvironmentVariable(name string) (string, error) {
	return Call("GetEnvironmentVariableW", name)
}

func GetFileAttributes(path string) (int, error) {
	s, err := Call("GetFileAttributesW", path)
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func SetEnvironmentVariable(name string, value string) error {
	_, err := Call("SetEnvironmentVariableW", name+"\x1f"+value)
	return err
}

func GetEnvironmentStrings() (string, error) {
	return Call("GetEnvironmentStringsW", "")
}

func SetCurrentDirectory(path string) error {
	_, err := Call("SetCurrentDirectoryW", path)
	return err
}

func GetCommandLine() (string, error) {
	return Call("GetCommandLineW", "")
}

func ExpandEnvironmentStrings(s string) (string, error) {
	return Call("ExpandEnvironmentStringsW", s)
}

func GetFullPathName(path string) (string, error) {
	return Call("GetFullPathNameW", path)
}

func GetTempPath() (string, error) {
	return Call("GetTempPathW", "")
}

func CreateDirectory(path string) error {
	_, err := Call("CreateDirectoryW", path)
	return err
}

func RemoveDirectory(path string) error {
	_, err := Call("RemoveDirectoryW", path)
	return err
}

func DeleteFile(path string) error {
	_, err := Call("DeleteFileW", path)
	return err
}

func MoveFile(src string, dst string) error {
	_, err := Call("MoveFileW", src+"\x1f"+dst)
	return err
}

func CopyFile(src string, dst string) error {
	_, err := Call("CopyFileW", src+"\x1f"+dst)
	return err
}

func GetUserName() (string, error) {
	return Call("GetUserNameW", "")
}

func GetSystemTime() (string, error) {
	return Call("GetSystemTime", "")
}

func GetLocalTime() (string, error) {
	return Call("GetLocalTime", "")
}

func GetSystemInfo() (string, error) {
	return Call("GetSystemInfo", "")
}

func GetACP() (int, error) {
	s, err := Call("GetACP", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func GetStdHandle(id string) (string, error) {
	return Call("GetStdHandle", id)
}

func CreateProcess(app string, cmdLine string) (pid int, process string, thread string, err error) {
	arg := app
	if cmdLine != "" {
		arg = app + "\x1f" + cmdLine
	}
	s, err := Call("CreateProcessW", arg)
	if err != nil {
		return 0, "", "", err
	}
	parts := strings.Split(s, "\x1f")
	if len(parts) < 3 {
		return 0, "", "", errors.New("win32: CreateProcessW: bad reply")
	}
	n, perr := strconv.Atoi(parts[0])
	if perr != nil {
		return 0, "", "", perr
	}
	return n, parts[1], parts[2], nil
}

func WaitForSingleObject(handle string, timeoutMs int) (int, error) {
	s, err := Call("WaitForSingleObject", handle+"\x1f"+strconv.Itoa(timeoutMs))
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func GetExitCodeProcess(handle string) (int, error) {
	s, err := Call("GetExitCodeProcess", handle)
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func CloseHandle(handle string) error {
	_, err := Call("CloseHandle", handle)
	return err
}

func CreateFile(path string, access uint32, disp uint32) (string, error) {
	return Call("CreateFileW", path+"\x1f"+strconv.FormatUint(uint64(access), 10)+"\x1f"+strconv.FormatUint(uint64(disp), 10))
}

func ReadFile(handle string, n int) (string, error) {
	return Call("ReadFile", handle+"\x1f"+strconv.Itoa(n))
}

func WriteFile(handle string, data string) (int, error) {
	s, err := Call("WriteFile", handle+"\x1f"+data)
	if err != nil {
		return 0, err
	}
	got, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return got, nil
}

func CreateEvent(manualReset bool, initialState bool) (string, error) {
	m := "0"
	if manualReset {
		m = "1"
	}
	i := "0"
	if initialState {
		i = "1"
	}
	return Call("CreateEventW", m+"\x1f"+i)
}

func SetEvent(handle string) error {
	_, err := Call("SetEvent", handle)
	return err
}

func ResetEvent(handle string) error {
	_, err := Call("ResetEvent", handle)
	return err
}

func CreateMutex(initialOwner bool) (string, error) {
	v := "0"
	if initialOwner {
		v = "1"
	}
	return Call("CreateMutexW", v)
}

func ReleaseMutex(handle string) error {
	_, err := Call("ReleaseMutex", handle)
	return err
}

func WaitForMultipleObjects(handles []string, waitAll bool, timeoutMs int) (int, error) {
	parts := make([]string, 0, len(handles)+3)
	parts = append(parts, strconv.Itoa(len(handles)))
	parts = append(parts, handles...)
	parts = append(parts, strconv.Itoa(timeoutMs))
	if waitAll {
		parts = append(parts, "1")
	} else {
		parts = append(parts, "0")
	}
	s, err := Call("WaitForMultipleObjects", strings.Join(parts, "\x1f"))
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func CreatePipe() (read string, write string, err error) {
	s, err := Call("CreatePipe", "")
	if err != nil {
		return "", "", err
	}
	parts := strings.Split(s, "\x1f")
	if len(parts) < 2 {
		return "", "", errors.New("win32: CreatePipe: bad reply")
	}
	return parts[0], parts[1], nil
}

func SearchPath(name string) (string, error) {
	return Call("SearchPathW", name)
}

func GetDriveType(root string) (int, error) {
	s, err := Call("GetDriveTypeW", root)
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func GetFileTime(handle string) (string, error) {
	return Call("GetFileTime", handle)
}

func CharUpper(s string) (string, error) {
	return Call("CharUpperW", s)
}

func MulDiv(n, num, den int) (int, error) {
	s, err := Call("MulDiv", strconv.Itoa(n)+"\x1f"+strconv.Itoa(num)+"\x1f"+strconv.Itoa(den))
	if err != nil {
		return 0, err
	}
	v, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return v, nil
}

func InitializeCriticalSection() (string, error) {
	return Call("InitializeCriticalSection", "")
}

func EnterCriticalSection(cs string) error {
	_, err := Call("EnterCriticalSection", cs)
	return err
}

func LeaveCriticalSection(cs string) error {
	_, err := Call("LeaveCriticalSection", cs)
	return err
}

func DeleteCriticalSection(cs string) error {
	_, err := Call("DeleteCriticalSection", cs)
	return err
}

func CreateFileMapping(size uint32) (string, error) {
	return Call("CreateFileMappingW", "0"+"\x1f"+"4"+"\x1f"+strconv.FormatUint(uint64(size), 10))
}

func MapViewOfFile(mapping string) (string, error) {
	return Call("MapViewOfFile", mapping)
}

func UnmapViewOfFile(view string) error {
	_, err := Call("UnmapViewOfFile", view)
	return err
}

func CreateIoCompletionPort() (string, error) {
	return Call("CreateIoCompletionPort", "")
}

func PostQueuedCompletionStatus(port string, bytes int, key int) error {
	_, err := Call("PostQueuedCompletionStatus", port+"\x1f"+strconv.Itoa(bytes)+"\x1f"+strconv.Itoa(key))
	return err
}

func GetQueuedCompletionStatus(port string, timeoutMs int) (string, error) {
	return Call("GetQueuedCompletionStatus", port+"\x1f"+strconv.Itoa(timeoutMs))
}

func GlobalAddAtom(name string) (string, error) {
	return Call("GlobalAddAtomW", name)
}

func CreateHardLink(newPath string, existing string) error {
	_, err := Call("CreateHardLinkW", newPath+"\x1f"+existing)
	return err
}

func CreateToolhelp32Snapshot() (string, error) {
	return Call("CreateToolhelp32Snapshot", "2")
}

func Process32First(snap string) (string, error) {
	return Call("Process32FirstW", snap)
}

func WritePrivateProfileString(section, key, value, file string) error {
	_, err := Call("WritePrivateProfileStringW", section+"\x1f"+key+"\x1f"+value+"\x1f"+file)
	return err
}

func GetPrivateProfileString(section, key, def, file string) (string, error) {
	return Call("GetPrivateProfileStringW", section+"\x1f"+key+"\x1f"+def+"\x1f"+file)
}

func CreateWaitableTimer(manualReset bool) (string, error) {
	v := "0"
	if manualReset {
		v = "1"
	}
	return Call("CreateWaitableTimerW", v)
}

func SetWaitableTimer(handle string, dueMs int) error {
	_, err := Call("SetWaitableTimer", handle+"\x1f"+strconv.Itoa(dueMs))
	return err
}

func CancelWaitableTimer(handle string) error {
	_, err := Call("CancelWaitableTimer", handle)
	return err
}

func CreateJobObject() (string, error) {
	return Call("CreateJobObjectW", "")
}

func AssignProcessToJobObject(job, process string) error {
	_, err := Call("AssignProcessToJobObject", job+"\x1f"+process)
	return err
}

func IsProcessInJob(process, job string) (bool, error) {
	s, err := Call("IsProcessInJob", process+"\x1f"+job)
	if err != nil {
		return false, err
	}
	return s == "1", nil
}

func QueryInformationJobObject(job string) (string, error) {
	return Call("QueryInformationJobObject", job)
}

func InitializeSRWLock() (string, error) {
	return Call("InitializeSRWLock", "")
}

func AcquireSRWLockExclusive(srw string) error {
	_, err := Call("AcquireSRWLockExclusive", srw)
	return err
}

func ReleaseSRWLockExclusive(srw string) error {
	_, err := Call("ReleaseSRWLockExclusive", srw)
	return err
}

func InitializeConditionVariable() (string, error) {
	return Call("InitializeConditionVariable", "")
}

func SleepConditionVariableCS(cv, cs string, timeoutMs int) (bool, error) {
	s, err := Call("SleepConditionVariableCS", cv+"\x1f"+cs+"\x1f"+strconv.Itoa(timeoutMs))
	if err != nil {
		return false, err
	}
	return s == "1", nil
}

func OpenThread(tid int) (string, error) {
	return Call("OpenThread", strconv.Itoa(tid))
}

func GetThreadTimes(thread string) (string, error) {
	return Call("GetThreadTimes", thread)
}

func SignalObjectAndWait(signal, wait string, timeoutMs int) (int, error) {
	s, err := Call("SignalObjectAndWait", signal+"\x1f"+wait+"\x1f"+strconv.Itoa(timeoutMs))
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func GetSystemPowerStatus() (string, error) {
	return Call("GetSystemPowerStatus", "")
}

func FindFirstVolume() (string, error) {
	return Call("FindFirstVolumeW", "")
}

func GetVolumeNameForVolumeMountPoint(mount string) (string, error) {
	return Call("GetVolumeNameForVolumeMountPointW", mount)
}

func CompareStringOrdinal(a, b string) (int, error) {
	s, err := Call("CompareStringOrdinal", a+"\x1f"+b)
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func LocaleNameToLCID(name string) (int, error) {
	s, err := Call("LocaleNameToLCID", name)
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func HeapSize(ptr string) (int, error) {
	s, err := Call("HeapSize", ptr)
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func QueueUserWorkItem() error {
	_, err := Call("QueueUserWorkItem", "")
	return err
}

func ProcessIdToSessionId() (int, error) {
	s, err := Call("ProcessIdToSessionId", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func EncodePointer(p string) (string, error) {
	return Call("EncodePointer", p)
}

func OpenEvent(name string) (string, error) {
	return Call("OpenEventW", name)
}

func CreateThreadpoolWork() (string, error) {
	return Call("CreateThreadpoolWork", "")
}

func SubmitThreadpoolWork(work string) error {
	_, err := Call("SubmitThreadpoolWork", work)
	return err
}

func WaitForThreadpoolWorkCallbacks(work string) error {
	_, err := Call("WaitForThreadpoolWorkCallbacks", work)
	return err
}

func CreateTimerQueue() (string, error) {
	return Call("CreateTimerQueue", "")
}

func GetUserDefaultLocaleName() (string, error) {
	return Call("GetUserDefaultLocaleName", "")
}

func GetTempPath2() (string, error) {
	return Call("GetTempPath2W", "")
}

func K32GetProcessMemoryInfo() (string, error) {
	return Call("K32GetProcessMemoryInfo", "")
}

func WaitOnAddress(ptr string, expected int, timeoutMs int) (bool, error) {
	s, err := Call("WaitOnAddress", ptr+"\x1f"+strconv.Itoa(expected)+"\x1f"+strconv.Itoa(timeoutMs))
	if err != nil {
		return false, err
	}
	return s == "1", nil
}

func RegisterWaitForSingleObject(handle string, timeoutMs int) (string, error) {
	return Call("RegisterWaitForSingleObject", handle+"\x1f"+strconv.Itoa(timeoutMs))
}

func ConvertThreadToFiber() (string, error) {
	return Call("ConvertThreadToFiber", "")
}

func InitializeSynchronizationBarrier(count int) (string, error) {
	return Call("InitializeSynchronizationBarrier", strconv.Itoa(count))
}

func RegCreateKey(hive string, subkey string) (string, error) {
	s, err := Call("RegCreateKeyExW", hive+"\x1f"+subkey)
	if err != nil {
		return "", err
	}
	if i := strings.IndexByte(s, '\x1f'); i >= 0 {
		return s[:i], nil
	}
	return s, nil
}

func RegOpenKey(hive string, subkey string) (string, error) {
	return Call("RegOpenKeyExW", hive+"\x1f"+subkey)
}

func RegSetValue(key, name string, typ int, data string) error {
	_, err := Call("RegSetValueExW", key+"\x1f"+name+"\x1f"+strconv.Itoa(typ)+"\x1f"+data)
	return err
}

func RegQueryValue(key, name string) (string, error) {
	return Call("RegQueryValueExW", key+"\x1f"+name)
}

func RegDeleteValue(key, name string) error {
	_, err := Call("RegDeleteValueW", key+"\x1f"+name)
	return err
}

func RegDeleteKey(hive, subkey string) error {
	_, err := Call("RegDeleteKeyW", hive+"\x1f"+subkey)
	return err
}

func RegCloseKey(key string) error {
	_, err := Call("RegCloseKey", key)
	return err
}

func RegQueryInfoKey(key string) (string, error) {
	return Call("RegQueryInfoKeyW", key)
}

func RegGetValue(hive, subkey, name string) (string, error) {
	return Call("RegGetValueW", hive+"\x1f"+subkey+"\x1f"+name)
}

func OpenProcessToken(proc string) (string, error) {
	return Call("OpenProcessToken", proc)
}

func GetTokenInformation(token string, class int) (string, error) {
	return Call("GetTokenInformation", token+"\x1f"+strconv.Itoa(class))
}

func LookupAccountSid(sid string) (string, error) {
	return Call("LookupAccountSidW", sid)
}

func CheckTokenMembership(token, sid string) (string, error) {
	return Call("CheckTokenMembership", token+"\x1f"+sid)
}

func VirtualAllocEx(proc string, size int) (string, error) {
	return Call("VirtualAllocEx", proc+"\x1f"+strconv.Itoa(size))
}

func ReadProcessMemory(proc, addr string, n int) (string, error) {
	return Call("ReadProcessMemory", proc+"\x1f"+addr+"\x1f"+strconv.Itoa(n))
}

func WriteProcessMemory(proc, addr, data string) (string, error) {
	return Call("WriteProcessMemory", proc+"\x1f"+addr+"\x1f"+data)
}

func DeviceIoControl(handle, code, input string) (string, error) {
	arg := handle + "\x1f" + code
	if input != "" {
		arg += "\x1f" + input
	}
	return Call("DeviceIoControl", arg)
}

func WSAStartup() (string, error) {
	return Call("WSAStartup", "")
}

func WSASocket() (string, error) {
	return Call("WSASocketW", "")
}

func WSABind(sock, name string) (string, error) {
	return Call("bind", sock+"\x1f"+name)
}

func WSAListen(sock string) error {
	_, err := Call("listen", sock)
	return err
}

func WSAConnect(sock, name string) error {
	_, err := Call("connect", sock+"\x1f"+name)
	return err
}

func WSAAccept(sock string) (string, error) {
	return Call("accept", sock)
}

func WSASend(sock, data string) (string, error) {
	return Call("WSASend", sock+"\x1f"+data)
}

func WSARecv(sock string, n int) (string, error) {
	return Call("WSARecv", sock+"\x1f"+strconv.Itoa(n))
}

func CloseSocket(sock string) error {
	_, err := Call("closesocket", sock)
	return err
}

func FindFirstChangeNotification(path string) (string, error) {
	return Call("FindFirstChangeNotificationW", path)
}

func GetNamedPipeInfo(handle string) (string, error) {
	return Call("GetNamedPipeInfo", handle)
}

func GetHandleInformation(handle string) (string, error) {
	return Call("GetHandleInformation", handle)
}

func SetThreadDescription(thread, name string) error {
	_, err := Call("SetThreadDescription", thread+"\x1f"+name)
	return err
}

func GetCurrentThreadStackLimits() (string, error) {
	return Call("GetCurrentThreadStackLimits", "")
}

func FileTimeToDosDateTime(ft string) (string, error) {
	return Call("FileTimeToDosDateTime", ft)
}

func NtQueryInformationProcess(proc string, classID int) (string, error) {
	return Call("NtQueryInformationProcess", proc+"\x1f"+strconv.Itoa(classID))
}

func RtlGetCurrentPeb() (string, error) {
	return Call("RtlGetCurrentPeb", "")
}

func NtCreateSection(size int) (string, error) {
	return Call("NtCreateSection", strconv.Itoa(size))
}

func NtMapViewOfSection(section string) (string, error) {
	return Call("NtMapViewOfSection", section)
}

func NtAllocateVirtualMemory(proc string, size int) (string, error) {
	return Call("NtAllocateVirtualMemory", proc+"\x1f"+strconv.Itoa(size))
}

func NtFreeVirtualMemory(proc, addr string) error {
	_, err := Call("NtFreeVirtualMemory", proc+"\x1f"+addr)
	return err
}

func NtCreateFile(path string) (string, error) {
	return Call("NtCreateFile", path)
}

func NtClose(h string) error {
	_, err := Call("NtClose", h)
	return err
}

func AddVectoredExceptionHandler() (string, error) {
	return Call("AddVectoredExceptionHandler", "")
}

func RaiseException(code string) (string, error) {
	return Call("RaiseException", code)
}

func BCryptOpenAlgorithmProvider(alg string) (string, error) {
	return Call("BCryptOpenAlgorithmProvider", alg)
}

func BCryptGenRandom(n int) (string, error) {
	return Call("BCryptGenRandom", strconv.Itoa(n))
}

func BCryptCreateHash(alg string) (string, error) {
	return Call("BCryptCreateHash", alg)
}

func BCryptHashData(hash, data string) error {
	_, err := Call("BCryptHashData", hash+"\x1f"+data)
	return err
}

func BCryptFinishHash(hash string) (string, error) {
	return Call("BCryptFinishHash", hash)
}

func NCryptOpenStorageProvider(name string) (string, error) {
	return Call("NCryptOpenStorageProvider", name)
}

func NCryptGenRandom(prov string, n int) (string, error) {
	return Call("NCryptGenRandom", prov+"\x1f"+strconv.Itoa(n))
}

func NCryptFreeObject(h string) error {
	_, err := Call("NCryptFreeObject", h)
	return err
}

func LdrLoadDll(name string) (string, error) {
	return Call("LdrLoadDll", name)
}

func LdrGetProcedureAddress(module, name string) (string, error) {
	return Call("LdrGetProcedureAddress", module+"\x1f"+name)
}

func RtlImageNtHeader(module string) (string, error) {
	return Call("RtlImageNtHeader", module)
}

func RtlGetVersion() (string, error) {
	return Call("RtlGetVersion", "")
}

func RtlAllocateHeap(n int) (string, error) {
	return Call("RtlAllocateHeap", strconv.Itoa(n))
}

func RtlFreeHeap(p string) error {
	_, err := Call("RtlFreeHeap", p)
	return err
}

func NtCreateSemaphore(initial, max int) (string, error) {
	return Call("NtCreateSemaphore", strconv.Itoa(initial)+"\x1f"+strconv.Itoa(max))
}

func NtQueryTimerResolution() (string, error) {
	return Call("NtQueryTimerResolution", "")
}

func NtCreateTimer() (string, error) {
	return Call("NtCreateTimer", "")
}

func NtCreateJobObject(name string) (string, error) {
	return Call("NtCreateJobObject", name)
}

func NtCreateIoCompletion() (string, error) {
	return Call("NtCreateIoCompletion", "")
}

func RtlInitializeCriticalSection() (string, error) {
	return Call("RtlInitializeCriticalSection", "")
}

func RtlDosPathNameToNtPathName(path string) (string, error) {
	return Call("RtlDosPathNameToNtPathName_U", path)
}

func RtlIsProcessorFeaturePresent(feature int) (string, error) {
	return Call("RtlIsProcessorFeaturePresent", strconv.Itoa(feature))
}

func TpAllocPool() (string, error) {
	return Call("TpAllocPool", "")
}

func CryptProtectData(data string) (string, error) {
	return Call("CryptProtectData", data)
}

func CryptUnprotectData(blob string) (string, error) {
	return Call("CryptUnprotectData", blob)
}

func CertOpenStore(name string) (string, error) {
	return Call("CertOpenStore", name)
}

func SHGetFolderPath(csidl int) (string, error) {
	return Call("SHGetFolderPathW", strconv.Itoa(csidl))
}

func PathFileExists(path string) (bool, error) {
	s, err := Call("PathFileExistsW", path)
	if err != nil {
		return false, err
	}
	return s == "1", nil
}

func WinHttpOpen(agent string) (string, error) {
	return Call("WinHttpOpen", agent)
}

func WinHttpCrackUrl(url string) (string, error) {
	return Call("WinHttpCrackUrl", url)
}

func WinHttpCloseHandle(h string) error {
	_, err := Call("WinHttpCloseHandle", h)
	return err
}

func GetAdaptersAddresses() (string, error) {
	return Call("GetAdaptersAddresses", "")
}

func GetFileVersionInfo(path string) (string, error) {
	return Call("GetFileVersionInfoW", path)
}

func InternetOpen(agent string) (string, error) {
	return Call("InternetOpenW", agent)
}

func GetUserNameEx() (string, error) {
	return Call("GetUserNameExW", "")
}

func DnsNameCompare(a, b string) (bool, error) {
	s, err := Call("DnsNameCompare_A", a+"\x1f"+b)
	if err != nil {
		return false, err
	}
	return s == "1", nil
}

func UuidCreate() (string, error) {
	return Call("UuidCreate", "")
}

func SetupDiGetClassDevs() (string, error) {
	return Call("SetupDiGetClassDevsW", "")
}

func NetGetJoinInformation() (string, error) {
	return Call("NetGetJoinInformation", "")
}

func PdhOpenQuery() (string, error) {
	return Call("PdhOpenQueryW", "")
}

func EvtQuery(path, query string) (string, error) {
	return Call("EvtQuery", path+"\x1f"+query)
}

func PathIsRelative(path string) (bool, error) {
	s, err := Call("PathIsRelativeW", path)
	if err != nil {
		return false, err
	}
	return s == "1", nil
}

func GetFileTitle(path string) (string, error) {
	return Call("GetFileTitleW", path)
}

func LoadLibrary(name string) (string, error) {
	return Call("LoadLibraryW", name)
}

func DllMain(module string, reason int) error {
	_, err := Call("DllMain", module+"\x1f"+strconv.Itoa(reason))
	return err
}

func WHvGetCapability() (string, error) {
	return Call("WHvGetCapability", "")
}

func WHvCreatePartition() (string, error) {
	return Call("WHvCreatePartition", "")
}

func WHvSetPartitionProperty(part, code, value string) error {
	_, err := Call("WHvSetPartitionProperty", part+"\x1f"+code+"\x1f"+value)
	return err
}

func WHvGetPartitionProperty(part, code string) (string, error) {
	return Call("WHvGetPartitionProperty", part+"\x1f"+code)
}

func WHvMapGpaRange(part string, gpa, size int) error {
	_, err := Call("WHvMapGpaRange", part+"\x1f"+strconv.Itoa(gpa)+"\x1f"+strconv.Itoa(size))
	return err
}

func WHvSetVirtualProcessorRegisters(part string, vp int, name, value string) error {
	_, err := Call("WHvSetVirtualProcessorRegisters",
		part+"\x1f"+strconv.Itoa(vp)+"\x1f"+name+"\x1f"+value)
	return err
}

func WHvGetVirtualProcessorRegisters(part string, vp int, name string) (string, error) {
	return Call("WHvGetVirtualProcessorRegisters", part+"\x1f"+strconv.Itoa(vp)+"\x1f"+name)
}

func WHvRunVirtualProcessor(part string, vp int) (string, error) {
	return Call("WHvRunVirtualProcessor", part+"\x1f"+strconv.Itoa(vp))
}

func WHvWriteGpaRange(part string, gpa int, image string) error {
	_, err := Call("WHvWriteGpaRange", part+"\x1f"+strconv.Itoa(gpa)+"\x1f"+image)
	return err
}

func WHvEmulatorCreateEmulator() (string, error) {
	return Call("WHvEmulatorCreateEmulator", "")
}

func WHvEmulatorDestroyEmulator(emu string) error {
	_, err := Call("WHvEmulatorDestroyEmulator", emu)
	return err
}

func WHvEmulatorTryIoEmulation(emu, part string, vp int) (string, error) {
	return Call("WHvEmulatorTryIoEmulation", emu+"\x1f"+part+"\x1f"+strconv.Itoa(vp))
}

func WHvEmulatorTryMmioEmulation(emu, part string, vp int) (string, error) {
	return Call("WHvEmulatorTryMmioEmulation", emu+"\x1f"+part+"\x1f"+strconv.Itoa(vp))
}

func GetProcAddress(module, name string) (string, error) {
	return Call("GetProcAddress", module+"\x1f"+name)
}

func CreateWindowEx(className, title string) (string, error) {
	return Call("CreateWindowExW", className+"\x1f"+title)
}

func DestroyWindow(hwnd string) error {
	_, err := Call("DestroyWindow", hwnd)
	return err
}

func GetDC(hwnd string) (string, error) {
	return Call("GetDC", hwnd)
}

func CreateSolidBrush(color int) (string, error) {
	return Call("CreateSolidBrush", strconv.Itoa(color))
}

func CoInitializeEx() error {
	_, err := Call("CoInitializeEx", "")
	return err
}

func CoCreateGuid() (string, error) {
	return Call("CoCreateGuid", "")
}

func CoUninitialize() error {
	_, err := Call("CoUninitialize", "")
	return err
}

func WslList() (string, error) {
	return call("wsl", "list")
}

func WslExec(cmd string) (string, error) {
	return call("wsl", "exec\x1f"+cmd)
}

func NixVersion() (string, error) {
	return call("nix", "version")
}

func NixRun(args string) (string, error) {
	return call("nix", "run\x1f"+args)
}
