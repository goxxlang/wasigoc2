// Package electron is a clean-room of Electron's public process and IPC
// surface, for a wasigocvm guest packed as a WASMUniLoader .unil app.
//
// Public documents (not Electron or VS Code source):
//   - Process model: https://www.electronjs.org/docs/latest/tutorial/process-model
//   - ipcMain.handle / ipcRenderer.invoke: https://www.electronjs.org/docs/latest/api/ipc-main
//   - contextBridge.exposeInMainWorld: https://www.electronjs.org/docs/latest/api/context-bridge
//   - BrowserWindow / webContents: https://www.electronjs.org/docs/latest/api/browser-window
//
// Backing (loaded, not rewritten):
//   - WASMLime lime::Main / lime::switches::kRole — process bootstrap.
//     Lime today has no OS browser/renderer split (offerer/peer only);
//     this package uses role strings "main" and "renderer" on that same
//     switch and runs both as cooperative goroutines in one guest.
//   - WASMv8Bindings v8::Isolate / Script::Run — renderer eval via
//     gocvm.Call("v8.eval") when a host isolate is linked.
//   - WASMUniLoader .unil bundle — app path, LoadFile, runtime bill of
//     materials (lime + wasmv8 + unil_display).
//
// This is not a derivative of Electron, Chromium, or VS Code.
package electron

import (
	"errors"
	"gocvm"
	"strconv"
	"strings"
	"unil"
)

const (
	RoleMain     = "main"
	RoleRenderer = "renderer"
	RoleOfferer  = "offerer"
	RolePeer     = "peer"
)

type WindowOptions struct {
	Width  int
	Height int
	Show   bool
}

type WebContents struct {
	HTML     string
	Preload  string
	LastEval string
	URL      string
}

type BrowserWindow struct {
	ID       int
	Width    int
	Height   int
	Host     string
	Contents *WebContents
}

type App struct {
	Role string
	Boot string
	Path string
}

type ipcHandle struct {
	Channel string
	Fn      func(string) string
}

type ipcListen struct {
	Channel string
	Fn      func(string) string
}

type exposedAPI struct {
	Name    string
	Methods []string
}

type ipcCmd struct {
	Kind     string
	Channel  string
	Arg      string
	Fn       func(string) string
	Reply    chan string
	Win      *BrowserWindow
	FileName string
	FileBody string
	Names    []string
	Bodies   []string
}

type runtime struct {
	role    string
	cmds    chan ipcCmd
	handles []ipcHandle
	listens []ipcListen
	windows []*BrowserWindow
	names   []string
	bodies  []string
	exposed []exposedAPI
	path    string
	nextWin int
	ready   bool
}

var theRT *runtime

func getRT() *runtime {
	if theRT == nil {
		panic("electron: NewApp first")
	}
	return theRT
}

func RuntimeComponents() []unil.Component {
	return []unil.Component{
		{Name: "lime", Role: "process", Origin: "WASMLime", Engine: "content-main"},
		{Name: "wasmv8", Role: "js", Origin: "WASMv8Bindings", Engine: "v8-facade"},
		{Name: "unil_display", Role: "display", Origin: "WASMUniLoader/cpp/display", Engine: "wasmv16"},
		{Name: "electron", Role: "host", Origin: "go++/examples/electronpkg", Engine: "wasigocvm"},
	}
}

func HasRuntime(d unil.Document, name string) bool {
	for i := 0; i < len(d.Runtime); i++ {
		if d.Runtime[i].Name == name {
			return true
		}
	}
	return false
}

func AppBundle(name string, files []unil.File) unil.Document {
	d := unil.Document{}
	d.BomFormat = "unil"
	d.SpecVersion = "1"
	d.Name = name
	d.Scope = "bundle"
	d.Files = files
	d.Runtime = RuntimeComponents()
	d.Capabilities = unil.DefaultCapabilities()
	d.Capabilities = append(d.Capabilities, "ipc")
	d.Capabilities = append(d.Capabilities, "contextBridge")
	d.Capabilities = append(d.Capabilities, "browserWindow")
	return d
}

