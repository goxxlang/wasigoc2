// Package android is the wasigocvm guest side of ~/WASMDroid: Bionic
// libc names plus Binder and the Android kernel / KVM hop. The session
// sits on CHPT, process/thread on TPT, catalog and Binder root on EPT.
// Query APIs the Bionic host implements (getpid, uname, properties, …)
// run through gocvm.Call("android"|"binder"|"kvm", …) —
// wasmdroid::bionic_call. One occupancy path: bionic_host.hpp in this module.
package android

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
	return call("android", payload)
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

func Uname() (string, error) {
	return Call("uname", "")
}

func Gethostname() (string, error) {
	return Call("gethostname", "")
}

func Getcwd() (string, error) {
	return Call("getcwd", "")
}

func DeviceApiLevel() (int, error) {
	s, err := Call("android_get_device_api_level", "")
	if err != nil {
		return 0, err
	}
	n, perr := strconv.Atoi(s)
	if perr != nil {
		return 0, perr
	}
	return n, nil
}

func PropertyGet(name string) (string, error) {
	return Call("__system_property_get", name)
}

func BinderGet(name string) (string, error) {
	return call("binder", "get\x1f"+name)
}

func KvmCreateVm() (string, error) {
	return call("kvm", "create")
}
