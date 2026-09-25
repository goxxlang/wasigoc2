// Command Prompt for wasigocvm. Native k32 (RtlGetVersion,
// CreateProcessW) is compiled into this module — WASMJsLoader instantiates
// the same wasm in the browser. Compilation (wasm + COOP/COEP) is
// the boundary. Not a host bridge, not HTTP/xterm inside the guest.
package main

import (
	"bufio"
	"fmt"
	"gocos"
	"os"
	"strings"
	"win32"
)

func writeOut(s string) {
	if s == "" {
		return
	}
	fmt.Printf("%s", s)
	if s[len(s)-1] != 10 {
		fmt.Printf("\n")
	}
}

func die(err error) {
	if err == nil {
		return
	}
	writeOut(err.Error())
}

func start() {
	_, err := gocos.Boot()
	die(err)
	_, err = gocos.Test()
	die(err)
	_, err = win32.RtlGetVersion()
	die(err)
	_, _, _, err = win32.CreateProcess("cmd.exe", "cmd.exe")
	die(err)
}

func runLine(line string) bool {
	line = strings.TrimSpace(line)
	if line == "" {
		return true
	}
	low := strings.ToLower(line)
	if low == "exit" || low == "quit" {
		_, _ = gocos.Halt()
		return false
	}
	out, err := gocos.Cmd(line)
	if err != nil {
		writeOut(err.Error())
		return true
	}
	writeOut(out)
	return true
}

func cmdSlashC(args []string) string {
	if len(args) < 2 {
		return ""
	}
	i := 1
	if strings.EqualFold(args[1], "/c") || strings.EqualFold(args[1], "/k") {
		i = 2
	}
	if i >= len(args) {
		return ""
	}
	return strings.Join(args[i:], " ")
}

func prompt() {
	p, err := gocos.Prompt()
	if err != nil || p == "" {
		fmt.Printf("C:\\>")
		return
	}
	fmt.Printf("%s", p)
}

func main() {
	start()
	banner, err := gocos.Conhost()
	if err != nil {
		ver, verr := win32.RtlGetVersion()
		if verr != nil {
			die(err)
			return
		}
		fmt.Printf("Microsoft Windows [Version %s]\n", ver)
		fmt.Printf("(c) Microsoft Corporation. All rights reserved.\n")
	} else {
		writeOut(banner)
	}

	one := cmdSlashC(os.Args)
	if one != "" {
		runLine(one)
		return
	}

	sc := bufio.NewScanner(os.Stdin)
	for {
		prompt()
		if !sc.Scan() {
			fmt.Printf("\n")
			_, _ = gocos.Halt()
			return
		}
		if !runLine(sc.Text()) {
			return
		}
	}
}
