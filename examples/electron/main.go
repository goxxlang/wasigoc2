package main

import (
	"../electronpkg"
	"fmt"
	"unil"
)

func ping(arg string) string {
	return "pong:" + arg
}

var helloArg string

func noteHello(arg string) string {
	helloArg = arg
	return arg
}

func main() {
	fmt.Println(electron.RoleMain == "main")
	fmt.Println(electron.RoleRenderer == "renderer")
	fmt.Println(electron.RoleOfferer == "offerer")

	page := "<html><body>shell</body></html>"
	var pageFile unil.File
	pageFile.Name = "index.html"
	pageFile.Mime = "text/html"
	pageFile.Kind = "html"
	pageFile.Size = len(page)
	files := []unil.File{pageFile}
	bundle := electron.AppBundle("electron-app", files)
	fmt.Println(bundle.Scope == "bundle")
	fmt.Println(bundle.Name == "electron-app")
	fmt.Println(electron.HasRuntime(bundle, "lime"))
	fmt.Println(electron.HasRuntime(bundle, "wasmv8"))
	fmt.Println(electron.HasRuntime(bundle, "unil_display"))
	fmt.Println(electron.HasRuntime(bundle, "electron"))
	digest := unil.DigestHex(bundle)
	fmt.Println(len(digest) == 64)

	fmt.Println(electron.PackageMain("{\"main\":\"guest.wasm\"}") == "guest.wasm")
	fmt.Println(electron.PackageMain("{}") == "main.js")

	app := electron.NewApp(electron.RoleMain)
	fmt.Println(app.LimeRole() == "main")
	app.WhenReady()
	fmt.Println(app.Boot == "in-guest")

	names := []string{"index.html"}
	bodies := []string{page}
	app.Mount(bundle, names, bodies)
	fmt.Println(app.AppPath() == "electron-app")

	electron.Handle("ping", ping)
	methods := []string{"ping"}
	electron.ExposeInMainWorld("api", methods)
	electron.On("hello", noteHello)

	opts := electron.WindowOptions{}
	opts.Width = 1024
	opts.Height = 768
	win := electron.NewBrowserWindow(opts)
	fmt.Println(win.Width == 1024)
	fmt.Println(win.Height == 768)
	fmt.Println(win.ID > 0)

	err := win.LoadFile("index.html")
	fmt.Println(err == nil)
	fmt.Println(win.Contents.HTML == page)
	fmt.Println(win.Contents.URL == "unil:index.html")

	win.LoadHTML("<p>live</p>")
	fmt.Println(win.Contents.HTML == "<p>live</p>")

	evaled := win.Contents.Eval("1+1")
	fmt.Println(evaled == "1+1")

	reply := electron.Invoke("ping", "hi")
	fmt.Println(reply == "pong:hi")

	exposed := electron.CallExposed("api", "ping", "yo")
	fmt.Println(exposed == "pong:yo")

	electron.Send("hello", "world")
	fmt.Println(helloArg == "world")
	electron.Quit()
}
