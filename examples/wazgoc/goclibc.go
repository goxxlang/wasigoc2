package wazgoc

import (
	"os"
)

// goclibc is the native side of libc/goclibc.c. The wasigocvm module's
// fopen/fwrite/mkdir call these imports. This process is the engine
// (wasitime), so the bytes land in a host file. A JS runner does not
// serve that role: JS stays in the unil sandbox on the client.

type gocBox struct {
	path string
	data []byte
	refs int
	used int
}

type gocFd struct {
	box    int
	pos    int
	append int
	used   int
}

var gocBoxes []gocBox
var gocFds []gocFd
var gocCwd string

func gocErr(stack []uint64, code int) {
	stack[0] = uint64(uint32(int32(code)))
}

func gocStr(m *Module, p int, n int) (string, bool) {
	if n < 0 {
		return "", false
	}
	if n == 0 {
		return "", true
	}
	b, ok := m.readBytes(p, n)
	if !ok {
		return "", false
	}
	return string(b), true
}

func gocNewBox(path string, data []byte) int {
	box := gocBox{}
	box.path = path
	box.data = data
	box.refs = 1
	box.used = 1
	if len(gocBoxes) == 0 {
		gocBoxes = append(gocBoxes, gocBox{})
	}
	for i := 1; i < len(gocBoxes); i++ {
		if gocBoxes[i].used == 0 {
			gocBoxes[i] = box
			return i
		}
	}
	gocBoxes = append(gocBoxes, box)
	return len(gocBoxes) - 1
}

func gocNewFd(box int, app int, pos int) int {
	fd := gocFd{}
	fd.box = box
	fd.pos = pos
	fd.append = app
	fd.used = 1
	for i := 1; i < len(gocFds); i++ {
		if gocFds[i].used == 0 {
			gocFds[i] = fd
			return i
		}
	}
	if len(gocFds) == 0 {
		gocFds = append(gocFds, gocFd{})
	}
	gocFds = append(gocFds, fd)
	return len(gocFds) - 1
}

func gocFdOk(fd int) bool {
	return fd > 0 && fd < len(gocFds) && gocFds[fd].used != 0
}

func gocFlush(box int) int {
	if box <= 0 || box >= len(gocBoxes) || gocBoxes[box].used == 0 {
		return -9
	}
	if gocBoxes[box].path == "" {
		return 0
	}
	err := os.WriteFile(gocBoxes[box].path, gocBoxes[box].data, 0644)
	if err != nil {
		return -5
	}
	return 0
}

func gocPutStat(m *Module, out int, isdir int, size int64) {
	var b [16]byte
	mode := uint32(0x8000 | 0644)
	if isdir != 0 {
		mode = uint32(0x4000 | 0755)
	}
	b[0] = byte(mode)
	b[1] = byte(mode >> 8)
	b[2] = byte(mode >> 16)
	b[3] = byte(mode >> 24)
	if isdir != 0 {
		b[4] = 1
	}
	u := uint64(size)
	for i := 0; i < 8; i++ {
		b[8+i] = byte(u >> uint(8*i))
	}
	_ = m.writeBytes(out, b[:])
}

func gocStatPath(m *Module, path string, out int) int {
	fi, err := os.Stat(path)
	if err != nil {
		return -2
	}
	dir := 0
	if fi.IsDir() {
		dir = 1
	}
	gocPutStat(m, out, dir, fi.Size())
	return 0
}

func goclibcDispatch(m *Module, name string, stack []uint64) bool {
	if len(name) < 8 || name[0:8] != "goclibc." {
		return false
	}
	op := name[8:]
	if op == "open" {
		gocOpen(m, stack)
		return true
	}
	if op == "read" {
		gocRead(m, stack)
		return true
	}
	if op == "write" {
		gocWrite(m, stack)
		return true
	}
	if op == "close" {
		gocClose(stack)
		return true
	}
	if op == "seek" {
		gocSeek(stack)
		return true
	}
	if op == "flush" {
		if len(stack) < 1 {
			return true
		}
		fd := int(uint32(stack[0]))
		if !gocFdOk(fd) {
			gocErr(stack, -9)
			return true
		}
		stack[0] = uint64(uint32(int32(gocFlush(gocFds[fd].box))))
		return true
	}
	if op == "stat" {
		gocStat(m, stack)
		return true
	}
	if op == "fstat" {
		gocFstat(m, stack)
		return true
	}
	if op == "mkdir" {
		gocMkdir(m, stack)
		return true
	}
	if op == "unlink" {
		gocUnlink(m, stack, 0)
		return true
	}
	if op == "rmdir" {
		gocUnlink(m, stack, 1)
		return true
	}
	if op == "rename" {
		gocRename(m, stack)
		return true
	}
	if op == "access" {
		gocAccess(m, stack)
		return true
	}
	if op == "getcwd" {
		gocGetcwd(m, stack)
		return true
	}
	if op == "chdir" {
		gocChdir(m, stack)
		return true
	}
	if op == "readdir" {
		gocReaddir(m, stack)
		return true
	}
	if op == "dup" {
		gocDup(stack)
		return true
	}
	gocErr(stack, -52)
	return true
}

