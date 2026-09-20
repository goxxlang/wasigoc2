# Docs

| Doc | Contents |
| --- | --- |
| [language.md](language.md) | Go++: syntax, Rosetta, modules |
| [stdlib.md](stdlib.md) | Standard library status |
| [build.md](build.md) | Building `wasigoc` and compiling to wasm |
| [architecture.md](architecture.md) | wasigocvm: EPT/TPT/CHPT second address space, in-module libc, load/call |
| [wasm2-fork.md](wasm2-fork.md) | WASM 2 hard fork: opcodes we keep, Oilpan/cage we replace, wasitime |
| [wasitime.md](wasitime.md) | wasitime engine: wasmtime-shaped CLI, Cranelift-slot interpreter |
| [host-abi.md](host-abi.md) | In-tree WASMWin32 / WASMNix / WASMDroid as the in-module host ABI |
| [wasigocvm.md](wasigocvm.md) | wasigocvm product: sysroot, libc, wasitime — vs Bytecode Alliance |
| [design-log.md](design-log.md) | Per-package tracker and compiler-bug diary |

**Go++** is the language. **wasigoc** is the compiler. **wasigocvm** is
the machine. **Wasigoc2** is this release of that full stack. This tree
is [goxxlang/wasigoc](https://github.com/goxxlang/wasigoc).
