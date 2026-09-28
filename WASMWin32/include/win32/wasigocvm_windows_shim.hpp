#ifndef WASMWIN32_INCLUDE_WIN32_WASIGOCVM_WINDOWS_SHIM_HPP_
#define WASMWIN32_INCLUDE_WIN32_WASIGOCVM_WINDOWS_SHIM_HPP_

// A <windows.h>/<windowsx.h> replacement for the small Win32 window/GDI
// surface FrameWindow (and friends) call directly, backed by WASMWin32's
// wasi_k32.hpp simulated session (see win32/dispatch.h's file comment --
// "Tokens/SIDs/PEB/TEB/HWND/GDI/COM are objects on the wasigocvm session",
// not a real OS window). Not a general win32 shim: only what's actually
// called grows here, same "grows with its callers" convention wasi_k32.hpp
// itself documents.
//
// Known, documented simplifications (same "honest stub" level as the rest
// of wasi_k32.hpp's window/GDI dispatch):
//   - No real display. StretchDIBits/FillRect are bookkeeping only (see
//     wasi_k32.hpp's own file comment on why pixel bytes aren't threaded
//     through wasi_call's char*-based, embedded-NUL-unsafe protocol).
//   - No live message pump. RegisterClassW's lpfnWndProc is recorded by
//     the caller's own WNDCLASSW but this shim never calls back into it --
//     there is no real input/paint source under wasigocvm to drive one.
//     Callers that need their WndProc invoked (this family's FrameWindow
//     does not -- see frame_window.cc, it drives OnPaint/OnClick/etc.
//     directly from its own C++ API, never through WndProc redispatch)
//     would need their own pump loop over PeekMessageW, same as any real
//     Win32 app's own message loop.
//   - GetKeyState/GetAsyncKeyState always report "up" (wasi_k32.hpp's own
//     stub); modifier-key detection (Alt/Ctrl/Shift) is always false.
//   - CreateWindowExW's parent argument is not preserved (every window is
//     simulated as a direct child of the session's one virtual desktop).

#include "win32/dispatch.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace lime_win32 {