func gocOpen(m *Module, stack []uint64) {
	if len(stack) < 3 {
		return
	}
	path, ok := gocStr(m, int(uint32(stack[0])), int(uint32(stack[1])))
	if !ok || path == "" {
		gocErr(stack, -22)
		return
	}
	flags := int(uint32(stack[2]))
	trunc := flags & 8
	creat := flags & 4
	app := 0
	if flags&16 != 0 {
		app = 1
	}
	var data []byte
	if trunc == 0 {
		b, err := os.ReadFile(path)
		if err != nil {
			if creat == 0 {
				gocErr(stack, -2)
				return
			}
			data = []byte{}
		} else {
			data = b
		}
	} else {
		data = []byte{}
	}
	box := gocNewBox(path, data)
	pos := 0
	if app != 0 {
		pos = len(gocBoxes[box].data)
	}
	fd := gocNewFd(box, app, pos)
	if creat != 0 || trunc != 0 {
		rc := gocFlush(box)
		if rc < 0 {
			gocErr(stack, rc)
			return
		}
	}
	stack[0] = uint64(uint32(fd))
}

func gocRead(m *Module, stack []uint64) {
	if len(stack) < 3 {
		return
	}
	fd := int(uint32(stack[0]))
	ptr := int(uint32(stack[1]))
	n := int(uint32(stack[2]))
	if !gocFdOk(fd) {
		gocErr(stack, -9)
		return
	}
	slot := gocFds[fd]
	box := gocBoxes[slot.box]
	if slot.pos >= len(box.data) || n <= 0 {
		stack[0] = 0
		return
	}
	avail := len(box.data) - slot.pos
	if avail > n {
		avail = n
	}
	if !m.writeBytes(ptr, box.data[slot.pos:slot.pos+avail]) {
		gocErr(stack, -5)
		return
	}
	slot.pos = slot.pos + avail
	gocFds[fd] = slot
	stack[0] = uint64(uint32(avail))
}

func gocWrite(m *Module, stack []uint64) {
	if len(stack) < 3 {
		return
	}
	fd := int(uint32(stack[0]))
	ptr := int(uint32(stack[1]))
	n := int(uint32(stack[2]))
	if !gocFdOk(fd) {
		gocErr(stack, -9)
		return
	}
	var buf []byte
	var ok bool
	if n > 0 {
		buf, ok = m.readBytes(ptr, n)
		if !ok {
			gocErr(stack, -5)
			return
		}
	}
	slot := gocFds[fd]
	box := gocBoxes[slot.box]
	if slot.append != 0 {
		slot.pos = len(box.data)
	}
	end := slot.pos + len(buf)
	for len(box.data) < end {
		box.data = append(box.data, 0)
	}
	for i := 0; i < len(buf); i++ {
		box.data[slot.pos+i] = buf[i]
	}
	slot.pos = end
	gocBoxes[slot.box] = box
	gocFds[fd] = slot
	rc := gocFlush(slot.box)
	if rc < 0 {
		gocErr(stack, rc)
		return
	}
	stack[0] = uint64(uint32(len(buf)))
}

func gocClose(stack []uint64) {
	if len(stack) < 1 {
		return
	}
	fd := int(uint32(stack[0]))
	if !gocFdOk(fd) {
		gocErr(stack, -9)
		return
	}
	slot := gocFds[fd]
	box := gocBoxes[slot.box]
	box.refs = box.refs - 1
	rc := 0
	if box.refs <= 0 {
		rc = gocFlush(slot.box)
		box.used = 0
		box.data = nil
	}
	gocBoxes[slot.box] = box
	slot.used = 0
	gocFds[fd] = slot
	if rc < 0 {
		gocErr(stack, rc)
		return
	}
	stack[0] = 0
}

func gocSeek(stack []uint64) {
	if len(stack) < 3 {
		return
	}
	fd := int(uint32(stack[0]))
	off := int(int32(uint32(stack[1])))
	whence := int(uint32(stack[2]))
	if !gocFdOk(fd) {
		gocErr(stack, -9)
		return
	}
	slot := gocFds[fd]
	box := gocBoxes[slot.box]
	base := 0
	if whence == 1 {
		base = slot.pos
	}
	if whence == 2 {
		base = len(box.data)
	}
	npos := base + off
	if npos < 0 {
		gocErr(stack, -22)
		return
	}
	slot.pos = npos
	gocFds[fd] = slot
	stack[0] = uint64(uint32(npos))
}

func gocStat(m *Module, stack []uint64) {
	if len(stack) < 3 {
		return
	}
	path, ok := gocStr(m, int(uint32(stack[0])), int(uint32(stack[1])))
	if !ok || path == "" {
		gocErr(stack, -22)
		return
	}
	stack[0] = uint64(uint32(int32(gocStatPath(m, path, int(uint32(stack[2]))))))
}

