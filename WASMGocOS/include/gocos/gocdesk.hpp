#ifndef WASMGOCOS_INCLUDE_GOCOS_GOCDESK_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_GOCDESK_HPP_

// GocDesk — GPU desktop surface. HWND is GocSys via hv.k32
// (RegisterClassW + CreateWindowExW). Present is named on that window.
// iframe of the swapchain is IFrame.

struct GocDesk {
  char hwnd[128]{};
  char dc[64]{};
  int width = 1280;
  int height = 720;
  bool gpu = false;
  float cam_x = 0, cam_y = 0, cam_zoom = 1;
};

inline GocDesk& gocdesk() {
  static GocDesk d;
  return d;
}

inline std::string gocdesk_ensure_window() {
  GocDesk& d = gocdesk();
  if (d.hwnd[0] && std::strncmp(d.hwnd, "error:", 6) != 0) return d.hwnd;
  std::string cls = hv::k32("RegisterClassW", "GocDeskWindow");
  if (cls.rfind("error:", 0) == 0) return cls;
  std::string args = "GocDeskWindow";
  args += '\x1f';
  args += "GocDesk";
  args += '\x1f';
  args += "13565952";
  std::string hwnd = hv::k32("CreateWindowExW", args.c_str());
  if (hwnd.rfind("error:", 0) == 0) return hwnd;
  copy_field(d.hwnd, sizeof(d.hwnd), hwnd);
  hv::k32("ShowWindow", join1f(hwnd.c_str(), "1").c_str());
  hv::k32("SetForegroundWindow", hwnd.c_str());
  std::string dc = hv::k32("GetDC", hwnd.c_str());
  copy_field(d.dc, sizeof(d.dc), dc);
  d.gpu = true;
  d.width = 1280;
  d.height = 720;
  return hwnd;
}

inline std::string gocdesk_present(const char* pass) {
  std::string hwnd = gocdesk_ensure_window();
  if (hwnd.rfind("error:", 0) == 0) return hwnd;
  GocDesk& d = gocdesk();
  std::string s = d.hwnd;
  s += "\x1f";
  s += "gpu=gocdesk";
  s += "\x1f";
  s += "swapchain=webgpu";
  s += "\x1f";
  s += "ui=FlushUI";
  s += "\x1f";
  s += (pass && pass[0] ? pass : "EndFrame");
  return s;
}

inline std::string gocdesk_draw(const char* api, const char* a) {
  std::string hwnd = gocdesk_ensure_window();
  if (hwnd.rfind("error:", 0) == 0) return hwnd;
  (void)a;
  std::string s = gocdesk().hwnd;
  s += "\x1f";
  s += "gpu=gocdesk";
  s += "\x1f";
  s += (api ? api : "DrawSprite");
  return s;
}

inline std::string gocdesk_set_camera(const char* a) {
  std::string hwnd = gocdesk_ensure_window();
  if (hwnd.rfind("error:", 0) == 0) return hwnd;
  GocDesk& d = gocdesk();
  std::string x, rest, y, z;
  split1f(a, &x, &rest);
  split1f(rest.c_str(), &y, &z);
  if (!x.empty()) d.cam_x = static_cast<float>(std::atof(x.c_str()));
  if (!y.empty()) d.cam_y = static_cast<float>(std::atof(y.c_str()));
  if (!z.empty()) d.cam_zoom = static_cast<float>(std::atof(z.c_str()));
  return gocdesk_draw("SetCamera2D", a);
}

inline std::string gocdesk_pump() {
  std::string hwnd = gocdesk_ensure_window();
  if (hwnd.rfind("error:", 0) == 0) return hwnd;
  std::string msg = hv::k32("PeekMessageW", hwnd.c_str());
  if (msg.rfind("error:", 0) == 0) return msg;
  if (eq(msg.c_str(), "0") || eq(msg.c_str(), "quit"))
    return eq(msg.c_str(), "quit") ? "0" : "1";
  hv::k32("TranslateMessage", msg.c_str());
  hv::k32("DispatchMessageW", msg.c_str());
  return "1";
}

inline std::string gocdesk_input(const char* api, const char* a) {
  std::string hwnd = gocdesk_ensure_window();
  if (hwnd.rfind("error:", 0) == 0) return hwnd;
  if (eq(api, "IsKeyDown")) return hv::k32("GetKeyState", a);
  if (eq(api, "IsMouseDown") || eq(api, "MouseX") || eq(api, "MouseY")) {
    std::string p = hv::k32("GetCursorPos", hwnd.c_str());
    if (eq(api, "IsMouseDown")) return hv::k32("GetAsyncKeyState", a);
    return p;
  }
  return "0";
}

inline std::string gocdesk_iframe() {
  std::string hwnd = gocdesk_ensure_window();
  if (hwnd.rfind("error:", 0) == 0) return hwnd;
  return std::string("pending: iframe gocdesk hwnd=") + gocdesk().hwnd;
}

inline bool gocdesk_is_api(const char* api) {
  return eq(api, "Present") || eq(api, "DesktopPresent") ||
         eq(api, "Swapchain") || eq(api, "DesktopSwapchain") ||
         eq(api, "GpuInit") || eq(api, "BeginFrame") || eq(api, "EndFrame") ||
         eq(api, "FlushUI") || eq(api, "BeginLoadPass") ||
         eq(api, "DrawSprite") || eq(api, "DrawString") ||
         eq(api, "SetCamera2D") || eq(api, "IsKeyDown") ||
         eq(api, "IsMouseDown") || eq(api, "MouseX") || eq(api, "MouseY") ||
         eq(api, "Pump") || eq(api, "PumpMessages") || eq(api, "IFrame") ||
         eq(api, "DesktopIFrame") || eq(api, "IFrameDesktop");
}

inline std::string gocdesk_call(const char* api, const char* a) {
  if (!api || !api[0]) return err_msg("gocdesk needs an API name");
  if (eq(api, "IFrame") || eq(api, "DesktopIFrame") || eq(api, "IFrameDesktop"))
    return gocdesk_iframe();
  if (eq(api, "Pump") || eq(api, "PumpMessages")) return gocdesk_pump();
  if (eq(api, "IsKeyDown") || eq(api, "IsMouseDown") || eq(api, "MouseX") ||
      eq(api, "MouseY"))
    return gocdesk_input(api, a);
  if (eq(api, "DrawSprite") || eq(api, "DrawString"))
    return gocdesk_draw(api, a);
  if (eq(api, "SetCamera2D")) return gocdesk_set_camera(a);
  if (eq(api, "GpuInit")) return gocdesk_ensure_window();
  return gocdesk_present(api);
}

inline bool desktopengine_is_api(const char* api) { return gocdesk_is_api(api); }
inline std::string desktopengine_call(const char* api, const char* a) {
  return gocdesk_call(api, a);
}

#endif  // WASMGOCOS_INCLUDE_GOCOS_GOCDESK_HPP_
