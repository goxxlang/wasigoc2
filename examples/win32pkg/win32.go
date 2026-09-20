// Package win32 is a local copy of stdlib/win32 for this example tree.
// New programs should `import "win32"` (stdlib). Same gocvm.Call surface.
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
