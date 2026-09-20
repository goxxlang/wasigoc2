package main

import (
	"flag"
	"fmt"
	"net/http"
	"os"

	"wasmloader/webui"
)

func cmdServe(args []string) error {
	fs := flag.NewFlagSet("serve", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	addr := fs.String("addr", "127.0.0.1:8787", "listen address")
	if err := fs.Parse(args); err != nil {
		return err
	}
	fmt.Fprintf(os.Stderr, "wasmloader listening on http://%s\n", *addr)
	return http.ListenAndServe(*addr, webui.Handler())
}
