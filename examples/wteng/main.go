package main

import (
	"../safespacepkg"
	"../wtengpkg"
	"fmt"
)

func main() {
	eng := wteng.NewEngine()
	st := wteng.NewStore(eng)
	fmt.Println(eng.Name == "wasigocvm")
	fmt.Println(st.Engine == eng)

	wasm := []byte{
		0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
		0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
		0x03, 0x02, 0x01, 0x00,
		0x07, 0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00,
		0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
	}
	mod, err := wteng.NewModule(eng, wasm)
	fmt.Println(err == nil)
	fmt.Println(mod.HasExport("add"))
	in, err2 := wteng.NewInstance(st, mod)
	fmt.Println(err2 == nil)

	cage := safespace.New(65536)
	safespace.SetCurrent(cage)
	code := cage.Allocate(len(mod.Funcs[0].Code), 8)
	fmt.Println(cage.Contains(code))
	fmt.Println(cage.Write(code, mod.Funcs[0].Code))

	var cpt safespace.CodePointerTable
	h := cpt.Register(code, 1)
	fmt.Println(cpt.GetEntrypoint(h) == 1)
	fmt.Println(cpt.GetCodeObject(h) == code)
	fmt.Println(cpt.GetEntrypoint(99) == 0)

	args := []int64{2, 3}
	got, err3 := in.Call("add", args)
	fmt.Println(err3 == nil)
	fmt.Println(len(got) == 1 && got[0] == 5)

	via := cpt.GetEntrypoint(h)
	fmt.Println(via != 0)
	got2, err4 := in.Call("add", args)
	fmt.Println(err4 == nil && via == 1 && got2[0] == 5)

	cage.TearDown()
	fmt.Println(cage.Contains(code) == false)
}
