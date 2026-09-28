// Command Prompt for wasigocvm. The terminal is GocOS's own interactive
// console (gocos.Call("Console", ...)): echo, line editing, the prompt, and
// command dispatch all run inside the module. This guest is only a byte pump
// between the WASI stdio the browser hop moves and that console device — no
// line logic here, and none in JS. WASMJsLoader instantiates the same wasm in
// the browser; compilation (wasm + COOP/COEP) is the boundary.
package main

import (
	"fmt"
	"gocos"
	"os"
)

func main() {
	// Prime: the first call boots GocOS and returns the banner + first prompt.
	out, err := gocos.Call("Console", "")
	if err == nil {
		fmt.Printf("%s", out)
	}

	// Pump raw keystrokes to the console and its display bytes back out. GocOS
	// echoes and line-edits each byte, so nothing here waits for a newline.
	buf := make([]byte, 256)
	for {
		n, rerr := os.Stdin.Read(buf)
		if n > 0 {
			reply, cerr := gocos.Call("Console", string(buf[:n]))
			if cerr == nil {
				fmt.Printf("%s", reply)
			}
		}
		if rerr != nil {
			return
		}
	}
}