inline std::string WideToUtf8(const wchar_t* s) {
  std::string out;
  if (!s) return out;
  for (; *s; ++s) {
    unsigned int cp = static_cast<unsigned int>(*s);
    if (cp <= 0x7F) {
      out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

// Fixed-size round trip over wasmwin32_call -- 8 KiB is comfortably past
// anything this file's own callers send/receive (handles, small rects,
// class/title strings), see wasi_k32.hpp's own K32Max-bounded replies.
inline std::string Win32Call(const char* api, const std::string& args) {
  static thread_local char buf[8192];
  wasmwin32_call(api, args.c_str(), buf, sizeof(buf));
  return std::string(buf);
}

inline bool IsErrorReply(const std::string& s) { return s.rfind("error:", 0) == 0; }

inline long ToLong(const std::string& s) { return std::strtol(s.c_str(), nullptr, 10); }

}  // namespace lime_win32

// ---- types --------------------------------------------------------------

using BOOL = int;
using UINT = unsigned int;
using DWORD = unsigned long;
using LONG = long;
using WORD = unsigned short;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using LONG_PTR = std::intptr_t;
using ATOM = unsigned short;
using HWND = void*;
using HDC = void*;
using HBRUSH = void*;
using HICON = void*;
using HCURSOR = void*;
using HINSTANCE = void*;
using HMENU = void*;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define CALLBACK

using WNDPROC = LRESULT(CALLBACK*)(HWND, UINT, WPARAM, LPARAM);

struct RECT {
  LONG left = 0;
  LONG top = 0;
  LONG right = 0;
  LONG bottom = 0;
};

struct POINT {
  LONG x = 0;
  LONG y = 0;
};

struct WNDCLASSW {
  UINT style = 0;
  WNDPROC lpfnWndProc = nullptr;
  int cbClsExtra = 0;
  int cbWndExtra = 0;
  HINSTANCE hInstance = nullptr;
  HICON hIcon = nullptr;
  HCURSOR hCursor = nullptr;
  HBRUSH hbrBackground = nullptr;
  const wchar_t* lpszMenuName = nullptr;
  const wchar_t* lpszClassName = nullptr;
};

struct CREATESTRUCTW {
  void* lpCreateParams = nullptr;
  HINSTANCE hInstance = nullptr;
  HMENU hMenu = nullptr;
  HWND hwndParent = nullptr;
  int cy = 0;
  int cx = 0;
  int y = 0;
  int x = 0;
  LONG style = 0;
  const wchar_t* lpszName = nullptr;
  const wchar_t* lpszClass = nullptr;
  DWORD dwExStyle = 0;
};

struct PAINTSTRUCT {
  HDC hdc = nullptr;
  BOOL fErase = FALSE;
  RECT rcPaint;
  BOOL fRestore = FALSE;
  BOOL fIncUpdate = FALSE;
  unsigned char rgbReserved[32] = {};
};

struct BITMAPINFOHEADER {
  DWORD biSize = 0;
  LONG biWidth = 0;
  LONG biHeight = 0;
  WORD biPlanes = 0;
  WORD biBitCount = 0;
  DWORD biCompression = 0;
  DWORD biSizeImage = 0;
  LONG biXPelsPerMeter = 0;
  LONG biYPelsPerMeter = 0;
  DWORD biClrUsed = 0;
  DWORD biClrImportant = 0;
};

struct BITMAPINFO {
  BITMAPINFOHEADER bmiHeader;
  DWORD bmiColors[1] = {};
};

struct MSG {
  HWND hwnd = nullptr;
  UINT message = 0;
  WPARAM wParam = 0;
  LPARAM lParam = 0;
};

// ---- constants ------------------------------------------------------------

enum {
  WM_NCCREATE = 0x0081,
  WM_DESTROY = 0x0002,
  WM_SIZE = 0x0005,
  WM_PAINT = 0x000F,
  WM_LBUTTONDOWN = 0x0201,
  WM_LBUTTONUP = 0x0202,
  WM_MOUSEWHEEL = 0x020A,
  WM_KEYDOWN = 0x0100,
  WM_CHAR = 0x0102,
  WM_GETDLGCODE = 0x0087,
};

enum { SIZE_MINIMIZED = 1 };

enum {
  DLGC_WANTARROWS = 0x0001,
  DLGC_WANTALLKEYS = 0x0004,
  DLGC_WANTCHARS = 0x0080,
};

enum {
  WS_CHILD = 0x40000000L,
  WS_CLIPSIBLINGS = 0x04000000L,
  WS_CLIPCHILDREN = 0x02000000L,
  WS_OVERLAPPEDWINDOW = 0x00CF0000L,
};

// Private contract with this shim's WASMWin32/include/win32/wasi_k32.hpp
// GetWindowLongPtrW/SetWindowLongPtrW addition, not winuser.h's real -16
// (GWL_STYLE) / -21 (GWLP_USERDATA) -- see that dispatch's own `i == -4`
// check. Only GWL_STYLE needs a value distinct from GWLP_USERDATA here;
// GWLP_USERDATA's value doesn't matter beyond that.
enum { GWL_STYLE = -4, GWLP_USERDATA = 0 };

#define CW_USEDEFAULT ((int)0x80000000)
inline HWND HWND_TOP = nullptr;

enum {
  SWP_NOSIZE = 0x0001,
  SWP_NOMOVE = 0x0002,
  SWP_SHOWWINDOW = 0x0040,
  SWP_NOACTIVATE = 0x0010,
};

enum { SW_HIDE = 0, SW_SHOW = 5 };

enum {
  VK_LEFT = 0x25,
  VK_RIGHT = 0x27,
  VK_SHIFT = 0x10,
  VK_CONTROL = 0x11,
  VK_MENU = 0x12,
  VK_RETURN = 0x0D,
  VK_BACK = 0x08,
  VK_DELETE = 0x2E,
  VK_F5 = 0x74,
};

inline const wchar_t* const IDC_ARROW = reinterpret_cast<const wchar_t*>(32512);
enum { BLACK_BRUSH = 4 };
enum { BI_RGB = 0, DIB_RGB_COLORS = 0 };
constexpr DWORD SRCCOPY = 0x00CC0020;

#define RGB(r, g, b) \
  ((DWORD)(((unsigned char)(r) | (((unsigned short)(unsigned char)(g)) << 8)) | \
           (((DWORD)(unsigned char)(b)) << 16)))

#define GET_X_LPARAM(lp) ((int)(short)((lp)&0xFFFF))
#define GET_Y_LPARAM(lp) ((int)(short)(((lp) >> 16) & 0xFFFF))
#define GET_WHEEL_DELTA_WPARAM(wp) ((short)(((wp) >> 16) & 0xFFFF))

// ---- functions ------------------------------------------------------------

inline HINSTANCE GetModuleHandleW(const wchar_t*) { return nullptr; }

inline HCURSOR LoadCursorW(HINSTANCE, const wchar_t*) {
  // wasi_k32.hpp's LoadCursorW/LoadIconW always ok("1") -- no real cursor
  // resource, this handle is never dereferenced by anything in this file.
  return reinterpret_cast<HCURSOR>(1);
}

inline ATOM RegisterClassW(const WNDCLASSW* wc) {
  std::string name = wc && wc->lpszClassName ? lime_win32::WideToUtf8(wc->lpszClassName) : "";
  std::string reply = lime_win32::Win32Call("RegisterClassW", name);
  return lime_win32::IsErrorReply(reply) ? 0 : 1;
}

inline HBRUSH CreateSolidBrush(DWORD color) {
  std::string reply = lime_win32::Win32Call("CreateSolidBrush", std::to_string(color));
  if (lime_win32::IsErrorReply(reply)) return nullptr;
  return reinterpret_cast<HBRUSH>(lime_win32::ToLong(reply));
}

inline void* GetStockObject(int index) {
  (void)index;  // wasi_k32.hpp's GetStockObject ignores its argument too.
  std::string reply = lime_win32::Win32Call("GetStockObject", "");
  if (lime_win32::IsErrorReply(reply)) return nullptr;
  return reinterpret_cast<void*>(lime_win32::ToLong(reply));
}

inline HWND CreateWindowExW(DWORD /*exStyle*/, const wchar_t* className, const wchar_t* title,
                            DWORD style, int x, int y, int w, int h, HWND /*parent*/,
                            HMENU /*hMenu*/, HINSTANCE /*hInstance*/, void* /*param*/) {
  std::string args = lime_win32::WideToUtf8(className);
  args += '\x1f';
  args += title ? lime_win32::WideToUtf8(title) : "";
  args += '\x1f';
  args += std::to_string(static_cast<long>(style));
  std::string reply = lime_win32::Win32Call("CreateWindowExW", args);
  if (lime_win32::IsErrorReply(reply)) return nullptr;
  HWND hwnd = reinterpret_cast<HWND>(lime_win32::ToLong(reply));
  // CreateWindowExW's simulated session doesn't take x/y/w/h at creation
  // (see wasi_k32.hpp's own handler) -- set real geometry with a follow-up
  // MoveWindow, same as a caller would after a real CW_USEDEFAULT create.
  int rx = x == CW_USEDEFAULT ? 0 : x;
  int ry = y == CW_USEDEFAULT ? 0 : y;
  int rw = w == CW_USEDEFAULT ? 640 : w;
  int rh = h == CW_USEDEFAULT ? 480 : h;
  std::string mv = std::to_string(reinterpret_cast<std::intptr_t>(hwnd));
  mv += '\x1f' + std::to_string(rx) + '\x1f' + std::to_string(ry) + '\x1f' + std::to_string(rw) +
        '\x1f' + std::to_string(rh) + '\x1f' + "1";
  lime_win32::Win32Call("MoveWindow", mv);
  return hwnd;
}

inline BOOL DestroyWindow(HWND hwnd) {
  std::string reply =
      lime_win32::Win32Call("DestroyWindow", std::to_string(reinterpret_cast<std::intptr_t>(hwnd)));
  return lime_win32::IsErrorReply(reply) ? FALSE : TRUE;
}

inline BOOL IsWindow(HWND hwnd) {
  std::string reply =
      lime_win32::Win32Call("IsWindow", std::to_string(reinterpret_cast<std::intptr_t>(hwnd)));
  return (!lime_win32::IsErrorReply(reply) && reply == "1") ? TRUE : FALSE;
}

inline LONG_PTR GetWindowLongPtrW(HWND hwnd, int index) {
  std::string args = std::to_string(reinterpret_cast<std::intptr_t>(hwnd));
  args += '\x1f' + std::to_string(index);
  std::string reply = lime_win32::Win32Call("GetWindowLongPtrW", args);
  if (lime_win32::IsErrorReply(reply)) return 0;
  return static_cast<LONG_PTR>(lime_win32::ToLong(reply));
}

inline LONG_PTR SetWindowLongPtrW(HWND hwnd, int index, LONG_PTR value) {
  std::string args = std::to_string(reinterpret_cast<std::intptr_t>(hwnd));
  args += '\x1f' + std::to_string(index) + '\x1f' + std::to_string(static_cast<long long>(value));
  std::string reply = lime_win32::Win32Call("SetWindowLongPtrW", args);
  if (lime_win32::IsErrorReply(reply)) return 0;
  return static_cast<LONG_PTR>(lime_win32::ToLong(reply));
}

inline BOOL MoveWindow(HWND hwnd, int x, int y, int w, int h, BOOL repaint) {
  std::string args = std::to_string(reinterpret_cast<std::intptr_t>(hwnd));
  args += '\x1f' + std::to_string(x) + '\x1f' + std::to_string(y) + '\x1f' + std::to_string(w) +
          '\x1f' + std::to_string(h) + '\x1f' + std::to_string(repaint ? 1 : 0);
  std::string reply = lime_win32::Win32Call("MoveWindow", args);
  return lime_win32::IsErrorReply(reply) ? FALSE : TRUE;
}

inline BOOL SetWindowPos(HWND hwnd, HWND /*after*/, int x, int y, int w, int h, UINT flags) {
  std::string args = std::to_string(reinterpret_cast<std::intptr_t>(hwnd));
  args += '\x1f' + (flags & SWP_NOMOVE ? std::string() : std::to_string(x));
  args += '\x1f' + (flags & SWP_NOMOVE ? std::string() : std::to_string(y));
  args += '\x1f' + (flags & SWP_NOSIZE ? std::string() : std::to_string(w));
  args += '\x1f' + (flags & SWP_NOSIZE ? std::string() : std::to_string(h));
  std::string reply = lime_win32::Win32Call("SetWindowPos", args);
  if (flags & SWP_SHOWWINDOW)
    lime_win32::Win32Call("ShowWindow", args.substr(0, args.find('\x1f')) + "\x1f" "5");
  return lime_win32::IsErrorReply(reply) ? FALSE : TRUE;
}

inline BOOL ShowWindow(HWND hwnd, int cmd) {
  std::string args = std::to_string(reinterpret_cast<std::intptr_t>(hwnd));
  args += '\x1f' + std::to_string(cmd);
  std::string reply = lime_win32::Win32Call("ShowWindow", args);
  return lime_win32::IsErrorReply(reply) ? FALSE : TRUE;
}

inline BOOL GetClientRect(HWND hwnd, RECT* out) {
  std::string reply =
      lime_win32::Win32Call("GetClientRect", std::to_string(reinterpret_cast<std::intptr_t>(hwnd)));
  if (lime_win32::IsErrorReply(reply) || !out) return FALSE;
  std::string x, y, w, h, rest = reply;
  auto take = [&rest]() {
    size_t p = rest.find('\x1f');
    std::string field = p == std::string::npos ? rest : rest.substr(0, p);
    rest = p == std::string::npos ? std::string() : rest.substr(p + 1);
    return field;
  };
  x = take();
  y = take();
  w = take();
  h = take();
  out->left = 0;
  out->top = 0;
  out->right = static_cast<LONG>(std::strtol(w.c_str(), nullptr, 10));
  out->bottom = static_cast<LONG>(std::strtol(h.c_str(), nullptr, 10));
  return TRUE;
}

inline BOOL InvalidateRect(HWND hwnd, const RECT*, BOOL) {
  std::string reply = lime_win32::Win32Call(
      "InvalidateRect", std::to_string(reinterpret_cast<std::intptr_t>(hwnd)));
  return lime_win32::IsErrorReply(reply) ? FALSE : TRUE;
}

inline HDC BeginPaint(HWND hwnd, PAINTSTRUCT* ps) {
  std::string reply =
      lime_win32::Win32Call("BeginPaint", std::to_string(reinterpret_cast<std::intptr_t>(hwnd)));
  if (lime_win32::IsErrorReply(reply)) return nullptr;
  HDC hdc = reinterpret_cast<HDC>(lime_win32::ToLong(reply));
  if (ps) ps->hdc = hdc;
  return hdc;
}

inline BOOL EndPaint(HWND hwnd, const PAINTSTRUCT*) {
  std::string reply =
      lime_win32::Win32Call("EndPaint", std::to_string(reinterpret_cast<std::intptr_t>(hwnd)));
  return lime_win32::IsErrorReply(reply) ? FALSE : TRUE;
}

inline int FillRect(HDC hdc, const RECT*, HBRUSH brush) {
  std::string args = std::to_string(reinterpret_cast<std::intptr_t>(hdc));
  args += '\x1f' + std::to_string(reinterpret_cast<std::intptr_t>(brush));
  std::string reply = lime_win32::Win32Call("FillRect", args);
  return lime_win32::IsErrorReply(reply) ? 0 : 1;
}

inline int StretchDIBits(HDC hdc, int, int, int destW, int destH, int, int, int, int,
                         const void*, const BITMAPINFO*, UINT, DWORD) {
  std::string args = std::to_string(reinterpret_cast<std::intptr_t>(hdc));
  args += '\x1f' + std::to_string(destW) + '\x1f' + std::to_string(destH);
  std::string reply = lime_win32::Win32Call("StretchDIBits", args);
  return lime_win32::IsErrorReply(reply) ? 0 : static_cast<int>(lime_win32::ToLong(reply));
}

inline BOOL ScreenToClient(HWND hwnd, POINT* pt) {
  if (!pt) return FALSE;
  std::string args = std::to_string(reinterpret_cast<std::intptr_t>(hwnd));
  args += '\x1f' + std::to_string(pt->x) + '\x1f' + std::to_string(pt->y);
  std::string reply = lime_win32::Win32Call("ScreenToClient", args);
  if (lime_win32::IsErrorReply(reply)) return FALSE;
  size_t sep = reply.find('\x1f');
  if (sep == std::string::npos) return FALSE;
  pt->x = static_cast<LONG>(std::strtol(reply.substr(0, sep).c_str(), nullptr, 10));
  pt->y = static_cast<LONG>(std::strtol(reply.substr(sep + 1).c_str(), nullptr, 10));
  return TRUE;
}

inline HWND SetFocus(HWND hwnd) {
  std::string reply =
      lime_win32::Win32Call("SetFocus", std::to_string(reinterpret_cast<std::intptr_t>(hwnd)));
  if (lime_win32::IsErrorReply(reply)) return nullptr;
  return reinterpret_cast<HWND>(lime_win32::ToLong(reply));
}

inline short GetKeyState(int vk) {
  std::string reply = lime_win32::Win32Call("GetKeyState", std::to_string(vk));
  return static_cast<short>(lime_win32::ToLong(reply));
}

inline LRESULT DefWindowProcW(HWND, UINT, WPARAM, LPARAM) { return 0; }

// GetMessageW/PeekMessageW's session-side queue (wasi_k32.hpp) has nothing
// that can actually post real input/paint into it under wasigocvm -- an
// empty queue reads as an immediate "quit", matching that dispatch's own
// `if (m.empty()) return ok("quit")` for the Get* form (there's no way to
// block a synchronous wasi_call waiting on external input here, so it
// resolves the same way a real WM_QUIT would). A caller's `while
// (GetMessageW(...))` loop (e.g. wlm_example's) exits promptly instead of
// hanging -- another documented simplification, not a bug.
inline int GetMessageW(MSG* msg, HWND hwnd, UINT, UINT) {
  std::string reply =
      lime_win32::Win32Call("GetMessageW", std::to_string(reinterpret_cast<std::intptr_t>(hwnd)));
  if (reply == "quit" || lime_win32::IsErrorReply(reply)) return 0;
  std::string rest = reply;
  auto take = [&rest]() {
    size_t p = rest.find('\x1f');
    std::string field = p == std::string::npos ? rest : rest.substr(0, p);
    rest = p == std::string::npos ? std::string() : rest.substr(p + 1);
    return field;
  };
  if (msg) {
    msg->hwnd = hwnd;
    msg->message = static_cast<UINT>(lime_win32::ToLong(take()));
    msg->wParam = static_cast<WPARAM>(lime_win32::ToLong(take()));
    msg->lParam = static_cast<LPARAM>(lime_win32::ToLong(take()));
  }
  return 1;
}

inline BOOL TranslateMessage(const MSG*) { return TRUE; }

// No live WndProc dispatch under wasigocvm -- see this file's header
// comment. A drained message is bookkeeping only past this point.
inline LRESULT DispatchMessageW(const MSG*) { return 0; }

#endif  // WASMWIN32_INCLUDE_WIN32_WASIGOCVM_WINDOWS_SHIM_HPP_
