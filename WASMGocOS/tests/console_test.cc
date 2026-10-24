// The interactive console lives in the module: raw bytes in, display bytes
// out. Echo, line editing, the prompt, and dispatch are all here, so the
// browser hop (and the guest) only move bytes.
#include "gocos/dispatch.h"

#include <cassert>
#include <cstdio>
#include <string>

static std::string Con(const char* s) {
  char b[65536];
  b[0] = 0;
  int rc = wasmgocos_call("Console", s, b, sizeof(b));
  assert(rc == 0);
  return b;
}

int main() {
  char boot[4096];
  assert(wasmgocos_call("Boot", "", boot, sizeof(boot)) == 0);

  // First call primes: banner + prompt.
  std::string first = Con("");
  assert(first.find("Microsoft Windows") != std::string::npos);
  assert(first.find(">") != std::string::npos);

  // Printable keystrokes echo back.
  assert(Con("xy") == "xy");
  // Backspace erases the last char on screen and in the line buffer.
  assert(Con("\b") == "\b \b");

  // Enter ends the line (now "x"), runs it, and reprints a prompt.
  std::string ent = Con("\r");
  assert(ent.rfind("\r\n", 0) == 0);            // starts with CRLF
  assert(ent.find(">") != std::string::npos);   // prompt returned

  // Ctrl-C abandons the current line.
  assert(Con("abc").size() == 3);
  std::string ctrlc = Con("\x03");
  assert(ctrlc.find("^C") != std::string::npos);
  assert(ctrlc.find(">") != std::string::npos);

  std::printf("console_test: ok\n");
  return 0;
}
