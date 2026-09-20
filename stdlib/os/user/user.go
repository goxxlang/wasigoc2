// Package user: libc USER/USERNAME/HOME/uid inside the module via
// gocvm.Call. Not GetUserNameW as a host hop.
package user

import (
	"errors"
	"gocvm"
	"strings"
)

func isRealError(reply string) bool {
	return strings.HasPrefix(reply, "error:")
}

type User struct {
	Uid      string
	Gid      string
	Username string
	Name     string
	HomeDir  string
}

// "<uid>\x1f<username>\x1f<name>\x1f<homedir>" -- os.user reply.
func parseUser(reply string) (*User, error) {
	f := strings.Split(reply, "\x1f")
	if len(f) != 4 {
		return nil, errors.New("os/user: malformed reply")
	}
	return &User{Uid: f[0], Username: f[1], Name: f[2], HomeDir: f[3]}, nil
}

func call(op string) (*User, error) {
	reply, err := gocvm.Call("os.user", op)
	if err != nil {
		return nil, err
	}
	if isRealError(reply) {
		return nil, errors.New(reply)
	}
	return parseUser(reply)
}

func Current() (*User, error) {
	reply, err := gocvm.Call("win32", "GetUserNameW")
	if err != nil {
		return call("")
	}
	if isRealError(reply) || reply == "" {
		return call("")
	}
	home, herr := gocvm.Call("win32", "GetEnvironmentVariableW\x1fHOME")
	if herr != nil || isRealError(home) || home == "" {
		home, herr = gocvm.Call("win32", "GetEnvironmentVariableW\x1fUSERPROFILE")
		if herr != nil || isRealError(home) {
			home = ""
		}
	}
	return &User{Uid: "0", Gid: "0", Username: reply, Name: reply, HomeDir: home}, nil
}

func Lookup(username string) (*User, error) {
	return call("lookup " + username)
}

func LookupId(uid string) (*User, error) {
	return call("lookupid " + uid)
}
