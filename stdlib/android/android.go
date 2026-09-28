// Package android is Phone Link, the Windows YourPhone app.
// gocvm.Call("android"|"phonelink", …) → wasmdroid::phonelink_call.
package android

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
	return call("phonelink", payload)
}

func PackageFamilyName() (string, error) {
	return Call("PackageFamilyName", "")
}

func Aumid() (string, error) {
	return Call("Aumid", "")
}

func Protocol() (string, error) {
	return Call("Protocol", "")
}

func Open(path string) (string, error) {
	return Call("Open", path)
}

func Status() (string, error) {
	return Call("Status", "")
}

func Search(query string) (string, error) {
	return Call("Search", query)
}

func Read(id string) (string, error) {
	return Call("Read", id)
}

func Write(kind string, peer string, body string) (string, error) {
	return Call("Write", kind+"\x1f"+peer+"\x1f"+body)
}

func Monitor(kind string) (string, error) {
	return Call("Monitor", kind)
}

func SmsList() (string, error) {
	return Call("Sms", "list")
}

func SmsSend(to string, body string) (string, error) {
	return Call("Sms", "send\x1f"+to+"\x1f"+body)
}

func Photos() (string, error) {
	return Call("Photos", "")
}

func Notifications() (string, error) {
	return Call("Notifications", "")
}

func Calls() (string, error) {
	return Call("Calls", "")
}

func Apps() (string, error) {
	return Call("Apps", "")
}