func PackageMain(packageJSON string) string {
	key := "\"main\""
	i := strings.Index(packageJSON, key)
	if i < 0 {
		return "main.js"
	}
	rest := packageJSON[i+len(key):]
	colon := strings.Index(rest, ":")
	if colon < 0 {
		return "main.js"
	}
	rest = rest[colon+1:]
	q := strings.Index(rest, "\"")
	if q < 0 {
		return "main.js"
	}
	rest = rest[q+1:]
	end := strings.Index(rest, "\"")
	if end < 0 {
		return "main.js"
	}
	return rest[0:end]
}

func (rt *runtime) fileBody(name string) string {
	for i := 0; i < len(rt.names); i++ {
		if rt.names[i] == name {
			return rt.bodies[i]
		}
	}
	return ""
}

func (rt *runtime) dispatchHandle(channel string, arg string) string {
	for i := 0; i < len(rt.handles); i++ {
		if rt.handles[i].Channel == channel {
			if rt.handles[i].Fn != nil {
				return rt.handles[i].Fn(arg)
			}
		}
	}
	return ""
}

func (rt *runtime) dispatchListen(channel string, arg string) {
	for i := 0; i < len(rt.listens); i++ {
		if rt.listens[i].Channel == channel {
			if rt.listens[i].Fn != nil {
				rt.listens[i].Fn(arg)
			}
		}
	}
}

func (rt *runtime) exposedChannel(name string, method string) string {
	for i := 0; i < len(rt.exposed); i++ {
		if rt.exposed[i].Name != name {
			continue
		}
		for j := 0; j < len(rt.exposed[i].Methods); j++ {
			if rt.exposed[i].Methods[j] == method {
				return method
			}
		}
	}
	return ""
}

func (rt *runtime) handle(c ipcCmd) {
	switch c.Kind {
	case "handle":
		var h ipcHandle
		h.Channel = c.Channel
		h.Fn = c.Fn
		rt.handles = append(rt.handles, h)
		if c.Reply != nil {
			c.Reply <- "ok"
		}
	case "listen":
		var l ipcListen
		l.Channel = c.Channel
		l.Fn = c.Fn
		rt.listens = append(rt.listens, l)
		if c.Reply != nil {
			c.Reply <- "ok"
		}
	case "invoke":
		out := rt.dispatchHandle(c.Channel, c.Arg)
		if c.Reply != nil {
			c.Reply <- out
		}
	case "send":
		rt.dispatchListen(c.Channel, c.Arg)
		if c.Reply != nil {
			c.Reply <- "ok"
		}
	case "window":
		rt.nextWin = rt.nextWin + 1
		if c.Win != nil {
			c.Win.ID = rt.nextWin
			rt.windows = append(rt.windows, c.Win)
		}
		if c.Reply != nil {
			c.Reply <- strconv.Itoa(rt.nextWin)
		}
	case "mount":
		rt.path = c.FileName
		rt.names = c.Names
		rt.bodies = c.Bodies
		if c.Reply != nil {
			c.Reply <- "ok"
		}
	case "loadfile":
		body := rt.fileBody(c.FileName)
		if c.Win != nil && c.Win.Contents != nil {
			c.Win.Contents.HTML = body
			c.Win.Contents.URL = "unil:" + c.FileName
		}
		if c.Reply != nil {
			if body == "" {
				c.Reply <- "error: electron: file not in unil bundle"
			} else {
				c.Reply <- body
			}
		}
	case "expose":
		var e exposedAPI
		e.Name = c.Channel
		e.Methods = c.Names
		rt.exposed = append(rt.exposed, e)
		if c.Reply != nil {
			c.Reply <- "ok"
		}
	case "call":
		ch := rt.exposedChannel(c.Channel, c.FileName)
		out := ""
		if ch != "" {
			out = rt.dispatchHandle(ch, c.Arg)
		}
		if c.Reply != nil {
			c.Reply <- out
		}
	case "quit":
		if c.Reply != nil {
			c.Reply <- "ok"
		}
	default:
		if c.Reply != nil {
			c.Reply <- "error: electron: unknown cmd"
		}
	}
}

func (rt *runtime) loop() {
	for {
		c := <-rt.cmds
		stop := c.Kind == "quit"
		rt.handle(c)
		if stop {
			return
		}
	}
}

