package websocket

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"net"
	"strconv"
	"strings"
	"unicode/utf8"
)

const (
	finalBit = 1 << 7
	rsv1Bit  = 1 << 6
	rsv2Bit  = 1 << 5
	rsv3Bit  = 1 << 4
	maskBit  = 1 << 7

	maxFrameHeaderSize         = 14
	maxControlFramePayloadSize = 125
)

const (
	CloseNormalClosure           = 1000
	CloseGoingAway               = 1001
	CloseProtocolError           = 1002
	CloseUnsupportedData         = 1003
	CloseNoStatusReceived        = 1005
	CloseAbnormalClosure         = 1006
	CloseInvalidFramePayloadData = 1007
	ClosePolicyViolation         = 1008
	CloseMessageTooBig           = 1009
	CloseMandatoryExtension      = 1010
	CloseInternalServerErr       = 1011
	CloseServiceRestart          = 1012
	CloseTryAgainLater           = 1013
	CloseTLSHandshake            = 1015
)

const (
	TextMessage   = 1
	BinaryMessage = 2
	CloseMessage  = 8
	PingMessage   = 9
	PongMessage   = 10
)

var ErrCloseSent = errors.New("websocket: close sent")
var ErrReadLimit = errors.New("websocket: read limit exceeded")
var ErrBadHandshake = errors.New("websocket: bad handshake")

var errBadWriteOpCode = errors.New("websocket: bad write message type")
var errInvalidControlFrame = errors.New("websocket: invalid control frame")

type CloseError struct {
	Code int
	Text string
}

func (e *CloseError) Error() string {
	s := "websocket: close " + strconv.Itoa(e.Code)
	switch e.Code {
	case CloseNormalClosure:
		s = s + " (normal)"
	case CloseGoingAway:
		s = s + " (going away)"
	case CloseProtocolError:
		s = s + " (protocol error)"
	case CloseUnsupportedData:
		s = s + " (unsupported data)"
	case CloseNoStatusReceived:
		s = s + " (no status)"
	case CloseAbnormalClosure:
		s = s + " (abnormal closure)"
	case CloseInvalidFramePayloadData:
		s = s + " (invalid payload data)"
	case ClosePolicyViolation:
		s = s + " (policy violation)"
	case CloseMessageTooBig:
		s = s + " (message too big)"
	case CloseMandatoryExtension:
		s = s + " (mandatory extension missing)"
	case CloseInternalServerErr:
		s = s + " (internal server error)"
	case CloseTLSHandshake:
		s = s + " (TLS handshake error)"
	}
	if e.Text != "" {
		s = s + ": " + e.Text
	}
	return s
}

func closeCodeFromError(err error) (int, bool) {
	if err == nil {
		return 0, false
	}
	s := err.Error()
	pref := "websocket: close "
	if !strings.HasPrefix(s, pref) {
		return 0, false
	}
	rest := s[len(pref):]
	n := 0
	i := 0
	if i >= len(rest) || rest[i] < '0' || rest[i] > '9' {
		return 0, false
	}
	for i < len(rest) && rest[i] >= '0' && rest[i] <= '9' {
		n = n*10 + int(rest[i]-'0')
		i = i + 1
	}
	return n, true
}

func IsCloseError(err error, codes ...int) bool {
	n, ok := closeCodeFromError(err)
	if !ok {
		return false
	}
	for i := 0; i < len(codes); i++ {
		if n == codes[i] {
			return true
		}
	}
	return false
}

func IsUnexpectedCloseError(err error, expectedCodes ...int) bool {
	n, ok := closeCodeFromError(err)
	if !ok {
		return false
	}
	for i := 0; i < len(expectedCodes); i++ {
		if n == expectedCodes[i] {
			return false
		}
	}
	return true
}

func isValidReceivedCloseCode(code int) bool {
	if code == CloseNormalClosure || code == CloseGoingAway || code == CloseProtocolError ||
		code == CloseUnsupportedData || code == CloseInvalidFramePayloadData ||
		code == ClosePolicyViolation || code == CloseMessageTooBig ||
		code == CloseMandatoryExtension || code == CloseInternalServerErr ||
		code == CloseServiceRestart || code == CloseTryAgainLater {
		return true
	}
	return code >= 3000 && code <= 4999
}

func isControl(frameType int) bool {
	return frameType == CloseMessage || frameType == PingMessage || frameType == PongMessage
}

func isData(frameType int) bool {
	return frameType == TextMessage || frameType == BinaryMessage
}

func newMaskKey() [4]byte {
	b := make([]byte, 4)
	_, _ = maskRand(b)
	var k [4]byte
	k[0] = b[0]
	k[1] = b[1]
	k[2] = b[2]
	k[3] = b[3]
	return k
}

