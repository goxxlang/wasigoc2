// Package net: TCP/UDP on wasigocvm (in-module poll() on sysroot
// sockets). Pipe() / PacketPipe() are in-process duplex, not a host
// fallback. gocvm.Call stays in-module.
package net

import (
	"errors"
	"gocvm"
	"strconv"
	"strings"
)

var errNotSupported = errors.New("net: unsupported network")
var errClosedPipe = errors.New("net: pipe closed")
var errClosed = errors.New("net: listener closed")

func isRealError(reply string) bool {
	return strings.HasPrefix(reply, "error:")
}

// "handle=<id>" optionally followed by " remote=<addr>" -- the shape
// net.dial/listen/accept replies share.
func parseHandleReply(reply string) (handle string, remote string) {
	i := strings.Index(reply, "handle=")
	if i < 0 {
		return "", ""
	}
	rest := reply[i+len("handle="):]
	sp := strings.Index(rest, " ")
	if sp < 0 {
		return rest, ""
	}
	handle = rest[0:sp]
	if ri := strings.Index(rest[sp+1:], "remote="); ri >= 0 {
		remote = rest[sp+1+ri+len("remote="):]
	}
	return handle, remote
}

type Conn struct {
	valid    bool
	closed   bool
	recv     chan []byte
	send     chan []byte
	leftover []byte
	laddr    string
	raddr    string
	real     bool
	handle   string
}

func (c *Conn) Read(p []byte) (int, error) {
	if !c.valid {
		return 0, errNotSupported
	}
	if c.closed {
		return 0, errClosedPipe
	}
	if c.real {
		reply, err := gocvm.Call("net.io.read", c.handle+"\x1f"+strconv.Itoa(len(p)))
		if err != nil {
			return 0, err
		}
		if isRealError(reply) {
			return 0, errors.New(reply)
		}
		if reply == "" {
			return 0, errors.New("EOF")
		}
		n := copy(p, []byte(reply))
		return n, nil
	}
	if len(c.leftover) == 0 {
		chunk, ok := <-c.recv
		if !ok {
			return 0, errors.New("EOF")
		}
		c.leftover = chunk
	}
	n := copy(p, c.leftover)
	c.leftover = c.leftover[n:]
	return n, nil
}

func (c *Conn) Write(p []byte) (int, error) {
	if !c.valid {
		return 0, errNotSupported
	}
	if c.closed {
		return 0, errClosedPipe
	}
	if c.real {
		reply, err := gocvm.Call("net.io.write", c.handle+"\x1f"+string(p))
		if err != nil {
			return 0, err
		}
		if isRealError(reply) {
			return 0, errors.New(reply)
		}
		return len(p), nil
	}
	cp := make([]byte, len(p))
	copy(cp, p)
	c.send <- cp
	return len(p), nil
}

func (c *Conn) Close() error {
	if !c.valid {
		return errNotSupported
	}
	if c.closed {
		return nil
	}
	c.closed = true
	if c.real {
		gocvm.Call("net.io.close", c.handle)
		return nil
	}
	close(c.send)
	return nil
}

func (c *Conn) LocalAddr() string  { return c.laddr }
func (c *Conn) RemoteAddr() string { return c.raddr }

type TCPConn = Conn

type Addr interface {
	Network() string
	String() string
}

type TCPAddr struct {
	Host string
	Port int
}

func (a *TCPAddr) Network() string { return "tcp" }

func (a *TCPAddr) String() string {
	if a == nil {
		return ""
	}
	return JoinHostPort(a.Host, strconv.Itoa(a.Port))
}

func Pipe() (*Conn, *Conn) {
	ab := make(chan []byte, 16)
	ba := make(chan []byte, 16)
	a := &Conn{valid: true, recv: ba, send: ab}
	b := &Conn{valid: true, recv: ab, send: ba}
	return a, b
}

type TCPListener struct {
	valid  bool
	closed bool
	addr   string
	accept chan *Conn
	real   bool
	handle string
}

type Listener interface {
	Accept() (*Conn, error)
	Close() error
	Addr() Addr
}

func (l *TCPListener) Addr() Addr {
	if l == nil {
		return nil
	}
	host, port, err := SplitHostPort(l.addr)
	n := 0
	if err == nil {
		n, _ = strconv.Atoi(port)
	}
	return &TCPAddr{Host: host, Port: n}
}

func (l *TCPListener) Accept() (*Conn, error) {
	if l == nil || !l.valid {
		return nil, errNotSupported
	}
	if l.closed {
		return nil, errClosed
	}
	reply, err := gocvm.Call("net.accept", l.handle)
	if err != nil {
		return nil, err
	}
	if isRealError(reply) {
		return nil, errors.New(reply)
	}
	h, remote := parseHandleReply(reply)
	return &Conn{valid: true, real: true, handle: h, laddr: l.addr, raddr: remote}, nil
}

func (l *TCPListener) Close() error {
	if l == nil || !l.valid {
		return errNotSupported
	}
	if l.closed {
		return nil
	}
	l.closed = true
	gocvm.Call("net.io.close", l.handle)
	return nil
}

func Listen(network string, address string) (*TCPListener, error) {
	if network != "tcp" && network != "tcp4" && network != "tcp6" {
		return nil, errNotSupported
	}
	reply, err := gocvm.Call("net.listen", network+" "+address)
	if err != nil {
		return nil, err
	}
	if isRealError(reply) {
		return nil, errors.New(reply)
	}
	h, _ := parseHandleReply(reply)
	return &TCPListener{valid: true, real: true, handle: h, addr: address, accept: make(chan *Conn, 1)}, nil
}

