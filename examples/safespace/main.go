package main

import (
	"../safespacepkg"
	"fmt"
)

func main() {
	cage := safespace.New(safespace.DefaultSize)
	safespace.SetCurrent(cage)
	fmt.Println(cage.Size == safespace.DefaultSize)
	fmt.Println(cage.End == cage.Base+cage.Size)
	fmt.Println(cage.Contains(cage.Base))
	fmt.Println(cage.Contains(cage.End) == false)
	fmt.Println(safespace.Inside(cage.Base))
	fmt.Println(safespace.Outside(0))

	slot := cage.Allocate(4, 4)
	fmt.Println(cage.Contains(slot))
	fmt.Println(slot%4 == 0)
	fmt.Println(cage.StoreI32(slot, 42))
	got, ok := cage.LoadI32(slot)
	fmt.Println(ok && got == 42)

	enc, encOK := safespace.Encode(cage, slot)
	fmt.Println(encOK)
	back := safespace.Decode(cage, enc)
	fmt.Println(back == slot)
	v2, ok2 := cage.LoadI32(back)
	fmt.Println(ok2 && v2 == 42)

	evil := safespace.Decode(cage, 4294967295)
	fmt.Println(cage.Contains(evil))

	_, badEnc := safespace.Encode(cage, 0)
	fmt.Println(badEnc == false)

	trusted := 7
	var tpt safespace.TrustedPointerTable
	th := tpt.Put(trusted, safespace.TagTrusted)
	fmt.Println(tpt.Get(th, safespace.TagTrusted) == trusted)
	fmt.Println(tpt.Get(th, safespace.TagCode) == 0)
	fmt.Println(tpt.Get(safespace.NullHandle, safespace.TagTrusted) == 0)

	var ept safespace.ExternalPointerTable
	eh := ept.Put(99, safespace.TagExternal)
	fmt.Println(ept.Get(eh, safespace.TagExternal) == 99)
	fmt.Println(ept.Get(eh, safespace.TagTrusted) == 0)

	codeOff := cage.Allocate(8, 8)
	var cpt safespace.CodePointerTable
	h1 := cpt.Register(codeOff, 1)
	h2 := cpt.Register(codeOff, 2)
	fmt.Println(cpt.Contains(h1) && cpt.Contains(h2))
	fmt.Println(cpt.GetEntrypoint(h1) == 1)
	fmt.Println(cpt.GetEntrypoint(h2) == 2)
	fmt.Println(cpt.GetEntrypoint(99) == 0)
	fmt.Println(cpt.SetEntrypoint(h1, 3))
	fmt.Println(cpt.GetEntrypoint(h1) == 3)
	fmt.Println(cpt.GetCodeObject(h1) == codeOff)

	other := safespace.New(4096)
	fmt.Println(other.Contains(cage.Base) == false)
	fmt.Println(cage.Contains(other.Base) == false)

	cage.TearDown()
	fmt.Println(cage.Contains(slot) == false)
	fmt.Println(safespace.Current() == nil)
}
