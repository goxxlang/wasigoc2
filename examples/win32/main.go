package main

import (
	"fmt"
	"win32"
)

func main() {
	pid, err := win32.GetCurrentProcessId()
	fmt.Println(err == nil)
	fmt.Println(pid > 0)

	name, err3 := win32.GetComputerName()
	fmt.Println(err3 == nil)
	fmt.Println(len(name) > 0)

	windir, err4 := win32.GetWindowsDirectory()
	fmt.Println(err4 == nil)
	fmt.Println(len(windir) > 0)

	list, wslErr := win32.WslList()
	fmt.Println(wslErr == nil)
	fmt.Println(len(list) > 0)
}
