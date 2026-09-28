#ifndef WASMWIN32_INCLUDE_WIN32_METADATA_H_
#define WASMWIN32_INCLUDE_WIN32_METADATA_H_

// win32metadata-derived call signatures for the JIT marshaller.
//
// The catalog's `ns` is a win32metadata namespace (Windows.Win32.*), which
// fully types every parameter and the return of each API. The JIT marshals
// generically from that type information instead of a hand-written thunk per
// API: this table is the machine-usable projection of those signatures.
//
// GENERATED (shape) from https://github.com/microsoft/win32metadata by
// tools/gen_metadata.py — winmd -> this table, keyed by the exported name.
// The subset below is what ~/WASMReact + ~/WASMWin32 currently answer; the
// generator widens it. Nothing here is per-API logic — only per-parameter
// TYPES, which the one generic marshaller in ~/WASMGocOS/peinterp consumes.

#include <cstring>

namespace wasmwin32meta {

// The win32metadata type of a parameter or return, reduced to what the
// 32-bit stdcall marshaller needs: how many stack dwords it occupies, how to
// turn it into a catalog field, and whether it is written back after the call.
enum Type : unsigned char {
  Void = 0,
  I32,        // int / LONG / NTSTATUS
  U32,        // DWORD / UINT
  I64,        // LONGLONG (two dwords)
  U64,        // ULONGLONG / ULARGE (two dwords)
  Handle,     // HANDLE / HWND / intptr -> one dword
  Bool,       // BOOL / BOOLEAN
  LpcStr,     // [in] LPCSTR  -> UTF-8 bytes read from guest memory
  LpcWStr,    // [in] LPCWSTR -> UTF-16 read from guest memory, emitted UTF-8
  OutPtr,     // [out] pointer to a scalar -> the reply is written back here
  Ptr,        // opaque / struct pointer passed as an integer (no deref yet)
  Buffer,     // [in] byte buffer -> `count` param bytes read from guest memory
  OutBuffer,  // [out] byte buffer -> the reply's data= is written back here
};

// `count` is the parameter index that holds this buffer's byte length
// (win32metadata's NativeArrayInfo.CountParamIndex). It is read only for
// Buffer / OutBuffer; other types leave it 0.
struct Param {
  Type type;
  bool out;
  unsigned char count;
};

struct Sig {
  Type ret;
  unsigned char n;
  Param p[16];
};

// Stack dwords a parameter of this type occupies in the 32-bit stdcall ABI.
inline int dwords(Type t) { return (t == I64 || t == U64) ? 2 : 1; }

// A 64-bit return lands in edx:eax.
inline bool ret64(Type t) { return t == I64 || t == U64; }

namespace detail {
struct Row {
  const char* name;
  Sig sig;
};

// clang-format off
inline const Row* table(int* n) {
  static const Row rows[] = {
    // Windows.Win32.System.Threading
    {"Sleep",                          {Void,   1, {{U32,false}}}},
    {"GetCurrentProcessId",            {U32,    0, {}}},
    {"GetCurrentThreadId",             {U32,    0, {}}},
    // Windows.Win32.Foundation
    {"CloseHandle",                    {Bool,   1, {{Handle,false}}}},
    {"SetEvent",                       {Bool,   1, {{Handle,false}}}},
    {"ResetEvent",                     {Bool,   1, {{Handle,false}}}},
    {"GetLastError",                   {U32,    0, {}}},
    {"SetLastError",                   {Void,   1, {{U32,false}}}},
    // Windows.Win32.System.SystemInformation
    {"GetTickCount",                   {U32,    0, {}}},
    {"GetTickCount64",                 {U64,    0, {}}},
    {"GetVersion",                     {U32,    0, {}}},
    {"GetSystemTimeAsFileTime",        {Void,   1, {{OutPtr,true}}}},
    {"GetSystemTimePreciseAsFileTime", {Void,   1, {{OutPtr,true}}}},
    // Windows.Win32.System.Threading (events)
    {"CreateEventW",                   {Handle, 4, {{Ptr,false},{Bool,false},{Bool,false},{LpcWStr,false}}}},
    {"CreateEventA",                   {Handle, 4, {{Ptr,false},{Bool,false},{Bool,false},{LpcStr,false}}}},
    // Windows.Win32.Storage.FileSystem
    {"CreateFileW",                    {Handle, 7, {{LpcWStr,false},{U32,false},{U32,false},{Ptr,false},{U32,false},{U32,false},{Handle,false}}}},
    {"CreateFileA",                    {Handle, 7, {{LpcStr,false},{U32,false},{U32,false},{Ptr,false},{U32,false},{U32,false},{Handle,false}}}},
    {"ReadFile",                       {Bool,   5, {{Handle,false,0},{OutBuffer,true,2},{U32,false,0},{OutPtr,true,0},{Ptr,false,0}}}},
    {"WriteFile",                      {Bool,   5, {{Handle,false,0},{Buffer,false,2},{U32,false,0},{OutPtr,true,0},{Ptr,false,0}}}},
    // Windows.Wdk.System.SystemServices (ntdll / Native API)
    {"NtClose",                        {I32,    1, {{Handle,false}}}},
    {"NtSetEvent",                     {I32,    2, {{Handle,false},{OutPtr,true}}}},
    {"NtResetEvent",                   {I32,    2, {{Handle,false},{OutPtr,true}}}},
  };
  if (n) *n = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
  return rows;
}
// clang-format on
}  // namespace detail

// Look up the metadata signature for an exported name. 1 when known.
inline int lookup(const char* name, Sig* out) {
  if (!name || !out) return 0;
  int n = 0;
  const detail::Row* rows = detail::table(&n);
  for (int i = 0; i < n; ++i)
    if (std::strcmp(rows[i].name, name) == 0) {
      *out = rows[i].sig;
      return 1;
    }
  return 0;
}

}  // namespace wasmwin32meta

#endif  // WASMWIN32_INCLUDE_WIN32_METADATA_H_