func gocFstat(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	fd := int(uint32(stack[0]))
	out := int(uint32(stack[1]))
	if !gocFdOk(fd) {
		gocErr(stack, -9)
		return
	}
	box := gocBoxes[gocFds[fd].box]
	if box.path != "" {
		stack[0] = uint64(uint32(int32(gocStatPath(m, box.path, out))))
		return
	}
	gocPutStat(m, out, 0, int64(len(box.data)))
	stack[0] = 0
}

func gocPath(m *Module, stack []uint64) (string, bool) {
	if len(stack) < 2 {
		return "", false
	}
	return gocStr(m, int(uint32(stack[0])), int(uint32(stack[1])))
}

func gocMkdir(m *Module, stack []uint64) {
	path, ok := gocPath(m, stack)
	if !ok || path == "" {
		gocErr(stack, -22)
		return
	}
	fi, err := os.Stat(path)
	if err == nil && fi.IsDir() {
		gocErr(stack, -17)
		return
	}
	err = os.Mkdir(path)
	if err != nil {
		gocErr(stack, -5)
		return
	}
	stack[0] = 0
}

func gocUnlink(m *Module, stack []uint64, dir int) {
	path, ok := gocPath(m, stack)
	if !ok || path == "" {
		gocErr(stack, -22)
		return
	}
	fi, err := os.Stat(path)
	if err != nil {
		gocErr(stack, -2)
		return
	}
	if dir != 0 && !fi.IsDir() {
		gocErr(stack, -20)
		return
	}
	if dir == 0 && fi.IsDir() {
		gocErr(stack, -21)
		return
	}
	err = os.Remove(path)
	if err != nil {
		gocErr(stack, -5)
		return
	}
	stack[0] = 0
}

func gocRename(m *Module, stack []uint64) {
	if len(stack) < 4 {
		return
	}
	oldp, ok1 := gocStr(m, int(uint32(stack[0])), int(uint32(stack[1])))
	newp, ok2 := gocStr(m, int(uint32(stack[2])), int(uint32(stack[3])))
	if !ok1 || !ok2 || oldp == "" || newp == "" {
		gocErr(stack, -22)
		return
	}
	err := os.Rename(oldp, newp)
	if err != nil {
		gocErr(stack, -5)
		return
	}
	stack[0] = 0
}

func gocAccess(m *Module, stack []uint64) {
	path, ok := gocPath(m, stack)
	if !ok || path == "" {
		gocErr(stack, -22)
		return
	}
	_, err := os.Stat(path)
	if err != nil {
		gocErr(stack, -2)
		return
	}
	stack[0] = 0
}

func gocGetcwd(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	buf := int(uint32(stack[0]))
	capn := int(uint32(stack[1]))
	if gocCwd == "" {
		gocCwd = "."
	}
	if capn < len(gocCwd)+1 {
		gocErr(stack, -22)
		return
	}
	b := []byte(gocCwd)
	b = append(b, 0)
	if !m.writeBytes(buf, b) {
		gocErr(stack, -5)
		return
	}
	stack[0] = uint64(uint32(len(gocCwd)))
}

func gocChdir(m *Module, stack []uint64) {
	path, ok := gocPath(m, stack)
	if !ok || path == "" {
		gocErr(stack, -22)
		return
	}
	fi, err := os.Stat(path)
	if err != nil || !fi.IsDir() {
		gocErr(stack, -2)
		return
	}
	gocCwd = path
	stack[0] = 0
}

func gocReaddir(m *Module, stack []uint64) {
	if len(stack) < 4 {
		return
	}
	path, ok := gocStr(m, int(uint32(stack[0])), int(uint32(stack[1])))
	if !ok || path == "" {
		gocErr(stack, -22)
		return
	}
	buf := int(uint32(stack[2]))
	capn := int(uint32(stack[3]))
	ents, err := os.ReadDir(path)
	if err != nil {
		gocErr(stack, -2)
		return
	}
	var out []byte
	for i := 0; i < len(ents); i++ {
		name := ents[i].Name()
		if name == "." || name == ".." {
			continue
		}
		kind := byte('f')
		if ents[i].IsDir() {
			kind = 'd'
		}
		out = append(out, kind)
		nb := []byte(name)
		for j := 0; j < len(nb); j++ {
			out = append(out, nb[j])
		}
		out = append(out, 0)
	}
	if len(out) > capn {
		gocErr(stack, -22)
		return
	}
	if len(out) > 0 && !m.writeBytes(buf, out) {
		gocErr(stack, -5)
		return
	}
	stack[0] = uint64(uint32(len(out)))
}

func gocDup(stack []uint64) {
	if len(stack) < 1 {
		return
	}
	fd := int(uint32(stack[0]))
	if !gocFdOk(fd) {
		gocErr(stack, -9)
		return
	}
	slot := gocFds[fd]
	box := gocBoxes[slot.box]
	box.refs = box.refs + 1
	gocBoxes[slot.box] = box
	n := gocNewFd(slot.box, slot.append, slot.pos)
	stack[0] = uint64(uint32(n))
}
