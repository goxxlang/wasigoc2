#!/usr/bin/env python3
"""Generate win32/metadata.h from win32metadata (the Windows.Win32.winmd).

The JIT marshaller in ~/WASMGocOS/peinterp does not carry per-API code: it
reads each API's parameter TYPES from win32/metadata.h and marshals
generically. That header is a machine projection of the public metadata at
https://github.com/microsoft/win32metadata, keyed by exported name and
restricted to the functions the catalog (~/WASMWin32 + ~/WASMReact) answers.

Input:  Windows.Win32.winmd (ECMA-335 metadata; read with a CIL reader such
        as `pip install python-cil` / `System.Reflection.Metadata` via pythonnet,
        or the winmd JSON dumps win32metadata ships).
Filter: the union of wasmwin32_catalog() and wasmreact_catalog() names.
Output: include/win32/metadata.h  (the `detail::table` rows).

Each metadata parameter/return type maps to one wasmwin32meta::Type:

    void                                  -> Void
    Int32 / LONG / NTSTATUS               -> I32
    UInt32 / DWORD / UINT                 -> U32
    Int64 / LARGE_INTEGER (by value)      -> I64
    UInt64 / ULONGLONG / ULARGE_INTEGER   -> U64
    IntPtr handle types (HANDLE, HWND...) -> Handle
    BOOL / BOOLEAN                        -> Bool
    PSTR  / [In] LPCSTR                    -> LpcStr
    PWSTR / [In] LPCWSTR                   -> LpcWStr
    [Out] pointer-to-scalar               -> OutPtr   (out=True)
    any other pointer / struct*           -> Ptr

An [Out] or [In,Out] attribute sets Param.out. Two-dword types (I64/U64)
occupy two stdcall slots; the marshaller and the stdcall cleanup use
wasmwin32meta::dwords().

This scaffold documents the mapping and the row shape; wire an actual winmd
reader in `rows_from_winmd()` to emit the full set. The checked-in header is
the hand-verified subset until this runs in CI.
"""

import sys

# name -> (ret, [(type, out), ...]) in win32metadata order. Kept in sync with
# the checked-in header; the winmd reader replaces this dict wholesale.
SUBSET = {
    "Sleep": ("Void", [("U32", False)]),
    "GetTickCount": ("U32", []),
    "GetTickCount64": ("U64", []),
    "GetVersion": ("U32", []),
    "GetLastError": ("U32", []),
    "SetLastError": ("Void", [("U32", False)]),
    "CloseHandle": ("Bool", [("Handle", False)]),
    "SetEvent": ("Bool", [("Handle", False)]),
    "ResetEvent": ("Bool", [("Handle", False)]),
    "GetSystemTimeAsFileTime": ("Void", [("OutPtr", True)]),
    "GetSystemTimePreciseAsFileTime": ("Void", [("OutPtr", True)]),
    "CreateEventW": ("Handle", [("Ptr", False), ("Bool", False), ("Bool", False), ("LpcWStr", False)]),
    "CreateFileW": ("Handle", [("LpcWStr", False), ("U32", False), ("U32", False),
                               ("Ptr", False), ("U32", False), ("U32", False), ("Handle", False)]),
    "NtClose": ("I32", [("Handle", False)]),
}


def rows_from_winmd(_winmd_path):
    """Read the winmd and return the same {name: (ret, params)} mapping.

    Not implemented here: plug in a CIL metadata reader, walk each
    MethodDef in the Apis type of every Windows.Win32.* namespace, map its
    signature with the table in this module's docstring, and filter to the
    catalog names. Until then, callers fall back to SUBSET.
    """
    raise NotImplementedError("wire a winmd reader here")


def emit_rows(mapping):
    for name, (ret, params) in mapping.items():
        ps = ", ".join("{%s,%s}" % (t, "true" if o else "false") for t, o in params)
        print('    {"%s", {%s, %d, {%s}}},' % (name, ret, len(params), ps))


if __name__ == "__main__":
    mapping = SUBSET
    if len(sys.argv) > 1:
        try:
            mapping = rows_from_winmd(sys.argv[1])
        except NotImplementedError as e:
            print("// %s; emitting the checked-in subset" % e, file=sys.stderr)
    emit_rows(mapping)
