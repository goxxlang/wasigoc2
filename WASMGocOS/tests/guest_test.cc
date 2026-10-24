// A wasigocvm guest as a container service: examples/service/main.go,
// built by ~/go++/wasigocvm.bat, run by `ctr run` on the kernel's WASI
// host (include/gocos/wasi.hpp).
#include "gocos/dispatch.h"
#include "gocos/files.hpp"
#include "gocos/wasi.hpp"

#if defined(WASMGOCOS_HAS_NET)
#include "wns/sock.h"
#endif
#if defined(WASMGOCOS_HAS_JITRT)
#include "gocos/klock.hpp"
#endif

#include <cassert>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <thread>
#include <string>
#include <vector>

namespace {

std::string Call(const char* api, const std::string& args, int* rc = nullptr) {
  static char buf[1 << 16];
  buf[0] = 0;
  int r = wasmgocos_call(api, args.c_str(), buf, sizeof(buf));
  if (rc != nullptr) *rc = r;
  return buf;
}

std::string F(std::initializer_list<std::string> fields) {
  std::string out;
  for (const std::string& f : fields) {
    if (!out.empty()) out.push_back('\x1f');
    out += f;
  }
  return out;
}

unsigned Pid(const std::string& line) {
  unsigned pid = 0;
  if (std::sscanf(line.c_str(), "ctr pid=%u", &pid) != 1) {
    std::fprintf(stderr, "not a container line: %s\n", line.c_str());
    assert(false);
  }
  return pid;
}

std::string Region(unsigned pid, const char* region) {
  return Call("Ctr", F({"read", std::to_string(pid), region}));
}

bool Has(const std::string& s, const std::string& part) {
  if (s.find(part) != std::string::npos) return true;
  std::fprintf(stderr, "missing \"%s\" in:\n%s\n", part.c_str(), s.c_str());
  return false;
}

std::vector<uint8_t> Slurp(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void PutLeb(std::vector<uint8_t>* out, uint32_t v) {
  do {
    uint8_t b = v & 0x7f;
    v >>= 7;
    if (v) b |= 0x80;
    out->push_back(b);
  } while (v);
}

}  // namespace

int main() {
  const std::string guest = GOCOS_GUEST_WASM;
  std::vector<uint8_t> core = Slurp(guest);
  if (core.empty()) {
    std::fprintf(stderr, "guest_test: %s is missing; build it with ~/go++/wasigocvm.bat\n", guest.c_str());
    return 1;
  }

  // The image: a core module, and the same module inside a component.
  wasmgocos::WasiImage image;
  std::string why;
  assert(wasmgocos::wasi_image(core.data(), core.size(), &image, &why));
  assert(image.run.rfind("wasi:cli/run@0.2.", 0) == 0 && !image.component && image.imports > 100);
  std::vector<uint8_t> component = {0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00, 0x01};
  PutLeb(&component, static_cast<uint32_t>(core.size()));
  component.insert(component.end(), core.begin(), core.end());
  assert(wasmgocos::wasi_image(component.data(), component.size(), &image, &why));
  assert(image.component && image.core == core);
  const uint8_t junk[] = {'n', 'o', 't', ' ', 'w', 'a', 's', 'm'};
  assert(!wasmgocos::wasi_image(junk, sizeof(junk), &image, &why) && why == "not wasm");
  std::vector<uint8_t> torn(core.begin(), core.begin() + core.size() / 2);
  assert(!wasmgocos::wasi_image(torn.data(), torn.size(), &image, &why) && why == "core module");

#if defined(WASMGOCOS_HAS_JITRT)
  Call("Boot", "");

  // A run: arguments, the session environment, the clock, both streams.
  int rc = 0;
  std::string line = Call("Ctr", F({"run", "svc", guest, "alpha", "beta"}), &rc);
  std::printf("%s\n", line.c_str());
  assert(rc == 0);
  assert(Has(line, " guest=wasigocvm exit=0 "));
  unsigned pid = Pid(line);
  std::string out = Region(pid, "stdout");
  assert(Has(out, "hello from a wasigocvm service\n"));
  assert(Has(out, "args=alpha,beta\n"));
  assert(Has(out, "PATH=/proc\n"));
  assert(Has(out, "clock true\n"));
  assert(Has(out, "recovered"));
  assert(Region(pid, "stderr") == "service: stderr line\n");
  assert(Call("Vfs", F({"read", "/ctr/svc/stdout"})) == out);
  std::string cons_fd = Call("Fd", F({"open", "#c/cons", "r"}));
  int fd = 0;
  assert(std::sscanf(cons_fd.c_str(), "fd=%d", &fd) == 1);
  assert(Has(Call("Fd", F({"read", std::to_string(fd)})), "hello from a wasigocvm service"));
  // The container is on the process table, and its memory is a GIA.
  std::string proc_fd = Call("Fd", F({"open", "#p/" + std::to_string(pid), "r"}));
  assert(std::sscanf(proc_fd.c_str(), "fd=%d", &fd) == 1);
  assert(Has(Call("Fd", F({"read", std::to_string(fd)})), "guest=wasigocvm"));
  assert(Has(Call("Ctr", "list"), " name=svc "));
  assert(Call("Ctr", F({"call", std::to_string(pid), "gocos", "Rc"})) == "ctr miss");
  assert(Call("Ctr", F({"save", "svc", guest + ".tmp"})) == "ctr miss");

  // os.Exit(3) is the exit code, and the shell's status.
  line = Call("Sh", "ctr run failing " + guest + " fail");
  assert(Has(line, " exit=3 "));
  assert(Call("Sh", "echo $?") == "3");

  // The guest boots its own ReactOS and bugchecks it: KeBugCheckEx
  // longjmps to ReactOSCall's setjmp, a wasm throw and try_table in the
  // guest (wasigocvm's -wasm-enable-sjlj and libsetjmp).
  line = Call("Ctr", F({"run", "crash", guest, "crash"}));
  assert(Has(line, " exit=0 "));
  out = Region(Pid(line), "stdout");
  assert(Has(out, "boot true true\n"));
  assert(Has(out, "bugcheck error: STOP 0x000000E2"));
  assert(Has(out, "MANUALLY_INITIATED_CRASH"));
  assert(Has(out, "state=crashed"));

  // A trap ends the guest, not the kernel: a go++ runtime panic aborts,
  // and wasi-libc's abort is `unreachable`. What the guest wrote first is
  // kept, the status is 1, and the next guest runs.
  line = Call("Sh", "ctr run trapper " + guest + " trap");
  assert(Has(line, " exit=1 "));
  assert(Has(line, " fault=trap: unreachable"));
  assert(Call("Sh", "echo $?") == "1");
  assert(Has(Region(Pid(line), "stdout"), "hello from a wasigocvm service\n"));
  line = Call("Ctr", F({"run", "after", guest}));
  assert(Has(line, " exit=0 "));
  assert(line.find("fault=") == std::string::npos);

  // The kernel's files. The guest's paths walk the session's namespace
  // (goclibc), its writes are the kernel's files, and a session bind is in
  // the guest's namespace too.
  line = Call("Ctr", F({"run", "files", guest, "files"}));
  assert(Has(line, " exit=0 "));
  out = Region(Pid(line), "stdout");
  for (const char* want : {"mkdir true\n", "write true\n", "read true 17 true true true true\n", "stat true 15 false\n", "mtime true\n",
                           "dir true\n", "rename true\n", "gone true\n", "keep true\n", "remove true\n",
                           "missing true\n", "ls b.txt,keep.txt,\n", "env /proc\n", "peer true\n",
                           "relative true\n"})
    assert(Has(out, want));
  assert(Call("Vfs", F({"read", "/work/keep.txt"})) == "kept");
  // A reply is a C string, so the NUL ends it here: the length is stat's.
  assert(Call("Vfs", F({"read", "/work/b.txt"})) == "alpha\x1f"
              "beef1");
  std::string b_stat = Call("Vfs", F({"stat", "/work/b.txt"}));
  assert(b_stat.size() > 3 && b_stat.compare(b_stat.size() - 3, 3, " 17") == 0);
  assert(Call("Vfs", F({"read", "/rel.txt"})) == "r");
  assert(Call("Sh", "cat /work/keep.txt") == "kept");
  // touch: a new file is made empty; an old one gets the time now.
  assert(Call("Sh", "touch /work/touched.txt").empty());
  assert(wasmgocos::files::StatOf(wasmgocos::files::Walk("/work/touched.txt")).kind == wasmgocos::files::Kind::kFile);
  const std::string keep = wasmgocos::files::Walk("/work/keep.txt");
  assert(wasmgocos::files::SetTime(keep, 1));
  assert(wasmgocos::files::StatOf(keep).mtime == 1);
  assert(Call("Sh", "cd /work; touch keep.txt").empty());
  assert(wasmgocos::files::StatOf(keep).mtime > 1780000000LL * 1000000000LL);
  assert(Call("Sh", "cat /work/keep.txt") == "kept");
  assert(Has(Call("Sh", "bind /work /box"), "bind"));
  line = Call("Ctr", F({"run", "boxed", guest, "boxed"}));
  assert(Has(Region(Pid(line), "stdout"), "box true kept\n"));

  // Background guests (include/gocos/klock.hpp). A spinning guest and the
  // session take turns on the kernel lock: the session answers while it
  // runs, `jobs` shows it, and `kill` ends it at its next yield point.
  std::string started = Call("Sh", "ctr run spinner " + guest + " spin &");
  assert(Has(started, " ctr name=spinner running"));
  std::string spin_job = std::to_string(std::atoi(started.c_str() + 4));
  // Wait (outside the kernel) until the guest is in its loop.
  for (int i = 0; i < 200 && Call("Vfs", F({"read", "/spinning"})) != "spinner"; i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  assert(Call("Vfs", F({"read", "/spinning"})) == "spinner");
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 5; i++) assert(Call("Sh", "echo alive") == "alive");
  auto answered = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0);
  std::printf("five shell commands beside a spinning guest: %lld ms\n", static_cast<long long>(answered.count()));
  assert(answered.count() < 3000);
  assert(Has(Call("Sh", "jobs"), "ctr spinner running"));
  assert(Call("Sh", "kill " + spin_job) == "kill " + spin_job);
  assert(Has(Call("Sh", "wait " + spin_job), "exit=137"));
  line = Call("Ctr", F({"wait", "spinner"}));
  assert(Has(line, " exit=137 ") && Has(line, " fault=killed"));
  assert(Call("Ctr", F({"state", "spinner"})) == "exited 137");

