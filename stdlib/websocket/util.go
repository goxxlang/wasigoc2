// Package websocket is a Go++ port of gorilla/websocket (BSD-3, v1.5.3):
// the RFC 6455 protocol, Conn.ReadMessage/WriteMessage, Upgrader, Dialer,
// and ReadJSON/WriteJSON.
//
// gorilla's public names are kept: TextMessage/BinaryMessage/CloseMessage/
// PingMessage/PongMessage, CloseError, IsCloseError, FormatCloseMessage,
// the RFC 6455 accept-key GUID, client-to-server masking, server
// unmasking, default ping-to-pong, close echo.
//
// Bounds vs gorilla: net/http has no ResponseWriter/Hijacker.
// Upgrader.Upgrade takes (*http.Request, *http.Response); the accepted
// TCP conn is Request.Conn (set by ServeHandler) and a successful
// Upgrade sets Response.Hijack so the server does not Close the conn.
// Dialer.Dial(url) only -- no DialContext, Proxy, TLS, CookieJar,
// httptrace; wss returns a clear error. No NextReader/NextWriter,
// PreparedMessage, permessage-deflate, or write deadlines.
// maskBytes is gorilla's appengine/safe byte loop, not the unsafe
// word-at-a-time path. crypto/rand.Read here is xorshift, not a CSPRNG.
// IsCloseError matches the Error() text (this error type is not an
// interface, so gorilla's err.(*CloseError) assertion is not representable).
package websocket

import (
	"crypto/rand"
	"crypto/sha1"
	"encoding/base64"
	"net"
	"net/http"
	"strings"
)

// RFC 6455 section 1.3 magic GUID.
const keyGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

func computeAcceptKey(challengeKey string) string {
	sum := sha1.Sum([]byte(challengeKey + keyGUID))
	return base64.StdEncoding.EncodeToString(sum)
}

func generateChallengeKey() (string, error) {
	p := make([]byte, 16)
	_, err := rand.Read(p)
	if err != nil {
		return "", err
	}
	return base64.StdEncoding.EncodeToString(p), nil
}

func isValidChallengeKey(s string) bool {
	if s == "" {
		return false
	}
	decoded, err := base64.StdEncoding.DecodeString(s)
	return err == nil && len(decoded) == 16
}

// maskBytes is gorilla's appengine/safe path (mask.go uses unsafe
// word-at-a-time XOR; this project has no unsafe pointer punch-through).
func maskBytes(key [4]byte, pos int, b []byte) int {
	for i := 0; i < len(b); i++ {
		b[i] = b[i] ^ key[pos&3]
		pos = pos + 1
	}
	return pos & 3
}

func headerTokenContains(h map[string]string, name string, value string) bool {
	if h == nil {
		return false
	}
	s := h[strings.ToLower(name)]
	parts := strings.Split(s, ",")
	for i := 0; i < len(parts); i++ {
		if strings.EqualFold(strings.TrimSpace(parts[i]), value) {
			return true
		}
	}
	return false
}

func requestTokenContains(r *http.Request, name string, value string) bool {
	if r == nil {
		return false
	}
	return headerTokenContains(r.Header, name, value)
}

func skipSpace(s string) string {
	i := 0
	for i < len(s) {
		if s[i] != ' ' && s[i] != '\t' {
			break
		}
		i = i + 1
	}
	return s[i:]
}

func nextToken(s string) (string, string) {
	i := 0
	for i < len(s) {
		c := s[i]
		if c == ' ' || c == '\t' || c == ',' || c == ';' {
			break
		}
		i = i + 1
	}
	return s[0:i], s[i:]
}

func equalASCIIFold(s string, t string) bool {
	return strings.EqualFold(s, t)
}

func hostOfOrigin(origin string) string {
	u := origin
	if strings.HasPrefix(strings.ToLower(u), "http://") {
		u = u[7:]
	} else if strings.HasPrefix(strings.ToLower(u), "https://") {
		u = u[8:]
	}
	slash := strings.Index(u, "/")
	if slash >= 0 {
		u = u[0:slash]
	}
	return u
}

func checkSameOrigin(r *http.Request) bool {
	if r == nil {
		return false
	}
	origin := r.HeaderGet("Origin")
	if origin == "" {
		return true
	}
	return equalASCIIFold(hostOfOrigin(origin), r.Host)
}

func maskRand(b []byte) (int, error) {
	return rand.Read(b)
}

func readUntil(c *net.Conn, sep string) (string, error) {
	var out []byte
	buf := make([]byte, 1)
	for {
		n, err := c.Read(buf)
		if n > 0 {
			out = append(out, buf[0:n]...)
			s := string(out)
			if strings.Contains(s, sep) {
				return s, nil
			}
		}
		if err != nil {
			if len(out) > 0 {
				return string(out), err
			}
			return "", err
		}
		if n == 0 {
			return string(out), nil
		}
	}
}

func headerMapFromRaw(raw string) map[string]string {
	m := map[string]string{}
	rest := raw
	nl := strings.Index(rest, "\r\n")
	if nl >= 0 {
		rest = rest[nl+2:]
	}
	for rest != "" {
		lineEnd := strings.Index(rest, "\r\n")
		line := rest
		if lineEnd >= 0 {
			line = rest[0:lineEnd]
			rest = rest[lineEnd+2:]
		} else {
			rest = ""
		}
		if line == "" {
			break
		}
		colon := strings.Index(line, ":")
		if colon < 0 {
			continue
		}
		k := strings.ToLower(strings.TrimSpace(line[0:colon]))
		v := strings.TrimSpace(line[colon+1:])
		m[k] = v
	}
	return m
}

func parseStatusLine(raw string) int {
	nl := strings.Index(raw, "\r\n")
	line := raw
	if nl >= 0 {
		line = raw[0:nl]
	}
	parts := strings.Split(line, " ")
	if len(parts) < 2 {
		return 0
	}
	n := 0
	for i := 0; i < len(parts[1]); i++ {
		c := parts[1][i]
		if c < '0' || c > '9' {
			break
		}
		n = n*10 + int(c-'0')
	}
	return n
}
