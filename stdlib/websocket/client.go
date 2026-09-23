package websocket

import (
	"errors"
	"net"
	"net/url"
	"strings"
)

var errMalformedURL = errors.New("malformed ws or wss URL")
var errWSS = errors.New("websocket: wss is not supported (no TLS wrap on Dial)")

type Dialer struct {
	ReadBufferSize    int
	WriteBufferSize   int
	Subprotocols      []string
	EnableCompression bool
}

var DefaultDialer = &Dialer{}

func hostPortNoPort(host string, scheme string) string {
	if strings.Contains(host, ":") {
		return host
	}
	if scheme == "wss" || scheme == "https" {
		return host + ":443"
	}
	return host + ":80"
}

func (d *Dialer) Dial(urlStr string) (*Conn, error) {
	if d == nil {
		d = DefaultDialer
	}
	u, err := url.Parse(urlStr)
	if err != nil {
		return nil, err
	}
	if u.Scheme != "ws" {
		if u.Scheme == "wss" {
			return nil, errWSS
		}
		return nil, errMalformedURL
	}
	challengeKey, kerr := generateChallengeKey()
	if kerr != nil {
		return nil, kerr
	}
	hostPort := hostPortNoPort(u.Host, u.Scheme)
	path := u.Path
	if path == "" {
		path = "/"
	}
	if u.RawQuery != "" {
		path = path + "?" + u.RawQuery
	}
	netConn, derr := net.Dial("tcp", hostPort)
	if derr != nil {
		return nil, derr
	}
	req := "GET " + path + " HTTP/1.1\r\nHost: " + u.Host + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + challengeKey + "\r\nSec-WebSocket-Version: 13\r\n"
	if len(d.Subprotocols) > 0 {
		req = req + "Sec-WebSocket-Protocol: " + strings.Join(d.Subprotocols, ", ") + "\r\n"
	}
	req = req + "\r\n"
	_, werr := netConn.Write([]byte(req))
	if werr != nil {
		netConn.Close()
		return nil, werr
	}
	raw, rerr := readUntil(netConn, "\r\n\r\n")
	if rerr != nil && raw == "" {
		netConn.Close()
		return nil, rerr
	}
	st := parseStatusLine(raw)
	hdrs := headerMapFromRaw(raw)
	if st != 101 ||
		!headerTokenContains(hdrs, "Upgrade", "websocket") ||
		!headerTokenContains(hdrs, "Connection", "upgrade") ||
		hdrs["sec-websocket-accept"] != computeAcceptKey(challengeKey) {
		netConn.Close()
		return nil, ErrBadHandshake
	}
	c := NewConn(netConn, false)
	c.subprotocol = hdrs["sec-websocket-protocol"]
	return c, nil
}

func Dial(urlStr string) (*Conn, error) {
	return DefaultDialer.Dial(urlStr)
}
