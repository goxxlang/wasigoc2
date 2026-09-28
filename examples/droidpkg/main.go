package main

import (
	"fmt"
	"android"
)

func main() {
	fam, err := android.PackageFamilyName()
	fmt.Println(err == nil)
	fmt.Println(fam == "Microsoft.YourPhone_8wekyb3d8bbwe")

	st, err2 := android.Status()
	fmt.Println(err2 == nil)
	fmt.Println(st == "unlinked")
}
