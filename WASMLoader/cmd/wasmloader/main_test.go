package main

import (
	"reflect"
	"testing"
)

func TestPeelLinkArgs(t *testing.T) {
	specs, call, args := peelLinkArgs([]string{
		"math=examples/add.wasm",
		"app=examples/double.wasm",
		"--call",
		"app.double",
		"21",
	}, "")
	if !reflect.DeepEqual(specs, []string{"math=examples/add.wasm", "app=examples/double.wasm"}) {
		t.Fatalf("specs %v", specs)
	}
	if call != "app.double" || !reflect.DeepEqual(args, []string{"21"}) {
		t.Fatalf("call=%q args=%v", call, args)
	}

	specs, call, args = peelLinkArgs([]string{
		"math=add.wasm", "app=double.wasm", "app.double", "21",
	}, "")
	if call != "app.double" || args[0] != "21" || len(specs) != 2 {
		t.Fatalf("bare call specs=%v call=%q args=%v", specs, call, args)
	}
}
