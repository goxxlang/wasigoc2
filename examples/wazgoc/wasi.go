package wazgoc

import (
	"time"
)

func instantiateDefaultHosts(r *Runtime) {
	i32 := []byte{ValueTypeI32}
	i32x2 := []byte{ValueTypeI32, ValueTypeI32}
	i32x3 := []byte{ValueTypeI32, ValueTypeI32, ValueTypeI32}
	i32x4 := []byte{ValueTypeI32, ValueTypeI32, ValueTypeI32, ValueTypeI32}
	i32x2ret := []byte{ValueTypeI32}
	none := []byte{}

	wasi := r.NewHostModuleBuilder("wasi_snapshot_preview1")
	wasi.ExportFunction("fd_write", i32x4, i32x2ret)
	wasi.ExportFunction("fd_read", i32x4, i32x2ret)
	wasi.ExportFunction("proc_exit", i32, none)
	wasi.ExportFunction("args_sizes_get", i32x2, i32x2ret)
	wasi.ExportFunction("args_get", i32x2, i32x2ret)
	wasi.ExportFunction("environ_sizes_get", i32x2, i32x2ret)
	wasi.ExportFunction("environ_get", i32x2, i32x2ret)
	wasi.ExportFunction("clock_time_get", []byte{ValueTypeI32, ValueTypeI64, ValueTypeI32}, i32x2ret)
	wasi.ExportFunction("random_get", i32x2, i32x2ret)
	wasi.ExportFunction("fd_close", i32, i32x2ret)
	wasi.ExportFunction("fd_fdstat_get", i32x2, i32x2ret)
	wasi.ExportFunction("fd_prestat_get", i32x2, i32x2ret)
	wasi.ExportFunction("fd_prestat_dir_name", i32x3, i32x2ret)
	wasi.ExportFunction("fd_seek", []byte{ValueTypeI32, ValueTypeI64, ValueTypeI32, ValueTypeI32}, i32x2ret)
	wasi.ExportFunction("sched_yield", none, i32x2ret)
	wasi.ExportFunction("poll_oneoff", i32x4, i32x2ret)
	_ = wasi.Instantiate()

	old := r.NewHostModuleBuilder("wasi_unstable")
	old.ExportFunction("fd_write", i32x4, i32x2ret)
	old.ExportFunction("fd_read", i32x4, i32x2ret)
	old.ExportFunction("proc_exit", i32, none)
	old.ExportFunction("args_sizes_get", i32x2, i32x2ret)
	old.ExportFunction("args_get", i32x2, i32x2ret)
	old.ExportFunction("environ_sizes_get", i32x2, i32x2ret)
	old.ExportFunction("environ_get", i32x2, i32x2ret)
	old.ExportFunction("clock_time_get", []byte{ValueTypeI32, ValueTypeI64, ValueTypeI32}, i32x2ret)
	old.ExportFunction("random_get", i32x2, i32x2ret)
	_ = old.Instantiate()

	env := r.NewHostModuleBuilder("env")
	env.ExportFunction("log", i32x2, none)
	env.ExportFunction("abort", i32, none)
	_ = env.Instantiate()

	libc := r.NewHostModuleBuilder("goclibc")
	libc.ExportFunction("open", i32x3, i32x2ret)
	libc.ExportFunction("read", i32x3, i32x2ret)
	libc.ExportFunction("write", i32x3, i32x2ret)
	libc.ExportFunction("close", i32, i32x2ret)
	libc.ExportFunction("seek", i32x3, i32x2ret)
	libc.ExportFunction("flush", i32, i32x2ret)
	libc.ExportFunction("stat", i32x3, i32x2ret)
	libc.ExportFunction("fstat", i32x2, i32x2ret)
	libc.ExportFunction("mkdir", i32x2, i32x2ret)
	libc.ExportFunction("unlink", i32x2, i32x2ret)
	libc.ExportFunction("rmdir", i32x2, i32x2ret)
	libc.ExportFunction("rename", i32x4, i32x2ret)
	libc.ExportFunction("access", i32x2, i32x2ret)
	libc.ExportFunction("getcwd", i32x2, i32x2ret)
	libc.ExportFunction("chdir", i32x2, i32x2ret)
	libc.ExportFunction("readdir", i32x4, i32x2ret)
	libc.ExportFunction("dup", i32, i32x2ret)
	_ = libc.Instantiate()
}