func Dial(network string, address string) (*Conn, error) {
	if network != "tcp" && network != "tcp4" && network != "tcp6" {
		return nil, errNotSupported
	}
	reply, err := gocvm.Call("net.dial", network+" "+address)
	if err != nil {
		return nil, err
	}
	if isRealError(reply) {
		return nil, errors.New(reply)
	}
	h, _ := parseHandleReply(reply)
	return &Conn{valid: true, real: true, handle: h, laddr: "", raddr: address}, nil
}

type PacketConn struct {
	valid  bool
	closed bool
	listen bool
	conn   *Conn
	laddr  string
	raddr  string
	attach chan *Conn
	real   bool
	handle string
}

func (c *PacketConn) LocalAddr() string  { return c.laddr }
func (c *PacketConn) RemoteAddr() string { return c.raddr }

func (c *PacketConn) Close() error {
	if c == nil || !c.valid {
		return errNotSupported
	}
	if c.closed {
		return nil
	}
	c.closed = true
	if c.real {
		gocvm.Call("net.io.close", c.handle)
		return nil
	}
	if c.conn == nil {
		return nil
	}
	return c.conn.Close()
}

func readFull(c *Conn, p []byte) error {
	off := 0
	for off < len(p) {
		n, err := c.Read(p[off:])
		if err != nil {
			return err
		}
		if n == 0 {
			return errors.New("EOF")
		}
		off = off + n
	}
	return nil
}

func (c *PacketConn) WriteTo(p []byte, addr string) (int, error) {
	if c == nil || !c.valid || c.closed {
		return 0, errClosedPipe
	}
	if c.real {
		reply, err := gocvm.Call("net.io.writeto", c.handle+"\x1f"+addr+"\x1f"+string(p))
		if err != nil {
			return 0, err
		}
		if isRealError(reply) {
			return 0, errors.New(reply)
		}
		return len(p), nil
	}
	if c.conn == nil {
		return 0, errClosedPipe
	}
	n := len(p)
	hdr := []byte{byte(n >> 24), byte(n >> 16), byte(n >> 8), byte(n)}
	if _, err := c.conn.Write(hdr); err != nil {
		return 0, err
	}
	if n > 0 {
		if _, err := c.conn.Write(p); err != nil {
			return 0, err
		}
	}
	return n, nil
}

func (c *PacketConn) ReadFrom(p []byte) (int, string, error) {
	if c == nil || !c.valid || c.closed {
		return 0, "", errClosedPipe
	}
	if c.real {
		reply, err := gocvm.Call("net.io.readfrom", c.handle+"\x1f"+strconv.Itoa(len(p)))
		if err != nil {
			return 0, "", err
		}
		if isRealError(reply) {
			return 0, "", errors.New(reply)
		}
		i := strings.Index(reply, "\x1f")
		if i < 0 {
			return 0, "", errors.New("net: malformed readfrom reply")
		}
		from := reply[0:i]
		data := reply[i+1:]
		n := copy(p, []byte(data))
		return n, from, nil
	}
	if c.listen && c.conn == nil {
		peer, ok := <-c.attach
		if !ok || peer == nil {
			return 0, "", errClosed
		}
		c.conn = peer
	}
	if c.conn == nil {
		return 0, "", errClosedPipe
	}
	hdr := make([]byte, 4)
	if err := readFull(c.conn, hdr); err != nil {
		return 0, "", err
	}
	n := int(hdr[0])<<24 | int(hdr[1])<<16 | int(hdr[2])<<8 | int(hdr[3])
	if n < 0 {
		return 0, "", errClosedPipe
	}
	buf := make([]byte, n)
	if n > 0 {
		if err := readFull(c.conn, buf); err != nil {
			return 0, "", err
		}
	}
	copyN := copy(p, buf)
	return copyN, "pipe", nil
}

func PacketPipe() (*PacketConn, *PacketConn) {
	a, b := Pipe()
	pa := &PacketConn{valid: true, conn: a, laddr: "pipe", raddr: "pipe"}
	pb := &PacketConn{valid: true, conn: b, laddr: "pipe", raddr: "pipe"}
	return pa, pb
}

func ListenPacket(network string, address string) (*PacketConn, error) {
	if network != "udp" && network != "udp4" && network != "udp6" {
		return nil, errNotSupported
	}
	reply, err := gocvm.Call("net.listen", network+" "+address)
	if err != nil {
		return nil, err
	}
	if isRealError(reply) {
		return nil, errors.New(reply)
	}
	h, _ := parseHandleReply(reply)
	return &PacketConn{valid: true, real: true, handle: h, laddr: address}, nil
}

func DialPacket(network string, address string) (*PacketConn, error) {
	if network != "udp" && network != "udp4" && network != "udp6" {
		return nil, errNotSupported
	}
	reply, err := gocvm.Call("net.dial", network+" "+address)
	if err != nil {
		return nil, err
	}
	if isRealError(reply) {
		return nil, errors.New(reply)
	}
	h, _ := parseHandleReply(reply)
	return &PacketConn{valid: true, real: true, handle: h, laddr: "", raddr: address}, nil
}

func SplitHostPort(hostport string) (string, string, error) {
	if len(hostport) == 0 {
		return "", "", errors.New("missing port in address")
	}
	if hostport[0] == 91 {
		end := strings.Index(hostport, "]:")
		if end < 0 {
			return "", "", errors.New("missing port in address")
		}
		return hostport[1:end], hostport[end+2:], nil
	}
	i := strings.LastIndex(hostport, ":")
	if i < 0 {
		return "", "", errors.New("missing port in address")
	}
	return hostport[0:i], hostport[i+1:], nil
}

func JoinHostPort(host string, port string) string {
	if strings.Contains(host, ":") {
		return "[" + host + "]:" + port
	}
	return host + ":" + port
}
