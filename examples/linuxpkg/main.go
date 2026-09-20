package main

import (
	"fmt"
	"linux"
)

func main() {
	pid, err := linux.Getpid()
	fmt.Println(err == nil)
	fmt.Println(pid > 0)

	u, err2 := linux.Uname()
	fmt.Println(err2 == nil)
	fmt.Println(len(u) > 0)
}
