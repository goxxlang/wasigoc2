// Package syscall: Getpid/Getwd/Getenv/Environ/Chdir are win32metadata
// names (`GetCurrentProcessId`, `GetCurrentDirectoryW`, …) via
// gocvm.Call("win32", ...). Kill/Getppid stay the in-module libc
// syscall topic. Getpid is the occupancy table (not WASI emulated
// getpid). Without a machine Getpid/Getppid report 1 and 0.
package syscall

import (
	"errors"
	"gocvm"
	"strconv"
	"strings"
)

func isRealError(reply string) bool {
	return strings.HasPrefix(reply, "error:")
}

func Getpid() int {
	reply, err := gocvm.Call("win32", "GetCurrentProcessId")
	if err != nil || isRealError(reply) {
		reply, err = gocvm.Call("syscall", "getpid")
		if err != nil {
			return 1
		}
	}
	n, perr := strconv.Atoi(reply)
	if perr != nil {
		return 1
	}
	return n
}

func Getppid() int {
	reply, err := gocvm.Call("syscall", "getppid")
	if err != nil {
		return 0
	}
	n, perr := strconv.Atoi(reply)
	if perr != nil {
		return 0
	}
	return n
}

func Getwd() (string, error) {
	reply, err := gocvm.Call("win32", "GetCurrentDirectoryW")
	if err != nil {
		return "", err
	}
	if isRealError(reply) {
		return "", errors.New(reply)
	}
	if reply == "" {
		return ".", nil
	}
	return reply, nil
}

func Chdir(dir string) error {
	reply, err := gocvm.Call("win32", "SetCurrentDirectoryW\x1f"+dir)
	if err != nil {
		return err
	}
	if isRealError(reply) {
		return errors.New(reply)
	}
	return nil
}

func Kill(pid int, sig int) error {
	reply, err := gocvm.Call("syscall", "kill "+strconv.Itoa(pid)+" "+strconv.Itoa(sig))
	if err != nil {
		return err
	}
	if isRealError(reply) {
		return errors.New(reply)
	}
	return nil
}

// reply is \x1f-joined "KEY=VALUE" entries (syscall environ).
func Environ() []string {
	reply, err := gocvm.Call("win32", "GetEnvironmentStringsW")
	if err != nil || reply == "" || isRealError(reply) {
		return nil
	}
	return strings.Split(reply, "\x1f")
}

func Getenv(key string) (string, bool) {
	reply, err := gocvm.Call("win32", "GetEnvironmentVariableW\x1f"+key)
	if err != nil || isRealError(reply) || reply == "" {
		return "", false
	}
	return reply, true
}

func ByteSliceFromString(s string) ([]byte, error) {
	return append([]byte(s), 0), nil
}