func syncCmd(c ipcCmd) string {
	done := make(chan string, 1)
	c.Reply = done
	getRT().cmds <- c
	return <-done
}

func NewApp(role string) *App {
	if theRT == nil {
		rt := &runtime{}
		rt.role = role
		rt.cmds = make(chan ipcCmd, 16)
		theRT = rt
		go rt.loop()
	} else {
		theRT.role = role
	}
	a := &App{}
	a.Role = role
	return a
}

func (a *App) LimeRole() string {
	return a.Role
}

func (a *App) WhenReady() {
	rt := getRT()
	reply, err := gocvm.Call("electron.boot", a.Role)
	if err == nil && reply != "" && !strings.HasPrefix(reply, "error:") {
		a.Boot = reply
	} else {
		a.Boot = "in-guest"
	}
	rt.ready = true
}

func (a *App) Mount(doc unil.Document, names []string, bodies []string) {
	a.Path = doc.Name
	var c ipcCmd
	c.Kind = "mount"
	c.FileName = doc.Name
	c.Names = names
	c.Bodies = bodies
	syncCmd(c)
}

func (a *App) AppPath() string {
	if a.Path != "" {
		return a.Path
	}
	return "."
}

func NewBrowserWindow(opts WindowOptions) *BrowserWindow {
	if opts.Width <= 0 {
		opts.Width = 800
	}
	if opts.Height <= 0 {
		opts.Height = 600
	}
	payload := strconv.Itoa(opts.Width) + "x" + strconv.Itoa(opts.Height)
	host := ""
	reply, err := gocvm.Call("electron.window", payload)
	if err == nil && !strings.HasPrefix(reply, "error:") {
		host = reply
	}
	w := &BrowserWindow{}
	w.Width = opts.Width
	w.Height = opts.Height
	w.Host = host
	w.Contents = &WebContents{}
	var c ipcCmd
	c.Kind = "window"
	c.Win = w
	syncCmd(c)
	return w
}

func (w *BrowserWindow) LoadHTML(html string) {
	if w.Contents == nil {
		c := &WebContents{}
		w.Contents = c
	}
	w.Contents.HTML = html
	w.Contents.URL = "about:blank"
}

func (w *BrowserWindow) LoadURL(url string) {
	if w.Contents == nil {
		c := &WebContents{}
		w.Contents = c
	}
	w.Contents.URL = url
}

func (w *BrowserWindow) LoadFile(name string) error {
	var c ipcCmd
	c.Kind = "loadfile"
	c.FileName = name
	c.Win = w
	body := syncCmd(c)
	if strings.HasPrefix(body, "error:") {
		return errors.New(body)
	}
	return nil
}

func (w *WebContents) Eval(js string) string {
	if w == nil {
		return ""
	}
	reply, err := gocvm.Call("v8.eval", js)
	if err == nil && !strings.HasPrefix(reply, "error:") {
		w.LastEval = reply
		return reply
	}
	w.LastEval = js
	return js
}

func Handle(channel string, fn func(string) string) {
	var c ipcCmd
	c.Kind = "handle"
	c.Channel = channel
	c.Fn = fn
	syncCmd(c)
}

func On(channel string, fn func(string) string) {
	var c ipcCmd
	c.Kind = "listen"
	c.Channel = channel
	c.Fn = fn
	syncCmd(c)
}

func Invoke(channel string, arg string) string {
	var c ipcCmd
	c.Kind = "invoke"
	c.Channel = channel
	c.Arg = arg
	return syncCmd(c)
}

func Send(channel string, arg string) {
	var c ipcCmd
	c.Kind = "send"
	c.Channel = channel
	c.Arg = arg
	syncCmd(c)
}

func ExposeInMainWorld(name string, methods []string) {
	var c ipcCmd
	c.Kind = "expose"
	c.Channel = name
	c.Names = methods
	syncCmd(c)
}

func CallExposed(name string, method string, arg string) string {
	var c ipcCmd
	c.Kind = "call"
	c.Channel = name
	c.FileName = method
	c.Arg = arg
	return syncCmd(c)
}

func Quit() {
	var c ipcCmd
	c.Kind = "quit"
	syncCmd(c)
}
