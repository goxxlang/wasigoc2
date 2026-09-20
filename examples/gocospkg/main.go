package main

import (
	"fmt"
	"gocos"
	"strings"
)

func main() {
	boot, err := gocos.Boot()
	fmt.Println(err == nil)
	fmt.Println(strings.Contains(boot, "os=gocos") && strings.Contains(boot, "gpu=gocdesk"))

	ver, err2 := gocos.RtlGetVersion()
	fmt.Println(err2 == nil)
	fmt.Println(len(ver) > 0)
}
