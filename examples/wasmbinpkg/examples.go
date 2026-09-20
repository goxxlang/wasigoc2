package wasmbin

func AddModule() []byte {
	e := NewEncoder()
	params := []ValType{ValI32, ValI32}
	results := []ValType{ValI32}
	ty := e.Type(params, results)
	fn := e.Func(ty, Body(LocalGet(0), LocalGet(1), I32Add()))
	e.AddExport("add", KindFunc, fn)
	return e.Bytes()
}

func DoubleModule() []byte {
	e := NewEncoder()
	binP := []ValType{ValI32, ValI32}
	binR := []ValType{ValI32}
	unP := []ValType{ValI32}
	unR := []ValType{ValI32}
	bin := e.Type(binP, binR)
	un := e.Type(unP, unR)
	add := e.ImportFunc("math", "add", bin)
	fn := e.Func(un, Body(LocalGet(0), LocalGet(0), Call(add)))
	e.AddExport("double", KindFunc, fn)
	return e.Bytes()
}

func HelloModule() []byte {
	msg := []byte("hello, wasm\n")
	const iovPtr = 0
	const strPtr = 8
	const nwritten = 24
	e := NewEncoder()
	fdP := []ValType{ValI32, ValI32, ValI32, ValI32}
	fdR := []ValType{ValI32}
	fdWriteTy := e.Type(fdP, fdR)
	var noneP []ValType
	var noneR []ValType
	startTy := e.Type(noneP, noneR)
	fdWrite := e.ImportFunc("wasi_snapshot_preview1", "fd_write", fdWriteTy)
	e.Memory(1)
	start := e.Func(startTy, Body(
		I32Const(iovPtr),
		I32Const(strPtr),
		I32Store(),
		I32Const(iovPtr+4),
		I32Const(int32(len(msg))),
		I32Store(),
		I32Const(1),
		I32Const(iovPtr),
		I32Const(1),
		I32Const(nwritten),
		Call(fdWrite),
		Drop(),
	))
	e.AddExport("_start", KindFunc, start)
	e.AddExport("memory", KindMemory, 0)
	e.Data(strPtr, msg)
	return e.Bytes()
}

func PluginModule() []byte {
	msg := []byte("plugin loaded\n")
	e := NewEncoder()
	logP := []ValType{ValI32, ValI32}
	var logR []ValType
	logTy := e.Type(logP, logR)
	var runP []ValType
	var runR []ValType
	runTy := e.Type(runP, runR)
	logFn := e.ImportFunc("env", "log", logTy)
	e.Memory(1)
	run := e.Func(runTy, Body(
		I32Const(0),
		I32Const(int32(len(msg))),
		Call(logFn),
	))
	e.AddExport("run", KindFunc, run)
	e.AddExport("memory", KindMemory, 0)
	e.Data(0, msg)
	return e.Bytes()
}

func Example(name string) []byte {
	if name == "add" {
		return AddModule()
	}
	if name == "double" {
		return DoubleModule()
	}
	if name == "hello" {
		return HelloModule()
	}
	if name == "plugin" {
		return PluginModule()
	}
	return nil
}

func ExampleNames() []string {
	return []string{"add", "double", "hello", "plugin"}
}
