package main

import (
	"errors"
	"fmt"
	"net"
	"net/http"
	"strings"
	"websocket"
)

type Msg struct {
	Name string
}

func allowAll(r *http.Request) bool {
	return true
}

func serveOneWS(ln *net.TCPListener) {
	c, err := ln.Accept()
	if err != nil {
		return
	}
	req, _ := http.ReadRequest(c)
	req.Conn = c
	var u websocket.Upgrader
	u.CheckOrigin = allowAll
	ws, uerr := u.Upgrade(req, &http.Response{})
	if uerr != nil {
		return
	}
	mt, p, rerr := ws.ReadMessage()
	if rerr != nil {
		return
	}
	_ = ws.WriteMessage(mt, p)
}

func readHTTP(c *net.Conn) string {
	var out []byte
	buf := make([]byte, 1)
	for {
		n, err := c.Read(buf)
		if n > 0 {
			out = append(out, buf[0:n]...)
			if strings.Contains(string(out), "\r\n\r\n") {
				return string(out)
			}
		}
		if err != nil {
			return string(out)
		}
		if n == 0 {
			return string(out)
		}
	}
}

func main() {
	handshake := "GET /chat HTTP/1.1\r\nHost: server.example.com\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n"
	a, b := net.Pipe()
	_, _ = b.Write([]byte(handshake))
	req, rerr := http.ReadRequest(a)
	fmt.Println(rerr == nil)
	req.Conn = a
	var u websocket.Upgrader
	u.CheckOrigin = allowAll
	resp := &http.Response{}
	srv, uerr := u.Upgrade(req, resp)
	fmt.Println(uerr == nil)
	fmt.Println(resp.Hijack)
	head := readHTTP(b)
	fmt.Println(strings.Contains(head, "101"))
	fmt.Println(strings.Contains(head, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="))

	cli := websocket.NewConn(b, false)
	_ = cli.WriteMessage(websocket.TextMessage, []byte("hello"))
	mt, p, err := srv.ReadMessage()
	fmt.Println(err == nil)
	fmt.Println(mt == websocket.TextMessage)
	fmt.Println(string(p) == "hello")

	_ = srv.WriteMessage(websocket.TextMessage, []byte("world"))
	_, p2, err2 := cli.ReadMessage()
	fmt.Println(err2 == nil)
	fmt.Println(string(p2) == "world")

	_ = cli.WriteJSON(Msg{Name: "Ada"})
	var got Msg
	jerr := srv.ReadJSON(&got)
	fmt.Println(jerr == nil)
	fmt.Println(got.Name == "Ada")

	r := &http.Request{Header: map[string]string{"connection": "Upgrade", "upgrade": "websocket"}}
	fmt.Println(websocket.IsWebSocketUpgrade(r))
	r2 := &http.Request{Header: map[string]string{"connection": "keep-alive"}}
	fmt.Println(websocket.IsWebSocketUpgrade(r2))

	cm := websocket.FormatCloseMessage(websocket.CloseNormalClosure, "")
	fmt.Println(len(cm) == 2)
	ce := &websocket.CloseError{Code: websocket.CloseNormalClosure}
	fmt.Println(strings.Contains(ce.Error(), "normal"))
	fmt.Println(websocket.IsCloseError(errors.New(ce.Error()), websocket.CloseNormalClosure))

	_, wssErr := websocket.Dial("wss://example.com/ws")
	fmt.Println(wssErr != nil)

	ln, lerr := net.Listen("tcp", "127.0.0.1:34582")
	if lerr != nil {
		fmt.Println("http-skip")
	} else {
		go serveOneWS(ln)
		c, derr := websocket.Dial("ws://127.0.0.1:34582/ws")
		fmt.Println("http-ok")
		fmt.Println(derr == nil)
		_ = c.WriteMessage(websocket.TextMessage, []byte("ping"))
		_, p3, rerr3 := c.ReadMessage()
		fmt.Println(rerr3 == nil)
		fmt.Println(string(p3) == "ping")
		ln.Close()
	}
}
