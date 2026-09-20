// Package linux is the wasigocvm guest side of ~/WASMNix: Linux man-pages
// / POSIX names plus WSL Linux-side and Nix CLI. The session sits on
// CHPT, process/thread on TPT, catalog on EPT. Query APIs the libc host
// implements (getpid, uname, getcwd, …) run through
// gocvm.Call("linux"|"wsl"|"nix", …) — wasmnix::posix_call. One occupancy
// path: posix_host.hpp in this module.
package linux

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
	return call("linux", payload)
}

func Getpid() (int, error) {
	s, err := Call("getpid", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func Getppid() (int, error) {
	s, err := Call("getppid", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func Getuid() (int, error) {
	s, err := Call("getuid", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func Uname() (string, error) {
	return Call("uname", "")
}

func Gethostname() (string, error) {
	return Call("gethostname", "")
}

func Getcwd() (string, error) {
	return Call("getcwd", "")
}

func Getenv(name string) (string, error) {
	return Call("getenv", name)
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
