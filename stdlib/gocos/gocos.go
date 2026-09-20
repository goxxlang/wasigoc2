// Package gocos is the wasigocvm edge kernel. One guest binding:
// gocvm.Call("gocos", "Boot") brings the OS up on gocvm.wasm —
// GocKrnl session, GocSys subsystem, GocDesk shell. After boot every
// hop is gocvm hypervision: k32 (~/WASMWin32) and nix (~/WASMNix) for
// WSL. Memory is gocvm vmem (EPT + VirtualAlloc / mmap).
package gocos

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

func payload(api string, arg string) string {
	if arg == "" {
		return api
	}
	return api + "\x1f" + arg
}

func Call(api string, arg string) (string, error) {
	return call("gocos", payload(api, arg))
}

func Boot() (string, error) {
	return Call("Boot", "")
}

func Halt() (string, error) {
	return Call("Halt", "")
}

func Session() (string, error) {
	return Call("Session", "")
}

func Station() (string, error) {
	return Call("Station", "")
}

func Version() (string, error) {
	return Call("Version", "")
}

func CreateProcess(app string) (string, error) {
	return Call("CreateProcess", app)
}

func Alloc(n int) (string, error) {
	return Call("Alloc", strconv.Itoa(n))
}

func Free(addr string) (string, error) {
	return Call("Free", addr)
}

func LoadModule(name string) (string, error) {
	return Call("LoadModule", name)
}

func CreateWindow(class string) (string, error) {
	return Call("CreateWindow", class)
}

func Present() (string, error) {
	return Call("Present", "")
}

func IFrame() (string, error) {
	return Call("IFrame", "")
}

func Launch(cmd string) (string, error) {
	return Call("Launch", cmd)
}

func List() (string, error) {
	return Call("List", "")
}

func Registered(name string) (bool, error) {
	s, err := Call("Registered", name)
	if err != nil {
		return false, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return s == "1" || len(s) > 0, nil
	}
	return n != 0, nil
}

func Cmd(line string) (string, error) {
	return Call("Cmd", line)
}

func Pwsh(line string) (string, error) {
	return Call("Pwsh", line)
}

func Conhost() (string, error) {
	return Call("Conhost", "")
}

func Prompt() (string, error) {
	return Call("Prompt", "")
}

func Shell() (string, error) {
	return Call("Shell", "")
}

func OccupyCmd(image string) (string, error) {
	if image == "" {
		image = "cmd.exe"
	}
	return Call("OccupyCmd", image)
}

func OccupyCalc(image string) (string, error) {
	if image == "" {
		image = "calc.exe"
	}
	return Call("OccupyCalc", image)
}

func OccupyConsole() (string, error) {
	return Call("Occupy", "")
}

func OccupyProcess() (string, error) {
	return Call("CreateProcess", "")
}

func OccupyWasmtty() (string, error) {
	return Call("Conhost", "")
}

func OccupyGocvm() (string, error) {
	return Call("Boot", "")
}

// Compat aliases used by older guests. They still hop gocos → k32/nix.

func RtlGetVersion() (string, error) {
	return Version()
}

func NtCreateProcess(app string) (string, error) {
	return CreateProcess(app)
}

func WslLaunch(cmd string) (string, error) {
	return Launch(cmd)
}

func WslList() (string, error) {
	return List()
}

func WslIsDistributionRegistered(name string) (bool, error) {
	return Registered(name)
}

func WineGetVersion() (string, error) {
	return Version()
}

func WineInit() (string, error) {
	return Boot()
}

func LoadLibraryW(name string) (string, error) {
	return LoadModule(name)
}

func CreateWindowExW(class string) (string, error) {
	return CreateWindow(class)
}

func DesktopPresent() (string, error) {
	return Present()
}

func DesktopIFrame() (string, error) {
	return IFrame()
}

func CallWin32(api string, arg string) (string, error) {
	return Call(api, arg)
}

func CallDesktop(api string, arg string) (string, error) {
	return Call(api, arg)
}

func CallLinux(api string, arg string) (string, error) {
	return Call(api, arg)
}
