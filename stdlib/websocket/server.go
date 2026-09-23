package websocket

import (
	"errors"
	"net/http"
	"strings"
)

type HandshakeError struct {
	Message string
}

func (e HandshakeError) Error() string {
	return e.Message
}

type Upgrader struct {
	ReadBufferSize    int
	WriteBufferSize   int
	Subprotocols      []string
	CheckOrigin       func(r *http.Request) bool
	EnableCompression bool
}

func (u *Upgrader) returnError(w *http.Response, status int, reason string) (*Conn, error) {
	if w != nil {
		w.Status = status
		w.Body = reason
		w.ContentType = "text/plain"
	}
	return nil, errors.New(reason)
}

func (u *Upgrader) selectSubprotocol(r *http.Request) string {
	if u == nil || u.Subprotocols == nil {
		return ""
	}
	client := requestSubprotocols(r)
	for i := 0; i < len(u.Subprotocols); i++ {
		for j := 0; j < len(client); j++ {
			if client[j] == u.Subprotocols[i] {
				return u.Subprotocols[i]
			}
		}
	}
	return ""
}

// Upgrade upgrades the HTTP server connection to the WebSocket protocol.
//
// gorilla takes (http.ResponseWriter, *http.Request, http.Header) and
// Hijack()s the TCP conn. This net/http has no ResponseWriter: ServeHandler
// stores the accepted conn on Request.Conn and skips Close when
// Response.Hijack is set. Upgrade writes the 101 itself and sets Hijack.
func (u *Upgrader) Upgrade(r *http.Request, w *http.Response) (*Conn, error) {
	if r == nil || r.Conn == nil {
		return u.returnError(w, http.StatusInternalServerError, "websocket: missing hijacked connection")
	}
	if !requestTokenContains(r, "Connection", "upgrade") {
		return u.returnError(w, http.StatusBadRequest, "websocket: the client is not using the websocket protocol: 'upgrade' token not found in 'Connection' header")
	}
	if !requestTokenContains(r, "Upgrade", "websocket") {
		return u.returnError(w, http.StatusBadRequest, "websocket: the client is not using the websocket protocol: 'websocket' token not found in 'Upgrade' header")
	}
	if r.Method != "GET" {
		return u.returnError(w, http.StatusMethodNotAllowed, "websocket: the client is not using the websocket protocol: request method is not GET")
	}
	if !requestTokenContains(r, "Sec-Websocket-Version", "13") {
		return u.returnError(w, http.StatusBadRequest, "websocket: unsupported version: 13 not found in 'Sec-Websocket-Version' header")
	}
	checkOrigin := u.CheckOrigin
	if checkOrigin == nil {
		checkOrigin = checkSameOrigin
	}
	if !checkOrigin(r) {
		return u.returnError(w, http.StatusForbidden, "websocket: request origin not allowed by Upgrader.CheckOrigin")
	}
	challengeKey := r.HeaderGet("Sec-Websocket-Key")
	if !isValidChallengeKey(challengeKey) {
		return u.returnError(w, http.StatusBadRequest, "websocket: not a websocket handshake: 'Sec-WebSocket-Key' header must be Base64 encoded value of 16-byte in length")
	}
	subprotocol := u.selectSubprotocol(r)
	accept := computeAcceptKey(challengeKey)
	p := "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n"
	if subprotocol != "" {
		p = p + "Sec-WebSocket-Protocol: " + subprotocol + "\r\n"
	}
	p = p + "\r\n"
	_, err := r.Conn.Write([]byte(p))
	if err != nil {
		return nil, err
	}
	if w != nil {
		w.Hijack = true
		w.Status = http.StatusSwitchingProtocols
	}
	c := NewConn(r.Conn, true)
	c.subprotocol = subprotocol
	return c, nil
}

func Upgrade(r *http.Request, w *http.Response) (*Conn, error) {
	u := Upgrader{}
	u.CheckOrigin = func(req *http.Request) bool {
		return true
	}
	return u.Upgrade(r, w)
}

func requestSubprotocols(r *http.Request) []string {
	if r == nil {
		return nil
	}
	h := strings.TrimSpace(r.HeaderGet("Sec-Websocket-Protocol"))
	if h == "" {
		return nil
	}
	protocols := strings.Split(h, ",")
	for i := 0; i < len(protocols); i++ {
		protocols[i] = strings.TrimSpace(protocols[i])
	}
	return protocols
}

func Subprotocols(r *http.Request) []string {
	return requestSubprotocols(r)
}

func IsWebSocketUpgrade(r *http.Request) bool {
	return requestTokenContains(r, "Connection", "upgrade") &&
		requestTokenContains(r, "Upgrade", "websocket")
}