  // A foreground guest (no `&`) holds the calling thread until it ends,
  // so `kill` has no job to reach; kernel_interrupt_foreground is what
  // Ctrl-C on the console calls instead (include/gocos/klock.hpp). It
  // raises the flag wasmgocos_call's ForegroundGuard installed and
  // ends the guest at its next yield point, the same as a killed job.
  std::string fg_line;
  std::thread fg([&] { fg_line = Call("Ctr", F({"run", "fgspin", guest, "spin"})); });
  bool fg_spinning = false;
  for (int i = 0; i < 200 && !fg_spinning; i++) {
    {
      wasmgocos::KernelLock lock;  // synchronizes with the guest's write, without a
                                    // second top-level foreground call to clobber
                                    // the one ForegroundGuard slot fg's call holds.
      std::string content;
      fg_spinning = wasmgocos::files::Read(wasmgocos::files::Walk("/spinning"), &content) &&
                    content == "fgspin";
    }
    if (!fg_spinning) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  assert(fg_spinning);
  wasmgocos::kernel_interrupt_foreground();
  fg.join();
  assert(Has(fg_line, " exit=137 ") && Has(fg_line, " fault=killed"));

  // A guest that finishes on its own: `wait` has its exit and output.
  started = Call("Sh", "ctr run counter " + guest + " count &");
  std::string count_job = std::to_string(std::atoi(started.c_str() + 4));
  std::string waited = Call("Sh", "wait " + count_job);
  assert(Has(waited, "exit=0") && Has(waited, "counted true"));
  assert(Call("Sh", "echo $?") == "0");

  // A daemon is a background guest; stopping it kills it.
  assert(Has(Call("Sh", "svc daemon web ctr run web " + guest + " spin &"), "kind=daemon"));
  assert(Has(Call("Sh", "svc start web"), "job "));
  for (int i = 0; i < 200 && Call("Vfs", F({"read", "/spinning"})) != "web"; i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  assert(Call("Vfs", F({"read", "/spinning"})) == "web");
  assert(Call("Sh", "echo up") == "up");
  assert(Has(Call("Sh", "svc stop web"), "exit=137"));

#if defined(WASMGOCOS_HAS_NET)
  // TCP on the kernel's network stack. A guest server answers a client in
  // this process, a guest client reaches a server in this process, and two
  // guests talk to each other.
  auto wait_listening = [&](const std::string& who) {
    for (int i = 0; i < 400 && Call("Vfs", F({"read", "/listening"})) != who; i++)
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    assert(Call("Vfs", F({"read", "/listening"})) == who);
  };
  auto endpoint = [](const char* text) {
    wns::Endpoint ep;
    assert(wns::ParseEndpoint(text, &ep));
    return ep;
  };
  auto recv_all = [](wns::TcpConn& conn, size_t want) {
    std::string got;
    for (int i = 0; i < 400 && got.size() < want; i++) {
      uint8_t buf[256];
      size_t n = 0;
      wns::IoStatus st = conn.Recv(buf, sizeof(buf), &n);
      if (st == wns::IoStatus::kOk) got.append(reinterpret_cast<char*>(buf), n);
      else if (st != wns::IoStatus::kAgain) break;
      else std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return got;
  };
  auto send_all = [](wns::TcpConn& conn, const std::string& text) {
    conn.Queue(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
    for (int i = 0; i < 400 && conn.Flush() == wns::IoStatus::kAgain; i++)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
  };

  started = Call("Sh", "ctr run echo " + guest + " serve 127.0.0.1:47811 &");
  std::string echo_job = std::to_string(std::atoi(started.c_str() + 4));
  wait_listening("echo");
  {
    wns::TcpConn client;
    assert(client.Connect(endpoint("127.0.0.1:47811")));
    send_all(client, "ping");
    assert(recv_all(client, 9) == "echo:ping");
  }
  waited = Call("Sh", "wait " + echo_job);
  assert(Has(waited, "exit=0") && Has(waited, "listen true") && Has(waited, "accept true"));

  {
    wns::TcpListener server;
    assert(server.Listen(endpoint("127.0.0.1:47812")));
    started = Call("Sh", "ctr run pinger " + guest + " dial 127.0.0.1:47812 &");
    std::string pinger_job = std::to_string(std::atoi(started.c_str() + 4));
    wns::SockHandle h = wns::kBadSock;
    for (int i = 0; i < 400 && h == wns::kBadSock; i++) {
      h = server.Accept(nullptr);
      if (h == wns::kBadSock) std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    assert(h != wns::kBadSock);
    wns::TcpConn conn;
    conn.Adopt(h, wns::Endpoint{});
    assert(recv_all(conn, 4) == "ping");
    send_all(conn, "pong");
    waited = Call("Sh", "wait " + pinger_job);
    assert(Has(waited, "exit=0") && Has(waited, "dial true") && Has(waited, "got pong"));
  }

  // Guest to guest: the client runs in the foreground, holding the kernel
  // lock, and the server gets its turns while the client waits.
  started = Call("Sh", "ctr run echo2 " + guest + " serve 127.0.0.1:47813 &");
  std::string echo2_job = std::to_string(std::atoi(started.c_str() + 4));
  wait_listening("echo2");
  line = Call("Ctr", F({"run", "pinger2", guest, "dial", "127.0.0.1:47813"}));
  assert(Has(line, " exit=0 "));
  assert(Has(Region(Pid(line), "stdout"), "got echo:ping\n"));
  assert(Has(Call("Sh", "wait " + echo2_job), "exit=0"));

  // UDP: a guest answers a datagram from this process, and two guests
  // exchange datagrams.
  started = Call("Sh", "ctr run uecho " + guest + " userve 127.0.0.1:47821 &");
  std::string uecho_job = std::to_string(std::atoi(started.c_str() + 4));
  wait_listening("uecho");
  {
    wns::UdpSocket client;
    assert(client.Open(false));
    const std::string ping = "ping";
    assert(client.SendTo(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(ping.data()), ping.size()),
                         endpoint("127.0.0.1:47821")) == wns::IoStatus::kOk);
    std::string got;
    for (int i = 0; i < 400 && got.empty(); i++) {
      uint8_t buf[256];
      size_t n = 0;
      wns::Endpoint from;
      if (client.RecvFrom(buf, sizeof(buf), &n, &from) == wns::IoStatus::kOk) got.assign(reinterpret_cast<char*>(buf), n);
      else std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    assert(got == "echo:ping");
  }
  waited = Call("Sh", "wait " + uecho_job);
  assert(Has(waited, "exit=0") && Has(waited, "read true 4") && Has(waited, "wrote true"));

  started = Call("Sh", "ctr run uecho2 " + guest + " userve 127.0.0.1:47822 &");
  std::string uecho2_job = std::to_string(std::atoi(started.c_str() + 4));
  wait_listening("uecho2");
  line = Call("Ctr", F({"run", "uping", guest, "udial", "127.0.0.1:47822"}));
  assert(Has(line, " exit=0 "));
  assert(Has(Region(Pid(line), "stdout"), "got echo:ping\n"));
  assert(Has(Call("Sh", "wait " + uecho2_job), "exit=0"));
#endif

  // A guest stored in the OS runs from there, through a bind too.
  assert(Has(Call("Sh", "ctr install " + guest + " /bin/service.wasm"), "install /bin/service.wasm "));
  line = Call("Sh", "ctr run fromos /bin/service.wasm alpha");
  assert(Has(line, " exit=0 "));
  assert(Has(Region(Pid(line), "stdout"), "args=alpha\n"));
  assert(Has(Call("Sh", "bind /bin /usr/bin"), "bind"));
  assert(Has(Call("Sh", "ctr run viabind /usr/bin/service.wasm"), " exit=0 "));
  assert(Has(Call("Sh", "ctr run nobin /bin/none.wasm"), "error: ctr: open /bin/none.wasm"));

  // A guest in /bin is a command: stdin from a pipe or `<`, its stdout
  // into the pipeline, and its exit status is `$?`.
  std::string up = Call("Sh", "echo hello | service upper");
  if (up != "HELLO\n") std::printf("upper: [%s]\n", up.c_str());
  assert(up == "HELLO\n");
  assert(Call("Sh", "service upper < /work/keep.txt") == "KEPT\n");
  assert(Call("Sh", "echo abc | service upper | /bin/service.wasm upper > /work/up.txt").empty());
  assert(Call("Vfs", F({"read", "/work/up.txt"})) == "ABC\n");
  assert(Call("Sh", "service upper < /work/up.txt > /work/up2.txt; echo $?") == "0");
  std::string empty = Call("Sh", "service upper; echo $?");
  if (empty != "3") std::printf("empty stdin: [%s]\n", empty.c_str());
  assert(empty == "3");
  assert(Call("Sh", "service upper && echo yes || echo no") == "no");
  // Nothing stays: run after run gives its memory back to the cage.
  for (int i = 0; i < 40; ++i) assert(Call("Sh", "echo x | service upper") == "X\n");

  // A pipeline with a guest in it streams: its stages run at once, so a
  // writer that never stops ends when its reader does (a broken pipe).
  std::string heads = Call("Sh", "service yes | service head 3");
  if (heads != "y\ny\ny\n") std::printf("yes|head: [%s]\n", heads.c_str());
  assert(heads == "y\ny\ny\n");
  assert(Call("Sh", "service yes | service head 2 | service upper") == "Y\nY\n");
  assert(Call("Sh", "service yes | service head 2 | cat") == "y\ny\n");
  assert(Call("Sh", "service yes | service head 2 > /work/yy.txt; echo $?") == "0");
  assert(Call("Vfs", F({"read", "/work/yy.txt"})) == "y\ny\n");
  // The status is the last stage's: upper reads nothing and says 3.
  assert(Call("Sh", "echo hi | service head 0 | service upper; echo $?") == "3");

  // A command with a guest in it runs in the background as a session job:
  // `wait` gives its status and output, `jobs` lists it, and `kill` ends
  // every stage of it.
  started = Call("Sh", "echo hi | service upper > /work/bg.txt &");
  assert(started.rfind("job ", 0) == 0);
  waited = Call("Sh", "wait " + std::to_string(std::atoi(started.c_str() + 4)));
  if (!Has(waited, "exit=0")) std::printf("bg wait: [%s]\n", waited.c_str());
  assert(Has(waited, "exit=0"));
  assert(Call("Vfs", F({"read", "/work/bg.txt"})) == "HI\n");
  started = Call("Sh", "echo hey | service upper &");
  waited = Call("Sh", "wait " + std::to_string(std::atoi(started.c_str() + 4)));
  assert(Has(waited, "exit=0") && Has(waited, "out=[HEY]"));
  started = Call("Sh", "service yes | service upper > /work/forever.txt &");
  std::string forever = std::to_string(std::atoi(started.c_str() + 4));
  std::string listed = Call("Sh", "jobs");
  if (!Has(listed, "service yes | service upper") || !Has(listed, "running"))
    std::printf("jobs: [%s]\n", listed.c_str());
  assert(Has(listed, "service yes | service upper > /work/forever.txt running"));
  assert(Call("Sh", "kill " + forever) == "kill " + forever);
  waited = Call("Sh", "wait " + forever);
  if (!Has(waited, "exit=137")) std::printf("killed wait: [%s]\n", waited.c_str());
  assert(Has(waited, "exit=137"));

  // A C guest on wasi:filesystem: libc's open, read, write, and the rest,
  // on the kernel's files. What it leaves behind the kernel reads.
  line = Call("Ctr", F({"run", "fsguest", GOCOS_FS_WASM}));
  std::string fs_out = Region(Pid(line), "stdout");
  if (!Has(line, " exit=0 ") || !Has(fs_out, "fsguest ok\n")) std::printf("%s\n%s", line.c_str(), fs_out.c_str());
  assert(Has(line, " exit=0 ") && Has(fs_out, "fsguest ok\n"));
  assert(Has(fs_out, "ls a.txt,c.txt,sub/,\n"));
  assert(Call("Vfs", F({"read", "/fsguest/kept.txt"})) == "from C");
  assert(Call("Vfs", F({"read", "/fsguest/a.txt"})) == "hello");
  // Run again: the directory is there now, so mkdir fails, and says so.
  line = Call("Ctr", F({"run", "fsguest2", GOCOS_FS_WASM}));
  assert(Has(line, " exit=1 ") && Has(Region(Pid(line), "stdout"), "FAIL mkdir\n"));

  // The same module inside a component runs the same way.
  std::string wrapped = guest + ".component.tmp";
  {
    std::ofstream o(wrapped, std::ios::binary);
    o.write(reinterpret_cast<const char*>(component.data()), static_cast<std::streamsize>(component.size()));
  }
  line = Call("Ctr", F({"run", "wrapped", wrapped, "gamma"}));
  std::remove(wrapped.c_str());
  assert(Has(line, " exit=0 ") && Has(line, " component=1"));
  assert(Has(Region(Pid(line), "stdout"), "args=gamma\n"));

  // Refusals are replies.
  assert(Has(Call("Ctr", F({"run", "svc", guest}), &rc), "error: ctr: svc exists"));
  assert(rc != 0);
  assert(Has(Call("Ctr", F({"run", "junk", std::string(GOCOS_TEST_DATA) + "/plain.unil"})),
             "error: ctr: guest not wasm"));
  assert(Has(Call("Ctr", F({"run", "none", guest + ".none"})), "error: ctr: open"));
#endif
  std::printf("guest_test ok\n");
  return 0;
}
