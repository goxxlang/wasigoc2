// Package linux is WSL.
// gocvm.Call("linux"|"wsl"|"nix", …) → wasmnix::wsl_call.
package linux

import (
	"errors"
	"gocvm"
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
	return call("wsl", payload)
}

func List() (string, error) {
	return Call("List", "")
}

func ListOnline() (string, error) {
	return Call("ListOnline", "")
}

func Install(name string) (string, error) {
	return Call("Install", name)
}

func Exec(distro string, command string) (string, error) {
	if distro == "" {
		return Call("Exec", command)
	}
	return Call("Exec", distro+"\x1f"+command)
}

func Read(distro string, path string) (string, error) {
	return Call("Read", distro+"\x1f"+path)
}

func Write(distro string, path string, data string) (string, error) {
	return Call("Write", distro+"\x1f"+path+"\x1f"+data)
}

func Path(flag string, path string) (string, error) {
	return Call("Path", flag+"\x1f"+path)
}

func Status() (string, error) {
	return Call("Status", "")
}

func Shutdown() (string, error) {
	return Call("Shutdown", "")
}

func SetVersion(distro string, version string) (string, error) {
	return Call("SetVersion", distro+"\x1f"+version)
}

func SetDefault(name string) (string, error) {
	return Call("SetDefault", name)
}