type Conn struct {
	conn        *net.Conn
	isServer    bool
	subprotocol string
	leftover    []byte
	closeSent   bool
	readLimit   int
	readErr     error
}

func NewConn(c *net.Conn, isServer bool) *Conn {
	return &Conn{conn: c, isServer: isServer}
}

func (c *Conn) Subprotocol() string {
	if c == nil {
		return ""
	}
	return c.subprotocol
}

func (c *Conn) NetConn() *net.Conn {
	if c == nil {
		return nil
	}
	return c.conn
}

func (c *Conn) Close() error {
	if c == nil || c.conn == nil {
		return nil
	}
	return c.conn.Close()
}

func (c *Conn) SetReadLimit(limit int) {
	if c != nil {
		c.readLimit = limit
	}
}

func (c *Conn) readN(n int) ([]byte, error) {
	if n <= 0 {
		return []byte{}, nil
	}
	out := make([]byte, n)
	off := 0
	if len(c.leftover) > 0 {
		copied := copy(out, c.leftover)
		c.leftover = c.leftover[copied:]
		off = copied
	}
	for off < n {
		got, err := c.conn.Read(out[off:])
		if got > 0 {
			off = off + got
		}
		if off >= n {
			return out, nil
		}
		if err != nil {
			return out[0:off], err
		}
		if got == 0 {
			return out[0:off], errors.New("EOF")
		}
	}
	return out, nil
}

func (c *Conn) writeRaw(p []byte) error {
	if c.closeSent {
		return ErrCloseSent
	}
	_, err := c.conn.Write(p)
	return err
}

func (c *Conn) writeFrame(opcode int, payload []byte, fin bool) error {
	if c == nil || c.conn == nil {
		return errors.New("websocket: nil conn")
	}
	if isControl(opcode) && len(payload) > maxControlFramePayloadSize {
		return errInvalidControlFrame
	}
	b0 := byte(opcode)
	if fin {
		b0 = b0 | finalBit
	}
	n := len(payload)
	mask := byte(0)
	if !c.isServer {
		mask = maskBit
	}
	var hdr []byte
	if n < 126 {
		hdr = []byte{b0, mask | byte(n)}
	} else if n < 65536 {
		hdr = []byte{b0, mask | 126, byte(n >> 8), byte(n)}
	} else {
		hdr = []byte{b0, mask | 127, 0, 0, 0, 0, 0, 0, 0, 0}
		binary.BigEndian.PutUint64(hdr[2:], uint64(n))
	}
	data := payload
	if !c.isServer {
		key := newMaskKey()
		hdr = append(hdr, key[0], key[1], key[2], key[3])
		data = make([]byte, len(payload))
		copy(data, payload)
		maskBytes(key, 0, data)
	}
	buf := append(hdr, data...)
	err := c.writeRaw(buf)
	if err != nil {
		return err
	}
	if opcode == CloseMessage {
		c.closeSent = true
	}
	return nil
}

func (c *Conn) WriteMessage(messageType int, data []byte) error {
	if !isControl(messageType) && !isData(messageType) {
		return errBadWriteOpCode
	}
	if data == nil {
		data = []byte{}
	}
	return c.writeFrame(messageType, data, true)
}

func (c *Conn) WriteControl(messageType int, data []byte) error {
	if !isControl(messageType) {
		return errBadWriteOpCode
	}
	if data == nil {
		data = []byte{}
	}
	return c.writeFrame(messageType, data, true)
}

func (c *Conn) readFrame() (int, []byte, bool, error) {
	h, err := c.readN(2)
	if err != nil {
		return 0, nil, false, err
	}
	if len(h) < 2 {
		return 0, nil, false, errors.New("EOF")
	}
	b0 := h[0]
	b1 := h[1]
	fin := b0&finalBit != 0
	if b0&rsv1Bit != 0 || b0&rsv2Bit != 0 || b0&rsv3Bit != 0 {
		return 0, nil, false, errors.New("websocket: RSV bits set")
	}
	opcode := int(b0 & 0x0f)
	mask := b1&maskBit != 0
	n := int(b1 & 0x7f)
	if n == 126 {
		ext, e2 := c.readN(2)
		if e2 != nil || len(ext) < 2 {
			return 0, nil, false, e2
		}
		n = int(binary.BigEndian.Uint16(ext))
	} else if n == 127 {
		ext, e2 := c.readN(8)
		if e2 != nil || len(ext) < 8 {
			return 0, nil, false, e2
		}
		n64 := binary.BigEndian.Uint64(ext)
		if n64 > 0x7fffffff {
			return 0, nil, false, ErrReadLimit
		}
		n = int(n64)
	}
	if isControl(opcode) {
		if !fin {
			return 0, nil, false, errors.New("websocket: FIN not set on control")
		}
		if n > maxControlFramePayloadSize {
			return 0, nil, false, errors.New("websocket: len > 125 for control")
		}
	}
	if mask != c.isServer {
		return 0, nil, false, errors.New("websocket: bad MASK")
	}
	var key [4]byte
	if mask {
		kb, e2 := c.readN(4)
		if e2 != nil || len(kb) < 4 {
			return 0, nil, false, e2
		}
		key[0] = kb[0]
		key[1] = kb[1]
		key[2] = kb[2]
		key[3] = kb[3]
	}
	payload := []byte{}
	if n > 0 {
		payload, err = c.readN(n)
		if err != nil {
			return opcode, payload, fin, err
		}
		if mask {
			maskBytes(key, 0, payload)
		}
	}
	return opcode, payload, fin, nil
}

