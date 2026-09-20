# Go++ standard library

Public `go list std` minus `internal/` / `vendor/`, compiled as ordinary
`.go` under `stdlib/` the same way `strings` is. Do **not** add compiler
builtins unless the package must touch WASI or the Rosetta runtime (`os`
fds, `time.Now`). One thread / no growable stacks still applies.

**Status:** 4 builtins + compiled packages under `stdlib/`. Everyday ported Go++
source compiles. **wasigocvm** ([architecture.md](architecture.md), [wasigocvm.md](wasigocvm.md)) is the machine:
own sysroot, full libc++, EPT/TPT/CHPT, own runtime (`wasitime`). `net` /
`net/http` are real sockets; `syscall` / `os/user` / `os/exec` / `win32` /
`linux` / `android` / `crypto/tls` stay in-guest. `gocvm.Call` errors
surface as-is — there is no shim_sandbox fallback and no userspace TCP
stand-in. `net.Pipe()` is an in-process duplex, not a host hop.

Per-package notes, bounds, and the compiler bugs each package
surfaced: [design-log.md](design-log.md) (tracker from
`### Tracker` onward).

## Builtins (not `.go`)

| Package | In | Missing |
| --- | --- | --- |
| `fmt` | `Print`/`Println`/`Sprint`/`Sprintln`/`Printf`/`Sprintf`/`Errorf`/`Fprint*` (a non-literal format string, including a `log.Printf`-shaped `v ...any` wrapper, falls back to a runtime verb walk with no compile-time argument checking) | verbs `%d %s %f %v %t %c %w %%` only; `Errorf`'s `%w` only wraps with a literal format string; no `Scan*` |
| `errors` | `New`, `Is`, `Unwrap`, `Join` | `As` |
| `os` | `Args`, `Exit`, `Getenv`, `File` (`Open`/`Create`/`ReadFile`/`WriteFile`, `Read`/`Write`/`Close`), std streams, `Stat`/`FileInfo`, `ReadDir`/`DirEntry` (real, via `stat(2)`/`opendir`+`readdir`) | `Setenv`, process, `Remove`/`Mkdir` |
| `reflect` | `TypeOf`/`ValueOf`, `Value`/`Type` (`Kind`/`Name`/`NumField`/`Field`/`FieldName`/`Interface`/`Int`/`Float`/`Bool`/`String`, `Set*`) | no Chan/Func Kind |

File I/O is libc in the module (sysroot `fopen` / `stat`). Isolation is
the WASMSafeSpace cage, not a host shim.

## In-module machine

These packages talk `gocvm.Call` into wasigocvm
([architecture.md](architecture.md)):

| Package | In-module path |
| --- | --- |
| `net` / `net/http` | sysroot sockets + `poll()` |
| `syscall` | libc + win32metadata (`GetCurrentProcessId`, `GetCurrentDirectoryW`, `GetEnvironmentVariableW`, …) |
| `os/user` | libc USER/USERNAME/HOME |
| `os/exec` | child in EPT/TPT/CHPT; work is `WASMWin32/` `wasi_call`; `LookPath` is `os.exec.lookpath` |
| `win32` | WASMWin32 catalog/modules on EPT, process/thread on TPT, token/SID/PEB/TEB/HWND/GDI/COM on CHPT, vmem/WSA/bcrypt/ncrypt/WASMPELoader/crypt32/WinHttp/WinHvPlatform/WinHvEmulation/MainDLL/wininet/setupapi/pdh/wevtapi on EPT; ntdll Nt/Zw/Rtl/Ldr/Tp at kernel32 catalog breadth |
| `linux` | WASMNix catalog on EPT, process/thread on TPT, session on CHPT; `posix_call` for Linux/WSL/Nix names |
| `android` | WASMDroid catalog and Binder root on EPT, process/thread on TPT, session on CHPT; `bionic_call` for Bionic/Binder/KVM names |
| `gocos` | edge kernel; GocKrnl / GocSys hop k32 and nix through gocvm hypervision; vmem on EPT |
| `crypto/tls` | OpenSSL wasm, memory BIOs (WASMLime `TlsTransport`), not Schannel |

`net.Pipe()` is an in-process duplex. `net/http` is HTTP/1.0 over `net`
(`Get`/`Post`/`Serve`/`ServeMux`).

## n/a on this target

`plugin`, `runtime/cgo`, `runtime/race`, `log/syslog`, `time/tzdata`
(this `time` is UTC-only).

