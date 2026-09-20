// Package exec: the child is a table-named process (EPT/TPT/CHPT) on a
// std::thread; work is WASMWin32 wasi_call (catalog / WslExec /
// CreateProcessW). Not a BusyBox table. A .wasm payload is load/call
// via wasitime / WASMLoader. gocvm.Call stays in-module.
package exec

import (
	"errors"
	"gocvm"
	"io"
	"strconv"
	"strings"
)

var ErrNotFound = errors.New("exec: executable file not found in $PATH")

func isRealError(reply string) bool {
	return strings.HasPrefix(reply, "error:")
}

type Cmd struct {
	Path   string
	Args   []string
	Dir    string
	Env    []string
	Stdout io.Writer
	Stderr io.Writer
	Stdin  io.Reader

	started  bool
	handle   string
	pumpDone chan bool
}

func Command(name string, arg ...string) *Cmd {
	c := &Cmd{Path: name}
	c.Args = append(c.Args, name)
	c.Args = append(c.Args, arg...)
	return c
}

func (c *Cmd) argv() string {
	s := ""
	for i, a := range c.Args {
		if i > 0 {
			s = s + "\x1f"
		}
		s = s + a
	}
	return s
}

// exit=<n>\n<output> -- wasigocvm_exec.hpp combined-wait reply.
func parseExecReply(reply string) (int, string) {
	if !strings.HasPrefix(reply, "exit=") {
		return -1, reply
	}
	i := strings.Index(reply, "\n")
	if i < 0 {
		return -1, reply[5:]
	}
	n, err := strconv.Atoi(reply[5:i])
	if err != nil {
		return -1, reply[i+1:]
	}
	return n, reply[i+1:]
}

// "exit=<n>" -- os.exec.wait reply (no trailing output).
func parseExitCode(reply string) int {
	if !strings.HasPrefix(reply, "exit=") {
		return -1
	}
	n, err := strconv.Atoi(reply[5:])
	if err != nil {
		return -1
	}
	return n
}

// "ok handle=<id>" -- os.exec.start reply.
func parseStartHandle(reply string) string {
	const p = "handle="
	i := strings.Index(reply, p)
	if i < 0 {
		return ""
	}
	return reply[i+len(p):]
}

// CombinedOutput runs the command. Stdout and stderr are one EPT-named
// buffer, so Output() returns the same combined bytes.
func (c *Cmd) CombinedOutput() ([]byte, error) {
	reply, err := gocvm.Call("os.exec", c.argv())
	if err != nil {
		return nil, err
	}
	if isRealError(reply) {
		return nil, errors.New(reply)
	}
	code, out := parseExecReply(reply)
	if code != 0 {
		return []byte(out), errors.New("exit status " + strconv.Itoa(code))
	}
	return []byte(out), nil
}

func (c *Cmd) Output() ([]byte, error) {
	return c.CombinedOutput()
}

func (c *Cmd) Run() error {
	_, err := c.CombinedOutput()
	return err
}

// pump drains the child's combined stdout+stderr into whichever of
// Stdout/Stderr is set (Stdout preferred -- the real backend can't
// separate the two streams, see the package doc above) until EOF, then
// signals pumpDone so Wait knows output has been fully flushed.
func (c *Cmd) pump() {
	w := c.Stdout
	if w == nil {
		w = c.Stderr
	}
	for {
		reply, err := gocvm.Call("os.exec.stdout.read", c.handle+"\x1f"+"4096")
		if err != nil || isRealError(reply) || reply == "" {
			break
		}
		if w != nil {
			w.Write([]byte(reply))
		}
	}
	c.pumpDone <- true
}

// Start launches the command without waiting for it to exit. If Stdout
// or Stderr is set, a background goroutine streams the child's combined
// output into it as it arrives; Wait joins that goroutine first.
func (c *Cmd) Start() error {
	reply, err := gocvm.Call("os.exec.start", c.argv())
	if err != nil {
		return err
	}
	if isRealError(reply) {
		return errors.New(reply)
	}
	h := parseStartHandle(reply)
	if h == "" {
		return errors.New("exec: malformed start reply")
	}
	c.handle = h
	c.started = true
	if c.Stdout != nil || c.Stderr != nil {
		c.pumpDone = make(chan bool, 1)
		go c.pump()
	}
	return nil
}

func (c *Cmd) Wait() error {
	if !c.started {
		return errors.New("exec: not started")
	}
	if c.pumpDone != nil {
		<-c.pumpDone
	}
	reply, err := gocvm.Call("os.exec.wait", c.handle)
	if err != nil {
		return err
	}
	if isRealError(reply) {
		return errors.New(reply)
	}
	code := parseExitCode(reply)
	if code != 0 {
		return errors.New("exit status " + strconv.Itoa(code))
	}
	return nil
}

func LookPath(file string) (string, error) {
	reply, err := gocvm.Call("os.exec.lookpath", file)
	if err != nil {
		if strings.Contains(err.Error(), "not found") {
			return "", ErrNotFound
		}
		return "", err
	}
	if isRealError(reply) {
		return "", ErrNotFound
	}
	if reply == "" {
		return "", ErrNotFound
	}
	return reply, nil
}