func (c *Conn) handleControl(opcode int, payload []byte) error {
	if opcode == PingMessage {
		return c.WriteControl(PongMessage, payload)
	}
	if opcode == PongMessage {
		return nil
	}
	if opcode == CloseMessage {
		code := CloseNoStatusReceived
		text := ""
		if len(payload) >= 2 {
			code = int(binary.BigEndian.Uint16(payload))
			if !isValidReceivedCloseCode(code) {
				_ = c.WriteControl(CloseMessage, FormatCloseMessage(CloseProtocolError, ""))
				return errors.New("websocket: bad close code " + strconv.Itoa(code))
			}
			text = string(payload[2:])
			if !utf8.ValidString(text) {
				_ = c.WriteControl(CloseMessage, FormatCloseMessage(CloseProtocolError, ""))
				return errors.New("websocket: invalid utf8 payload in close frame")
			}
		}
		if !c.closeSent {
			_ = c.WriteControl(CloseMessage, FormatCloseMessage(code, ""))
		}
		return closeError(code, text)
	}
	return errors.New("websocket: bad opcode " + strconv.Itoa(opcode))
}

func (c *Conn) ReadMessage() (int, []byte, error) {
	if c.readErr != nil {
		return 0, nil, c.readErr
	}
	var data []byte
	gotData := false
	dataType := 0
	for {
		opcode, payload, fin, err := c.readFrame()
		if err != nil && payload == nil {
			c.readErr = err
			return 0, nil, err
		}
		if isControl(opcode) {
			cerr := c.handleControl(opcode, payload)
			if cerr != nil {
				c.readErr = cerr
				return 0, nil, cerr
			}
			continue
		}
		if opcode == TextMessage || opcode == BinaryMessage {
			if gotData {
				c.readErr = errors.New("websocket: data before FIN")
				return 0, nil, c.readErr
			}
			gotData = true
			dataType = opcode
			data = payload
		} else if opcode == 0 {
			if !gotData {
				c.readErr = errors.New("websocket: continuation after FIN")
				return 0, nil, c.readErr
			}
			data = append(data, payload...)
		} else {
			c.readErr = errors.New("websocket: bad opcode " + strconv.Itoa(opcode))
			return 0, nil, c.readErr
		}
		if c.readLimit > 0 && len(data) > c.readLimit {
			_ = c.WriteControl(CloseMessage, FormatCloseMessage(CloseMessageTooBig, ""))
			c.readErr = ErrReadLimit
			return 0, nil, ErrReadLimit
		}
		if fin {
			return dataType, data, nil
		}
		if err != nil {
			c.readErr = err
			return 0, nil, err
		}
	}
}

func (c *Conn) WriteJSON(v any) error {
	b, err := json.Marshal(v)
	if err != nil {
		return err
	}
	return c.WriteMessage(TextMessage, b)
}

func (c *Conn) ReadJSON(v any) error {
	_, p, err := c.ReadMessage()
	if err != nil {
		return err
	}
	return json.Unmarshal(p, v)
}

func closeError(code int, text string) error {
	e := &CloseError{Code: code, Text: text}
	return errors.New(e.Error())
}

func FormatCloseMessage(closeCode int, text string) []byte {
	if closeCode == CloseNoStatusReceived {
		return []byte{}
	}
	buf := make([]byte, 2+len(text))
	binary.BigEndian.PutUint16(buf, uint16(closeCode))
	copy(buf[2:], text)
	return buf
}

func WriteJSON(c *Conn, v any) error {
	return c.WriteJSON(v)
}

func ReadJSON(c *Conn, v any) error {
	return c.ReadJSON(v)
}
