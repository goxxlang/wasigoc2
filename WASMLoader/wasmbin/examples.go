package wasmbin

// AddModule exports add(i32, i32) -> i32.
func AddModule() []byte {
	e := NewEncoder()
	ty := e.Type([]ValType{ValI32, ValI32}, []ValType{ValI32})
	fn := e.Func(ty, Body(LocalGet(0), LocalGet(1), I32Add()))
	e.Export("add", KindFunc, fn)
	return e.Bytes()
}

// DoubleModule imports math.add and exports double(i32) -> i32.
func DoubleModule() []byte {
	e := NewEncoder()
	bin := e.Type([]ValType{ValI32, ValI32}, []ValType{ValI32})
	un := e.Type([]ValType{ValI32}, []ValType{ValI32})
	add := e.ImportFunc("math", "add", bin)
	fn := e.Func(un, Body(LocalGet(0), LocalGet(0), Call(add)))
	e.Export("double", KindFunc, fn)
	return e.Bytes()
}

// HelloModule is a WASI command that writes "hello, wasm\n" to stdout.
func HelloModule() []byte {
	msg := []byte("hello, wasm\n")
	const (
		iovPtr   = 0
		strPtr   = 8
		nwritten = 24
	)
	e := NewEncoder()
	fdWriteTy := e.Type([]ValType{ValI32, ValI32, ValI32, ValI32}, []ValType{ValI32})
	startTy := e.Type(nil, nil)
	fdWrite := e.ImportFunc("wasi_snapshot_preview1", "fd_write", fdWriteTy)
	e.Memory(1)
	start := e.Func(startTy, Body(
		I32Const(iovPtr),
		I32Const(strPtr),
		I32Store(),
		I32Const(iovPtr+4),
		I32Const(int32(len(msg))),
		I32Store(),
		I32Const(1), // stdout
		I32Const(iovPtr),
		I32Const(1),
		I32Const(nwritten),
		Call(fdWrite),
		Drop(),
	))
	e.Export("_start", KindFunc, start)
	e.Export("memory", KindMemory, 0)
	e.Data(strPtr, msg)
	return e.Bytes()
}

// PluginModule imports env.log and exports run().
func PluginModule() []byte {
	msg := []byte("plugin loaded\n")
	e := NewEncoder()
	logTy := e.Type([]ValType{ValI32, ValI32}, nil)
	runTy := e.Type(nil, nil)
	logFn := e.ImportFunc("env", "log", logTy)
	e.Memory(1)
	run := e.Func(runTy, Body(
		I32Const(0),
		I32Const(int32(len(msg))),
		Call(logFn),
	))
	e.Export("run", KindFunc, run)
	e.Export("memory", KindMemory, 0)
	e.Data(0, msg)
	return e.Bytes()
}

// Example returns a named built-in module, or nil if unknown.
func Example(name string) []byte {
	switch name {
	case "add":
		return AddModule()
	case "double":
		return DoubleModule()
	case "hello":
		return HelloModule()
	case "plugin":
		return PluginModule()
	default:
		return nil
	}
}

func ExampleNames() []string {
	return []string{"add", "double", "hello", "plugin"}
}