// HostExtra handles a host import before the built-in wasi/env table.
// name is "module.func". Return true when the call was handled.
var HostExtra func(m *Module, name string, stack []uint64) bool

func hostDispatch(m *Module, name string, stack []uint64) {
	if HostExtra != nil && HostExtra(m, name, stack) {
		return
	}
	if goclibcDispatch(m, name, stack) {
		return
	}
	if name == "wasi_snapshot_preview1.fd_write" || name == "wasi_unstable.fd_write" {
		wasiFdWrite(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.fd_read" || name == "wasi_unstable.fd_read" {
		wasiFdRead(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.proc_exit" || name == "wasi_unstable.proc_exit" || name == "env.abort" {
		wasiProcExit(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.args_sizes_get" || name == "wasi_unstable.args_sizes_get" {
		wasiArgsSizesGet(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.args_get" || name == "wasi_unstable.args_get" {
		wasiArgsGet(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.environ_sizes_get" || name == "wasi_unstable.environ_sizes_get" {
		wasiEnvironSizesGet(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.environ_get" || name == "wasi_unstable.environ_get" {
		wasiEnvironGet(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.clock_time_get" || name == "wasi_unstable.clock_time_get" {
		wasiClockTimeGet(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.random_get" || name == "wasi_unstable.random_get" {
		wasiRandomGet(m, stack)
		return
	}
	if name == "env.log" {
		envLog(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.fd_fdstat_get" || name == "wasi_unstable.fd_fdstat_get" {
		wasiFdFdstatGet(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.fd_prestat_get" || name == "wasi_snapshot_preview1.fd_prestat_dir_name" {
		wasiEBADF(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.fd_close" || name == "wasi_unstable.fd_close" ||
		name == "wasi_snapshot_preview1.sched_yield" || name == "wasi_unstable.sched_yield" {
		wasiErrno0(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.fd_seek" || name == "wasi_unstable.fd_seek" {
		wasiFdSeek(m, stack)
		return
	}
	if name == "wasi_snapshot_preview1.poll_oneoff" || name == "wasi_unstable.poll_oneoff" {
		wasiErrno0(m, stack)
		return
	}
	if sysrootTrap(m, name, stack) {
		return
	}
	wasiErrno0(m, stack)
}

// sysrootTrap covers leftover host imports the sysroot libc still
// calls (stdio, args, clocks, exit). Sockets, exec, and the rest of
// POSIX live in the guest module (wasigocvm_net / wasigocvm_exec).
func sysrootTrap(m *Module, name string, stack []uint64) bool {
	if nameHas(name, "get-stdout") {
		if len(stack) > 0 {
			stack[0] = 1
		}
		return true
	}
	if nameHas(name, "get-stderr") {
		if len(stack) > 0 {
			stack[0] = 2
		}
		return true
	}
	if nameHas(name, "get-stdin") {
		if len(stack) > 0 {
			stack[0] = 3
		}
		return true
	}
	if nameHas(name, "exit-with-code") || nameHas(name, ".exit") || nameHas(name, "/exit") {
		wasiProcExit(m, stack)
		return true
	}
	if nameHas(name, "output-stream.write") {
		sysrootWrite(m, stack)
		return true
	}
	if nameHas(name, "output-stream.check-write") {
		sysrootCheckWrite(m, stack)
		return true
	}
	if nameHas(name, "output-stream.blocking-flush") {
		sysrootOk(m, stack)
		return true
	}
	if nameHas(name, "output-stream.subscribe") || nameHas(name, "input-stream.subscribe") {
		if len(stack) > 0 {
			stack[0] = 4
		}
		return true
	}
	if nameHas(name, "input-stream.read") {
		sysrootRead(m, stack)
		return true
	}
	if nameHas(name, "get-arguments") {
		sysrootStringList(m, stack, m.args)
		return true
	}
	if nameHas(name, "get-environment") {
		var list []string
		for i := 0; i < len(m.env); i++ {
			list = append(list, m.env[i].key+"="+m.env[i].val)
		}
		sysrootStringList(m, stack, list)
		return true
	}
	if nameHas(name, "monotonic-clock") && nameHas(name, ".now") {
		if len(stack) > 0 {
			stack[0] = uint64(time.Now().UnixNano())
		}
		return true
	}
	if nameHas(name, "wall-clock") && nameHas(name, ".now") {
		sysrootWall(m, stack)
		return true
	}
	if nameHas(name, "get-directories") {
		sysrootStringList(m, stack, nil)
		return true
	}
	if nameHas(name, "pollable.block") || nameHas(name, "subscribe-duration") {
		if nameHas(name, "subscribe-duration") && len(stack) > 0 {
			stack[0] = 4
		}
		return true
	}
	if nameHas(name, "get-terminal") {
		if len(stack) > 0 {
			writeU32(m, int(stack[len(stack)-1]), 0)
		}
		return true
	}
	if nameHas(name, "resource-drop") {
		return true
	}
	return false
}

func nameHas(s string, sub string) bool {
	if len(sub) == 0 || len(s) < len(sub) {
		return s == sub
	}
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return true
		}
	}
	return false
}

func sysrootWrite(m *Module, stack []uint64) {
	if len(stack) < 4 {
		return
	}
	p := int(stack[1])
	n := int(stack[2])
	ret := int(stack[3])
	if n > 0 {
		buf, ok := m.readBytes(p, n)
		if ok {
			fd := uint32(stack[0])
			if fd == 2 && m.stderr != nil {
				_, _ = m.stderr.Write(buf)
			} else if m.stdout != nil {
				_, _ = m.stdout.Write(buf)
			}
		}
	}
	writeU32(m, ret, 0)
}

func sysrootCheckWrite(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	ret := int(stack[1])
	writeU32(m, ret, 0)
	var b [8]byte
	n := uint64(4096)
	b[0] = byte(n)
	b[1] = byte(n >> 8)
	b[2] = byte(n >> 16)
	b[3] = byte(n >> 24)
	b[4] = byte(n >> 32)
	b[5] = byte(n >> 40)
	b[6] = byte(n >> 48)
	b[7] = byte(n >> 56)
	_ = m.writeBytes(ret+8, b[:])
}

func sysrootOk(m *Module, stack []uint64) {
	if len(stack) < 1 {
		return
	}
	ret := int(stack[len(stack)-1])
	writeU32(m, ret, 0)
}

func sysrootErr(m *Module, stack []uint64) {
	if len(stack) < 1 {
		return
	}
	ret := int(stack[len(stack)-1])
	writeU32(m, ret, 1)
}

func sysrootRead(m *Module, stack []uint64) {
	if len(stack) < 3 {
		return
	}
	ret := int(stack[len(stack)-1])
	writeU32(m, ret, 0)
	writeU32(m, ret+4, 0)
	writeU32(m, ret+8, 0)
}

func sysrootWall(m *Module, stack []uint64) {
	if len(stack) < 1 {
		return
	}
	ret := int(stack[0])
	ns := uint64(time.Now().UnixNano())
	sec := ns / 1000000000
	nsec := uint32(ns % 1000000000)
	var b [16]byte
	b[0] = byte(sec)
	b[1] = byte(sec >> 8)
	b[2] = byte(sec >> 16)
	b[3] = byte(sec >> 24)
	b[4] = byte(sec >> 32)
	b[5] = byte(sec >> 40)
	b[6] = byte(sec >> 48)
	b[7] = byte(sec >> 56)
	b[8] = byte(nsec)
	b[9] = byte(nsec >> 8)
	b[10] = byte(nsec >> 16)
	b[11] = byte(nsec >> 24)
	_ = m.writeBytes(ret, b[:])
}

func sysrootStringList(m *Module, stack []uint64, list []string) {
	if len(stack) < 1 {
		return
	}
	ret := int(stack[len(stack)-1])
	n := len(list)
	lp := m.guestAlloc(uint32(n * 8))
	for i := 0; i < n; i++ {
		b := []byte(list[i])
		sp := m.guestAlloc(uint32(len(b)))
		if sp != 0 && len(b) > 0 {
			_ = m.writeBytes(int(sp), b)
		}
		writeU32(m, int(lp)+i*8, sp)
		writeU32(m, int(lp)+i*8+4, uint32(len(b)))
	}
	writeU32(m, ret, lp)
	writeU32(m, ret+4, uint32(n))
}

func (m *Module) guestAlloc(n uint32) uint32 {
	if n == 0 {
		return 0
	}
	name := m.allocExport()
	if name == "" {
		return 0
	}
	got, err := m.Call(name, 0, 0, 1, uint64(n))
	if err != nil || len(got) < 1 {
		return 0
	}
	return uint32(got[0])
}

func (m *Module) allocExport() string {
	if m == nil || m.img == nil {
		return ""
	}
	if m.img.HasExport("realloc") {
		return "realloc"
	}
	for i := 0; i < len(m.img.Exports); i++ {
		if nameHas(m.img.Exports[i].Name, "realloc") {
			return m.img.Exports[i].Name
		}
	}
	return ""
}

func wasiFdFdstatGet(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	ptr := int(stack[1])
	buf := make([]byte, 24)
	buf[0] = 2
	_ = m.writeBytes(ptr, buf)
	stack[0] = 0
}

func wasiFdSeek(m *Module, stack []uint64) {
	if len(stack) < 4 {
		return
	}
	writeU32(m, int(stack[3]), 0)
	writeU32(m, int(stack[3])+4, 0)
	stack[0] = 0
}

func wasiErrno0(m *Module, stack []uint64) {
	if len(stack) > 0 {
		stack[0] = 0
	}
}

func wasiEBADF(m *Module, stack []uint64) {
	if len(stack) > 0 {
		stack[0] = 8
	}
}

func wasiProcExit(m *Module, stack []uint64) {
	code := uint32(0)
	if len(stack) > 0 {
		code = uint32(stack[0])
	}
	m.Exited = true
	m.ExitCode = code
}

func wasiFdWrite(m *Module, stack []uint64) {
	if len(stack) < 4 {
		return
	}
	iovs := int(stack[1])
	iovlen := int(stack[2])
	nwritten := int(stack[3])
	fd := uint32(stack[0])
	wrote := 0
	for i := 0; i < iovlen; i++ {
		ptrb, ok1 := m.readBytes(iovs+i*8, 4)
		lenb, ok2 := m.readBytes(iovs+i*8+4, 4)
		if !ok1 || !ok2 {
			stack[0] = 8
			return
		}
		p := int(uint32(ptrb[0]) | uint32(ptrb[1])<<8 | uint32(ptrb[2])<<16 | uint32(ptrb[3])<<24)
		n := int(uint32(lenb[0]) | uint32(lenb[1])<<8 | uint32(lenb[2])<<16 | uint32(lenb[3])<<24)
		if n > 0 {
			buf, ok := m.readBytes(p, n)
			if !ok {
				stack[0] = 8
				return
			}
			if fd == 2 {
				if m.stderr != nil {
					_, _ = m.stderr.Write(buf)
				}
			} else if m.stdout != nil {
				_, _ = m.stdout.Write(buf)
			}
			wrote = wrote + n
		}
	}
	var nb [4]byte
	nb[0] = byte(wrote)
	nb[1] = byte(wrote >> 8)
	nb[2] = byte(wrote >> 16)
	nb[3] = byte(wrote >> 24)
	_ = m.writeBytes(nwritten, nb[:])
	stack[0] = 0
}

func wasiFdRead(m *Module, stack []uint64) {
	if len(stack) < 4 {
		return
	}
	if m.stdin == nil {
		stack[0] = 0
		_ = m.writeBytes(int(stack[3]), []byte{0, 0, 0, 0})
		return
	}
	iovs := int(stack[1])
	iovlen := int(stack[2])
	nreadp := int(stack[3])
	got := 0
	for i := 0; i < iovlen; i++ {
		ptrb, ok1 := m.readBytes(iovs+i*8, 4)
		lenb, ok2 := m.readBytes(iovs+i*8+4, 4)
		if !ok1 || !ok2 {
			break
		}
		p := int(uint32(ptrb[0]) | uint32(ptrb[1])<<8 | uint32(ptrb[2])<<16 | uint32(ptrb[3])<<24)
		n := int(uint32(lenb[0]) | uint32(lenb[1])<<8 | uint32(lenb[2])<<16 | uint32(lenb[3])<<24)
		if n <= 0 {
			continue
		}
		buf := make([]byte, n)
		rn, _ := m.stdin.Read(buf)
		if rn > 0 {
			_ = m.writeBytes(p, buf[0:rn])
			got = got + rn
		}
		if rn < n {
			break
		}
	}
	var nb [4]byte
	nb[0] = byte(got)
	nb[1] = byte(got >> 8)
	nb[2] = byte(got >> 16)
	nb[3] = byte(got >> 24)
	_ = m.writeBytes(nreadp, nb[:])
	stack[0] = 0
}

func wasiArgsSizesGet(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	argc := len(m.args)
	size := 0
	for i := 0; i < argc; i++ {
		size = size + len(m.args[i]) + 1
	}
	writeU32(m, int(stack[0]), uint32(argc))
	writeU32(m, int(stack[1]), uint32(size))
	stack[0] = 0
}

func wasiArgsGet(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	argv := int(stack[0])
	buf := int(stack[1])
	for i := 0; i < len(m.args); i++ {
		writeU32(m, argv+i*4, uint32(buf))
		b := []byte(m.args[i])
		_ = m.writeBytes(buf, b)
		buf = buf + len(b)
		_ = m.writeBytes(buf, []byte{0})
		buf = buf + 1
	}
	stack[0] = 0
}

func wasiEnvironSizesGet(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	n := len(m.env)
	size := 0
	for i := 0; i < n; i++ {
		size = size + len(m.env[i].key) + 1 + len(m.env[i].val) + 1
	}
	writeU32(m, int(stack[0]), uint32(n))
	writeU32(m, int(stack[1]), uint32(size))
	stack[0] = 0
}

func wasiEnvironGet(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	arr := int(stack[0])
	buf := int(stack[1])
	for i := 0; i < len(m.env); i++ {
		writeU32(m, arr+i*4, uint32(buf))
		s := m.env[i].key + "=" + m.env[i].val
		b := []byte(s)
		_ = m.writeBytes(buf, b)
		buf = buf + len(b)
		_ = m.writeBytes(buf, []byte{0})
		buf = buf + 1
	}
	stack[0] = 0
}

func wasiClockTimeGet(m *Module, stack []uint64) {
	if len(stack) < 3 {
		return
	}
	ns := uint64(time.Now().UnixNano())
	ptr := int(stack[2])
	var b [8]byte
	b[0] = byte(ns)
	b[1] = byte(ns >> 8)
	b[2] = byte(ns >> 16)
	b[3] = byte(ns >> 24)
	b[4] = byte(ns >> 32)
	b[5] = byte(ns >> 40)
	b[6] = byte(ns >> 48)
	b[7] = byte(ns >> 56)
	_ = m.writeBytes(ptr, b[:])
	stack[0] = 0
}

func wasiRandomGet(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	ptr := int(stack[0])
	n := int(stack[1])
	if n < 0 {
		n = 0
	}
	buf := make([]byte, n)
	x := uint64(time.Now().UnixNano())
	for i := 0; i < n; i++ {
		x = x*6364136223846793005 + 1
		buf[i] = byte(x >> 33)
	}
	_ = m.writeBytes(ptr, buf)
	stack[0] = 0
}

func envLog(m *Module, stack []uint64) {
	if len(stack) < 2 {
		return
	}
	p := int(stack[0])
	n := int(stack[1])
	if n > 0 {
		buf, ok := m.readBytes(p, n)
		if ok && m.stdout != nil {
			_, _ = m.stdout.Write(buf)
		}
	}
}

func writeU32(m *Module, addr int, v uint32) {
	var b [4]byte
	b[0] = byte(v)
	b[1] = byte(v >> 8)
	b[2] = byte(v >> 16)
	b[3] = byte(v >> 24)
	_ = m.writeBytes(addr, b[:])
}
