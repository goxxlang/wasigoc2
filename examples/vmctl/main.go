// vmctl is the go++ side of the WSL VM controls in WASMNix.
// linux.Call is gocvm.Call("linux", …) → wasmnix::wsl_call.
// HeapVM's Win32 bridge owns topics "wsl" and "nix".
package main

import (
	"fmt"
	"linux"
	"os"
	"strings"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "vmctl status|path|exec|call ...")
		os.Exit(2)
	}
	var out string
	var err error
	switch os.Args[1] {
	case "status":
		out, err = linux.Status()
	case "path":
		if len(os.Args) < 4 {
			fmt.Fprintln(os.Stderr, "vmctl path <flag> <path>")
			os.Exit(2)
		}
		out, err = linux.Path(os.Args[2], os.Args[3])
	case "exec":
		out, err = linux.Exec("", strings.Join(os.Args[2:], " "))
	default:
		out, err = linux.Call(os.Args[1], strings.Join(os.Args[2:], "\x1f"))
	}
	if out != "" {
		fmt.Println(out)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, err.Error())
		os.Exit(1)
	}
}
