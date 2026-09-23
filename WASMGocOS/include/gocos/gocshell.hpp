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

#endif  // WASMGOCOS_INCLUDE_GOCOS_GOCSHELL_HPP_
