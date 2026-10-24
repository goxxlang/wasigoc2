#ifndef WASMGOCOS_INCLUDE_GOCOS_GOCSHELL_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_GOCSHELL_HPP_

// GocShell — console. Cmd/Pwsh hop CreateProcessW through
// hv.k32. Not a builtin command table.

inline std::string gocshell_trim(const char* a) {
  std::string s = a ? a : "";
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' ||
                        s.back() == '\n'))
    s.pop_back();
  return s;
}

inline std::string gocshell_tok(const std::string& s) {
  if (s.empty()) return s;
  if (s[0] == '"') {
    auto e = s.find('"', 1);
    return e == std::string::npos ? s.substr(1) : s.substr(1, e - 1);
  }
  size_t i = 0;
  while (i < s.size() && s[i] != ' ' && s[i] != '\t') ++i;
  return s.substr(0, i);
}

inline std::string gocshell_lower(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
  return s;
}

inline bool gocshell_is(const std::string& tok, const char* name) {
  std::string a = gocshell_lower(tok);
  if (a.size() >= 4 && a.compare(a.size() - 4, 4, ".exe") == 0) a.resize(a.size() - 4);
  return a == name;
}

inline std::string gocshell_cwd() {
  std::string d = hv::k32("GetCurrentDirectoryW", "");
  if (d.rfind("error:", 0) != 0 && !d.empty()) {
    copy_field(gockrnl().cwd, sizeof(gockrnl().cwd), d);
    return d;
  }
  if (gockrnl().cwd[0]) return gockrnl().cwd;
  return "C:\\";
}

inline std::string gocshell_prompt() {
  std::string d = gocshell_cwd();
  std::string nt = gocsys_unix_to_nt(d.c_str());
  if (nt.empty()) nt = "C:\\";
  if (nt.back() != '\\' && nt.back() != '/') nt += '\\';
  for (char& c : nt)
    if (c == '/') c = '\\';
  if (gockrnl().shell == 1) return std::string("PS ") + nt + "> ";
  return nt + ">";
}

inline std::string gocshell_conhost() {
  if (gockrnl().shell == 1) {
    copy_field(gockrnl().title, sizeof(gockrnl().title), "Windows PowerShell");
    return "Windows PowerShell — GocOS\r\n"
           "Copyright (C) Microsoft Corporation. All rights reserved.\r\n\r\n";
  }
  std::string ver = gockrnl_version();
  if (ver.rfind("error:", 0) == 0) ver.clear();
  copy_field(gockrnl().title, sizeof(gockrnl().title), "Command Prompt");
  return std::string("Microsoft Windows [Version ") + ver +
         "]\r\n(c) Microsoft Corporation. All rights reserved.\r\n";
}

inline std::string gocshell_cmd(const char* line) {
  std::string s = gocshell_trim(line);
  if (s.empty()) return "";
  std::string tok = gocshell_tok(s);
  if (gocshell_is(tok, "powershell") || gocshell_is(tok, "pwsh")) {
    gockrnl().shell = 1;
    copy_field(gockrnl().title, sizeof(gockrnl().title), "Windows PowerShell");
    gockrnl_create_process("powershell.exe", "powershell.exe", false);
    return gocshell_conhost();
  }
  if (gocshell_is(tok, "cmd") || gocshell_is(tok, "command")) {
    gockrnl().shell = 0;
    copy_field(gockrnl().title, sizeof(gockrnl().title), "Command Prompt");
    gockrnl_create_process("cmd.exe", "cmd.exe", false);
    return gocshell_conhost();
  }
  if (gocshell_is(tok, "exit") && gockrnl().shell == 1) {
    gockrnl().shell = 0;
    copy_field(gockrnl().title, sizeof(gockrnl().title), "Command Prompt");
    return "";
  }
  std::string low = gocshell_lower(tok);
  bool pe = low.size() >= 4 && (low.compare(low.size() - 4, 4, ".exe") == 0 ||
                                low.compare(low.size() - 4, 4, ".com") == 0);
  if (pe || gocshell_is(tok, "calc") || gocshell_is(tok, "notepad") ||
      gocshell_is(tok, "mspaint")) {
    std::string img = tok;
    if (!pe) img += ".exe";
    return gockrnl_create_process(img.c_str(), s.c_str(), true);
  }
  if (gockrnl().shell == 1) {
    std::string cmd = "powershell.exe -NoLogo -Command ";
    cmd += s;
    return gockrnl_create_process("powershell.exe", cmd.c_str(), true);
  }
  std::string cmd = "cmd.exe /c ";
  cmd += s;
  return gockrnl_create_process("cmd.exe", cmd.c_str(), true);
}

inline std::string gocshell_pwsh(const char* line) {
  gockrnl().shell = 1;
  std::string s = gocshell_trim(line);
  if (s.empty()) return gocshell_conhost();
  return gocshell_cmd(s.c_str());
}

// Interactive console. The whole terminal — echo, line editing, the prompt,
// and command dispatch — lives here in the module, not in the guest's Go and
// not in JS. The browser hop only moves raw bytes: it feeds keystrokes in and
// blits the returned bytes to the display. State (the line being typed)
// persists across calls, so input may arrive one byte or many at a time.
struct GocConsole {
  std::string line;
  bool primed = false;
};

inline GocConsole& gocconsole() {
  static GocConsole c;
  return c;
}

inline void gocconsole_reset() { gocconsole() = GocConsole{}; }

// Feed raw input bytes; return the bytes to write to the display.
inline std::string gocshell_console(const char* input) {
  GocConsole& c = gocconsole();
  std::string out;
  if (!c.primed) {
    c.primed = true;
    out += gocshell_conhost();
    out += gocshell_prompt();
  }
  for (const char* p = input ? input : ""; *p; ++p) {
    unsigned char ch = static_cast<unsigned char>(*p);
    if (ch == '\r' || ch == '\n') {
      out += "\r\n";
      std::string reply = gocshell_cmd(c.line.c_str());
      c.line.clear();
      if (!reply.empty()) {
        out += reply;
        if (reply.back() != '\n') out += "\r\n";
      }
      out += gocshell_prompt();
    } else if (ch == 0x08 || ch == 0x7f) {  // backspace / delete
      if (!c.line.empty()) {
        c.line.pop_back();
        out += "\b \b";
      }
    } else if (ch == 0x03) {  // Ctrl-C: abandon the line
      c.line.clear();
      out += "^C\r\n";
      out += gocshell_prompt();
    } else if (ch >= 0x20) {  // printable: buffer and echo
      c.line.push_back(static_cast<char>(ch));
      out += static_cast<char>(ch);
    }
  }
  return out;
}

#endif  // WASMGOCOS_INCLUDE_GOCOS_GOCSHELL_HPP_
