package main

import (
	"fmt"
	"linux"
	"strings"
)

func main() {
	_, err := linux.Install("Ubuntu")
	fmt.Println(err == nil)

	u, err2 := linux.Exec("", "uname")
	fmt.Println(err2 == nil)
	fmt.Println(strings.Contains(u, "microsoft-standard-WSL2"))

	list, err3 := linux.List()
	fmt.Println(err3 == nil)
	fmt.Println(strings.Contains(list, "Ubuntu"))
}
