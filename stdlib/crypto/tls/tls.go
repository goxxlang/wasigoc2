// Package tls: OpenSSL 3 wasm, memory BIOs, same shape as WASMLime
// TlsTransport (SSL_do_handshake + SNI). Not Schannel. gocvm.Call
// stays in-module. LoadX509KeyPair (client certificates) stays stubbed.
package tls

import (
	"errors"
	"gocvm"
	"strconv"
	"strings"
)

var errNotConnected = errors.New("tls: not connected")

func isRealError(reply string) bool {
	return strings.HasPrefix(reply, "error:")
}

type Config struct {
	ServerName string
}

type Conn struct {
	valid  bool
	closed bool
	real   bool
	handle string
}

// "ok handle=<id>" -- tls.dial reply.
func parseDialHandle(reply string) string {
	const p = "handle="
	i := strings.Index(reply, p)
	if i < 0 {
		return ""
	}
	return reply[i+len(p):]
}

// Dial connects and completes the full TLS handshake in one call (same
// as real Go's tls.Dial) -- Handshake() below is a no-op once Dial has
// succeeded.
func Dial(network string, addr string, config *Config) (*Conn, error) {
	payload := addr
	if config != nil && config.ServerName != "" {
		payload = addr + "\x1f" + config.ServerName
	}
	reply, err := gocvm.Call("tls.dial", payload)
	if err != nil {
		return nil, err
	}
	if isRealError(reply) {
		return nil, errors.New(reply)
	}
	h := parseDialHandle(reply)
	if h == "" {
		return nil, errors.New("tls: malformed dial reply")
	}
	return &Conn{valid: true, real: true, handle: h}, nil
}

func (c *Conn) Read(p []byte) (int, error) {
	if c == nil || !c.valid || !c.real {
		return 0, errNotConnected
	}
	if c.closed {
		return 0, errors.New("tls: connection closed")
	}
	reply, err := gocvm.Call("tls.io.read", c.handle+"\x1f"+strconv.Itoa(len(p)))
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

func (c *Conn) Write(p []byte) (int, error) {
	if c == nil || !c.valid || !c.real {
		return 0, errNotConnected
	}
	if c.closed {
		return 0, errors.New("tls: connection closed")
	}
	reply, err := gocvm.Call("tls.io.write", c.handle+"\x1f"+string(p))
	if err != nil {
		return 0, err
	}
	if isRealError(reply) {
		return 0, errors.New(reply)
	}
	return len(p), nil
}

// Handshake is a no-op returning nil once Dial has already completed
// it (matches real Go: tls.Dial already returns a handshaken Conn).
func (c *Conn) Handshake() error {
	if c == nil || !c.valid || !c.real {
		return errNotConnected
	}
	return nil
}

func (c *Conn) Close() error {
	if c == nil || !c.valid {
		return errNotConnected
	}
	if c.closed {
		return nil
	}
	c.closed = true
	if c.real {
		gocvm.Call("tls.io.close", c.handle)
	}
	return nil
}

func LoadX509KeyPair(certFile string, keyFile string) error {
	return errors.New("tls: LoadX509KeyPair not implemented")
}
