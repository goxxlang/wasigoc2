package main

import (
	"fmt"
	"os/exec"
	"strings"
)

func main() {
	out, err := exec.Command("echo", "grown-up").CombinedOutput()
	fmt.Println(err == nil)
	fmt.Println(strings.Contains(string(out), "grown-up"))

	err2 := exec.Command("true").Run()
	fmt.Println(err2 == nil)

	err3 := exec.Command("false").Run()
	fmt.Println(err3 != nil)

	u, err4 := exec.Command("uname").CombinedOutput()
	fmt.Println(err4 == nil)
	fmt.Println(len(u) > 0)
}
