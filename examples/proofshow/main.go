package main

import (
	"fmt"
	"gocvm"
	"strings"
)

func main() {
	sep := "\x1f"
	// proof.exe is the unil member windows/system32/proof.exe.
	reply, err := gocvm.Call("win32", "CreateProcessW"+sep+"proof.exe"+sep+"proof.exe")
	if err != nil {
		fmt.Println(err.Error())
		return
	}
	fmt.Println(reply)

	parts := strings.Split(reply, sep)
	if len(parts) < 2 {
		fmt.Println("CreateProcessW: bad reply")
		return
	}
	out, err := gocvm.Call("win32", "GetProcessOutput"+sep+parts[1])
	if err != nil {
		fmt.Println(err.Error())
		return
	}
	fmt.Println(out)

	path := `C:\Users\grego\go++\fs\pe-proof.txt`
	// GENERIC_WRITE, CREATE_ALWAYS. Same catalog hop as CreateProcessW.
	h, err := gocvm.Call("win32", "CreateFileW"+sep+path+sep+"1073741824"+sep+"2")
	if err != nil {
		fmt.Println(err.Error())
		return
	}
	fmt.Println("create=" + h)

	n, err := gocvm.Call("win32", "WriteFile"+sep+h+sep+"pe-executed\n")
	if err != nil {
		fmt.Println(err.Error())
		return
	}
	fmt.Println("write=" + n)

	fl, err := gocvm.Call("win32", "FlushFileBuffers"+sep+h)
	if err != nil {
		fmt.Println(err.Error())
		return
	}
	fmt.Println("flush=" + fl)

	cl, err := gocvm.Call("win32", "CloseHandle"+sep+strings.TrimSpace(h))
	if err != nil {
		fmt.Println(err.Error())
		return
	}
	fmt.Println("close=" + cl)
}
