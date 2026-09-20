package main

import (
	"fmt"
	"android"
)

func main() {
	pid, err := android.Getpid()
	fmt.Println(err == nil)
	fmt.Println(pid > 0)

	api, err2 := android.DeviceApiLevel()
	fmt.Println(err2 == nil)
	fmt.Println(api == 34)
}