## Partial (real, bounded)

Everything else under `stdlib/` is present and exercised by a golden.
Typical bounds, also in each package comment:

- crypto: SHA-2/3, HMAC, HKDF, PBKDF2, AES-128, DES/RC4 (legacy),
  textbook RSA/DSA, P-256 ECDH/ECDSA, Ed25519. TLS handshake is
  `crypto/tls` on wasigocvm (OpenSSL wasm), not these packages.
- compress/image: real codecs, often decode-general / encode-simple.
- `encoding/json`: Marshal/Unmarshal of structs via reflect (including
  `json:"name"` / `json:"-"` tags); Unmarshal into a struct pointer
  writes through settable Values.
- `go/*`: tokenizer/parser/printer plus an interned (object-type
  identity) checker: defined types by name, anonymous interfaces by
  method set, `Set[int]` instantiations, range-over-func signatures.
  wasigoc now emits those same shapes as C++ (defined-type method
  wrappers, class templates, interned iface adapters, yield loops).
- `math/cmplx`: `complex128` (`1+2i`, `real`/`imag`/`complex`).
- `regexp`: backtracking; no `{m,n}`, no non-greedy, no lookaround.
- `sync`: no-op mutexes; `WaitGroup.Wait` cannot block the runqueue.
- `time.Sleep`: no-op (must not block the cooperative scheduler).

## Extensions beyond `go list std`

`stdlib/win32` is the guest API for in-tree `WASMWin32/` (`import "win32"`):
win32metadata names. WslList/WslExec/Nix still `gocvm.Call("wsl"|"nix")`,
now dispatched to `WASMNix/`. Stock wasip1 has no bridge.

`stdlib/linux` is the guest API for in-tree `WASMNix/` (`import "linux"`):
man-pages getpid/uname plus WslList/NixVersion.

`stdlib/android` is the guest API for in-tree `WASMDroid/` (`import "android"`):
Bionic getpid/uname/api-level plus Binder/KVM hops.

`stdlib/gocos` is the wasigocvm edge kernel (`import "gocos"`). Binding is
`gocvm.Call("gocos", …)` on gocvm.wasm. GocKrnl / GocSys hop **k32**
(`WASMWin32/`) and **nix** (`WASMNix/`) through gocvm hypervision.
Memory is gocvm vmem (EPT + VirtualAlloc / mmap). Not an emulator and
not a copy of ntoskrnl or Wine. GocShell is `gocvm.Call("gocos", "Cmd")`
shown with xterm.js in `~/WASMJsLoader` (`cmd.html`); the guest is
instantiated in the browser, no host bridge.

`stdlib/ogchan` is a clean-room Open Graph document channel (public OG
/ preview specs, not messenger source).

`stdlib/unil` is not part of real Go's standard library — it's the
WASMUniLoader "unil" bill-of-materials format (files, runtime
components, capabilities, canonical JSON, SHA-256 digest, Ed25519
signing), a Go++ port of `~/WASMUniLoader/cpp/src/sbom.cc`'s C++ core
so a Go++ program can produce and verify byte-identical documents
without linking that C++ code. `import "unil"` resolves the same way
any stdlib package does (`wasigoc` searches `stdlib/` regardless of
whether the import path is real Go). See its package doc comment and
[design-log.md](design-log.md)'s tracker entry.

`stdlib/guac` builds on unil: the on-disk shape of a distributable Go++
wasm package — a directory with compiled wasm file(s) plus a
`guac.json` manifest, which is an ordinary `unil.Document` (no schema
changes, just two naming conventions — see its package doc comment).
`HashFile`/`BuildManifest` hash files already on disk; `WriteManifest`/
`ReadManifest` persist and reload the manifest; `Verify` catches drift
between a directory and its manifest. It does not create directories
(no `Mkdir` in this `os`), compile anything, or fetch a dependency —
a `guac` CLI wrapping `wasigoc`/`goclang++` builds around this layer
doesn't exist yet.

Building it surfaced a real, previously-latent bug in
`crypto/ed25519`: the `dEd` curve constant had a digit transcription
error, so the compiled-in base point never actually satisfied the
curve equation — invisible until something did a genuine external
Sign-then-Verify round trip, which no existing golden did. Fixed; see
the tracker entry for `crypto/ed25519`.

## Growing it

See [CONTRIBUTING.md](../CONTRIBUTING.md). Tick the tracker in the
design log when a package lands.
