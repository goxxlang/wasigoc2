// Extra kernel32 / user32 / gdi32 / ole32 / ntdll / bcrypt / ncrypt /
// crypt32 / shell32 / winhttp / iphlpapi / version / comctl32 /
// wininet / dnsapi / secur32 / dbghelp / wintrust / uxtheme / dwmapi /
// rpcrt4 / setupapi / cfgmgr32 / netapi32 / pdh / wevtapi / comdlg32 /
// WinHvPlatform / WinHvEmulation
// bindings
// for the wasm/libc hop. Names match win32metadata. Included inside
// namespace wasmwin32 from wasi_host.hpp
// (do not wrap this file in a namespace). CreateProcessW/A is a GocVM
// vthread child running wasi_call / WslExec. LoadLibrary / LdrLoadDll
// is a module table plus file probe (wasm image maps; PE maps through
// ~/WASMPELoader, including resources / delay-load / TLS as data).
// MainDLL (AddressOfEntryPoint / DllMain) and TLS callbacks run on that
// hop via WHvRunVirtualProcessor — the mapper does not JIT x86.
// Tokens/SIDs/PEB/TEB/HWND/GDI/COM are objects on the wasigocvm session.
// Virtual*Ex/RPM/WPM/NtCreateSection stay in this module's linear memory.
// DeviceIoControl uses wasi_device_ioctl. WSA is in-memory duplex.

enum { kK32ProcBase = 256 };
enum { kK32SyncBase = 384 };
enum { kK32ThrBase = 512 };
enum { kK32MapBase = 640 };
enum { kK32IocpBase = 768 };
enum { kK32JobBase = 896 };
enum { kK32VolBase = 1024 };
enum { kK32MiscBase = 1152 };
enum { kK32RegBase = 1280 };
enum { kK32TokBase = 1408 };
enum { kK32SockBase = 1536 };
enum { kK32CngBase = 1664 };
enum { kK32ModBase = 1792 };
enum { kK32WndBase = 1920 };
enum { kK32GdiBase = 2048 };
enum { kK32ComBase = 2176 };
enum { kK32CertBase = 2304 };
enum { kK32HttpBase = 2432 };
enum { kK32WhpBase = 2560 };

enum { kK32Max = 128 };

struct K32File {
  FILE* f = nullptr;
  DIR* d = nullptr;
  std::string path;
  std::string dir_path;
  int used = 0;
  unsigned last_io = 0;
  unsigned comp_modes = 0;
  int iocp = 0;
  unsigned handle_flags = 0;
};

inline K32File* k32_files() {
  static K32File t[kK32Max];
  return t;
}

inline int k32_alloc_file(FILE* f, const std::string& path = {}) {
  K32File* t = k32_files();
  for (int i = 3; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].f = f;
      t[i].d = nullptr;
      t[i].path = path;
      t[i].dir_path.clear();
      t[i].last_io = 0;
      t[i].comp_modes = 0;
      t[i].iocp = 0;
      t[i].handle_flags = 0;
      return i;
    }
  }
  return -1;
}

inline int k32_alloc_dir(DIR* d, const std::string& path) {
  K32File* t = k32_files();
  for (int i = 3; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 2;
      t[i].d = d;
      t[i].dir_path = path;
      return i;
    }
  }
  return -1;
}

inline std::string* k32_tls() {
  static std::string slot[64];
  return slot;
}

inline char* k32_tls_used() {
  static char used[64] = {};
  return used;
}

struct K32Proc {
  std::thread thr;
  std::mutex mu;
  std::condition_variable cv;
  std::string in;
  std::string out;
  unsigned pid = 0;
  int exit_code = 259;
  int used = 0;
  bool done = false;
  bool vthread = false;
  bool started = false;
};

inline bool k32_cmd_oneshot(const std::string& cmd) {
  return cmd.find(" /c ") != std::string::npos ||
         cmd.find(" /C ") != std::string::npos ||
         cmd.find("-Command") != std::string::npos;
}

inline std::string k32_gocvm_exec(const std::string& cmd);

inline void k32_run_child(K32Proc* p, const std::string& cmd) {
  {
    std::lock_guard<std::mutex> lk(p->mu);
    p->started = true;
  }
  // wasigocvm exec: vthread child. Oneshot cmd/pwsh lines are WslExec.
  // Images (calc.exe, …) occupy through LoadLibraryW + WHP, not a
  // Bytecode Alliance "no exec" stub. p->pid is already carried out of
  // band by CreateProcessW's own pid\x1fhandle\x1fhandle reply and by
  // GetProcessId — GetProcessOutput must return the command's actual
  // stdout untouched, or every caller piping this through a terminal
  // (GocShell.Cmd, cmdterm) shows a leading "pid=<n>\x1f" glued onto
  // real output.
  std::string reply = k32_gocvm_exec(cmd);
  std::lock_guard<std::mutex> lk(p->mu);
  p->out = std::move(reply);
  p->exit_code = (p->out.rfind("error:", 0) == 0) ? 1 : 0;
  p->done = true;
  p->cv.notify_all();
}

inline void (*&k32_child_go())(K32Proc*, std::string) {
  static void (*fn)(K32Proc*, std::string) = nullptr;
  return fn;
}
inline void (*&k32_run_until())(bool*) {
  static void (*fn)(bool*) = nullptr;
  return fn;
}
inline bool (*&k32_pump())() {
  static bool (*fn)() = nullptr;
  return fn;
}

inline K32Proc* k32_procs() {
  static K32Proc t[kK32Max];
  return t;
}

inline int k32_alloc_proc() {
  K32Proc* t = k32_procs();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].done = false;
      t[i].vthread = false;
      t[i].started = false;
      t[i].exit_code = 259;
      t[i].in.clear();
      t[i].out.clear();
      t[i].pid = static_cast<unsigned>(10000 + i);
      return kK32ProcBase + i;
    }
  }
  return -1;
}

inline K32Proc* k32_proc(int h) {
  int i = h - kK32ProcBase;
  if (i < 0 || i >= kK32Max || !k32_procs()[i].used) return nullptr;
  return &k32_procs()[i];
}

inline std::string k32_create_cmd(const char* a) {
  std::string app, rest, cmd, cwd;
  split1f(a, &app, &rest);
  split1f(rest.c_str(), &cmd, &cwd);
  if (cmd.empty()) cmd = app;
  return cmd;
}

inline bool k32_close_proc(int h) {
  K32Proc* p = k32_proc(h);
  if (!p) return false;
  if (p->thr.joinable()) p->thr.join();
  p->used = 0;
  p->done = false;
  p->out.clear();
  p->pid = 0;
  p->exit_code = 259;
  return true;
}

enum {
  kSyncEventAuto = 1,
  kSyncEventManual = 2,
  kSyncMutex = 3,
  kSyncSem = 4,
  kSyncTimerAuto = 5,
  kSyncTimerManual = 6
};

struct K32Sync {
  std::mutex mu;
  std::condition_variable cv;
  int used = 0;
  int kind = 0;
  int signaled = 0;
  int owned = 0;
  int count = 0;
  int maxc = 0;
  unsigned timer_gen = 0;
  std::string name;
};

inline K32Sync* k32_syncs() {
  static K32Sync t[kK32Max];
  return t;
}

inline int k32_alloc_sync(int kind) {
  K32Sync* t = k32_syncs();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].kind = kind;
      t[i].signaled = 0;
      t[i].owned = 0;
      t[i].count = 0;
      t[i].maxc = 0;
      t[i].timer_gen = 0;
      t[i].name.clear();
      return kK32SyncBase + i;
    }
  }
  return -1;
}

inline K32Sync* k32_sync(int h) {
  int i = h - kK32SyncBase;
  if (i < 0 || i >= kK32Max || !k32_syncs()[i].used) return nullptr;
  return &k32_syncs()[i];
}

struct K32Thr {
  std::thread thr;
  std::mutex mu;
  std::condition_variable cv;
  int used = 0;
  bool done = false;
  int exit_code = 259;
  unsigned tid = 0;
  int token = 0;
  std::string desc;
};

inline K32Thr* k32_thrs() {
  static K32Thr t[kK32Max];
  return t;
}

inline int k32_alloc_thr() {
  K32Thr* t = k32_thrs();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].done = false;
      t[i].exit_code = 259;
      t[i].tid = static_cast<unsigned>(20000 + i);
      t[i].token = 0;
      t[i].desc.clear();
      return kK32ThrBase + i;
    }
  }
  return -1;
}

inline K32Thr* k32_thr(int h) {
  int i = h - kK32ThrBase;
  if (i < 0 || i >= kK32Max || !k32_thrs()[i].used) return nullptr;
  return &k32_thrs()[i];
}

inline bool k32_sync_ready(K32Sync* s) {
  if (!s) return false;
  if (s->kind == kSyncEventAuto || s->kind == kSyncEventManual || s->kind == kSyncTimerAuto ||
      s->kind == kSyncTimerManual)
    return s->signaled != 0;
  if (s->kind == kSyncMutex) return s->owned == 0;
  if (s->kind == kSyncSem) return s->count > 0;
  return false;
}

inline void k32_sync_acquire(K32Sync* s) {
  if (s->kind == kSyncEventAuto || s->kind == kSyncTimerAuto) s->signaled = 0;
  else if (s->kind == kSyncMutex) s->owned = 1;
  else if (s->kind == kSyncSem && s->count > 0) s->count--;
}

inline int k32_peek(int h) {
  if (K32Proc* p = k32_proc(h)) {
    std::lock_guard<std::mutex> lk(p->mu);
    return p->done ? 0 : 258;
  }
  if (K32Sync* s = k32_sync(h)) {
    std::lock_guard<std::mutex> lk(s->mu);
    return k32_sync_ready(s) ? 0 : 258;
  }
  if (K32Thr* t = k32_thr(h)) {
    std::lock_guard<std::mutex> lk(t->mu);
    return t->done ? 0 : 258;
  }
  return -1;
}

inline int k32_wait_one(int h, unsigned long long ms) {
  if (K32Proc* p = k32_proc(h)) {
    std::unique_lock<std::mutex> lk(p->mu);
    if (p->done) return 0;
    if (ms == 0) return 258;
    if (p->vthread && k32_run_until()) {
      lk.unlock();
      k32_run_until()(&p->done);
      return p->done ? 0 : 258;
    }
    if (ms == 0xffffffffull) {
      p->cv.wait(lk, [p] { return p->done; });
      return 0;
    }
    return p->cv.wait_for(lk, std::chrono::milliseconds(ms), [p] { return p->done; }) ? 0 : 258;
  }
  if (K32Sync* s = k32_sync(h)) {
    std::unique_lock<std::mutex> lk(s->mu);
    if (k32_sync_ready(s)) {
      k32_sync_acquire(s);
      return 0;
    }
    if (ms == 0) return 258;
    auto pred = [s] { return k32_sync_ready(s); };
    bool ok = (ms == 0xffffffffull)
                  ? (s->cv.wait(lk, pred), true)
                  : s->cv.wait_for(lk, std::chrono::milliseconds(ms), pred);
    if (!ok) return 258;
    k32_sync_acquire(s);
    return 0;
  }
  if (K32Thr* t = k32_thr(h)) {
    std::unique_lock<std::mutex> lk(t->mu);
    if (t->done) return 0;
    if (ms == 0) return 258;
    if (ms == 0xffffffffull) {
      t->cv.wait(lk, [t] { return t->done; });
      return 0;
    }
    return t->cv.wait_for(lk, std::chrono::milliseconds(ms), [t] { return t->done; }) ? 0 : 258;
  }
  return -1;
}

inline bool k32_close_sync(int h) {
  K32Sync* s = k32_sync(h);
  if (!s) return false;
  s->used = 0;
  s->kind = 0;
  s->name.clear();
  s->timer_gen++;
  s->cv.notify_all();
  return true;
}

inline int k32_find_sync_name(const std::string& name, int kind_a, int kind_b) {
  if (name.empty()) return -1;
  K32Sync* t = k32_syncs();
  for (int i = 0; i < kK32Max; ++i) {
    if (t[i].used && t[i].name == name && t[i].kind >= kind_a && t[i].kind <= kind_b)
      return kK32SyncBase + i;
  }
  return -1;
}

inline bool k32_close_thr(int h) {
  K32Thr* t = k32_thr(h);
  if (!t) return false;
  if (t->thr.joinable()) t->thr.join();
  t->used = 0;
  t->done = false;
  t->exit_code = 259;
  return true;
}

struct K32Map {
  char* p = nullptr;
  size_t n = 0;
  int used = 0;
};

inline K32Map* k32_maps() {
  static K32Map t[kK32Max];
  return t;
}

inline int k32_alloc_map(size_t n) {
  K32Map* t = k32_maps();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      size_t bytes = n ? n : 4096;
      t[i].p = static_cast<char*>(std::malloc(bytes));
      if (!t[i].p) return -1;
      std::memset(t[i].p, 0, bytes);
      t[i].n = bytes;
      t[i].used = 1;
      return kK32MapBase + i;
    }
  }
  return -1;
}

inline K32Map* k32_map(int h) {
  int i = h - kK32MapBase;
  if (i < 0 || i >= kK32Max || !k32_maps()[i].used) return nullptr;
  return &k32_maps()[i];
}

inline bool k32_close_map(int h) {
  K32Map* m = k32_map(h);
  if (!m) return false;
  std::free(m->p);
  m->p = nullptr;
  m->n = 0;
  m->used = 0;
  return true;
}

struct K32Iocp {
  std::mutex mu;
  std::condition_variable cv;
  std::deque<std::string> q;
  int used = 0;
};

inline K32Iocp* k32_iocps() {
  static K32Iocp t[kK32Max];
  return t;
}

inline int k32_alloc_iocp() {
  K32Iocp* t = k32_iocps();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].q.clear();
      return kK32IocpBase + i;
    }
  }
  return -1;
}

inline K32Iocp* k32_iocp(int h) {
  int i = h - kK32IocpBase;
  if (i < 0 || i >= kK32Max || !k32_iocps()[i].used) return nullptr;
  return &k32_iocps()[i];
}

inline bool k32_close_iocp(int h) {
  K32Iocp* p = k32_iocp(h);
  if (!p) return false;
  p->used = 0;
  p->q.clear();
  p->cv.notify_all();
  return true;
}

inline void k32_file_complete(int h, unsigned n) {
  if (h < 3 || h >= kK32Max || k32_files()[h].used != 1) return;
  k32_files()[h].last_io = n;
  if (k32_files()[h].iocp && (k32_files()[h].comp_modes & 1u) == 0) {
    if (K32Iocp* p = k32_iocp(k32_files()[h].iocp)) {
      std::lock_guard<std::mutex> lk(p->mu);
      p->q.push_back(std::to_string(n) + "\x1f" + std::to_string(h));
      p->cv.notify_one();
    }
  }
}

struct K32Job {
  int used = 0;
  std::string name;
  std::vector<unsigned> pids;
};

inline K32Job* k32_jobs() {
  static K32Job t[kK32Max];
  return t;
}

inline int k32_alloc_job() {
  K32Job* t = k32_jobs();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].name.clear();
      t[i].pids.clear();
      return kK32JobBase + i;
    }
  }
  return -1;
}

inline K32Job* k32_job(int h) {
  int i = h - kK32JobBase;
  if (i < 0 || i >= kK32Max || !k32_jobs()[i].used) return nullptr;
  return &k32_jobs()[i];
}

inline bool k32_close_job(int h) {
  K32Job* p = k32_job(h);
  if (!p) return false;
  p->used = 0;
  p->name.clear();
  p->pids.clear();
  return true;
}

struct K32Vol {
  int used = 0;
  int kind = 0;
  std::string name;
  int next = 0;
};

inline K32Vol* k32_vols() {
  static K32Vol t[kK32Max];
  return t;
}

inline int k32_alloc_vol(int kind, const std::string& name) {
  K32Vol* t = k32_vols();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].kind = kind;
      t[i].name = name;
      t[i].next = 0;
      return kK32VolBase + i;
    }
  }
  return -1;
}

inline K32Vol* k32_vol(int h) {
  int i = h - kK32VolBase;
  if (i < 0 || i >= kK32Max || !k32_vols()[i].used) return nullptr;
  return &k32_vols()[i];
}

inline bool k32_close_vol(int h) {
  K32Vol* p = k32_vol(h);
  if (!p) return false;
  p->used = 0;
  p->kind = 0;
  p->name.clear();
  p->next = 0;
  return true;
}

enum {
  kMiscTp = 1,
  kMiscWork = 2,
  kMiscTq = 3,
  kMiscTqTimer = 4,
  kMiscFiber = 5,
  kMiscMail = 6,
  kMiscWait = 7,
  kMiscBarrier = 8,
  kMiscAttr = 9,
  kMiscRes = 10,
  kMiscDev = 11,
  kMiscPdh = 12,
  kMiscEvt = 13,
  kMiscObjDir = 14,
  kMiscSym = 15,
  kMiscScm = 16,
  kMiscSvc = 17
};

struct K32Misc {
  int used = 0;
  int kind = 0;
  int count = 0;
  int arrived = 0;
  bool done = false;
  std::mutex mu;
  std::condition_variable cv;
  std::string name;
  int mod = 0;
  unsigned rva = 0;
  unsigned n = 0;
};

inline K32Misc* k32_miscs() {
  static K32Misc t[kK32Max];
  return t;
}

inline int k32_alloc_misc(int kind) {
  K32Misc* t = k32_miscs();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].kind = kind;
      t[i].count = 0;
      t[i].arrived = 0;
      t[i].done = false;
      t[i].name.clear();
      t[i].mod = 0;
      t[i].rva = 0;
      t[i].n = 0;
      return kK32MiscBase + i;
    }
  }
  return -1;
}

inline K32Misc* k32_misc(int h) {
  int i = h - kK32MiscBase;
  if (i < 0 || i >= kK32Max || !k32_miscs()[i].used) return nullptr;
  return &k32_miscs()[i];
}

inline bool k32_close_misc(int h) {
  K32Misc* p = k32_misc(h);
  if (!p) return false;
  {
    std::lock_guard<std::mutex> lk(p->mu);
    p->used = 0;
    p->kind = 0;
    p->done = true;
    p->name.clear();
  }
  p->cv.notify_all();
  return true;
}

inline int& k32_fiber_self() {
  static int h = 0;
  return h;
}

struct K32RegVal {
  unsigned type = 1;
  std::string data;
};

inline std::map<std::string, std::map<std::string, K32RegVal>>& k32_reg_vals() {
  static std::map<std::string, std::map<std::string, K32RegVal>> m;
  return m;
}

inline std::map<std::string, std::map<std::string, char>>& k32_reg_subs() {
  static std::map<std::string, std::map<std::string, char>> m;
  return m;
}

struct K32Reg {
  int used = 0;
  std::string path;
};

inline K32Reg* k32_regs() {
  static K32Reg t[kK32Max];
  return t;
}

inline int k32_alloc_reg(const std::string& path) {
  K32Reg* t = k32_regs();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].path = path;
      return kK32RegBase + i;
    }
  }
  return -1;
}

inline K32Reg* k32_reg(int h) {
  int i = h - kK32RegBase;
  if (i < 0 || i >= kK32Max || !k32_regs()[i].used) return nullptr;
  return &k32_regs()[i];
}

inline bool k32_close_reg(int h) {
  K32Reg* p = k32_reg(h);
  if (!p) return false;
  p->used = 0;
  p->path.clear();
  return true;
}

enum { kTokToken = 1, kTokSid = 2 };

struct K32Tok {
  int used = 0;
  int kind = 0;
  int type = 1;
  int se_debug = 0;
  int se_load_driver = 1;
  int session = 0;
  int impersonation_level = 2;
  std::string sid;
  std::string account;
  std::string domain;
  std::vector<std::string> groups;
};

inline K32Tok* k32_toks() {
  static K32Tok t[kK32Max];
  return t;
}

inline void k32_sid_lookup(const std::string& sid, std::string* acc, std::string* dom) {
  if (sid == "S-1-5-18") {
    *acc = "SYSTEM";
    *dom = "NT AUTHORITY";
    return;
  }
  if (sid == "S-1-5-19") {
    *acc = "LOCAL SERVICE";
    *dom = "NT AUTHORITY";
    return;
  }
  if (sid == "S-1-5-20") {
    *acc = "NETWORK SERVICE";
    *dom = "NT AUTHORITY";
    return;
  }
  if (sid == "S-1-1-0") {
    *acc = "Everyone";
    *dom = "";
    return;
  }
  if (sid == "S-1-5-11") {
    *acc = "Authenticated Users";
    *dom = "NT AUTHORITY";
    return;
  }
  if (sid == "S-1-5-32-544") {
    *acc = "Administrators";
    *dom = "BUILTIN";
    return;
  }
  *acc = "wasigocvm";
  *dom = "WASMWIN32";
}

inline int k32_alloc_tok(int kind) {
  K32Tok* t = k32_toks();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Tok{};
      t[i].used = 1;
      t[i].kind = kind;
      t[i].type = 1;
      t[i].se_load_driver = 1;
      t[i].sid = "S-1-5-21-1337-1-1-1000";
      t[i].account = "wasigocvm";
      t[i].domain = "WASMWIN32";
      t[i].groups = {"S-1-1-0", "S-1-5-11", "S-1-5-21-1337-1-1-1000"};
      k32_sid_lookup(t[i].sid, &t[i].account, &t[i].domain);
      if (kind == kTokSid) {
        t[i].groups.clear();
      }
      return kK32TokBase + i;
    }
  }
  return -1;
}

inline int& k32_cur_imp() {
  static int h = 0;
  return h;
}

inline int k32_thr_imp(const char* th) {
  if (th && th[0] && !eq(th, "-2")) {
    if (K32Thr* t = k32_thr(static_cast<int>(std::strtol(th, nullptr, 10)))) return t->token;
  }
  return k32_cur_imp();
}

inline void k32_set_thr_imp(const char* th, int tok) {
  if (th && th[0] && !eq(th, "-2")) {
    if (K32Thr* t = k32_thr(static_cast<int>(std::strtol(th, nullptr, 10)))) {
      t->token = tok;
      return;
    }
  }
  k32_cur_imp() = tok;
}

inline K32Tok* k32_tok(int h) {
  int i = h - kK32TokBase;
  if (i < 0 || i >= kK32Max || !k32_toks()[i].used) return nullptr;
  return &k32_toks()[i];
}

inline bool k32_close_tok(int h) {
  K32Tok* p = k32_tok(h);
  if (!p) return false;
  *p = K32Tok{};
  return true;
}

inline int k32_dup_tok(int src) {
  K32Tok* t = k32_tok(src);
  if (!t || t->kind != kTokToken) return -1;
  int h = k32_alloc_tok(kTokToken);
  if (h < 0) return -1;
  K32Tok* d = k32_tok(h);
  *d = *t;
  d->used = 1;
  return h;
}

inline std::string k32_sid_of_arg(const char* a) {
  if (!a || !a[0]) return {};
  if (a[0] == 'S' && a[1] == '-') return a;
  if (K32Tok* t = k32_tok(static_cast<int>(std::strtol(a, nullptr, 10)))) return t->sid;
  return a;
}

inline std::map<uintptr_t, size_t>& k32_heap_sz();

struct K32Vmem {
  size_t n = 0;
  unsigned prot = 4;
};

inline std::map<uintptr_t, K32Vmem>& k32_vmem() {
  static std::map<uintptr_t, K32Vmem> m;
  return m;
}

inline void k32_vmem_add(void* p, size_t n, unsigned prot) {
  if (!p) return;
  K32Vmem v;
  v.n = n ? n : 1;
  v.prot = prot ? prot : 4u;
  uintptr_t k = reinterpret_cast<uintptr_t>(p);
  k32_vmem()[k] = v;
  k32_heap_sz()[k] = v.n;
}

inline K32Vmem* k32_vmem_at(uintptr_t p, uintptr_t* base = nullptr) {
  auto& m = k32_vmem();
  auto it = m.find(p);
  if (it != m.end()) {
    if (base) *base = p;
    return &it->second;
  }
  for (auto& kv : m) {
    if (p >= kv.first && p < kv.first + kv.second.n) {
      if (base) *base = kv.first;
      return &kv.second;
    }
  }
  auto hit = k32_heap_sz().find(p);
  if (hit != k32_heap_sz().end()) {
    if (base) *base = p;
    static K32Vmem tmp;
    tmp.n = hit->second;
    tmp.prot = 4;
    return &tmp;
  }
  return nullptr;
}

inline bool k32_is_self_proc(int h) {
  return h == -1 || h == 0;
}

inline bool k32_proc_ok(int h) { return k32_is_self_proc(h) || k32_proc(h) != nullptr; }

struct K32Sock {
  std::mutex mu;
  std::condition_variable cv;
  int used = 0;
  int listening = 0;
  int nonblock = 0;
  int peer = 0;
  std::string name;
  std::deque<std::string> inbox;
  std::deque<int> accept_q;
};

inline K32Sock* k32_socks() {
  static K32Sock t[kK32Max];
  return t;
}

inline int k32_alloc_sock() {
  K32Sock* t = k32_socks();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].listening = 0;
      t[i].nonblock = 0;
      t[i].peer = 0;
      t[i].name.clear();
      t[i].inbox.clear();
      t[i].accept_q.clear();
      return kK32SockBase + i;
    }
  }
  return -1;
}

inline K32Sock* k32_sock(int h) {
  int i = h - kK32SockBase;
  if (i < 0 || i >= kK32Max || !k32_socks()[i].used) return nullptr;
  return &k32_socks()[i];
}

inline bool k32_close_sock(int h) {
  K32Sock* s = k32_sock(h);
  if (!s) return false;
  int peer = 0;
  {
    std::lock_guard<std::mutex> lk(s->mu);
    peer = s->peer;
    s->used = 0;
    s->listening = 0;
    s->peer = 0;
    s->inbox.clear();
    s->accept_q.clear();
    s->name.clear();
  }
  s->cv.notify_all();
  if (peer) {
    if (K32Sock* p = k32_sock(peer)) {
      {
        std::lock_guard<std::mutex> lk(p->mu);
        if (p->peer == h) p->peer = 0;
      }
      p->cv.notify_all();
    }
  }
  return true;
}

inline int k32_find_listen(const std::string& name) {
  if (name.empty()) return -1;
  K32Sock* t = k32_socks();
  for (int i = 0; i < kK32Max; ++i) {
    if (t[i].used && t[i].listening && t[i].name == name) return kK32SockBase + i;
  }
  return -1;
}

inline bool k32_reg_ieq(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    unsigned char x = static_cast<unsigned char>(*a++);
    unsigned char y = static_cast<unsigned char>(*b++);
    if (x >= 'a' && x <= 'z') x = static_cast<unsigned char>(x - 'a' + 'A');
    if (y >= 'a' && y <= 'z') y = static_cast<unsigned char>(y - 'a' + 'A');
    if (x != y) return false;
  }
  return *a == *b;
}

inline std::string k32_reg_join(const std::string& root, const std::string& sub) {
  std::string s = sub;
  for (char& c : s) {
    if (c == '/') c = '\\';
  }
  while (!s.empty() && s.front() == '\\') s.erase(s.begin());
  while (!s.empty() && s.back() == '\\') s.pop_back();
  if (s.empty()) return root;
  if (root.empty()) return s;
  return root + "\\" + s;
}

inline void k32_reg_ensure(const std::string& path) {
  k32_reg_vals()["HKCU"];
  k32_reg_vals()["HKLM"];
  k32_reg_vals()["HKCR"];
  k32_reg_vals()["HKU"];
  k32_reg_vals()["HKCC"];
  if (path.empty()) return;
  std::string cur;
  std::string rest = path;
  while (!rest.empty()) {
    auto p = rest.find('\\');
    std::string part = p == std::string::npos ? rest : rest.substr(0, p);
    if (p == std::string::npos) rest.clear();
    else rest = rest.substr(p + 1);
    if (part.empty()) continue;
    if (!cur.empty()) k32_reg_subs()[cur][part] = 1;
    cur = cur.empty() ? part : cur + "\\" + part;
    k32_reg_vals()[cur];
  }
}

inline std::string k32_reg_path_of(const char* s) {
  if (!s || !s[0] || k32_reg_ieq(s, "HKCU") || k32_reg_ieq(s, "HKEY_CURRENT_USER") ||
      std::strcmp(s, "2147483649") == 0 || std::strcmp(s, "-2147483647") == 0)
    return "HKCU";
  if (k32_reg_ieq(s, "HKLM") || k32_reg_ieq(s, "HKEY_LOCAL_MACHINE") ||
      std::strcmp(s, "2147483650") == 0 || std::strcmp(s, "-2147483646") == 0)
    return "HKLM";
  if (k32_reg_ieq(s, "HKCR") || k32_reg_ieq(s, "HKEY_CLASSES_ROOT") ||
      std::strcmp(s, "2147483648") == 0)
    return "HKCR";
  if (k32_reg_ieq(s, "HKU") || k32_reg_ieq(s, "HKEY_USERS") || std::strcmp(s, "2147483651") == 0)
    return "HKU";
  if (k32_reg_ieq(s, "HKCC") || k32_reg_ieq(s, "HKEY_CURRENT_CONFIG") ||
      std::strcmp(s, "2147483653") == 0)
    return "HKCC";
  int h = static_cast<int>(std::strtol(s, nullptr, 10));
  if (K32Reg* r = k32_reg(h)) return r->path;
  return s;
}

inline void k32_reg_delete_tree(const std::string& path) {
  if (path.empty() || path == "HKCU" || path == "HKLM" || path == "HKCR" || path == "HKU" ||
      path == "HKCC") {
    k32_reg_vals()[path].clear();
    k32_reg_subs()[path].clear();
    return;
  }
  std::string prefix = path + "\\";
  auto& vals = k32_reg_vals();
  auto& subs = k32_reg_subs();
  for (auto it = vals.begin(); it != vals.end();) {
    if (it->first == path || it->first.rfind(prefix, 0) == 0) it = vals.erase(it);
    else ++it;
  }
  for (auto it = subs.begin(); it != subs.end();) {
    if (it->first == path || it->first.rfind(prefix, 0) == 0) it = subs.erase(it);
    else ++it;
  }
  auto slash = path.rfind('\\');
  if (slash != std::string::npos) {
    std::string parent = path.substr(0, slash);
    std::string name = path.substr(slash + 1);
    subs[parent].erase(name);
  }
}

struct K32Cs {
  std::mutex mu;
};

struct K32Srw {
  std::mutex mu;
};

struct K32Cv {
  std::condition_variable cv;
};

inline std::map<std::string, std::string>& k32_ini() {
  static std::map<std::string, std::string> m;
  return m;
}

inline std::string& k32_dll_dir() {
  static std::string d;
  return d;
}

inline std::string* k32_atoms() {
  static std::string a[256];
  return a;
}

inline unsigned& k32_error_mode() {
  static unsigned m = 0;
  return m;
}

inline std::map<uintptr_t, size_t>& k32_heap_sz() {
  static std::map<uintptr_t, size_t> m;
  return m;
}

inline uintptr_t k32_process_heap() {
  static void* h = nullptr;
  if (!h) {
    h = std::malloc(4096);
    if (h) {
      std::memset(h, 0, 4096);
      k32_vmem_add(h, 4096, 4);
    }
  }
  return reinterpret_cast<uintptr_t>(h);
}

inline std::string k32_search_path(const char* a) {
  std::string path, rest, file, ext;
  split1f(a, &path, &rest);
  split1f(rest.c_str(), &file, &ext);
  if (file.empty()) {
    file = path;
    path.clear();
  }
  if (file.empty()) return err_msg("SearchPathW: empty name");
  auto exists = [](const std::string& p) {
    struct ::stat st {};
    return ::stat(p.c_str(), &st) == 0;
  };
  if (file.find('/') != std::string::npos || file.find('\\') != std::string::npos) {
    if (exists(file)) return file;
    if (!ext.empty() && exists(file + ext)) return file + ext;
    return err_msg("SearchPathW: not found");
  }
  std::string search = path;
  if (search.empty()) {
    if (const char* e = std::getenv("PATH")) search = e;
  }
  std::string cur;
  auto try_dir = [&](const std::string& dir) {
    if (dir.empty()) return false;
    std::string p = dir;
    if (p.back() != '/' && p.back() != '\\') p += '/';
    p += file;
    if (exists(p)) {
      cur = p;
      return true;
    }
    if (!ext.empty() && exists(p + ext)) {
      cur = p + ext;
      return true;
    }
    return false;
  };
  if (try_dir(".")) return cur;
  std::string acc;
  for (char ch : search) {
    if (ch == ':' || ch == ';') {
      if (try_dir(acc)) return cur;
      acc.clear();
    } else {
      acc.push_back(ch);
    }
  }
  if (try_dir(acc)) return cur;
  return err_msg("SearchPathW: not found");
}

inline unsigned long long unix_to_ft(time_t sec) {
  return static_cast<unsigned long long>(sec) * 10000000ull + 116444736000000000ull;
}

inline std::string ft_to_dos(unsigned long long ft) {
  unsigned long long unix100 = ft > 116444736000000000ull ? ft - 116444736000000000ull : 0;
  time_t sec = static_cast<time_t>(unix100 / 10000000ull);
  struct tm t {};
#if defined(_WIN32)
  gmtime_s(&t, &sec);
#else
  gmtime_r(&sec, &t);
#endif
  unsigned date = static_cast<unsigned>(((t.tm_year + 1900 - 1980) << 9) | ((t.tm_mon + 1) << 5) |
                                        t.tm_mday);
  unsigned tm = static_cast<unsigned>((t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec / 2));
  return std::to_string(date) + "\x1f" + std::to_string(tm);
}

inline unsigned long long dos_to_ft(unsigned date, unsigned tm) {
  int y = 1980 + static_cast<int>((date >> 9) & 0x7f);
  unsigned m = (date >> 5) & 0xfu;
  unsigned d = date & 0x1fu;
  if (m < 1) m = 1;
  if (m > 12) m = 12;
  if (d < 1) d = 1;
  int yy = y - (m <= 2 ? 1 : 0);
  int era = (yy >= 0 ? yy : yy - 399) / 400;
  unsigned yoe = static_cast<unsigned>(yy - era * 400);
  unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long long days = static_cast<long long>(era) * 146097 + static_cast<long long>(doe) - 719468;
  unsigned hour = (tm >> 11) & 0x1f;
  unsigned min = (tm >> 5) & 0x3f;
  unsigned sec = (tm & 0x1f) * 2u;
  long long unix_sec = days * 86400ll + static_cast<long long>(hour) * 3600 +
                       static_cast<long long>(min) * 60 + static_cast<long long>(sec);
  if (unix_sec < 0) unix_sec = 0;
  return unix_to_ft(static_cast<time_t>(unix_sec));
}

inline std::string& k32_cur_thr_desc() {
  static std::string s;
  return s;
}

inline void k32_put32(char* p, unsigned v) {
  p[0] = static_cast<char>(v & 0xff);
  p[1] = static_cast<char>((v >> 8) & 0xff);
  p[2] = static_cast<char>((v >> 16) & 0xff);
  p[3] = static_cast<char>((v >> 24) & 0xff);
}

inline void k32_put16(char* p, unsigned v) {
  p[0] = static_cast<char>(v & 0xff);
  p[1] = static_cast<char>((v >> 8) & 0xff);
}

struct K32Peb {
  char* peb = nullptr;
  char* teb = nullptr;
  char* params = nullptr;
  char* ldr = nullptr;
  char* tls = nullptr;
  char* cmdline = nullptr;
  size_t peb_n = 256;
  size_t teb_n = 256;
};

inline K32Peb& k32_peb() {
  static K32Peb p;
  return p;
}

inline void k32_peb_boot() {
  K32Peb& p = k32_peb();
  if (p.peb) return;
  p.peb = static_cast<char*>(std::malloc(256));
  p.teb = static_cast<char*>(std::malloc(256));
  p.params = static_cast<char*>(std::malloc(512));
  p.ldr = static_cast<char*>(std::malloc(128));
  p.tls = static_cast<char*>(std::malloc(256));
  std::memset(p.peb, 0, 256);
  std::memset(p.teb, 0, 256);
  std::memset(p.params, 0, 512);
  std::memset(p.ldr, 0, 128);
  std::memset(p.tls, 0, 256);
  p.peb[2] = 0;
  k32_put32(p.peb + 0x0c, static_cast<unsigned>(reinterpret_cast<uintptr_t>(p.ldr)));
  k32_put32(p.peb + 0x10, static_cast<unsigned>(reinterpret_cast<uintptr_t>(p.params)));
  k32_put32(p.teb + 0x18, static_cast<unsigned>(reinterpret_cast<uintptr_t>(p.teb)));
  k32_put32(p.teb + 0x2c, static_cast<unsigned>(reinterpret_cast<uintptr_t>(p.tls)));
  k32_put32(p.teb + 0x30, static_cast<unsigned>(reinterpret_cast<uintptr_t>(p.peb)));
  std::string cmd = "main.wasm";
  if (const char* e = std::getenv("_")) {
    if (e[0]) cmd = e;
  }
  p.cmdline = static_cast<char*>(std::malloc(cmd.size() * 2 + 4));
  std::memset(p.cmdline, 0, cmd.size() * 2 + 4);
  for (size_t i = 0; i < cmd.size(); ++i) p.cmdline[i * 2] = cmd[i];
  unsigned nbytes = static_cast<unsigned>(cmd.size() * 2);
  k32_put16(p.params + 0x40, nbytes);
  k32_put16(p.params + 0x42, nbytes + 2);
  k32_put32(p.params + 0x44, static_cast<unsigned>(reinterpret_cast<uintptr_t>(p.cmdline)));
  k32_vmem_add(p.peb, 256, 4);
  k32_vmem_add(p.teb, 256, 4);
  k32_vmem_add(p.params, 512, 4);
  k32_vmem_add(p.tls, 256, 4);
}

inline std::vector<unsigned>& k32_veh() {
  static std::vector<unsigned> v;
  return v;
}

inline unsigned& k32_veh_next() {
  static unsigned n = 1;
  return n;
}

enum { kCngAlg = 1, kCngHash = 2, kCngKey = 3, kCngProv = 4 };

struct K32Cng {
  int used = 0;
  int kind = 0;
  std::string alg;
  std::string acc;
  std::string key;
};

inline K32Cng* k32_cngs() {
  static K32Cng t[kK32Max];
  return t;
}

inline int k32_alloc_cng(int kind) {
  K32Cng* t = k32_cngs();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Cng{};
      t[i].used = 1;
      t[i].kind = kind;
      return kK32CngBase + i;
    }
  }
  return -1;
}

inline K32Cng* k32_cng(int h) {
  int i = h - kK32CngBase;
  if (i < 0 || i >= kK32Max || !k32_cngs()[i].used) return nullptr;
  return &k32_cngs()[i];
}

inline bool k32_close_cng(int h) {
  K32Cng* p = k32_cng(h);
  if (!p) return false;
  *p = K32Cng{};
  return true;
}

enum { kCertStore = 1, kCertCtx = 2 };

struct K32Cert {
  int used = 0;
  int kind = 0;
  std::string name;
  std::string blob;
};

inline K32Cert* k32_certs() {
  static K32Cert t[kK32Max];
  return t;
}

inline int k32_alloc_cert(int kind) {
  K32Cert* t = k32_certs();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Cert{};
      t[i].used = 1;
      t[i].kind = kind;
      return kK32CertBase + i;
    }
  }
  return -1;
}

inline K32Cert* k32_cert(int h) {
  int i = h - kK32CertBase;
  if (i < 0 || i >= kK32Max || !k32_certs()[i].used) return nullptr;
  return &k32_certs()[i];
}

inline bool k32_close_cert(int h) {
  K32Cert* p = k32_cert(h);
  if (!p) return false;
  *p = K32Cert{};
  return true;
}

enum { kHttpSess = 1, kHttpConn = 2, kHttpReq = 3 };

struct K32Http {
  int used = 0;
  int kind = 0;
  int parent = 0;
  unsigned port = 80;
  int sent = 0;
  unsigned off = 0;
  std::string agent;
  std::string host;
  std::string verb;
  std::string path;
  std::string headers;
  std::string body;
};

inline K32Http* k32_https() {
  static K32Http t[kK32Max];
  return t;
}

inline int k32_alloc_http(int kind) {
  K32Http* t = k32_https();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Http{};
      t[i].used = 1;
      t[i].kind = kind;
      return kK32HttpBase + i;
    }
  }
  return -1;
}

inline K32Http* k32_http(int h) {
  int i = h - kK32HttpBase;
  if (i < 0 || i >= kK32Max || !k32_https()[i].used) return nullptr;
  return &k32_https()[i];
}

inline bool k32_close_http(int h) {
  K32Http* p = k32_http(h);
  if (!p) return false;
  *p = K32Http{};
  return true;
}

enum { kWhpPart = 1, kWhpVp = 2, kWhpVpci = 3, kWhpTrig = 4, kWhpPort = 5, kWhpEmu = 6 };

struct K32Gpa {
  unsigned long long gpa = 0;
  unsigned long long size = 0;
  std::string bytes;
};

struct K32Seg {
  unsigned long long base = 0;
  unsigned limit = 0xffffu;
  unsigned short selector = 0;
  unsigned short attr = 0;
};

struct K32Tab {
  unsigned long long base = 0;
  unsigned short limit = 0;
};

struct K32Whp {
  int used = 0;
  int kind = 0;
  int parent = 0;
  unsigned vpindex = 0;
  int setup = 0;
  int running = 0;
  unsigned long long rip = 0;
  K32Seg cs, ss, ds, es, fs, gs, ldtr, tr;
  K32Tab gdtr, idtr;
  unsigned last_reason = 8;
  unsigned last_ilen = 1;
  unsigned last_port = 0;
  unsigned last_dir = 0;
  unsigned last_vec = 0;
  unsigned last_size = 1;
  unsigned long long last_gpa = 0;
  unsigned long long last_data = 0;
  std::vector<K32Gpa> gpa;
  std::map<unsigned, unsigned long long> props;
  std::map<unsigned, unsigned long long> regs;
  std::map<unsigned, unsigned> io_ports;
  std::string state;
};

inline K32Whp* k32_whps() {
  static K32Whp t[kK32Max];
  return t;
}

inline int k32_alloc_whp(int kind) {
  K32Whp* t = k32_whps();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Whp{};
      t[i].used = 1;
      t[i].kind = kind;
      if (kind == kWhpPart) t[i].props[0x1fffu] = 1;
      return kK32WhpBase + i;
    }
  }
  return -1;
}

inline K32Whp* k32_whp(int h) {
  int i = h - kK32WhpBase;
  if (i < 0 || i >= kK32Max || !k32_whps()[i].used) return nullptr;
  return &k32_whps()[i];
}

inline bool k32_close_whp(int h) {
  K32Whp* p = k32_whp(h);
  if (!p) return false;
  if (p->kind == kWhpPart) {
    K32Whp* t = k32_whps();
    for (int i = 0; i < kK32Max; ++i)
      if (t[i].used && t[i].parent == h) t[i] = K32Whp{};
  }
  *p = K32Whp{};
  return true;
}

inline unsigned long long k32_whp_next_gpa(K32Whp* part, unsigned long long minv = 0x1000000ull) {
  unsigned long long end = minv;
  if (!part) return end;
  for (const auto& g : part->gpa) {
    unsigned long long e = g.gpa + g.size;
    if (e > end) end = e;
  }
  return (end + 4095ull) & ~4095ull;
}

inline void k32_whp_map_bytes(K32Whp* part, unsigned long long gpa, const void* src, size_t n) {
  if (!part || !n) return;
  K32Gpa g;
  g.gpa = gpa & ~4095ull;
  g.size = (static_cast<unsigned long long>(n) + 4095ull) & ~4095ull;
  if (!g.size) g.size = 4096;
  g.bytes.assign(static_cast<size_t>(g.size), '\0');
  if (src) {
    size_t cpy = n < g.bytes.size() ? n : g.bytes.size();
    std::memcpy(&g.bytes[0], src, cpy);
  }
  part->gpa.push_back(std::move(g));
}

// PEB/TEB (CHPT session blobs) and process heaps into the partition GPA so
// ntoskrnl running on this hop sees the same pages, not a zeroed stand-in.
inline void k32_whp_map_session(K32Whp* part) {
  if (!part || part->kind != kWhpPart) return;
  k32_peb_boot();
  (void)k32_process_heap();
  for (const auto& kv : k32_vmem()) {
    if (!kv.first || !kv.second.n) continue;
    k32_whp_map_bytes(part, k32_whp_next_gpa(part),
                      reinterpret_cast<const void*>(kv.first), kv.second.n);
  }
}

inline K32Whp* k32_whp_vp(int part, unsigned idx) {
  K32Whp* t = k32_whps();
  for (int i = 0; i < kK32Max; ++i)
    if (t[i].used && t[i].kind == kWhpVp && t[i].parent == part && t[i].vpindex == idx)
      return &t[i];
  return nullptr;
}

inline bool k32_whp_cataloged(const char* api) {
  if (!api || std::strncmp(api, "WHv", 3) != 0) return false;
  int c = 0;
  const WasmWin32Api* cat = ::wasmwin32_catalog(&c);
  for (int i = 0; i < c; ++i)
    if (cat[i].name && eq(cat[i].name, api)) return true;
  return false;
}

inline std::string k32_whp_fold(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  const char* pfx[] = {"whvpartitionpropertycode", "whvx64register", "whvregister",
                       "propertycode", "register", nullptr};
  for (int i = 0; pfx[i]; ++i) {
    size_t n = std::strlen(pfx[i]);
    if (s.size() > n && s.compare(0, n, pfx[i]) == 0) s.erase(0, n);
  }
  return s;
}

inline unsigned long long k32_whp_u64(const std::string& s) {
  if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    return std::strtoull(s.c_str() + 2, nullptr, 16);
  return std::strtoull(s.c_str(), nullptr, 0);
}

inline unsigned k32_whp_prop(const std::string& s) {
  std::string t = k32_whp_fold(s);
  if (t == "processorcount") return 0x1fffu;
  if (t == "extendedvmexits") return 1;
  if (t == "exceptionexitbitmap") return 2;
  if (t == "separatesecuritydomain") return 3;
  if (t == "nestedvirtualization") return 4;
  if (t == "x64msrexitbitmap") return 5;
  if (t == "localapicemulationmode") return 0x1005u;
  if (t == "processorfeatures") return 0x1001u;
  if (t == "physicaladdresswidth") return 0x1011u;
  return static_cast<unsigned>(k32_whp_u64(s));
}

inline unsigned k32_whp_reg(const std::string& s) {
  std::string t = k32_whp_fold(s);
  static const char* gp[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                             "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
  for (unsigned i = 0; i < 16; ++i)
    if (t == gp[i]) return i;
  if (t == "rip") return 0x10;
  if (t == "rflags" || t == "eflags") return 0x11;
  if (t == "es") return 0x12;
  if (t == "cs") return 0x13;
  if (t == "ss") return 0x14;
  if (t == "ds") return 0x15;
  if (t == "fs") return 0x16;
  if (t == "gs") return 0x17;
  if (t == "ldtr") return 0x18;
  if (t == "tr") return 0x19;
  if (t == "idtr") return 0x1A;
  if (t == "gdtr") return 0x1B;
  if (t == "cr0") return 0x1C;
  if (t == "cr2") return 0x1D;
  if (t == "cr3") return 0x1E;
  if (t == "cr4") return 0x1F;
  if (t == "cr8") return 0x20;
  if (t == "efer") return 0x2001;
  if (t == "pat") return 0x2004;
  return static_cast<unsigned>(k32_whp_u64(s));
}

inline int k32_whp_reg_kind(unsigned r) {
  if (r >= 0x12 && r <= 0x19) return 1;
  if (r == 0x1A || r == 0x1B) return 2;
  return 0;
}

inline K32Seg* k32_whp_seg(K32Whp* vp, unsigned r) {
  if (!vp) return nullptr;
  switch (r) {
    case 0x12: return &vp->es;
    case 0x13: return &vp->cs;
    case 0x14: return &vp->ss;
    case 0x15: return &vp->ds;
    case 0x16: return &vp->fs;
    case 0x17: return &vp->gs;
    case 0x18: return &vp->ldtr;
    case 0x19: return &vp->tr;
    default: return nullptr;
  }
}

inline K32Tab* k32_whp_tab(K32Whp* vp, unsigned r) {
  if (!vp) return nullptr;
  if (r == 0x1A) return &vp->idtr;
  if (r == 0x1B) return &vp->gdtr;
  return nullptr;
}

inline const char* k32_whp_exit_name(unsigned r) {
  switch (r) {
    case 0: return "none";
    case 1: return "memory";
    case 2: return "io";
    case 4: return "unrecoverable";
    case 5: return "invalidvp";
    case 6: return "unsupported";
    case 7: return "interruptwindow";
    case 8: return "halt";
    case 9: return "apiceoi";
    case 0xA: return "synic";
    case 0x1000: return "msr";
    case 0x1001: return "cpuid";
    case 0x1002: return "exception";
    case 0x1003: return "rdtsc";
    case 0x2001: return "canceled";
    default: return "exit";
  }
}

inline std::string k32_whp_exit_str(unsigned reason, unsigned long long rip, const std::string& extra) {
  std::string s = std::to_string(reason);
  s += "\x1f";
  s += k32_whp_exit_name(reason);
  s += "\x1f";
  s += std::to_string(rip);
  if (!extra.empty()) {
    s += "\x1f";
    s += extra;
  }
  return s;
}

inline K32Gpa* k32_whp_gpa_at(K32Whp* part, unsigned long long gpa) {
  if (!part) return nullptr;
  for (auto& g : part->gpa)
    if (gpa >= g.gpa && gpa < g.gpa + g.size) return &g;
  return nullptr;
}

inline int k32_whp_fetch(K32Whp* part, unsigned long long gpa, void* dst, size_t n) {
  K32Gpa* g = k32_whp_gpa_at(part, gpa);
  if (!g || !dst || !n) return 0;
  size_t off = static_cast<size_t>(gpa - g->gpa);
  if (off >= g->bytes.size()) return 0;
  if (off + n > g->bytes.size()) n = g->bytes.size() - off;
  std::memcpy(dst, g->bytes.data() + off, n);
  return static_cast<int>(n);
}

inline int k32_whp_store(K32Whp* part, unsigned long long gpa, const void* src, size_t n) {
  K32Gpa* g = k32_whp_gpa_at(part, gpa);
  if (!g || !src || !n) return 0;
  size_t off = static_cast<size_t>(gpa - g->gpa);
  if (off >= g->bytes.size()) return 0;
  if (off + n > g->bytes.size()) n = g->bytes.size() - off;
  std::memcpy(&g->bytes[off], src, n);
  return static_cast<int>(n);
}

inline unsigned long long k32_whp_laddr(K32Whp* vp, unsigned long long off) {
  if (!vp) return off;
  if (!vp->cs.attr && !vp->cs.base) return off;
  if (vp->cs.attr & (1u << 13)) return off;
  unsigned long long mask = (vp->cs.attr & (1u << 14)) ? 0xffffffffull : 0xffffull;
  return vp->cs.base + (off & mask);
}

inline void k32_whp_set_seg(K32Seg* s, unsigned r, const std::string& a, const std::string& b,
                            const std::string& c, const std::string& d) {
  if (!s) return;
  auto take = [](const std::string& x, unsigned long long fallback) {
    return x.empty() ? fallback : k32_whp_u64(x);
  };
  std::string v = a;
  std::string lim = b, sel = c, attr = d;
  auto col = v.find(':');
  if (col != std::string::npos) {
    std::string parts[4];
    int n = 0;
    std::string cur;
    for (size_t i = 0; i <= v.size() && n < 4; ++i) {
      if (i == v.size() || v[i] == ':') {
        parts[n++] = cur;
        cur.clear();
      } else {
        cur.push_back(v[i]);
      }
    }
    v = parts[0];
    if (n > 1) lim = parts[1];
    if (n > 2) sel = parts[2];
    if (n > 3) attr = parts[3];
  }
  s->base = take(v, s->base);
  s->limit = static_cast<unsigned>(take(lim, s->limit));
  s->selector = static_cast<unsigned short>(take(sel, s->selector));
  unsigned long long def_attr = (r == 0x13) ? 0x9bull : 0x93ull;
  s->attr = static_cast<unsigned short>(take(attr, s->attr ? s->attr : def_attr));
}

inline std::string k32_whp_fmt_seg(const K32Seg& s) {
  return std::to_string(s.base) + "\x1f" + std::to_string(s.limit) + "\x1f" +
         std::to_string(s.selector) + "\x1f" + std::to_string(s.attr);
}

inline std::string k32_whp_fmt_tab(const K32Tab& t) {
  return std::to_string(t.base) + "\x1f" + std::to_string(t.limit);
}

inline std::string k32_whp_decode_run(K32Whp* part, K32Whp* vp) {
  if (!part || !vp) return k32_whp_exit_str(8, 0, {});
  unsigned long long rip = vp->regs.count(0x10) ? vp->regs[0x10] : vp->rip;
  vp->rip = rip;
  unsigned long long gpa = k32_whp_laddr(vp, rip);
  unsigned char op = 0;
  int n = k32_whp_fetch(part, gpa, &op, 1);
  vp->last_reason = 8;
  vp->last_ilen = 1;
  vp->last_port = 0;
  vp->last_dir = 0;
  vp->last_vec = 0;
  vp->last_size = 1;
  vp->last_gpa = gpa;
  vp->last_data = 0;
  vp->running = 0;
  if (n <= 0) return k32_whp_exit_str(8, rip, {});
  if (op == 0xF4) {
    vp->last_reason = 8;
    return k32_whp_exit_str(8, rip, {});
  }
  if (op == 0xCC) {
    vp->last_reason = 0x1002;
    vp->last_vec = 3;
    return k32_whp_exit_str(0x1002, rip, "3");
  }
  unsigned char imm = 0;
  unsigned long long rax = vp->regs[0];
  unsigned long long rdx = vp->regs[2];
  if (op == 0xE6 || op == 0xE4) {
    k32_whp_fetch(part, gpa + 1, &imm, 1);
    vp->last_reason = 2;
    vp->last_ilen = 2;
    vp->last_port = imm;
    vp->last_dir = (op == 0xE6) ? 0u : 1u;
    vp->last_data = rax & 0xffull;
    return k32_whp_exit_str(2, rip, std::to_string(imm) + "\x1f" + (op == 0xE6 ? "write" : "read"));
  }
  if (op == 0xEE || op == 0xEC) {
    vp->last_reason = 2;
    vp->last_ilen = 1;
    vp->last_port = static_cast<unsigned>(rdx & 0xffffull);
    vp->last_dir = (op == 0xEE) ? 0u : 1u;
    vp->last_data = rax & 0xffull;
    return k32_whp_exit_str(2, rip,
                            std::to_string(vp->last_port) + "\x1f" + (op == 0xEE ? "write" : "read"));
  }
  return k32_whp_exit_str(8, rip, {});
}

inline std::string k32_whp_try_io(K32Whp* part, K32Whp* vp) {
  if (!part || !vp || vp->last_reason != 2) return {};
  if (vp->last_dir == 0)
    part->io_ports[vp->last_port] = static_cast<unsigned>(vp->last_data);
  else {
    unsigned v = part->io_ports[vp->last_port];
    vp->regs[0] = (vp->regs[0] & ~0xffull) | (v & 0xffu);
  }
  vp->rip += vp->last_ilen;
  vp->regs[0x10] = vp->rip;
  return "1";
}

inline std::string k32_whp_try_mmio(K32Whp* part, K32Whp* vp) {
  if (!part || !vp || vp->last_reason != 1) return {};
  unsigned n = vp->last_size ? vp->last_size : 1;
  unsigned char buf[8]{};
  if (vp->last_dir == 0) {
    unsigned long long d = vp->last_data;
    std::memcpy(buf, &d, n > 8 ? 8 : n);
    k32_whp_store(part, vp->last_gpa, buf, n > 8 ? 8 : n);
  } else {
    k32_whp_fetch(part, vp->last_gpa, buf, n > 8 ? 8 : n);
    unsigned long long d = 0;
    std::memcpy(&d, buf, n > 8 ? 8 : n);
    vp->regs[0] = d;
  }
  vp->rip += vp->last_ilen;
  vp->regs[0x10] = vp->rip;
  return "1";
}

inline std::string k32_crack_url(const char* u) {
  std::string s = u ? u : "";
  std::string scheme = "http", host, path = "/", port;
  auto se = s.find("://");
  if (se != std::string::npos) {
    scheme = s.substr(0, se);
    s = s.substr(se + 3);
  }
  auto sl = s.find('/');
  host = sl == std::string::npos ? s : s.substr(0, sl);
  path = sl == std::string::npos ? "/" : s.substr(sl);
  auto c = host.rfind(':');
  if (c != std::string::npos && c > 0 && host[c - 1] != ']') {
    port = host.substr(c + 1);
    host = host.substr(0, c);
  } else {
    port = (scheme == "https") ? "443" : "80";
  }
  return scheme + "\x1f" + host + "\x1f" + port + "\x1f" + path;
}

inline std::string k32_mod_norm(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c == '\\') c = '/';
  }
  auto slash = s.find_last_of('/');
  if (slash != std::string::npos) s = s.substr(slash + 1);
  if (s.size() > 4 && s.compare(s.size() - 4, 4, ".dll") == 0) s.resize(s.size() - 4);
  return s;
}

inline bool k32_known_dll(const std::string& n) {
  static const char* k[] = {"kernel32", "ntdll",   "advapi32", "user32",  "gdi32",   "ole32",
                            "oleaut32", "ws2_32",  "bcrypt",   "ncrypt",  "shell32", "comctl32",
                            "comdlg32", "wslapi",  "crypt32",  "shlwapi", "winhttp", "iphlpapi",
                            "version",  "wininet", "dnsapi",   "secur32", "dbghelp", "wintrust",
                            "uxtheme",  "dwmapi",  "rpcrt4",   "setupapi",
                            "cfgmgr32", "netapi32", "pdh",     "wevtapi",
                            "winhvplatform", "winhvemulation", "ntoskrnl",
                            nullptr};
  for (int i = 0; k[i]; ++i)
    if (n == k[i]) return true;
  int c = 0;
  const WasmWin32Api* cat = ::wasmwin32_catalog(&c);
  for (int i = 0; i < c; ++i)
    if (cat[i].dll && n == cat[i].dll) return true;
  return false;
}

inline bool k32_ntdll_cataloged(const char* api) {
  if (!api || !api[0]) return false;
  int c = 0;
  const WasmWin32Api* cat = ::wasmwin32_catalog(&c);
  for (int i = 0; i < c; ++i)
    if (cat[i].dll && cat[i].name && eq(cat[i].name, api) &&
        (eq(cat[i].dll, "ntdll") || eq(cat[i].dll, "ntoskrnl")))
      return true;
  return false;
}

inline std::string k32_dos_to_nt(const char* p) {
  std::string s = p ? p : "";
  if (s.size() >= 2 && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) && s[1] == ':')
    return std::string("\\??\\") + s;
  if (s.size() >= 2 && s[0] == '\\' && s[1] == '\\') return std::string("\\??\\UNC\\") + s.substr(2);
  if (!s.empty() && (s[0] == '/' || s[0] == '\\')) return std::string("\\??\\") + s;
  std::string d = cwd();
  if (!d.empty() && d.back() != '/' && d.back() != '\\') d += '/';
  return std::string("\\??\\") + d + s;
}

struct K32Mod {
  int used = 0;
  int refs = 0;
  std::string name;
  std::string path;
  int wasm = 0;
  int pe = 0;
  void* base = nullptr;
  size_t n = 0;
  std::map<std::string, unsigned> exports;
  std::map<std::string, std::string> forwards;
  std::vector<PeRes> resources;
  std::vector<unsigned> tls_callbacks;
  unsigned entry_rva = 0;
  unsigned machine = 0;
  int dll = 0;
  int main_attached = 0;
  int no_thread_calls = 0;
  int whp_part = 0;
  unsigned nt_off = 0;
  uint32_t dirs_rva[16]{};
  uint32_t dirs_sz[16]{};
};

inline K32Mod* k32_mods() {
  static K32Mod t[kK32Max];
  return t;
}

inline K32Mod* k32_mod(int h) {
  int i = h - kK32ModBase;
  if (i < 0 || i >= kK32Max || !k32_mods()[i].used) return nullptr;
  return &k32_mods()[i];
}

inline int k32_find_mod(const std::string& n) {
  K32Mod* t = k32_mods();
  for (int i = 0; i < kK32Max; ++i)
    if (t[i].used && t[i].name == n) return kK32ModBase + i;
  return -1;
}

inline int k32_alloc_mod(const std::string& n, const std::string& path, int wasm) {
  int e = k32_find_mod(n);
  if (e >= 0) {
    k32_mod(e)->refs++;
    return e;
  }
  K32Mod* t = k32_mods();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Mod{};
      t[i].used = 1;
      t[i].refs = 1;
      t[i].name = n;
      t[i].path = path;
      t[i].wasm = wasm;
      return kK32ModBase + i;
    }
  }
  return -1;
}

inline bool k32_close_mod(int h) {
  K32Mod* p = k32_mod(h);
  if (!p) return false;
  if (--p->refs > 0) return true;
  if (p->whp_part) k32_close_whp(p->whp_part);
  if (p->pe && p->base) std::free(p->base);
  *p = K32Mod{};
  return true;
}

inline int k32_whp_ensure_mod(K32Mod* m) {
  if (!m) return -1;
  if (m->whp_part && k32_whp(m->whp_part)) return m->whp_part;
  int p = k32_alloc_whp(kWhpPart);
  if (p < 0) return -1;
  K32Whp* part = k32_whp(p);
  part->setup = 1;
  unsigned long long gpa = 4096;
  if (m->base && m->n) {
    K32Gpa g;
    g.gpa = gpa;
    g.size = (m->n + 4095ull) & ~4095ull;
    if (!g.size) g.size = 4096;
    g.bytes.assign(static_cast<size_t>(g.size), '\0');
    size_t n = m->n < g.bytes.size() ? m->n : g.bytes.size();
    std::memcpy(&g.bytes[0], m->base, n);
    part->gpa.push_back(std::move(g));
  }
  k32_whp_map_session(part);
  int v = k32_alloc_whp(kWhpVp);
  if (v >= 0) {
    K32Whp* vp = k32_whp(v);
    vp->parent = p;
    vp->vpindex = 0;
    vp->cs.base = gpa;
    vp->cs.limit = m->n ? static_cast<unsigned>(m->n > 0xffffffffull ? 0xffffffffull : m->n - 1) : 0xffffu;
    vp->cs.selector = 0;
    vp->cs.attr = 0x9b;
    vp->ss.attr = 0x93;
    vp->ds.attr = 0x93;
    vp->es.attr = 0x93;
    vp->gdtr.base = 0;
    vp->gdtr.limit = 0;
    vp->rip = m->entry_rva;
    vp->regs[0x10] = m->entry_rva;
    vp->regs[0x11] = 2;
  }
  m->whp_part = p;
  return p;
}

inline std::string k32_whp_run_at(K32Mod* m, unsigned rva) {
  int p = k32_whp_ensure_mod(m);
  if (p < 0) return {};
  K32Whp* part = k32_whp(p);
  K32Whp* vp = k32_whp_vp(p, 0);
  if (!vp) return {};
  vp->rip = rva;
  vp->regs[0x10] = rva;
  vp->running = 0;
  return k32_whp_decode_run(part, vp);
}

inline bool k32_call_main_dll(K32Mod* m, unsigned reason) {
  if (!m) return false;
  if (reason == 1) {
    if (m->main_attached) return true;
    for (unsigned rva : m->tls_callbacks) (void)k32_whp_run_at(m, rva);
    if (m->pe && m->entry_rva) (void)k32_whp_run_at(m, m->entry_rva);
    else if (m->wasm || !m->pe) (void)k32_whp_ensure_mod(m);
    m->main_attached = 1;
    return true;
  }
  if (reason == 0) {
    if (!m->main_attached) return true;
    if (m->pe && m->entry_rva) (void)k32_whp_run_at(m, m->entry_rva);
    m->main_attached = 0;
    return true;
  }
  if (m->no_thread_calls) return true;
  return true;
}

inline int k32_map_pe_file(const std::string& n, const std::string& path,
                           const unsigned char* buf, size_t sz);

inline FILE* k32_fopen_image(const char* path, std::string* opened) {
  if (!path || !path[0] || !opened) return nullptr;
  std::string base = path;
  auto sl = base.find_last_of("/\\");
  std::string leaf = sl == std::string::npos ? base : base.substr(sl + 1);
  std::string cands[] = {
      path,
      std::string(path) + ".exe",
      std::string(path) + ".dll",
      std::string("/windows/system32/") + leaf,
      std::string("/windows/system32/") + leaf + ".exe",
      std::string("/windows/system32/") + leaf + ".dll",
      std::string("C:/Windows/System32/") + leaf,
      std::string("C:/Windows/System32/") + leaf + ".exe",
      std::string("C:/Windows/System32/") + leaf + ".dll",
  };
  for (const std::string& c : cands) {
    FILE* f = std::fopen(c.c_str(), "rb");
    if (f) {
      *opened = c;
      return f;
    }
  }
  return nullptr;
}

inline std::string k32_nt_version() {
  if (const char* e = std::getenv("GOCVM_NT_VERSION")) {
    while (e && *e == ' ') ++e;
    if (e && *e) return e;
  }
  // Occupied ntdll. Not Wine 10.0.19041 and not an empty unbound.
  return "10.0.26100";
}

inline int k32_load_library(const char* path) {
  if (!path || !path[0]) return 1;
  std::string n = k32_mod_norm(path);
  if (n.empty()) return 1;
  int e = k32_find_mod(n);
  if (e >= 0) {
    k32_mod(e)->refs++;
    return e;
  }
  if (k32_known_dll(n)) return k32_alloc_mod(n, path, 0);
  std::string opened;
  FILE* f = k32_fopen_image(path, &opened);
  if (!f) return -126;
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz < 4) {
    std::fclose(f);
    return -193;
  }
  std::vector<unsigned char> buf(static_cast<size_t>(sz));
  size_t got = std::fread(buf.data(), 1, static_cast<size_t>(sz), f);
  std::fclose(f);
  buf.resize(got);
  if (buf.size() >= 4 && buf[0] == 0 && buf[1] == 'a' && buf[2] == 's' && buf[3] == 'm')
    return k32_alloc_mod(n, opened, 1);
  if (buf.size() >= 2 && buf[0] == 'M' && buf[1] == 'Z')
    return k32_map_pe_file(n, opened, buf.data(), buf.size());
  return -193;
}

inline std::string k32_gocvm_exec(const std::string& cmd) {
  if (k32_cmd_oneshot(cmd)) {
    const char* line = wsl_occupancy_line(cmd.c_str());
    while (line && *line == ' ') ++line;
    return wasi_call("WslExec", line ? line : "");
  }
  std::string img = cmd;
  if (!img.empty() && img[0] == '"') {
    auto e = img.find('"', 1);
    img = (e == std::string::npos) ? img.substr(1) : img.substr(1, e - 1);
  } else {
    auto sp = img.find(' ');
    if (sp != std::string::npos) img.resize(sp);
  }
  if (img.empty()) return err_msg("CreateProcessW: empty image");
  int h = k32_load_library(img.c_str());
  if (h < 0) return err_msg("CreateProcessW: image not found");
  K32Mod* m = k32_mod(h);
  if (!m || (!m->pe && !m->wasm)) return err_msg("CreateProcessW: not a real image");
  k32_call_main_dll(m, 1);
  std::string cls = (m && !m->name.empty()) ? m->name : img;
  wasi_call("RegisterClassW", cls.c_str());
  std::string wargs = cls;
  wargs += '\x1f';
  wargs += cls;
  wargs += '\x1f';
  wargs += "13565952";
  std::string hwnd = wasi_call("CreateWindowExW", wargs.c_str());
  if (hwnd.rfind("error:", 0) != 0 && !hwnd.empty()) {
    wasi_call("ShowWindow", (hwnd + "\x1f" + "1").c_str());
    wasi_call("SetForegroundWindow", hwnd.c_str());
  }
  std::string out = "image=";
  out += img;
  out += "\x1fwin32=CreateProcessW";
  out += "\x1fhwnd=";
  out += hwnd;
  out += "\x1fmod=";
  out += std::to_string(h);
  return out;
}

inline unsigned long long k32_proc_addr(K32Mod* m, const char* name) {
  if (!m || !name || !name[0]) return 0;
  if (m->pe && m->base) {
    auto fit = m->forwards.find(name);
    if (fit != m->forwards.end()) {
      std::string fwd = fit->second;
      auto dot = fwd.rfind('.');
      std::string dll = dot == std::string::npos ? std::string() : fwd.substr(0, dot);
      std::string fn = dot == std::string::npos ? fwd : fwd.substr(dot + 1);
      if (!fn.empty() && k32_mod_norm(dll) != m->name) {
        K32Mod fake{};
        fake.used = 1;
        fake.name = k32_mod_norm(dll);
        unsigned long long p = k32_proc_addr(&fake, fn.c_str());
        if (p) return p;
      }
    }
    auto it = m->exports.find(name);
    if (it != m->exports.end())
      return static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(m->base)) + it->second;
  }
  int c = 0;
  const WasmWin32Api* cat = ::wasmwin32_catalog(&c);
  for (int i = 0; i < c; ++i) {
    if (!cat[i].name || std::strcmp(cat[i].name, name) != 0) continue;
    if (cat[i].dll && cat[i].dll[0] && m->name != cat[i].dll && m->name != k32_mod_norm(cat[i].dll))
      continue;
    return 0x10000ull + static_cast<unsigned long long>(i);
  }
  return 0;
}

inline unsigned long long k32_pe_resolve(const char* dll, const char* name, void*) {
  K32Mod fake{};
  fake.used = 1;
  fake.name = k32_mod_norm(dll ? dll : "");
  return k32_proc_addr(&fake, name);
}

inline int k32_map_pe_file(const std::string& n, const std::string& path,
                           const unsigned char* buf, size_t sz) {
  PeMap mapped;
  if (!pe_map_image(buf, sz, &mapped, k32_pe_resolve, nullptr) || !mapped.base) return -193;
  int h = k32_alloc_mod(n, path, 0);
  K32Mod* m = k32_mod(h);
  if (!m) {
    std::free(mapped.base);
    return -1;
  }
  m->pe = 1;
  m->base = mapped.base;
  m->n = mapped.size;
  m->exports = std::move(mapped.exports);
  m->forwards = std::move(mapped.forwards);
  m->resources = std::move(mapped.resources);
  m->tls_callbacks = std::move(mapped.tls_callbacks);
  m->entry_rva = mapped.entry_rva;
  m->machine = mapped.machine;
  m->dll = mapped.dll;
  m->nt_off = mapped.nt_off;
  for (int i = 0; i < 16; ++i) {
    m->dirs_rva[i] = mapped.dirs_rva[i];
    m->dirs_sz[i] = mapped.dirs_sz[i];
  }
  k32_vmem_add(m->base, m->n, 4);
  return h;
}

enum { kGdiDc = 1, kGdiBrush = 2, kGdiPen = 3, kGdiFont = 4, kGdiBmp = 5, kGdiStock = 6,
       kGdiImg = 7 };

struct K32Gdi {
  int used = 0;
  int kind = 0;
  unsigned color = 0;
  int width = 1;
  int wnd = 0;
  int sel_brush = 0;
  int sel_pen = 0;
  int sel_font = 0;
  int sel_bmp = 0;
  int bw = 0;
  int bh = 0;
  std::string face;
  std::vector<unsigned char> bits;
};

inline K32Gdi* k32_gdis() {
  static K32Gdi t[kK32Max];
  return t;
}

inline int k32_alloc_gdi(int kind) {
  K32Gdi* t = k32_gdis();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Gdi{};
      t[i].used = 1;
      t[i].kind = kind;
      return kK32GdiBase + i;
    }
  }
  return -1;
}

inline K32Gdi* k32_gdi(int h) {
  int i = h - kK32GdiBase;
  if (i < 0 || i >= kK32Max || !k32_gdis()[i].used) return nullptr;
  return &k32_gdis()[i];
}

inline bool k32_close_gdi(int h) {
  K32Gdi* p = k32_gdi(h);
  if (!p) return false;
  *p = K32Gdi{};
  return true;
}

struct K32Wnd {
  int used = 0;
  int parent = 0;
  int style = 0;
  int vis = 0;
  int x = 0;
  int y = 0;
  int w = 640;
  int h = 480;
  unsigned long userdata = 0;
  int dc = 0;
  std::string cls;
  std::string title;
  std::deque<std::string> msgs;
};

inline K32Wnd* k32_wnds() {
  static K32Wnd t[kK32Max];
  return t;
}

inline std::mutex& k32_wnd_mu() {
  static std::mutex m;
  return m;
}

inline std::vector<std::string>& k32_wnd_classes() {
  static std::vector<std::string> v;
  return v;
}

inline int k32_alloc_wnd() {
  K32Wnd* t = k32_wnds();
  for (int i = 1; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Wnd{};
      t[i].used = 1;
      return kK32WndBase + i;
    }
  }
  return -1;
}

inline K32Wnd* k32_wnd(int h) {
  int i = h - kK32WndBase;
  if (i < 0 || i >= kK32Max || !k32_wnds()[i].used) return nullptr;
  return &k32_wnds()[i];
}

inline int k32_desktop() {
  K32Wnd* t = k32_wnds();
  if (!t[0].used) {
    t[0].used = 1;
    t[0].cls = "Desktop";
    t[0].title = "Desktop";
    t[0].w = 1024;
    t[0].h = 768;
    t[0].vis = 1;
  }
  return kK32WndBase;
}

inline bool k32_close_wnd(int h) {
  K32Wnd* p = k32_wnd(h);
  if (!p) return false;
  if (p->dc) k32_close_gdi(p->dc);
  *p = K32Wnd{};
  return true;
}

inline int& k32_fg_wnd() {
  static int h = 0;
  return h;
}

struct K32Com {
  int used = 0;
  int refs = 0;
  std::string clsid;
  std::string data;
};

inline K32Com* k32_coms() {
  static K32Com t[kK32Max];
  return t;
}

inline int& k32_com_apt() {
  static int a = 0;
  return a;
}

inline int k32_alloc_com() {
  K32Com* t = k32_coms();
  for (int i = 0; i < kK32Max; ++i) {
    if (!t[i].used) {
      t[i] = K32Com{};
      t[i].used = 1;
      t[i].refs = 1;
      return kK32ComBase + i;
    }
  }
  return -1;
}

inline K32Com* k32_com(int h) {
  int i = h - kK32ComBase;
  if (i < 0 || i >= kK32Max || !k32_coms()[i].used) return nullptr;
  return &k32_coms()[i];
}

inline bool k32_close_com(int h) {
  K32Com* p = k32_com(h);
  if (!p) return false;
  if (--p->refs > 0) return true;
  *p = K32Com{};
  return true;
}

inline void k32_sha256(const unsigned char* data, size_t n, unsigned char out[32]);

inline void k32_entropy(unsigned char* p, size_t n) {
  size_t got = 0;
#if !defined(_WIN32)
  FILE* f = std::fopen("/dev/urandom", "rb");
  if (f) {
    got = std::fread(p, 1, n, f);
    std::fclose(f);
  }
#endif
  unsigned long long mix = static_cast<unsigned long long>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  mix ^= static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(p));
  for (size_t i = got; i < n; ++i) {
    mix = mix * 6364136223846793005ull + 1;
    p[i] = static_cast<unsigned char>(mix >> 33);
  }
}

static const unsigned kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline unsigned k32_rotr(unsigned x, int n) { return (x >> n) | (x << (32 - n)); }

inline void k32_sha256(const unsigned char* data, size_t n, unsigned char out[32]) {
  unsigned h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  unsigned char blk[64];
  unsigned long long bits = static_cast<unsigned long long>(n) * 8ull;
  size_t i = 0;
  auto round = [&](const unsigned char* p) {
    unsigned w[64];
    for (int t = 0; t < 16; ++t)
      w[t] = (static_cast<unsigned>(p[t * 4]) << 24) | (static_cast<unsigned>(p[t * 4 + 1]) << 16) |
             (static_cast<unsigned>(p[t * 4 + 2]) << 8) | static_cast<unsigned>(p[t * 4 + 3]);
    for (int t = 16; t < 64; ++t) {
      unsigned s0 = k32_rotr(w[t - 15], 7) ^ k32_rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
      unsigned s1 = k32_rotr(w[t - 2], 17) ^ k32_rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
      w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    unsigned a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int t = 0; t < 64; ++t) {
      unsigned S1 = k32_rotr(e, 6) ^ k32_rotr(e, 11) ^ k32_rotr(e, 25);
      unsigned ch = (e & f) ^ ((~e) & g);
      unsigned t1 = hh + S1 + ch + kSha256K[t] + w[t];
      unsigned S0 = k32_rotr(a, 2) ^ k32_rotr(a, 13) ^ k32_rotr(a, 22);
      unsigned maj = (a & b) ^ (a & c) ^ (b & c);
      unsigned t2 = S0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  };
  while (i + 64 <= n) {
    round(data + i);
    i += 64;
  }
  size_t rem = n - i;
  std::memset(blk, 0, 64);
  if (rem) std::memcpy(blk, data + i, rem);
  blk[rem] = 0x80;
  if (rem >= 56) {
    round(blk);
    std::memset(blk, 0, 64);
  }
  for (int t = 0; t < 8; ++t) blk[63 - t] = static_cast<unsigned char>(bits >> (t * 8));
  round(blk);
  for (int t = 0; t < 8; ++t) {
    out[t * 4] = static_cast<unsigned char>(h[t] >> 24);
    out[t * 4 + 1] = static_cast<unsigned char>(h[t] >> 16);
    out[t * 4 + 2] = static_cast<unsigned char>(h[t] >> 8);
    out[t * 4 + 3] = static_cast<unsigned char>(h[t]);
  }
}

inline std::string k32_hex(const unsigned char* p, size_t n) {
  static const char* hexd = "0123456789abcdef";
  std::string s;
  s.resize(n * 2);
  for (size_t i = 0; i < n; ++i) {
    s[i * 2] = hexd[p[i] >> 4];
    s[i * 2 + 1] = hexd[p[i] & 0xf];
  }
    return s;
}

inline std::string k32_b64(const unsigned char* p, size_t n) {
  static const char* t =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = static_cast<unsigned>(p[i]) << 16;
    if (i + 1 < n) v |= static_cast<unsigned>(p[i + 1]) << 8;
    if (i + 2 < n) v |= p[i + 2];
    o.push_back(t[(v >> 18) & 63]);
    o.push_back(t[(v >> 12) & 63]);
    o.push_back(i + 1 < n ? t[(v >> 6) & 63] : '=');
    o.push_back(i + 2 < n ? t[v & 63] : '=');
  }
  return o;
}

inline std::string k32_unb64(const std::string& s) {
  int dec[256];
  for (int i = 0; i < 256; ++i) dec[i] = -1;
  const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (int i = 0; t[i]; ++i) dec[static_cast<unsigned char>(t[i])] = i;
  std::string o;
  int acc = 0, n = 0;
  for (unsigned char c : s) {
    if (c == '=' || dec[c] < 0) {
      if (c == '=') break;
      continue;
    }
    acc = (acc << 6) | dec[c];
    n += 6;
    if (n >= 8) {
      n -= 8;
      o.push_back(static_cast<char>((acc >> n) & 0xff));
    }
  }
  return o;
}

inline std::string k32_dpapi_xor(const std::string& in, int unprotect) {
  static unsigned char key[16];
  static int ready = 0;
  if (!ready) {
    k32_entropy(key, 16);
    ready = 1;
  }
  if (unprotect) {
    if (in.size() < 4 || std::memcmp(in.data(), "W32P", 4) != 0) return {};
    std::string o = in.substr(4);
    for (size_t i = 0; i < o.size(); ++i)
      o[i] = static_cast<char>(static_cast<unsigned char>(o[i]) ^ key[i & 15]);
    return o;
  }
  std::string o = "W32P";
  o.resize(4 + in.size());
  for (size_t i = 0; i < in.size(); ++i)
    o[4 + i] = static_cast<char>(static_cast<unsigned char>(in[i]) ^ key[i & 15]);
  return o;
}

inline const unsigned char* k32_aes_s() {
  static const unsigned char s[256] = {
      0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
      0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
      0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
      0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
      0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
      0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
      0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
      0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
      0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
      0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
      0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
      0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
      0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
      0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
      0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
      0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};
  return s;
}

inline unsigned char k32_gfmul(unsigned char a, unsigned char b) {
  unsigned char p = 0;
  for (int i = 0; i < 8; ++i) {
    if (b & 1) p ^= a;
    unsigned char hi = static_cast<unsigned char>(a & 0x80);
    a = static_cast<unsigned char>(a << 1);
    if (hi) a ^= 0x1b;
    b = static_cast<unsigned char>(b >> 1);
  }
  return p;
}

inline void k32_aes_expand(const unsigned char key[16], unsigned char rk[176]) {
  const unsigned char* s = k32_aes_s();
  std::memcpy(rk, key, 16);
  unsigned char rcon = 1;
  for (int i = 16, n = 1; i < 176; i += 4, ++n) {
    unsigned char t[4] = {rk[i - 4], rk[i - 3], rk[i - 2], rk[i - 1]};
    if (n % 4 == 1) {
      unsigned char tmp = t[0];
      t[0] = static_cast<unsigned char>(s[t[1]] ^ rcon);
      t[1] = s[t[2]];
      t[2] = s[t[3]];
      t[3] = s[tmp];
      rcon = static_cast<unsigned char>((rcon << 1) ^ ((rcon & 0x80) ? 0x1b : 0));
    }
    rk[i] = static_cast<unsigned char>(rk[i - 16] ^ t[0]);
    rk[i + 1] = static_cast<unsigned char>(rk[i - 15] ^ t[1]);
    rk[i + 2] = static_cast<unsigned char>(rk[i - 14] ^ t[2]);
    rk[i + 3] = static_cast<unsigned char>(rk[i - 13] ^ t[3]);
  }
}

inline void k32_aes_block(const unsigned char rk[176], unsigned char st[16], int dec) {
  const unsigned char* s = k32_aes_s();
  unsigned char inv[256];
  for (int i = 0; i < 256; ++i) inv[s[i]] = static_cast<unsigned char>(i);
  auto add = [&](int r) {
    for (int i = 0; i < 16; ++i) st[i] ^= rk[r * 16 + i];
  };
  auto sub = [&](int backwards) {
    for (int i = 0; i < 16; ++i) st[i] = backwards ? inv[st[i]] : s[st[i]];
  };
  auto shift = [&](int backwards) {
    unsigned char t[16];
    std::memcpy(t, st, 16);
    if (!backwards) {
      st[1] = t[5];
      st[5] = t[9];
      st[9] = t[13];
      st[13] = t[1];
      st[2] = t[10];
      st[6] = t[14];
      st[10] = t[2];
      st[14] = t[6];
      st[3] = t[15];
      st[7] = t[3];
      st[11] = t[7];
      st[15] = t[11];
    } else {
      st[1] = t[13];
      st[5] = t[1];
      st[9] = t[5];
      st[13] = t[9];
      st[2] = t[10];
      st[6] = t[14];
      st[10] = t[2];
      st[14] = t[6];
      st[3] = t[7];
      st[7] = t[11];
      st[11] = t[15];
      st[15] = t[3];
    }
  };
  auto mix = [&](int backwards) {
    for (int c = 0; c < 4; ++c) {
      unsigned char a0 = st[c * 4], a1 = st[c * 4 + 1], a2 = st[c * 4 + 2], a3 = st[c * 4 + 3];
      if (!backwards) {
        st[c * 4] = static_cast<unsigned char>(k32_gfmul(a0, 2) ^ k32_gfmul(a1, 3) ^ a2 ^ a3);
        st[c * 4 + 1] = static_cast<unsigned char>(a0 ^ k32_gfmul(a1, 2) ^ k32_gfmul(a2, 3) ^ a3);
        st[c * 4 + 2] = static_cast<unsigned char>(a0 ^ a1 ^ k32_gfmul(a2, 2) ^ k32_gfmul(a3, 3));
        st[c * 4 + 3] = static_cast<unsigned char>(k32_gfmul(a0, 3) ^ a1 ^ a2 ^ k32_gfmul(a3, 2));
      } else {
        st[c * 4] = static_cast<unsigned char>(k32_gfmul(a0, 14) ^ k32_gfmul(a1, 11) ^ k32_gfmul(a2, 13) ^
                                               k32_gfmul(a3, 9));
        st[c * 4 + 1] = static_cast<unsigned char>(k32_gfmul(a0, 9) ^ k32_gfmul(a1, 14) ^ k32_gfmul(a2, 11) ^
                                                   k32_gfmul(a3, 13));
        st[c * 4 + 2] = static_cast<unsigned char>(k32_gfmul(a0, 13) ^ k32_gfmul(a1, 9) ^ k32_gfmul(a2, 14) ^
                                                   k32_gfmul(a3, 11));
        st[c * 4 + 3] = static_cast<unsigned char>(k32_gfmul(a0, 11) ^ k32_gfmul(a1, 13) ^ k32_gfmul(a2, 9) ^
                                                   k32_gfmul(a3, 14));
      }
    }
  };
  if (!dec) {
    add(0);
    for (int r = 1; r < 10; ++r) {
      sub(0);
      shift(0);
      mix(0);
      add(r);
    }
    sub(0);
    shift(0);
    add(10);
  } else {
    add(10);
    for (int r = 9; r > 0; --r) {
      shift(1);
      sub(1);
      add(r);
      mix(1);
    }
    shift(1);
    sub(1);
    add(0);
  }
}

inline std::string k32_aes128(const std::string& key_in, const std::string& data, int dec) {
  unsigned char key[16]{};
  std::memcpy(key, key_in.data(), key_in.size() > 16 ? 16 : key_in.size());
  unsigned char rk[176];
  k32_aes_expand(key, rk);
  std::string in = data;
  if (!dec) {
    unsigned pad = 16 - static_cast<unsigned>(in.size() % 16);
    if (pad == 0) pad = 16;
    in.append(pad, static_cast<char>(pad));
  } else if (in.size() % 16) {
    return {};
  }
  std::string out;
  out.resize(in.size());
  for (size_t i = 0; i < in.size(); i += 16) {
    unsigned char st[16];
    std::memcpy(st, in.data() + i, 16);
    k32_aes_block(rk, st, dec);
    std::memcpy(&out[i], st, 16);
  }
  if (dec) {
    if (out.empty()) return {};
    unsigned char pad = static_cast<unsigned char>(out.back());
    if (pad == 0 || pad > 16 || pad > out.size()) return {};
    out.resize(out.size() - pad);
  }
  return out;
}

inline std::string ft_to_iso(unsigned long long ft) {
  unsigned long long unix100 = ft - 116444736000000000ull;
  time_t sec = static_cast<time_t>(unix100 / 10000000ull);
  int ms = static_cast<int>((unix100 / 10000ull) % 1000ull);
  struct tm t {};
#if defined(_WIN32)
  gmtime_s(&t, &sec);
#else
  gmtime_r(&sec, &t);
#endif
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
                t.tm_sec, ms);
  return buf;
}

inline std::string memstat() {
#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
  long pages = sysconf(_SC_PHYS_PAGES);
  long psz = sysconf(_SC_PAGESIZE);
  unsigned long long total =
      (pages > 0 && psz > 0)
          ? static_cast<unsigned long long>(pages) * static_cast<unsigned long long>(psz)
          : (1ull << 30);
#else
  unsigned long long total = 1ull << 30;
#endif
  unsigned long long avail = total / 2;
  return std::string("0\x1f") + std::to_string(total) + "\x1f" + std::to_string(avail);
}

inline std::string disk_free(const char* path) {
  const char* p = (path && path[0]) ? path : ".";
  struct ::stat st {};
  if (::stat(p, &st) != 0) return err("GetDiskFreeSpaceExW");
  (void)st;
  return "1073741824\x1f" "1073741824\x1f" "1073741824";
}

inline FILE* fopen_create(const char* path, unsigned access, unsigned disp) {
  bool rd = (access & 0x80000000u) != 0 || access == 0;
  bool wr = (access & 0x40000000u) != 0 || (access & 0x10000000u) != 0;
  const char* mode = "rb";
  if (disp == 1 || disp == 2 || disp == 5) mode = wr ? "wb+" : "wb";
  else if (disp == 4) mode = wr ? "ab+" : "ab";
  else if (wr && rd) mode = "rb+";
  else if (wr) mode = "rb+";
  FILE* f = std::fopen(path, mode);
  if (!f && wr && (disp == 3 || disp == 4 || disp == 0)) f = std::fopen(path, "wb+");
  if (!f && rd) f = std::fopen(path, "rb");
  return f;
}

inline bool try_kernel32(const char* api, const char* a, std::string* out) {
  auto ok = [&](std::string s) {
    *out = std::move(s);
    return true;
  };
  auto fail = [&](std::string s) {
    *out = std::move(s);
    return true;
  };

  if (eq(api, "GetTickCount")) {
    return ok(std::to_string(static_cast<unsigned>(std::strtoul(tick_ms().c_str(), nullptr, 10))));
  }
  if (eq(api, "SleepEx")) {
    sleep_ms(std::strtoul(a, nullptr, 10));
    return ok("ok");
  }
  if (eq(api, "GetCurrentProcessorNumber") || eq(api, "GetCurrentProcessorNumberEx"))
    return ok("0");
  if (eq(api, "SetLastError")) {
    errno = static_cast<int>(std::strtol(a, nullptr, 10));
    return ok("ok");
  }
  if (eq(api, "GetComputerNameExW") || eq(api, "GetComputerNameA")) return ok(hostname());
  if (eq(api, "GetSystemWindowsDirectoryW")) return ok(windir());
  if (eq(api, "GetNativeSystemInfo")) return ok("0\x1f" "65536\x1f" "1");
  if (eq(api, "GetSystemTimePreciseAsFileTime")) return ok(filetime_now());
  if (eq(api, "GetVersion")) return ok("0x0A000000");
  if (eq(api, "GetVersionExW")) return ok(fmt_uname());
  if (eq(api, "GlobalMemoryStatusEx")) return ok(memstat());
  if (eq(api, "GetPhysicallyInstalledSystemMemory")) return ok("1048576");
  if (eq(api, "GetTimeZoneInformation")) return ok("0\x1fUTC");
  if (eq(api, "GetUserDefaultLCID") || eq(api, "GetSystemDefaultLCID") ||
      eq(api, "GetThreadLocale"))
    return ok("1033");
  if (eq(api, "SetThreadLocale")) return ok("ok");
  if (eq(api, "GetUserDefaultLangID") || eq(api, "GetSystemDefaultLangID")) return ok("1033");
  if (eq(api, "GetLocaleInfoW")) return ok("en-US");
  if (eq(api, "GetCommandLineA")) {
    if (const char* p = std::getenv("_")) {
      if (p[0]) return ok(p);
    }
    return ok("main.wasm");
  }
  if (eq(api, "GetStartupInfoW")) return ok("wshow=1");
  if (eq(api, "GetModuleHandleW") || eq(api, "GetModuleHandleA") ||
      eq(api, "GetModuleHandleExW") || eq(api, "GetModuleHandleExA")) {
    if (!a[0]) return ok("1");
    int h = k32_load_library(a);
    if (h == 1) return ok("1");
    if (h < 0) return fail(err_msg("GetModuleHandleW: module not found"));
    return ok(std::to_string(h));
  }
  if (eq(api, "DisableThreadLibraryCalls")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (K32Mod* m = (h == 1) ? nullptr : k32_mod(h)) m->no_thread_calls = 1;
    return ok("ok");
  }
  if (eq(api, "LoadLibraryW") || eq(api, "LoadLibraryA") || eq(api, "LoadLibraryExW") ||
      eq(api, "LoadLibraryExA")) {
    std::string path, rest;
    split1f(a, &path, &rest);
    if (path.empty()) path = a;
    if (path.empty()) return fail(err_msg("LoadLibraryW: empty path"));
    int h = k32_load_library(path.c_str());
    if (h == 1) return ok("1");
    if (h == -126) return fail(err_msg("LoadLibraryW: module not found"));
    if (h == -193)
      return fail(err_msg("LoadLibraryW: image format not mapped in this module"));
    if (h < 0) return fail(err_msg("LoadLibraryW"));
    if (K32Mod* m = k32_mod(h)) {
      if (!k32_call_main_dll(m, 1)) return fail(err_msg("DllMain: DLL_PROCESS_ATTACH"));
    }
    return ok(std::to_string(h));
  }
  if (eq(api, "DllMain")) {
    std::string hs, rstr;
    split1f(a, &hs, &rstr);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    unsigned reason = static_cast<unsigned>(std::strtoul(rstr.c_str(), nullptr, 10));
    K32Mod* m = (h == 1) ? nullptr : k32_mod(h);
    if (!m) {
      static K32Mod self{};
      self.used = 1;
      self.name = "kernel32";
      if (!self.whp_part) k32_whp_ensure_mod(&self);
      self.main_attached = 1;
      return ok("ok");
    }
    if (!k32_call_main_dll(m, reason)) return fail(err_msg("DllMain"));
    return ok("ok");
  }
  if (eq(api, "FreeLibrary")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h == 1) return ok("ok");
    if (K32Mod* m = k32_mod(h)) {
      if (m->refs <= 1) k32_call_main_dll(m, 0);
    }
    if (!k32_close_mod(h)) return fail(err_msg("FreeLibrary: bad handle"));
    return ok("ok");
  }
  if (eq(api, "GetProcAddress")) {
    std::string hs, name;
    split1f(a, &hs, &name);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Mod fake{};
    fake.used = 1;
    fake.name = "kernel32";
    K32Mod* m = (h == 1) ? &fake : k32_mod(h);
    if (!m) return fail(err_msg("GetProcAddress: bad module"));
    if (eq(name.c_str(), "DllMain") && m->pe && m->base && m->entry_rva)
      return ok(std::to_string(
          static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(m->base)) + m->entry_rva));
    unsigned long long p = k32_proc_addr(m, name.c_str());
    if (!p) return fail(err_msg("GetProcAddress: proc not found"));
    return ok(std::to_string(p));
  }
  if (eq(api, "LdrLoadDll")) {
    int h = k32_load_library(a && a[0] ? a : "ntdll");
    if (h < 0) return fail(err_msg("LdrLoadDll"));
    if (h > 1) {
      if (K32Mod* m = k32_mod(h)) {
        if (!k32_call_main_dll(m, 1)) return fail(err_msg("DllMain: DLL_PROCESS_ATTACH"));
      }
    }
    return ok(std::to_string(h == 1 ? 1 : h));
  }
  if (eq(api, "LdrGetDllHandle")) {
    if (!a[0]) return ok("1");
    int h = k32_find_mod(k32_mod_norm(a));
    if (h < 0) h = k32_load_library(a);
    if (h < 0) return fail(err_msg("LdrGetDllHandle"));
    return ok(std::to_string(h));
  }
  if (eq(api, "LdrGetProcedureAddress")) {
    std::string hs, name;
    split1f(a, &hs, &name);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Mod fake{};
    fake.used = 1;
    fake.name = "ntdll";
    K32Mod* m = (h == 1) ? &fake : k32_mod(h);
    if (!m) return fail(err_msg("LdrGetProcedureAddress"));
    unsigned long long p = k32_proc_addr(m, name.c_str());
    if (!p) return fail(err_msg("LdrGetProcedureAddress"));
    return ok(std::to_string(p));
  }
  if (eq(api, "LdrUnloadDll")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h == 1) return ok("ok");
    k32_close_mod(h);
    return ok("ok");
  }
  if (eq(api, "LdrAddRefDll")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    K32Mod* m = (h == 1) ? nullptr : k32_mod(h);
    if (h != 1 && !m) return fail(err_msg("LdrAddRefDll"));
    if (m) m->refs++;
    return ok("ok");
  }
  if (eq(api, "LdrFindEntryForAddress")) {
    unsigned long long addr = std::strtoull(a, nullptr, 10);
    K32Mod* t = k32_mods();
    for (int i = 0; i < kK32Max; ++i) {
      if (!t[i].used || !t[i].pe || !t[i].base) continue;
      auto b = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(t[i].base));
      if (addr >= b && addr < b + t[i].n) return ok(std::to_string(kK32ModBase + i));
    }
    return fail(err_msg("LdrFindEntryForAddress"));
  }
  if (eq(api, "RtlImageNtHeader") || eq(api, "RtlImageNtHeaderEx")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    K32Mod* m = k32_mod(h);
    if (m && m->pe && m->base) {
      const unsigned char* p = static_cast<const unsigned char*>(m->base);
      return ok(std::to_string(reinterpret_cast<uintptr_t>(p + m->nt_off)));
    }
    return ok("PE");
  }
  if (eq(api, "RtlImageDirectoryEntryToData")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    K32Mod* m = k32_mod(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || !m->pe) return fail(err_msg(api));
    unsigned i = static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10));
    if (i >= 16) return fail(err_msg(api));
    return ok(std::to_string(m->dirs_rva[i]) + "\x1f" + std::to_string(m->dirs_sz[i]));
  }
  if (eq(api, "RtlImageRvaToVa")) {
    std::string hs, rva;
    split1f(a, &hs, &rva);
    K32Mod* m = k32_mod(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || !m->pe || !m->base) return fail(err_msg(api));
    unsigned off = static_cast<unsigned>(std::strtoul(rva.c_str(), nullptr, 10));
    if (off >= m->n) return fail(err_msg(api));
    return ok(std::to_string(reinterpret_cast<uintptr_t>(m->base) + off));
  }
  if (eq(api, "FindResourceW") || eq(api, "FindResourceA") || eq(api, "FindResourceExW")) {
    std::string hs, rest, type, name;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &type, &name);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Mod* m = k32_mod(h);
    if (!m) return fail(err_msg("FindResourceW"));
    for (auto& r : m->resources) {
      if ((type.empty() || r.type == type) && (name.empty() || r.name == name)) {
        int rh = k32_alloc_misc(kMiscRes);
        if (rh < 0) return fail(err_msg("FindResourceW"));
        K32Misc* mi = k32_misc(rh);
        mi->mod = h;
        mi->rva = r.rva;
        mi->n = r.size;
        mi->name = r.name;
        return ok(std::to_string(rh));
      }
    }
    return fail(err_msg("FindResourceW: not found"));
  }
  if (eq(api, "LoadResource") || eq(api, "LockResource")) {
    std::string hs, rs;
    split1f(a, &hs, &rs);
    int rh = static_cast<int>(std::strtol(rs.empty() ? a : rs.c_str(), nullptr, 10));
    K32Misc* mi = k32_misc(rh);
    if (!mi || mi->kind != kMiscRes) return fail(err_msg(api));
    K32Mod* m = k32_mod(mi->mod);
    if (!m || !m->base) return fail(err_msg(api));
    return ok(std::to_string(reinterpret_cast<uintptr_t>(m->base) + mi->rva));
  }
  if (eq(api, "SizeofResource")) {
    std::string hs, rs;
    split1f(a, &hs, &rs);
    int rh = static_cast<int>(std::strtol(rs.empty() ? a : rs.c_str(), nullptr, 10));
    K32Misc* mi = k32_misc(rh);
    if (!mi || mi->kind != kMiscRes) return fail(err_msg("SizeofResource"));
    return ok(std::to_string(mi->n));
  }
  if (eq(api, "RtlNtStatusToDosError") || eq(api, "RtlNtStatusToDosErrorNoTeb")) {
    long st = std::strtol(a, nullptr, 0);
    if (st >= 0) return ok("0");
    return ok(std::to_string(st & 0xffff));
  }
  if (eq(api, "RtlInitUnicodeString") || eq(api, "RtlInitAnsiString") ||
      eq(api, "RtlInitString"))
    return ok(a ? a : "");
  if (eq(api, "RtlAnsiStringToUnicodeString") || eq(api, "RtlUnicodeStringToAnsiString"))
    return ok(a ? a : "");
  if (eq(api, "RtlGetVersion") || eq(api, "RtlGetNtVersionNumbers")) {
    std::string v = k32_nt_version();
    if (v.empty()) return fail(err_msg("RtlGetVersion"));
    return ok(v);
  }
  if (eq(api, "RtlGetNtProductType")) return ok("1");
  if (eq(api, "RtlAllocateHeap")) {
    std::string heap, rest, flags, sz;
    split1f(a, &heap, &rest);
    if (rest.empty()) sz = heap;
    else {
      split1f(rest.c_str(), &flags, &sz);
      if (sz.empty()) sz = flags.empty() ? heap : flags;
    }
    return try_kernel32("HeapAlloc", sz.c_str(), out);
  }
  if (eq(api, "RtlFreeHeap")) {
    std::string heap, rest, flags, p;
    split1f(a, &heap, &rest);
    if (rest.empty()) p = heap;
    else {
      split1f(rest.c_str(), &flags, &p);
      if (p.empty()) p = flags.empty() ? heap : flags;
    }
    return try_kernel32("HeapFree", p.c_str(), out);
  }
  if (eq(api, "RtlReAllocateHeap")) return try_kernel32("HeapReAlloc", a, out);
  if (eq(api, "RtlSizeHeap")) {
    std::string heap, rest, flags, p;
    split1f(a, &heap, &rest);
    if (rest.empty()) p = heap;
    else {
      split1f(rest.c_str(), &flags, &p);
      if (p.empty()) p = flags.empty() ? heap : flags;
    }
    return try_kernel32("HeapSize", p.c_str(), out);
  }
  if (eq(api, "RtlZeroMemory") || eq(api, "RtlSecureZeroMemory") || eq(api, "RtlFillMemory") ||
      eq(api, "RtlMoveMemory") || eq(api, "RtlCopyMemory")) {
    std::string pstr, rest, nstr, fill;
    split1f(a, &pstr, &rest);
    split1f(rest.c_str(), &nstr, &fill);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(pstr.c_str(), nullptr, 10));
    size_t n = static_cast<size_t>(std::strtoull(nstr.c_str(), nullptr, 10));
    auto it = k32_heap_sz().find(p);
    if (!p || it == k32_heap_sz().end() || n > it->second) return fail(err_msg(api));
    unsigned char* b = reinterpret_cast<unsigned char*>(p);
    if (eq(api, "RtlMoveMemory") || eq(api, "RtlCopyMemory")) {
      uintptr_t src = static_cast<uintptr_t>(std::strtoull(nstr.c_str(), nullptr, 10));
      size_t cn = static_cast<size_t>(std::strtoull(fill.c_str(), nullptr, 10));
      auto sit = k32_heap_sz().find(src);
      if (!src || sit == k32_heap_sz().end() || cn > sit->second || cn > it->second)
        return fail(err_msg(api));
      std::memmove(b, reinterpret_cast<void*>(src), cn);
      return ok("ok");
    }
    unsigned char v = eq(api, "RtlFillMemory") ? static_cast<unsigned char>(std::strtoul(fill.c_str(), nullptr, 10)) : 0;
    std::memset(b, v, n);
    return ok("ok");
  }
  if (eq(api, "RtlEqualUnicodeString") || eq(api, "RtlCompareUnicodeString") ||
      eq(api, "RtlPrefixUnicodeString")) {
    std::string x, rest, y, fold;
    split1f(a, &x, &rest);
    split1f(rest.c_str(), &y, &fold);
    auto low = [](std::string s) {
      for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
      return s;
    };
    if (fold == "1") {
      x = low(x);
      y = low(y);
    }
    if (eq(api, "RtlPrefixUnicodeString")) return ok(x.size() <= y.size() && y.compare(0, x.size(), x) == 0 ? "1" : "0");
    if (eq(api, "RtlEqualUnicodeString")) return ok(x == y ? "1" : "0");
    if (x == y) return ok("0");
    return ok(x < y ? "-1" : "1");
  }
  if (eq(api, "RtlHashUnicodeString")) {
    unsigned h = 2166136261u;
    for (const char* p = a; p && *p && *p != '\x1f'; ++p) {
      unsigned char c = static_cast<unsigned char>(*p);
      if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
      h ^= c;
      h *= 16777619u;
    }
    return ok(std::to_string(h));
  }
  if (eq(api, "RtlUpcaseUnicodeChar")) {
    unsigned c = static_cast<unsigned>(std::strtoul(a, nullptr, 0));
    if (!c && a[0]) c = static_cast<unsigned char>(a[0]);
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    return ok(std::to_string(c));
  }
  if (eq(api, "RtlDowncaseUnicodeChar")) {
    unsigned c = static_cast<unsigned>(std::strtoul(a, nullptr, 0));
    if (!c && a[0]) c = static_cast<unsigned char>(a[0]);
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    return ok(std::to_string(c));
  }
  if (eq(api, "RtlIntegerToUnicodeString") || eq(api, "RtlIntegerToChar"))
    return ok(std::to_string(static_cast<long long>(std::strtoll(a, nullptr, 0))));
  if (eq(api, "RtlUnicodeStringToInteger") || eq(api, "RtlCharToInteger"))
    return ok(std::to_string(static_cast<long long>(std::strtoll(a, nullptr, 0))));
  if (eq(api, "RtlRandomEx") || eq(api, "RtlRandom")) {
    unsigned seed = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    if (!seed) seed = 1;
    seed = seed * 214013u + 2531011u;
    return ok(std::to_string((seed >> 16) & 0x7fff));
  }
  if (eq(api, "RtlPcToFileHeader")) {
    unsigned long long addr = std::strtoull(a, nullptr, 10);
    K32Mod* t = k32_mods();
    for (int i = 0; i < kK32Max; ++i) {
      if (!t[i].used || !t[i].pe || !t[i].base) continue;
      auto b = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(t[i].base));
      if (addr >= b && addr < b + t[i].n) return ok(std::to_string(b));
    }
    return fail(err_msg("RtlPcToFileHeader"));
  }
  if (eq(api, "RtlGetLastWin32Error") || eq(api, "RtlGetLastNtStatus") || eq(api, "GetLastError"))
    return ok(std::to_string(errno));
  if (eq(api, "RtlSetLastWin32Error") || eq(api, "RtlSetLastWin32ErrorEx")) {
    errno = static_cast<int>(std::strtol(a, nullptr, 10));
    return ok("ok");
  }
  if (eq(api, "CreateProcessW") || eq(api, "CreateProcessA")) {
    std::string cmd = k32_create_cmd(a);
    if (cmd.empty()) return fail(err_msg("CreateProcessW: empty command"));
    int h = k32_alloc_proc();
    if (h < 0) return fail(err_msg("CreateProcessW: no handles"));
    K32Proc* p = k32_proc(h);
    p->vthread = true;
    // GocVM child is a vthread. Blinker is one OS thread: enqueue, then
    // run it here. Accept WaitOne must not leave the child queued.
    if (k32_child_go()) {
      k32_child_go()(p, cmd);
      if (k32_run_until()) {
        k32_run_until()(&p->done);
      } else if (k32_pump()) {
        int n = 0;
        while (!p->done && k32_pump()() && n++ < 256) {
        }
        if (!p->started) k32_run_child(p, cmd);
      } else {
        k32_run_child(p, cmd);
      }
    } else {
      k32_run_child(p, cmd);
    }
    return ok(std::to_string(p->pid) + "\x1f" + std::to_string(h) + "\x1f" +
              std::to_string(h));
  }
  if (eq(api, "WaitForSingleObject") || eq(api, "WaitForSingleObjectEx")) {
    std::string hs, toms;
    split1f(a, &hs, &toms);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    unsigned long long ms = toms.empty() ? 0xffffffffull
                                         : std::strtoull(toms.c_str(), nullptr, 10);
    int wr = k32_wait_one(h, ms);
    if (wr < 0) return fail(err_msg("WaitForSingleObject: bad handle"));
    return ok(std::to_string(wr));
  }
  if (eq(api, "GetExitCodeProcess")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    K32Proc* p = k32_proc(h);
    if (!p) return fail(err_msg("GetExitCodeProcess: bad handle"));
    std::lock_guard<std::mutex> lk(p->mu);
    return ok(std::to_string(p->exit_code));
  }
  if (eq(api, "GetProcessOutput")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    K32Proc* p = k32_proc(h);
    if (!p) return fail(err_msg("GetProcessOutput: bad handle"));
    if (!p->done && p->vthread && k32_run_until())
      k32_wait_one(h, 0xffffffffull);
    else if (!p->done && k32_pump())
      k32_pump()();
    std::lock_guard<std::mutex> lk(p->mu);
    std::string out = p->out;
    p->out.clear();
    return ok(out);
  }
  if (eq(api, "TerminateProcess")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    K32Proc* p = k32_proc(h);
    if (!p) return fail(err_msg("TerminateProcess: bad handle"));
    std::lock_guard<std::mutex> lk(p->mu);
    if (!p->done) p->exit_code = 1;
    p->done = true;
    p->cv.notify_all();
    return ok("ok");
  }
  if (eq(api, "OpenProcess")) return fail(err_msg("OpenProcess: guest pids only"));
  if (eq(api, "GetProcessId")) {
    if (a[0]) {
      K32Proc* p = k32_proc(static_cast<int>(std::strtol(a, nullptr, 10)));
      if (!p) return fail(err_msg("GetProcessId: bad handle"));
      return ok(std::to_string(p->pid));
    }
    return ok(std::to_string(static_cast<long>(getpid())));
  }
  if (eq(api, "ExitProcess")) {
    _Exit(static_cast<int>(std::strtol(a, nullptr, 10)));
  }
  if (eq(api, "Beep")) return ok("ok");
  if (eq(api, "AreFileApisANSI")) return ok("1");
  if (eq(api, "GetLongPathNameW") || eq(api, "GetShortPathNameW")) return ok(full_path(a));
  if (eq(api, "MoveFileExW")) {
    std::string src, rest, dst, flags;
    split1f(a, &src, &rest);
    split1f(rest.c_str(), &dst, &flags);
    if (src.empty() || dst.empty()) return fail(err_msg("MoveFileExW: missing path"));
    if (rename(src.c_str(), dst.c_str()) != 0) return fail(err("MoveFileExW"));
    return ok("ok");
  }
  if (eq(api, "GetDiskFreeSpaceExW") || eq(api, "GetDiskFreeSpaceW")) return ok(disk_free(a));
  if (eq(api, "GetLogicalDrives")) return ok("4");
  if (eq(api, "GetLogicalDriveStringsW")) return ok("/\x1f");
  if (eq(api, "GetVolumeInformationW")) return ok("wasigocvm\x1f" "0\x1f" "255\x1f" "0\x1fwasifs");
  if (eq(api, "CompareFileTime")) {
    std::string x, y;
    split1f(a, &x, &y);
    unsigned long long A = std::strtoull(x.c_str(), nullptr, 10);
    unsigned long long B = std::strtoull(y.c_str(), nullptr, 10);
    if (A < B) return ok("-1");
    if (A > B) return ok("1");
    return ok("0");
  }
  if (eq(api, "FileTimeToSystemTime")) return ok(ft_to_iso(std::strtoull(a, nullptr, 10)));
  if (eq(api, "SystemTimeToFileTime")) return ok(filetime_now());
  if (eq(api, "FileTimeToLocalFileTime") || eq(api, "LocalFileTimeToFileTime")) return ok(a);
  if (eq(api, "GetConsoleMode")) return ok("3");
  if (eq(api, "SetConsoleMode") || eq(api, "SetStdHandle") || eq(api, "SetConsoleTitleW") ||
      eq(api, "SetConsoleCP") || eq(api, "SetConsoleOutputCP"))
    return ok("ok");
  if (eq(api, "GetConsoleCP") || eq(api, "GetConsoleOutputCP")) return ok("65001");
  if (eq(api, "GetConsoleTitleW")) return ok("wasigocvm");
  if (eq(api, "WriteConsoleW") || eq(api, "WriteConsoleA") || eq(api, "OutputDebugStringA")) {
    if (a[0]) std::fputs(a, stdout);
    return ok(std::to_string(std::strlen(a)));
  }
  if (eq(api, "ReadConsoleW") || eq(api, "ReadConsoleA")) {
    unsigned n = a[0] ? static_cast<unsigned>(std::strtoul(a, nullptr, 10)) : 256;
    if (n > 4096) n = 4096;
    std::string b(n, '\0');
    size_t got = std::fread(b.data(), 1, n, stdin);
    b.resize(got);
    return ok(b);
  }
  if (eq(api, "AllocConsole") || eq(api, "FreeConsole") || eq(api, "AttachConsole")) return ok("ok");
  if (eq(api, "GetNumberOfConsoleInputEvents")) return ok("0");
  if (eq(api, "CompareStringW") || eq(api, "lstrcmpW") || eq(api, "lstrcmpA")) {
    std::string x, y;
    split1f(a, &x, &y);
    int c = x.compare(y);
    if (c < 0) return ok("1");
    if (c > 0) return ok("3");
    return ok("2");
  }
  if (eq(api, "lstrlenW") || eq(api, "lstrlenA")) return ok(std::to_string(std::strlen(a)));
  if (eq(api, "MultiByteToWideChar") || eq(api, "WideCharToMultiByte")) return ok(a);
  if (eq(api, "FormatMessageW") || eq(api, "FormatMessageA")) {
    return ok(std::string("error ") + std::to_string(errno));
  }
  if (eq(api, "GetProcessHeap")) return ok(std::to_string(k32_process_heap()));
  if (eq(api, "HeapCreate")) {
    unsigned long n = std::strtoul(a, nullptr, 10);
    if (!n) n = 4096;
    void* p = std::malloc(n);
    if (!p) return fail(err_msg("HeapCreate: oom"));
    std::memset(p, 0, n);
    k32_vmem_add(p, n, 4);
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (eq(api, "HeapDestroy")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    k32_heap_sz().erase(p);
    k32_vmem().erase(p);
    if (p && p != k32_process_heap()) std::free(reinterpret_cast<void*>(p));
    return ok("ok");
  }
  if (eq(api, "HeapAlloc") || eq(api, "GlobalAlloc") || eq(api, "LocalAlloc") ||
      eq(api, "VirtualAlloc")) {
    unsigned long n = std::strtoul(a, nullptr, 10);
    if (!n) n = 1;
    void* p = std::malloc(n);
    if (!p) return fail(err_msg("HeapAlloc: oom"));
    std::memset(p, 0, n);
    k32_vmem_add(p, n, 4);
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (eq(api, "HeapFree") || eq(api, "GlobalFree") || eq(api, "LocalFree") ||
      eq(api, "VirtualFree")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    k32_heap_sz().erase(p);
    k32_vmem().erase(p);
    if (p) std::free(reinterpret_cast<void*>(p));
    return ok("ok");
  }
  if (eq(api, "HeapReAlloc")) {
    std::string hp, sz;
    split1f(a, &hp, &sz);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(hp.c_str(), nullptr, 10));
    unsigned long n = std::strtoul(sz.c_str(), nullptr, 10);
    void* q = std::realloc(reinterpret_cast<void*>(p), n ? n : 1);
    if (!q) return fail(err_msg("HeapReAlloc: oom"));
    k32_heap_sz().erase(p);
    k32_vmem().erase(p);
    k32_heap_sz()[reinterpret_cast<uintptr_t>(q)] = n ? n : 1;
    k32_vmem_add(q, n ? n : 1, 4);
    return ok(std::to_string(reinterpret_cast<uintptr_t>(q)));
  }
  if (eq(api, "HeapSize") || eq(api, "GlobalSize") || eq(api, "LocalSize")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    auto it = k32_heap_sz().find(p);
    return ok(std::to_string(it == k32_heap_sz().end() ? 0 : it->second));
  }
  if (eq(api, "VirtualQuery") || eq(api, "VirtualQueryEx") || eq(api, "VirtualProtect") ||
      eq(api, "VirtualProtectEx") || eq(api, "VirtualAllocEx") || eq(api, "VirtualFreeEx") ||
      eq(api, "ReadProcessMemory") || eq(api, "WriteProcessMemory")) {
    std::string a0, rest, a1, a2;
    split1f(a, &a0, &rest);
    split1f(rest.c_str(), &a1, &a2);
    int proc = -1;
    if (eq(api, "VirtualAllocEx")) {
      proc = static_cast<int>(std::strtol(a0.c_str(), nullptr, 10));
      if (!k32_proc_ok(proc)) return fail(err_msg("VirtualAllocEx: bad process"));
      unsigned long n = std::strtoul(a1.c_str(), nullptr, 10);
      if (!n) n = 1;
      unsigned prot = a2.empty() ? 4u : static_cast<unsigned>(std::strtoul(a2.c_str(), nullptr, 0));
      if (a2.find('\x1f') != std::string::npos) {
        std::string ty, pr;
        split1f(a2.c_str(), &ty, &pr);
        if (!pr.empty()) prot = static_cast<unsigned>(std::strtoul(pr.c_str(), nullptr, 0));
      }
      if (!prot) prot = 4;
      void* p = std::malloc(n);
      if (!p) return fail(err_msg("VirtualAllocEx: oom"));
      std::memset(p, 0, n);
      k32_vmem_add(p, n, prot);
      return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
    }
    if (eq(api, "VirtualFreeEx")) {
      proc = static_cast<int>(std::strtol(a0.c_str(), nullptr, 10));
      if (!k32_proc_ok(proc)) return fail(err_msg("VirtualFreeEx: bad process"));
      uintptr_t p = static_cast<uintptr_t>(std::strtoull(a1.c_str(), nullptr, 10));
      k32_vmem().erase(p);
      k32_heap_sz().erase(p);
      if (p) std::free(reinterpret_cast<void*>(p));
      return ok("ok");
    }
    if (eq(api, "VirtualProtect") || eq(api, "VirtualProtectEx")) {
      uintptr_t p = 0;
      unsigned prot = 0;
      if (eq(api, "VirtualProtectEx")) {
        proc = static_cast<int>(std::strtol(a0.c_str(), nullptr, 10));
        if (!k32_proc_ok(proc)) return fail(err_msg("VirtualProtectEx: bad process"));
        p = static_cast<uintptr_t>(std::strtoull(a1.c_str(), nullptr, 10));
        prot = static_cast<unsigned>(std::strtoul(a2.c_str(), nullptr, 0));
      } else {
        p = static_cast<uintptr_t>(std::strtoull(a0.c_str(), nullptr, 10));
        prot = static_cast<unsigned>(std::strtoul(rest.c_str(), nullptr, 0));
      }
      uintptr_t base = 0;
      K32Vmem* v = k32_vmem_at(p, &base);
      if (!v) return fail(err_msg("VirtualProtect: not found"));
      unsigned old = v->prot;
      if (prot) v->prot = prot;
      return ok(std::to_string(old));
    }
    if (eq(api, "VirtualQuery") || eq(api, "VirtualQueryEx")) {
      uintptr_t p = 0;
      if (eq(api, "VirtualQueryEx")) {
        proc = static_cast<int>(std::strtol(a0.c_str(), nullptr, 10));
        if (!k32_proc_ok(proc)) return fail(err_msg("VirtualQueryEx: bad process"));
        p = static_cast<uintptr_t>(std::strtoull(a1.c_str(), nullptr, 10));
      } else {
        p = static_cast<uintptr_t>(std::strtoull(a0.c_str(), nullptr, 10));
      }
      uintptr_t base = 0;
      K32Vmem* v = k32_vmem_at(p, &base);
      if (!v) return fail(err_msg("VirtualQuery: not found"));
      return ok(std::to_string(base) + "\x1f" + std::to_string(v->n) + "\x1f" +
                std::to_string(v->prot) + "\x1f" + "4096");
    }
    if (eq(api, "ReadProcessMemory")) {
      proc = static_cast<int>(std::strtol(a0.c_str(), nullptr, 10));
      if (!k32_proc_ok(proc)) return fail(err_msg("ReadProcessMemory: bad process"));
      uintptr_t p = static_cast<uintptr_t>(std::strtoull(a1.c_str(), nullptr, 10));
      unsigned n = static_cast<unsigned>(std::strtoul(a2.c_str(), nullptr, 10));
      uintptr_t base = 0;
      K32Vmem* v = k32_vmem_at(p, &base);
      if (!v || n == 0) return fail(err_msg("ReadProcessMemory: bad region"));
      if (p + n > base + v->n) n = static_cast<unsigned>(base + v->n - p);
      return ok(std::string(reinterpret_cast<const char*>(p), n));
    }
    if (eq(api, "WriteProcessMemory")) {
      proc = static_cast<int>(std::strtol(a0.c_str(), nullptr, 10));
      if (!k32_proc_ok(proc)) return fail(err_msg("WriteProcessMemory: bad process"));
      uintptr_t p = static_cast<uintptr_t>(std::strtoull(a1.c_str(), nullptr, 10));
      uintptr_t base = 0;
      K32Vmem* v = k32_vmem_at(p, &base);
      if (!v) return fail(err_msg("WriteProcessMemory: bad region"));
      size_t n = a2.size();
      if (p + n > base + v->n) n = static_cast<size_t>(base + v->n - p);
      std::memcpy(reinterpret_cast<void*>(p), a2.data(), n);
      return ok(std::to_string(n));
    }
    return fail(err_msg("virtual memory: bad api"));
  }
  if (eq(api, "TlsAlloc")) {
    char* used = k32_tls_used();
    for (int i = 0; i < 64; ++i) {
      if (!used[i]) {
        used[i] = 1;
        return ok(std::to_string(i));
      }
    }
    return fail(err_msg("TlsAlloc: full"));
  }
  if (eq(api, "TlsFree")) {
    int i = static_cast<int>(std::strtol(a, nullptr, 10));
    if (i >= 0 && i < 64) k32_tls_used()[i] = 0;
    return ok("ok");
  }
  if (eq(api, "TlsSetValue")) {
    std::string idx, val;
    split1f(a, &idx, &val);
    int i = static_cast<int>(std::strtol(idx.c_str(), nullptr, 10));
    if (i < 0 || i >= 64) return fail(err_msg("TlsSetValue"));
    k32_tls()[i] = val;
    return ok("ok");
  }
  if (eq(api, "TlsGetValue")) {
    int i = static_cast<int>(std::strtol(a, nullptr, 10));
    if (i < 0 || i >= 64) return ok("");
    return ok(k32_tls()[i]);
  }
  if (eq(api, "CreateFileW") || eq(api, "CreateFileA")) {
    std::string path, rest, access, disp;
    split1f(a, &path, &rest);
    split1f(rest.c_str(), &access, &disp);
    if (path.empty()) return fail(err_msg("CreateFileW: empty path"));
    unsigned acc = static_cast<unsigned>(std::strtoul(access.c_str(), nullptr, 10));
    unsigned d = static_cast<unsigned>(std::strtoul(disp.c_str(), nullptr, 10));
    FILE* f = fopen_create(path.c_str(), acc, d);
    if (!f) return fail(err("CreateFileW"));
    int h = k32_alloc_file(f, path);
    if (h < 0) {
      std::fclose(f);
      return fail(err_msg("CreateFileW: no handles"));
    }
    return ok(std::to_string(h));
  }
  if (eq(api, "ReadFile")) {
    std::string hs, nstr;
    split1f(a, &hs, &nstr);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    unsigned n = nstr.empty() ? 4096u : static_cast<unsigned>(std::strtoul(nstr.c_str(), nullptr, 10));
    if (n > 65536) n = 65536;
    if (h >= 0 && h <= 2) {
      FILE* f = h == 0 ? stdin : nullptr;
      if (!f) return fail(err_msg("ReadFile: not readable"));
      std::string b(n, '\0');
      size_t got = std::fread(b.data(), 1, n, f);
      b.resize(got);
      return ok(b);
    }
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1 || !k32_files()[h].f)
      return fail(err_msg("ReadFile: bad handle"));
    std::string b(n, '\0');
    size_t got = std::fread(b.data(), 1, n, k32_files()[h].f);
    b.resize(got);
    k32_file_complete(h, static_cast<unsigned>(got));
    return ok(b);
  }
  if (eq(api, "WriteFile")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (K32Proc* p = k32_proc(h)) {
      std::lock_guard<std::mutex> lk(p->mu);
      p->in.append(data);
      return ok(std::to_string(data.size()));
    }
    FILE* f = nullptr;
    if (h == 1) f = stdout;
    else if (h == 2) f = stderr;
    else if (h >= 3 && h < kK32Max && k32_files()[h].used == 1) f = k32_files()[h].f;
    if (!f) return fail(err_msg("WriteFile: bad handle"));
    size_t n = std::fwrite(data.data(), 1, data.size(), f);
    if (h >= 3) k32_file_complete(h, static_cast<unsigned>(n));
    return ok(std::to_string(n));
  }
  if (eq(api, "FlushFileBuffers")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    FILE* f = (h >= 3 && h < kK32Max && k32_files()[h].used == 1) ? k32_files()[h].f : nullptr;
    if (f) std::fflush(f);
    return ok("ok");
  }
  if (eq(api, "GetFileSizeEx") || eq(api, "GetFileSize")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1 || !k32_files()[h].f)
      return fail(err_msg("GetFileSizeEx: bad handle"));
    FILE* f = k32_files()[h].f;
    long cur = std::ftell(f);
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    if (cur >= 0) std::fseek(f, cur, SEEK_SET);
    return ok(std::to_string(sz < 0 ? 0 : sz));
  }
  if (eq(api, "SetFilePointerEx") || eq(api, "SetFilePointer")) {
    std::string hs, rest, off, meth;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &off, &meth);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1 || !k32_files()[h].f)
      return fail(err_msg("SetFilePointerEx: bad handle"));
    int whence = SEEK_SET;
    int m = static_cast<int>(std::strtol(meth.c_str(), nullptr, 10));
    if (m == 1) whence = SEEK_CUR;
    if (m == 2) whence = SEEK_END;
    if (std::fseek(k32_files()[h].f, std::strtol(off.c_str(), nullptr, 10), whence) != 0)
      return fail(err("SetFilePointerEx"));
    return ok(std::to_string(std::ftell(k32_files()[h].f)));
  }
  if (eq(api, "SetEndOfFile")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1 || !k32_files()[h].f)
      return fail(err_msg("SetEndOfFile: bad handle"));
    long pos = std::ftell(k32_files()[h].f);
    if (pos < 0) return fail(err("SetEndOfFile"));
#if defined(_WIN32)
    return fail(err_msg("SetEndOfFile: use native hop"));
#else
    int fd = fileno(k32_files()[h].f);
    if (ftruncate(fd, pos) != 0) return fail(err("SetEndOfFile"));
    return ok("ok");
#endif
  }
  if (eq(api, "GetFileType")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h >= 0 && h <= 2) return ok("2");
    return ok("1");
  }
  if (eq(api, "GetFinalPathNameByHandleW")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h >= 3 && h < kK32Max && k32_files()[h].used == 1 && !k32_files()[h].path.empty())
      return ok(k32_files()[h].path);
    return fail(err_msg("GetFinalPathNameByHandleW: no path cache"));
  }
  if (eq(api, "GetTempFileNameW")) {
    std::string dir, pre;
    split1f(a, &dir, &pre);
    if (dir.empty()) dir = "/tmp";
    if (pre.empty()) pre = "k32";
    std::string p = dir;
    if (p.back() != '/') p += '/';
    p += pre;
    p += "XXXXXX";
    std::vector<char> buf(p.begin(), p.end());
    buf.push_back(0);
#if defined(_WIN32) || defined(__wasi__)
    return ok(p);
#else
    int fd = mkstemp(buf.data());
    if (fd < 0) return fail(err("GetTempFileNameW"));
    close(fd);
    return ok(buf.data());
#endif
  }
  if (eq(api, "FindFirstFileW") || eq(api, "FindFirstFileA")) {
    std::string path = a;
    if (path.empty()) path = ".";
    if (path.size() >= 2 && path[path.size() - 1] == '*' &&
        (path[path.size() - 2] == '/' || path[path.size() - 2] == '\\')) {
      path.resize(path.size() - 1);
    }
    if (!path.empty() && (path.back() == '/' || path.back() == '\\')) path.pop_back();
    if (path.empty()) path = ".";
    DIR* d = opendir(path.c_str());
    if (!d) return fail(err("FindFirstFileW"));
    int h = k32_alloc_dir(d, path);
    if (h < 0) {
      closedir(d);
      return fail(err_msg("FindFirstFileW: no handles"));
    }
    struct dirent* e = readdir(d);
    if (!e) return ok(std::to_string(h) + "\x1f.\x1f" "16");
    unsigned attr = 0;
    std::string full = path + "/" + e->d_name;
    attr = file_attrs(full.c_str());
    if (attr == 0xffffffffu) attr = 0;
    return ok(std::to_string(h) + "\x1f" + e->d_name + "\x1f" + std::to_string(attr));
  }
  if (eq(api, "FindNextFileW") || eq(api, "FindNextFileA")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 2 || !k32_files()[h].d)
      return fail(err_msg("FindNextFileW: bad handle"));
    struct dirent* e = readdir(k32_files()[h].d);
    if (!e) return fail(err_msg("FindNextFileW: ERROR_NO_MORE_FILES"));
    std::string full = k32_files()[h].dir_path + "/" + e->d_name;
    unsigned attr = file_attrs(full.c_str());
    if (attr == 0xffffffffu) attr = 0;
    return ok(std::string(e->d_name) + "\x1f" + std::to_string(attr));
  }
  if (eq(api, "FindClose")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (k32_close_vol(h)) return ok("ok");
    if (h >= 3 && h < kK32Max && k32_files()[h].used == 2 && k32_files()[h].d) {
      closedir(k32_files()[h].d);
      k32_files()[h] = K32File{};
    }
    return ok("ok");
  }
  if (eq(api, "CloseHandle") || eq(api, "NtClose") || eq(api, "ZwClose")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (k32_close_proc(h) || k32_close_sync(h) || k32_close_thr(h) || k32_close_map(h) ||
        k32_close_iocp(h) || k32_close_job(h) || k32_close_vol(h) || k32_close_misc(h) ||
        k32_close_reg(h) || k32_close_tok(h) || k32_close_sock(h) || k32_close_cng(h) ||
        k32_close_cert(h) || k32_close_mod(h) || k32_close_http(h) || k32_close_gdi(h))
      return ok("ok");
    if (h >= 3 && h < kK32Max && k32_files()[h].used) {
      if (k32_files()[h].f) std::fclose(k32_files()[h].f);
      if (k32_files()[h].d) closedir(k32_files()[h].d);
      k32_files()[h] = K32File{};
    }
    return ok("ok");
  }
  if (eq(api, "DuplicateHandle")) {
#if defined(_WIN32)
    return fail(err_msg("DuplicateHandle: wasm hop"));
#else
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1 || !k32_files()[h].f)
      return fail(err_msg("DuplicateHandle: bad handle"));
    int fd = dup(fileno(k32_files()[h].f));
    if (fd < 0) return fail(err("DuplicateHandle"));
    FILE* f = fdopen(fd, "rb+");
    if (!f) {
      close(fd);
      return fail(err("DuplicateHandle"));
    }
    int nh = k32_alloc_file(f);
    if (nh < 0) {
      std::fclose(f);
      return fail(err_msg("DuplicateHandle: no handles"));
    }
    return ok(std::to_string(nh));
#endif
  }
  if (eq(api, "CreateEventW") || eq(api, "CreateEventA") || eq(api, "CreateEventExW")) {
    std::string manual, rest, initial;
    split1f(a, &manual, &rest);
    split1f(rest.c_str(), &initial, &rest);
    int kind = (std::strtol(manual.c_str(), nullptr, 10) != 0) ? kSyncEventManual
                                                               : kSyncEventAuto;
    int h = k32_alloc_sync(kind);
    if (h < 0) return fail(err_msg("CreateEventW: no handles"));
    K32Sync* s = k32_sync(h);
    s->signaled = std::strtol(initial.c_str(), nullptr, 10) != 0;
    s->name = rest;
    return ok(std::to_string(h));
  }
  if (eq(api, "OpenEventW") || eq(api, "OpenEventA")) {
    int h = k32_find_sync_name(a, kSyncEventAuto, kSyncEventManual);
    if (h < 0) return fail(err_msg("OpenEventW: not found"));
    return ok(std::to_string(h));
  }
  if (eq(api, "SetEvent") || eq(api, "PulseEvent")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s || (s->kind != kSyncEventAuto && s->kind != kSyncEventManual))
      return fail(err_msg("SetEvent: bad handle"));
    {
      std::lock_guard<std::mutex> lk(s->mu);
      s->signaled = 1;
    }
    s->cv.notify_all();
    if (eq(api, "PulseEvent")) {
      std::lock_guard<std::mutex> lk(s->mu);
      if (s->kind == kSyncEventAuto) s->signaled = 0;
    }
    return ok("ok");
  }
  if (eq(api, "ResetEvent")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s) return fail(err_msg("ResetEvent: bad handle"));
    std::lock_guard<std::mutex> lk(s->mu);
    s->signaled = 0;
    return ok("ok");
  }
  if (eq(api, "CreateMutexW") || eq(api, "CreateMutexA") || eq(api, "CreateMutexExW")) {
    std::string init, name;
    split1f(a, &init, &name);
    int h = k32_alloc_sync(kSyncMutex);
    if (h < 0) return fail(err_msg("CreateMutexW: no handles"));
    K32Sync* s = k32_sync(h);
    s->owned = (init[0] && std::strtol(init.c_str(), nullptr, 10) != 0) ? 1 : 0;
    s->name = name;
    return ok(std::to_string(h));
  }
  if (eq(api, "OpenMutexW") || eq(api, "OpenMutexA")) {
    int h = k32_find_sync_name(a, kSyncMutex, kSyncMutex);
    if (h < 0) return fail(err_msg("OpenMutexW: not found"));
    return ok(std::to_string(h));
  }
  if (eq(api, "ReleaseMutex")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s || s->kind != kSyncMutex) return fail(err_msg("ReleaseMutex: bad handle"));
    {
      std::lock_guard<std::mutex> lk(s->mu);
      s->owned = 0;
    }
    s->cv.notify_one();
    return ok("ok");
  }
  if (eq(api, "CreateSemaphoreW") || eq(api, "CreateSemaphoreA") ||
      eq(api, "CreateSemaphoreExW")) {
    std::string init, rest, maxc;
    split1f(a, &init, &rest);
    split1f(rest.c_str(), &maxc, &rest);
    int h = k32_alloc_sync(kSyncSem);
    if (h < 0) return fail(err_msg("CreateSemaphoreW: no handles"));
    K32Sync* s = k32_sync(h);
    s->count = static_cast<int>(std::strtol(init.c_str(), nullptr, 10));
    s->maxc = maxc.empty() ? 0x7fffffff : static_cast<int>(std::strtol(maxc.c_str(), nullptr, 10));
    if (s->count < 0) s->count = 0;
    s->name = rest;
    return ok(std::to_string(h));
  }
  if (eq(api, "OpenSemaphoreW") || eq(api, "OpenSemaphoreA")) {
    int h = k32_find_sync_name(a, kSyncSem, kSyncSem);
    if (h < 0) return fail(err_msg("OpenSemaphoreW: not found"));
    return ok(std::to_string(h));
  }
  if (eq(api, "ReleaseSemaphore")) {
    std::string hs, nstr;
    split1f(a, &hs, &nstr);
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!s || s->kind != kSyncSem) return fail(err_msg("ReleaseSemaphore: bad handle"));
    int n = nstr.empty() ? 1 : static_cast<int>(std::strtol(nstr.c_str(), nullptr, 10));
    {
      std::lock_guard<std::mutex> lk(s->mu);
      s->count += n;
      if (s->maxc && s->count > s->maxc) s->count = s->maxc;
    }
    s->cv.notify_all();
    return ok("ok");
  }
  if (eq(api, "WaitForMultipleObjects") || eq(api, "WaitForMultipleObjectsEx")) {
    std::vector<int> hs;
    std::vector<std::string> parts;
    std::string cur;
    for (const char* p = a;; ++p) {
      if (*p == '\x1f' || *p == 0) {
        parts.push_back(cur);
        if (*p == 0) break;
        cur.clear();
      } else {
        cur.push_back(*p);
      }
    }
    if (parts.size() < 3) return fail(err_msg("WaitForMultipleObjects"));
    int wait_all = std::strtol(parts.back().c_str(), nullptr, 10);
    unsigned long long ms = std::strtoull(parts[parts.size() - 2].c_str(), nullptr, 10);
    size_t start = 0;
    if (!parts[0].empty() && parts.size() >= 4 &&
        static_cast<size_t>(std::strtol(parts[0].c_str(), nullptr, 10)) == parts.size() - 3) {
      start = 1;
    }
    for (size_t i = start; i + 2 < parts.size(); ++i)
      hs.push_back(static_cast<int>(std::strtol(parts[i].c_str(), nullptr, 10)));
    if (hs.empty()) return fail(err_msg("WaitForMultipleObjects: no handles"));
    auto deadline = std::chrono::steady_clock::now() +
                    (ms == 0xffffffffull ? std::chrono::hours(24 * 365)
                                         : std::chrono::milliseconds(ms));
    for (;;) {
      if (wait_all) {
        bool all = true;
        for (int h : hs) {
          if (k32_peek(h) != 0) {
            all = false;
            break;
          }
        }
        if (all) {
          for (int h : hs) k32_wait_one(h, 0);
          return ok("0");
        }
      } else {
        for (size_t i = 0; i < hs.size(); ++i) {
          if (k32_peek(hs[i]) == 0) {
            k32_wait_one(hs[i], 0);
            return ok(std::to_string(static_cast<int>(i)));
          }
        }
      }
      if (ms == 0) return ok("258");
      if (ms != 0xffffffffull && std::chrono::steady_clock::now() >= deadline) return ok("258");
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  if (eq(api, "CreateThread")) {
    std::string api_name, arg;
    split1f(a, &api_name, &arg);
    if (api_name.empty()) return fail(err_msg("CreateThread: empty start"));
    int h = k32_alloc_thr();
    if (h < 0) return fail(err_msg("CreateThread: no handles"));
    K32Thr* t = k32_thr(h);
    try {
      t->thr = std::thread([t, api_name, arg]() {
        std::string reply = wasi_call(api_name.c_str(), arg.c_str());
        std::lock_guard<std::mutex> lk(t->mu);
        t->exit_code = (reply.rfind("error:", 0) == 0) ? 1 : 0;
        t->done = true;
        t->cv.notify_all();
      });
    } catch (...) {
      t->used = 0;
      return fail(err_msg("CreateThread: std::thread create failed"));
    }
    return ok(std::to_string(h) + "\x1f" + std::to_string(t->tid));
  }
  if (eq(api, "GetExitCodeThread")) {
    K32Thr* t = k32_thr(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!t) return fail(err_msg("GetExitCodeThread: bad handle"));
    std::lock_guard<std::mutex> lk(t->mu);
    return ok(std::to_string(t->exit_code));
  }
  if (eq(api, "GetThreadId")) {
    if (!a[0]) return ok("1");
    K32Thr* t = k32_thr(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!t) return fail(err_msg("GetThreadId: bad handle"));
    return ok(std::to_string(t->tid));
  }
  if (eq(api, "SetThreadDescription")) {
    std::string hs, name;
    split1f(a, &hs, &name);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (hs.empty() || h == -2) {
      k32_cur_thr_desc() = name;
      return ok("ok");
    }
    K32Thr* t = k32_thr(h);
    if (!t) return fail(err_msg("SetThreadDescription: bad handle"));
    t->desc = name;
    return ok("ok");
  }
  if (eq(api, "GetThreadDescription")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (!a[0] || h == -2) return ok(k32_cur_thr_desc());
    K32Thr* t = k32_thr(h);
    if (!t) return fail(err_msg("GetThreadDescription: bad handle"));
    return ok(t->desc);
  }
  if (eq(api, "GetCurrentThreadStackLimits"))
    return ok(std::string("65536") + "\x1f" + "8388608");
  if (eq(api, "ExitThread")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    (void)h;
    return ok("ok");
  }
  if (eq(api, "ResumeThread") || eq(api, "SuspendThread")) return ok("1");
  if (eq(api, "CreatePipe")) {
#if defined(_WIN32)
    return fail(err_msg("CreatePipe: use native hop"));
#else
    int fd[2];
    if (pipe(fd) != 0) return fail(err("CreatePipe"));
    FILE* r = fdopen(fd[0], "rb");
    FILE* w = fdopen(fd[1], "wb");
    if (!r || !w) {
      if (r) std::fclose(r);
      else close(fd[0]);
      if (w) std::fclose(w);
      else close(fd[1]);
      return fail(err_msg("CreatePipe: fdopen"));
    }
    int rh = k32_alloc_file(r, "pipe:r");
    int wh = k32_alloc_file(w, "pipe:w");
    if (rh < 0 || wh < 0) {
      std::fclose(r);
      std::fclose(w);
      return fail(err_msg("CreatePipe: no handles"));
    }
    return ok(std::to_string(rh) + "\x1f" + std::to_string(wh));
#endif
  }
  if (eq(api, "GetFileTime")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1)
      return fail(err_msg("GetFileTime: bad handle"));
    struct ::stat st {};
    int rc = -1;
    if (!k32_files()[h].path.empty()) rc = ::stat(k32_files()[h].path.c_str(), &st);
#if !defined(_WIN32)
    if (rc != 0 && k32_files()[h].f) rc = fstat(fileno(k32_files()[h].f), &st);
#endif
    if (rc != 0) return fail(err("GetFileTime"));
    std::string s = std::to_string(unix_to_ft(st.st_ctime));
    s += "\x1f";
    s += std::to_string(unix_to_ft(st.st_atime));
    s += "\x1f";
    s += std::to_string(unix_to_ft(st.st_mtime));
    return ok(s);
  }
  if (eq(api, "SetFileTime")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1 || k32_files()[h].path.empty())
      return fail(err_msg("SetFileTime: bad handle"));
#if defined(_WIN32)
    return fail(err_msg("SetFileTime: use native hop"));
#else
    (void)rest;
    return ok("ok");
#endif
  }
  if (eq(api, "GetFileInformationByHandle") || eq(api, "GetFileInformationByHandleEx")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1)
      return fail(err_msg("GetFileInformationByHandle: bad handle"));
    unsigned attr = 0;
    long long sz = 0;
    if (!k32_files()[h].path.empty()) {
      attr = file_attrs(k32_files()[h].path.c_str());
      struct ::stat st {};
      if (::stat(k32_files()[h].path.c_str(), &st) == 0) sz = st.st_size;
    }
    return ok(std::to_string(attr) + "\x1f" + std::to_string(sz));
  }
  if (eq(api, "SearchPathW") || eq(api, "SearchPathA")) {
    std::string r = k32_search_path(a);
    if (r.rfind("error:", 0) == 0) return fail(r);
    return ok(r);
  }
  if (eq(api, "GetDriveTypeW") || eq(api, "GetDriveTypeA")) {
    if (!a[0]) return ok("1");
    return ok("3");
  }
  if (eq(api, "QueryDosDeviceW")) return ok(a[0] ? a : "/");
  if (eq(api, "GetErrorMode")) return ok(std::to_string(k32_error_mode()));
  if (eq(api, "SetErrorMode")) {
    unsigned old = k32_error_mode();
    k32_error_mode() = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    return ok(std::to_string(old));
  }
  if (eq(api, "IsProcessorFeaturePresent")) return ok("0");
  if (eq(api, "GetProcessTimes")) {
    std::string now = filetime_now();
    return ok(now + "\x1f" + now + "\x1f" "0\x1f" "0");
  }
  if (eq(api, "GetPriorityClass") || eq(api, "GetThreadPriority")) return ok("32");
  if (eq(api, "SetPriorityClass") || eq(api, "SetThreadPriority")) return ok("ok");
  if (eq(api, "QueryFullProcessImageNameW")) {
    if (const char* p = std::getenv("_")) {
      if (p[0]) return ok(p);
    }
    return ok("main.wasm");
  }
  if (eq(api, "CopyFileExW")) {
    std::string src, dst;
    split1f(a, &src, &dst);
    std::string r = copy_file(src.c_str(), dst.c_str());
    if (r.rfind("error:", 0) == 0) return fail(r);
    return ok(r);
  }
  if (eq(api, "ReplaceFileW")) {
    std::string src, rest, dst;
    split1f(a, &src, &rest);
    split1f(rest.c_str(), &dst, &rest);
    if (src.empty() || dst.empty()) return fail(err_msg("ReplaceFileW"));
    if (rename(src.c_str(), dst.c_str()) != 0) return fail(err("ReplaceFileW"));
    return ok("ok");
  }
  if (eq(api, "GetCompressedFileSizeW")) {
    struct ::stat st {};
    if (::stat(a, &st) != 0) return fail(err("GetCompressedFileSizeW"));
    return ok(std::to_string(static_cast<long long>(st.st_size)));
  }
  if (eq(api, "GetCurrentDirectoryA")) return ok(cwd());
  if (eq(api, "SetCurrentDirectoryA")) {
    if (!a[0] || chdir(a) != 0) return fail(err("SetCurrentDirectoryA"));
    return ok("ok");
  }
  if (eq(api, "GetModuleFileNameA")) {
    if (const char* p = std::getenv("_")) {
      if (p[0]) return ok(p);
    }
    return ok("main.wasm");
  }
  if (eq(api, "GetFileAttributesA")) {
    unsigned attr = file_attrs(a);
    if (attr == 0xffffffffu) return fail(err("GetFileAttributesA"));
    return ok(std::to_string(attr));
  }
  if (eq(api, "GetEnvironmentVariableA")) {
    const char* v = std::getenv(a);
    if (!v) return fail(err_msg("GetEnvironmentVariableA: not found"));
    return ok(v);
  }
  if (eq(api, "GetTempPathA")) {
    if (const char* t = std::getenv("TMPDIR")) {
      if (t[0]) return ok(t);
    }
    return ok("/tmp");
  }
  if (eq(api, "GetWindowsDirectoryA") || eq(api, "GetSystemDirectoryA")) return ok(windir());
  if (eq(api, "lstrcmpiW") || eq(api, "lstrcmpiA") || eq(api, "CompareStringEx")) {
    std::string x, y;
    split1f(a, &x, &y);
    for (char& c : x)
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    for (char& c : y)
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    int c = x.compare(y);
    if (c < 0) return ok("1");
    if (c > 0) return ok("3");
    return ok("2");
  }
  if (eq(api, "CharUpperW") || eq(api, "CharUpperA")) {
    std::string s = a;
    for (char& c : s)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return ok(s);
  }
  if (eq(api, "CharLowerW") || eq(api, "CharLowerA")) {
    std::string s = a;
    for (char& c : s)
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return ok(s);
  }
  if (eq(api, "FreeEnvironmentStringsW") || eq(api, "FreeEnvironmentStringsA")) return ok("ok");
  if (eq(api, "DebugBreak") || eq(api, "FatalAppExitA") || eq(api, "FatalAppExitW")) return ok("ok");
  if (eq(api, "CreateFileMappingW") || eq(api, "CreateFileMappingA") ||
      eq(api, "CreateFileMappingNumaW")) {
    std::string hf, rest, prot, sz;
    split1f(a, &hf, &rest);
    split1f(rest.c_str(), &prot, &sz);
    unsigned long n = std::strtoul(sz.empty() ? prot.c_str() : sz.c_str(), nullptr, 10);
    int h = k32_alloc_map(n);
    if (h < 0) return fail(err_msg("CreateFileMappingW: oom"));
    return ok(std::to_string(h));
  }
  if (eq(api, "MapViewOfFile") || eq(api, "MapViewOfFileEx")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    K32Map* m = k32_map(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || !m->p) return fail(err_msg("MapViewOfFile: bad handle"));
    k32_vmem_add(m->p, m->n, 4);
    return ok(std::to_string(reinterpret_cast<uintptr_t>(m->p)));
  }
  if (eq(api, "UnmapViewOfFile")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    K32Map* t = k32_maps();
    for (int i = 0; i < kK32Max; ++i) {
      if (t[i].used && reinterpret_cast<uintptr_t>(t[i].p) == p) return ok("ok");
    }
    return ok("ok");
  }
  if (eq(api, "FlushViewOfFile")) return ok("ok");
  if (eq(api, "OpenFileMappingW") || eq(api, "OpenFileMappingA")) {
    return fail(err_msg("OpenFileMappingW: unnamed maps only"));
  }
  if (eq(api, "InitializeCriticalSection") ||
      eq(api, "InitializeCriticalSectionAndSpinCount") ||
      eq(api, "InitializeCriticalSectionEx")) {
    K32Cs* cs = new (std::nothrow) K32Cs();
    if (!cs) return fail(err_msg("InitializeCriticalSection: oom"));
    return ok(std::to_string(reinterpret_cast<uintptr_t>(cs)));
  }
  if (eq(api, "EnterCriticalSection")) {
    K32Cs* cs = reinterpret_cast<K32Cs*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!cs) return fail(err_msg("EnterCriticalSection"));
    cs->mu.lock();
    return ok("ok");
  }
  if (eq(api, "TryEnterCriticalSection")) {
    K32Cs* cs = reinterpret_cast<K32Cs*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!cs) return fail(err_msg("TryEnterCriticalSection"));
    return ok(cs->mu.try_lock() ? "1" : "0");
  }
  if (eq(api, "LeaveCriticalSection")) {
    K32Cs* cs = reinterpret_cast<K32Cs*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!cs) return fail(err_msg("LeaveCriticalSection"));
    cs->mu.unlock();
    return ok("ok");
  }
  if (eq(api, "DeleteCriticalSection")) {
    K32Cs* cs = reinterpret_cast<K32Cs*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    delete cs;
    return ok("ok");
  }
  if (eq(api, "CreateIoCompletionPort")) {
    std::string fh, rest, existing;
    split1f(a, &fh, &rest);
    split1f(rest.c_str(), &existing, &rest);
    int h = existing.empty() ? -1 : static_cast<int>(std::strtol(existing.c_str(), nullptr, 10));
    if (h < 0 || !k32_iocp(h)) h = k32_alloc_iocp();
    if (h < 0) return fail(err_msg("CreateIoCompletionPort: no handles"));
    int fileh = static_cast<int>(std::strtol(fh.c_str(), nullptr, 10));
    if (fileh >= 3 && fileh < kK32Max && k32_files()[fileh].used == 1) k32_files()[fileh].iocp = h;
    return ok(std::to_string(h));
  }
  if (eq(api, "PostQueuedCompletionStatus")) {
    std::string hs, rest, bytes, key;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &bytes, &key);
    K32Iocp* p = k32_iocp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p) return fail(err_msg("PostQueuedCompletionStatus: bad handle"));
    {
      std::lock_guard<std::mutex> lk(p->mu);
      p->q.push_back(bytes + "\x1f" + key);
    }
    p->cv.notify_one();
    return ok("ok");
  }
  if (eq(api, "GetQueuedCompletionStatus")) {
    std::string hs, toms;
    split1f(a, &hs, &toms);
    K32Iocp* p = k32_iocp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p) return fail(err_msg("GetQueuedCompletionStatus: bad handle"));
    unsigned long long ms = toms.empty() ? 0xffffffffull : std::strtoull(toms.c_str(), nullptr, 10);
    std::unique_lock<std::mutex> lk(p->mu);
    if (p->q.empty()) {
      if (ms == 0) return fail(err_msg("GetQueuedCompletionStatus: timeout"));
      if (ms == 0xffffffffull)
        p->cv.wait(lk, [p] { return !p->q.empty() || !p->used; });
      else if (!p->cv.wait_for(lk, std::chrono::milliseconds(ms),
                               [p] { return !p->q.empty() || !p->used; })) {
        return fail(err_msg("GetQueuedCompletionStatus: timeout"));
      }
    }
    if (p->q.empty()) return fail(err_msg("GetQueuedCompletionStatus: empty"));
    std::string item = p->q.front();
    p->q.pop_front();
    return ok(item);
  }
  if (eq(api, "CreateNamedPipeW") || eq(api, "CreateNamedPipeA")) {
#if defined(_WIN32)
    return fail(err_msg("CreateNamedPipeW: use native hop"));
#else
    int fd[2];
    if (pipe(fd) != 0) return fail(err("CreateNamedPipeW"));
    FILE* r = fdopen(fd[0], "rb");
    FILE* w = fdopen(fd[1], "wb");
    if (!r || !w) {
      if (r) std::fclose(r);
      else close(fd[0]);
      if (w) std::fclose(w);
      else close(fd[1]);
      return fail(err_msg("CreateNamedPipeW: fdopen"));
    }
    int rh = k32_alloc_file(r, a[0] ? a : "pipe");
    int wh = k32_alloc_file(w, "pipe:w");
    if (rh < 0 || wh < 0) return fail(err_msg("CreateNamedPipeW: no handles"));
    return ok(std::to_string(rh) + "\x1f" + std::to_string(wh));
#endif
  }
  if (eq(api, "ConnectNamedPipe") || eq(api, "DisconnectNamedPipe") ||
      eq(api, "WaitNamedPipeW"))
    return ok("ok");
  if (eq(api, "PeekNamedPipe")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1) return ok("0");
    return ok("0");
  }
  if (eq(api, "GetNamedPipeInfo")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h >= 3 && h < kK32Max && k32_files()[h].used == 1)
      return ok(std::string("0") + "\x1f" + "4096" + "\x1f" + "4096" + "\x1f" + "1");
    return ok(std::string("0") + "\x1f" + "4096" + "\x1f" + "4096" + "\x1f" + "1");
  }
  if (eq(api, "GetNamedPipeHandleStateW") || eq(api, "GetNamedPipeHandleStateA"))
    return ok(std::string("0") + "\x1f" + "1" + "\x1f" + "0" + "\x1f" + "0");
  if (eq(api, "SetNamedPipeHandleState")) return ok("ok");
  if (eq(api, "TransactNamedPipe")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    return ok(data);
  }
  if (eq(api, "CallNamedPipeW") || eq(api, "CallNamedPipeA")) {
    std::string name, data;
    split1f(a, &name, &data);
    (void)name;
    return ok(data);
  }
  if (eq(api, "GetNamedPipeClientProcessId") || eq(api, "GetNamedPipeServerProcessId") ||
      eq(api, "GetNamedPipeClientSessionId") || eq(api, "GetNamedPipeServerSessionId"))
    return ok(std::to_string(static_cast<long>(getpid())));
  if (eq(api, "LockFile") || eq(api, "LockFileEx") || eq(api, "UnlockFile") ||
      eq(api, "UnlockFileEx"))
    return ok("ok");
  if (eq(api, "CancelIo") || eq(api, "CancelIoEx")) return ok("ok");
  if (eq(api, "SetFileCompletionNotificationModes")) {
    std::string hs, flags;
    split1f(a, &hs, &flags);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (h < 3 || h >= kK32Max || k32_files()[h].used != 1)
      return fail(err_msg("SetFileCompletionNotificationModes: bad handle"));
    k32_files()[h].comp_modes = static_cast<unsigned>(std::strtoul(flags.c_str(), nullptr, 0));
    return ok("ok");
  }
  if (eq(api, "DeviceIoControl")) {
    std::string hs, rest, code, input;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &code, &input);
    unsigned ioctl = static_cast<unsigned>(std::strtoul(code.c_str(), nullptr, 0));
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (ioctl == kFsctlPipePeek) {
      if (h >= 3 && h < kK32Max && k32_files()[h].used == 1) return ok("0");
      return ok("0");
    }
    std::string r = wasi_device_ioctl(ioctl, input);
    if (r.empty()) return fail(err_msg("DeviceIoControl: no device"));
    return ok(r);
  }
  if (eq(api, "GetOverlappedResult") || eq(api, "GetOverlappedResultEx")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (h >= 3 && h < kK32Max && k32_files()[h].used == 1)
      return ok(std::to_string(k32_files()[h].last_io));
    return ok("0");
  }
  if (eq(api, "CreateHardLinkW") || eq(api, "CreateHardLinkA")) {
    std::string neu, existing;
    split1f(a, &neu, &existing);
#if defined(_WIN32)
    return fail(err_msg("CreateHardLinkW: use native hop"));
#else
    if (link(existing.c_str(), neu.c_str()) != 0) return fail(err("CreateHardLinkW"));
    return ok("ok");
#endif
  }
  if (eq(api, "CreateSymbolicLinkW") || eq(api, "CreateSymbolicLinkA")) {
    std::string neu, existing;
    split1f(a, &neu, &existing);
#if defined(_WIN32)
    return fail(err_msg("CreateSymbolicLinkW: use native hop"));
#else
    if (symlink(existing.c_str(), neu.c_str()) != 0) return fail(err("CreateSymbolicLinkW"));
    return ok("ok");
#endif
  }
  if (eq(api, "FindFirstFileExW") || eq(api, "FindFirstFileExA")) {
    return try_kernel32("FindFirstFileW", a, out);
  }
  if (eq(api, "GetVolumePathNameW") || eq(api, "GetVolumePathNameA")) return ok("/");
  if (eq(api, "SetVolumeLabelW")) return ok("ok");
  if (eq(api, "WritePrivateProfileStringW") || eq(api, "WritePrivateProfileStringA")) {
    std::string sec, rest, key, valfile, val, file;
    split1f(a, &sec, &rest);
    split1f(rest.c_str(), &key, &valfile);
    split1f(valfile.c_str(), &val, &file);
    if (sec.empty() || key.empty()) return fail(err_msg("WritePrivateProfileStringW"));
    k32_ini()[file + "\x1e" + sec + "\x1e" + key] = val;
    return ok("ok");
  }
  if (eq(api, "GetPrivateProfileStringW") || eq(api, "GetPrivateProfileStringA")) {
    std::string sec, rest, key, deffile, def, file;
    split1f(a, &sec, &rest);
    split1f(rest.c_str(), &key, &deffile);
    split1f(deffile.c_str(), &def, &file);
    auto it = k32_ini().find(file + "\x1e" + sec + "\x1e" + key);
    if (it == k32_ini().end()) return ok(def);
    return ok(it->second);
  }
  if (eq(api, "GetPrivateProfileIntW") || eq(api, "GetPrivateProfileIntA")) {
    std::string sec, rest, key, deffile, def, file;
    split1f(a, &sec, &rest);
    split1f(rest.c_str(), &key, &deffile);
    split1f(deffile.c_str(), &def, &file);
    auto it = k32_ini().find(file + "\x1e" + sec + "\x1e" + key);
    if (it == k32_ini().end()) return ok(def.empty() ? "0" : def);
    return ok(it->second);
  }
  if (eq(api, "GlobalAddAtomW") || eq(api, "GlobalAddAtomA") || eq(api, "AddAtomW") ||
      eq(api, "AddAtomA")) {
    if (!a[0]) return fail(err_msg("GlobalAddAtomW"));
    std::string* at = k32_atoms();
    for (int i = 0; i < 256; ++i) {
      if (at[i] == a) return ok(std::to_string(0xC000 + i));
    }
    for (int i = 0; i < 256; ++i) {
      if (at[i].empty()) {
        at[i] = a;
        return ok(std::to_string(0xC000 + i));
      }
    }
    return fail(err_msg("GlobalAddAtomW: full"));
  }
  if (eq(api, "GlobalFindAtomW") || eq(api, "GlobalFindAtomA") || eq(api, "FindAtomW")) {
    std::string* at = k32_atoms();
    for (int i = 0; i < 256; ++i) {
      if (at[i] == a) return ok(std::to_string(0xC000 + i));
    }
    return fail(err_msg("GlobalFindAtomW: not found"));
  }
  if (eq(api, "GlobalGetAtomNameW") || eq(api, "GlobalGetAtomNameA")) {
    int i = static_cast<int>(std::strtol(a, nullptr, 10)) - 0xC000;
    if (i < 0 || i >= 256 || k32_atoms()[i].empty()) return fail(err_msg("GlobalGetAtomNameW"));
    return ok(k32_atoms()[i]);
  }
  if (eq(api, "GlobalDeleteAtom") || eq(api, "DeleteAtom")) {
    int i = static_cast<int>(std::strtol(a, nullptr, 10)) - 0xC000;
    if (i >= 0 && i < 256) k32_atoms()[i].clear();
    return ok("ok");
  }
  if (eq(api, "CreateToolhelp32Snapshot")) return ok("900");
  if (eq(api, "Process32FirstW") || eq(api, "Process32First") || eq(api, "Process32FirstA")) {
    return ok(std::to_string(static_cast<long>(getpid())) + "\x1fmain.wasm");
  }
  if (eq(api, "Process32NextW") || eq(api, "Process32Next") || eq(api, "Process32NextA")) {
    return fail(err_msg("Process32NextW: ERROR_NO_MORE_FILES"));
  }
  if (eq(api, "Module32FirstW") || eq(api, "Module32First")) return ok("main.wasm");
  if (eq(api, "Module32NextW") || eq(api, "Thread32Next") || eq(api, "Thread32NextW")) {
    return fail(err_msg("ERROR_NO_MORE_FILES"));
  }
  if (eq(api, "Thread32First") || eq(api, "Thread32FirstW")) return ok("1");
  if (eq(api, "MulDiv")) {
    std::string n, rest, num, den;
    split1f(a, &n, &rest);
    split1f(rest.c_str(), &num, &den);
    long long v = std::strtoll(n.c_str(), nullptr, 10) * std::strtoll(num.c_str(), nullptr, 10);
    long long d = std::strtoll(den.c_str(), nullptr, 10);
    if (!d) return fail(err_msg("MulDiv: divide by zero"));
    return ok(std::to_string(v / d));
  }
  if (eq(api, "GetCPInfo")) return ok("4\x1f?\x1f" "65001");
  if (eq(api, "IsValidCodePage")) {
    unsigned cp = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    return ok(cp == 65001 || cp == 437 || cp == 1252 || cp == 1200 || cp == 0 ? "1" : "0");
  }
  if (eq(api, "GetDateFormatW") || eq(api, "GetDateFormatA") || eq(api, "GetTimeFormatW") ||
      eq(api, "GetTimeFormatA") || eq(api, "GetDateFormatEx") || eq(api, "GetTimeFormatEx")) {
    return ok(system_time(true));
  }
  if (eq(api, "GetNumberFormatW") || eq(api, "GetCurrencyFormatW")) return ok(a);
  if (eq(api, "LCMapStringW") || eq(api, "LCMapStringEx") || eq(api, "FoldStringW")) return ok(a);
  if (eq(api, "GetStringTypeW") || eq(api, "GetStringTypeExW")) return ok("1");
  if (eq(api, "GetUserDefaultUILanguage") || eq(api, "GetSystemDefaultUILanguage"))
    return ok("1033");
  if (eq(api, "lstrcpyW") || eq(api, "lstrcpyA")) return ok(a);
  if (eq(api, "lstrcatW") || eq(api, "lstrcatA")) {
    std::string x, y;
    split1f(a, &x, &y);
    return ok(x + y);
  }
  if (eq(api, "lstrcpynW") || eq(api, "lstrcpynA")) {
    std::string s, nstr;
    split1f(a, &s, &nstr);
    unsigned n = static_cast<unsigned>(std::strtoul(nstr.c_str(), nullptr, 10));
    if (n && s.size() > n) s.resize(n);
    return ok(s);
  }
  if (eq(api, "FlsAlloc")) return try_kernel32("TlsAlloc", a, out);
  if (eq(api, "FlsFree")) return try_kernel32("TlsFree", a, out);
  if (eq(api, "FlsSetValue")) return try_kernel32("TlsSetValue", a, out);
  if (eq(api, "FlsGetValue")) return try_kernel32("TlsGetValue", a, out);
  if (eq(api, "CheckRemoteDebuggerPresent")) return ok("0");
  if (eq(api, "IsWow64Process") || eq(api, "IsWow64Process2")) return ok("0");
  if (eq(api, "GetSystemWow64DirectoryW") || eq(api, "GetSystemWow64DirectoryA")) return ok("");
  if (eq(api, "Wow64DisableWow64FsRedirection") || eq(api, "Wow64RevertWow64FsRedirection"))
    return ok("ok");
  if (eq(api, "GetProcessHandleCount")) return ok("16");
  if (eq(api, "GetProcessAffinityMask") || eq(api, "GetProcessAffinityMaskW")) return ok("1\x1f" "1");
  if (eq(api, "SetProcessAffinityMask") || eq(api, "SetThreadAffinityMask")) return ok("1");
  if (eq(api, "GetSystemTimes")) {
    std::string now = filetime_now();
    return ok(now + "\x1f" + now + "\x1f" + now);
  }
  if (eq(api, "QueryUnbiasedInterruptTime") || eq(api, "QueryInterruptTime")) {
    return ok(std::to_string(std::strtoull(tick_ms().c_str(), nullptr, 10) * 10000ull));
  }
  if (eq(api, "GetNumaHighestNodeNumber")) return ok("0");
  if (eq(api, "GetProductInfo")) return ok("48");
  if (eq(api, "GetProcessVersion")) return ok("0x0A000000");
  if (eq(api, "NeedCurrentDirectoryForExePathW") || eq(api, "NeedCurrentDirectoryForExePathA"))
    return ok("1");
  if (eq(api, "GetDllDirectoryW") || eq(api, "GetDllDirectoryA")) return ok(k32_dll_dir());
  if (eq(api, "SetDllDirectoryW") || eq(api, "SetDllDirectoryA")) {
    k32_dll_dir() = a;
    return ok("ok");
  }
  if (eq(api, "AddDllDirectory") || eq(api, "SetDefaultDllDirectories") ||
      eq(api, "RemoveDllDirectory") || eq(api, "SetSearchPathMode"))
    return ok("1");
  if (eq(api, "GetModuleHandleA")) {
    if (!a[0]) return ok("1");
    int h = k32_load_library(a);
    if (h < 0) return fail(err_msg("GetModuleHandleA: module not found"));
    return ok(std::to_string(h));
  }
  if (eq(api, "GetStartupInfoA")) return ok("wshow=1");
  if (eq(api, "SetFileApisToOEM") || eq(api, "SetFileApisToANSI")) return ok("ok");
  if (eq(api, "GetLargestConsoleWindowSize")) return ok("80\x1f" "25");
  if (eq(api, "GetConsoleScreenBufferInfo") || eq(api, "GetConsoleScreenBufferInfoEx")) {
    return ok("80\x1f" "25\x1f" "0\x1f" "0\x1f" "7");
  }
  if (eq(api, "GetConsoleCursorInfo")) return ok("25\x1f" "1");
  if (eq(api, "PeekConsoleInputW") || eq(api, "PeekConsoleInputA") ||
      eq(api, "ReadConsoleInputW") || eq(api, "ReadConsoleInputA"))
    return ok("0");
  if (eq(api, "FillConsoleOutputCharacterW") || eq(api, "FillConsoleOutputCharacterA") ||
      eq(api, "FillConsoleOutputAttribute"))
    return ok("1");
  if (eq(api, "SetConsoleTextAttribute") || eq(api, "SetConsoleCursorPosition") ||
      eq(api, "SetConsoleCursorInfo") || eq(api, "ScrollConsoleScreenBufferW") ||
      eq(api, "SetConsoleWindowInfo") || eq(api, "SetConsoleScreenBufferSize") ||
      eq(api, "WriteConsoleOutputW") || eq(api, "GenerateConsoleCtrlEvent") ||
      eq(api, "SetConsoleCtrlHandler") || eq(api, "FlushConsoleInputBuffer"))
    return ok("ok");
  if (eq(api, "FindResourceW") || eq(api, "FindResourceA") || eq(api, "FindResourceExW") ||
      eq(api, "LoadResource") || eq(api, "LockResource") || eq(api, "SizeofResource")) {
    return fail(err_msg("FindResourceW: no resource directory in this image"));
  }
  if (eq(api, "LoadLibraryExA")) {
    std::string path, rest;
    split1f(a, &path, &rest);
    int h = k32_load_library(path.empty() ? a : path.c_str());
    if (h < 0) return fail(err_msg("LoadLibraryExA: module not found"));
    return ok(std::to_string(h));
  }
  if (eq(api, "GetModuleHandleExA")) {
    if (!a[0]) return ok("1");
    int h = k32_load_library(a);
    if (h < 0) return fail(err_msg("GetModuleHandleExA: module not found"));
    return ok(std::to_string(h));
  }
  if (eq(api, "GetLogicalProcessorInformation") ||
      eq(api, "GetLogicalProcessorInformationEx"))
    return ok("1");
  if (eq(api, "VerifyVersionInfoW") || eq(api, "VerifyVersionInfoA")) return ok("1");
  if (eq(api, "GetBinaryTypeW") || eq(api, "GetBinaryTypeA")) return ok("0");
  if (eq(api, "MoveFileWithProgressW")) {
    std::string src, dst;
    split1f(a, &src, &dst);
    if (rename(src.c_str(), dst.c_str()) != 0) return fail(err("MoveFileWithProgressW"));
    return ok("ok");
  }
  if (eq(api, "GetProcessWorkingSetSize") || eq(api, "SetProcessWorkingSetSize")) return ok("ok");
  if (eq(api, "GetProcessIoCounters")) return ok("0\x1f" "0\x1f" "0\x1f" "0");
  if (eq(api, "QueryDosDeviceA") || eq(api, "QueryDosDeviceW")) return ok(a[0] ? a : "/");
  if (eq(api, "CreateWaitableTimerW") || eq(api, "CreateWaitableTimerA") ||
      eq(api, "CreateWaitableTimerExW")) {
    std::string manual, name;
    split1f(a, &manual, &name);
    int kind = (manual[0] && std::strtol(manual.c_str(), nullptr, 10) != 0) ? kSyncTimerManual
                                                                           : kSyncTimerAuto;
    int h = k32_alloc_sync(kind);
    if (h < 0) return fail(err_msg("CreateWaitableTimerW: no handles"));
    k32_sync(h)->name = name;
    return ok(std::to_string(h));
  }
  if (eq(api, "OpenWaitableTimerW") || eq(api, "OpenWaitableTimerA")) {
    int h = k32_find_sync_name(a, kSyncTimerAuto, kSyncTimerManual);
    if (h < 0) return fail(err_msg("OpenWaitableTimerW: not found"));
    return ok(std::to_string(h));
  }
  if (eq(api, "SetWaitableTimer") || eq(api, "SetWaitableTimerEx")) {
    std::string hs, ms;
    split1f(a, &hs, &ms);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Sync* s = k32_sync(h);
    if (!s || (s->kind != kSyncTimerAuto && s->kind != kSyncTimerManual))
      return fail(err_msg("SetWaitableTimer: bad handle"));
    unsigned gen = 0;
    {
      std::lock_guard<std::mutex> lk(s->mu);
      gen = ++s->timer_gen;
      s->signaled = 0;
    }
    unsigned long due = std::strtoul(ms.c_str(), nullptr, 10);
    try {
      std::thread([h, gen, due]() {
        if (due) std::this_thread::sleep_for(std::chrono::milliseconds(due));
        K32Sync* t = k32_sync(h);
        if (!t) return;
        std::lock_guard<std::mutex> lk(t->mu);
        if (t->used && t->timer_gen == gen) {
          t->signaled = 1;
          t->cv.notify_all();
        }
      }).detach();
    } catch (...) {
      return fail(err_msg("SetWaitableTimer: std::thread create failed"));
    }
    return ok("ok");
  }
  if (eq(api, "CancelWaitableTimer")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s || (s->kind != kSyncTimerAuto && s->kind != kSyncTimerManual))
      return fail(err_msg("CancelWaitableTimer: bad handle"));
    std::lock_guard<std::mutex> lk(s->mu);
    s->timer_gen++;
    s->signaled = 0;
    return ok("ok");
  }
  if (eq(api, "CreateJobObjectW") || eq(api, "CreateJobObjectA")) {
    int h = k32_alloc_job();
    if (h < 0) return fail(err_msg("CreateJobObjectW: no handles"));
    k32_job(h)->name = a;
    return ok(std::to_string(h));
  }
  if (eq(api, "OpenJobObjectW") || eq(api, "OpenJobObjectA")) {
    if (!a[0]) return fail(err_msg("OpenJobObjectW: empty name"));
    K32Job* t = k32_jobs();
    for (int i = 0; i < kK32Max; ++i) {
      if (t[i].used && t[i].name == a) return ok(std::to_string(kK32JobBase + i));
    }
    return fail(err_msg("OpenJobObjectW: not found"));
  }
  if (eq(api, "AssignProcessToJobObject")) {
    std::string jh, ph;
    split1f(a, &jh, &ph);
    K32Job* j = k32_job(static_cast<int>(std::strtol(jh.c_str(), nullptr, 10)));
    if (!j) return fail(err_msg("AssignProcessToJobObject: bad job"));
    unsigned pid = static_cast<unsigned>(getpid());
    int phn = static_cast<int>(std::strtol(ph.c_str(), nullptr, 10));
    if (K32Proc* p = k32_proc(phn)) pid = p->pid;
    else if (phn > 0 && phn != -1) pid = static_cast<unsigned>(phn);
    for (unsigned x : j->pids) {
      if (x == pid) return ok("ok");
    }
    j->pids.push_back(pid);
    return ok("ok");
  }
  if (eq(api, "IsProcessInJob")) {
    std::string ph, jh;
    split1f(a, &ph, &jh);
    unsigned pid = static_cast<unsigned>(getpid());
    int phn = static_cast<int>(std::strtol(ph.c_str(), nullptr, 10));
    if (K32Proc* p = k32_proc(phn)) pid = p->pid;
    else if (phn > 0 && phn != -1) pid = static_cast<unsigned>(phn);
    if (!jh.empty()) {
      K32Job* j = k32_job(static_cast<int>(std::strtol(jh.c_str(), nullptr, 10)));
      if (!j) return fail(err_msg("IsProcessInJob: bad job"));
      for (unsigned x : j->pids) {
        if (x == pid) return ok("1");
      }
      return ok("0");
    }
    K32Job* t = k32_jobs();
    for (int i = 0; i < kK32Max; ++i) {
      if (!t[i].used) continue;
      for (unsigned x : t[i].pids) {
        if (x == pid) return ok("1");
      }
    }
    return ok("0");
  }
  if (eq(api, "TerminateJobObject")) {
    std::string jh, code;
    split1f(a, &jh, &code);
    K32Job* j = k32_job(static_cast<int>(std::strtol(jh.c_str(), nullptr, 10)));
    if (!j) return fail(err_msg("TerminateJobObject: bad job"));
    int exit_code = code.empty() ? 1 : static_cast<int>(std::strtol(code.c_str(), nullptr, 10));
    unsigned self = static_cast<unsigned>(getpid());
    for (unsigned pid : j->pids) {
      if (pid == self) continue;
      K32Proc* procs = k32_procs();
      for (int i = 0; i < kK32Max; ++i) {
        if (!procs[i].used || procs[i].pid != pid) continue;
        std::lock_guard<std::mutex> lk(procs[i].mu);
        procs[i].exit_code = exit_code;
        procs[i].done = true;
        procs[i].cv.notify_all();
      }
    }
    return ok("ok");
  }
  if (eq(api, "QueryInformationJobObject")) {
    K32Job* j = k32_job(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!j) return fail(err_msg("QueryInformationJobObject: bad job"));
    std::string s = std::to_string(j->pids.size());
    for (unsigned pid : j->pids) s += std::string("\x1f") + std::to_string(pid);
    return ok(s);
  }
  if (eq(api, "SetInformationJobObject")) return ok("ok");
  if (eq(api, "OpenThread")) {
    unsigned tid = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    if (!a[0] || tid == 1) return ok("-2");
    K32Thr* t = k32_thrs();
    for (int i = 0; i < kK32Max; ++i) {
      if (t[i].used && t[i].tid == tid) return ok(std::to_string(kK32ThrBase + i));
    }
    return fail(err_msg("OpenThread: guest tids only"));
  }
  if (eq(api, "GetThreadTimes") || eq(api, "GetProcessTimes")) {
    std::string ft = filetime_now();
    return ok(ft + "\x1f" + ft + "\x1f" + ft + "\x1f" + ft);
  }
  if (eq(api, "GetProcessIdOfThread")) {
    if (!a[0] || std::strtol(a, nullptr, 10) == -2) return ok(std::to_string(static_cast<long>(getpid())));
    K32Thr* t = k32_thr(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!t) return fail(err_msg("GetProcessIdOfThread: bad handle"));
    return ok(std::to_string(static_cast<long>(getpid())));
  }
  if (eq(api, "SignalObjectAndWait")) {
    std::string h1, rest, h2, ms;
    split1f(a, &h1, &rest);
    split1f(rest.c_str(), &h2, &ms);
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(h1.c_str(), nullptr, 10)));
    if (s && (s->kind == kSyncEventAuto || s->kind == kSyncEventManual ||
              s->kind == kSyncTimerAuto || s->kind == kSyncTimerManual)) {
      {
        std::lock_guard<std::mutex> lk(s->mu);
        s->signaled = 1;
      }
      s->cv.notify_all();
    }
    unsigned long long wait_ms = ms.empty() ? 0xffffffffull : std::strtoull(ms.c_str(), nullptr, 10);
    int w = k32_wait_one(static_cast<int>(std::strtol(h2.c_str(), nullptr, 10)), wait_ms);
    if (w < 0) return fail(err_msg("SignalObjectAndWait: bad handle"));
    return ok(std::to_string(w));
  }
  if (eq(api, "InitializeSRWLock")) {
    K32Srw* p = new (std::nothrow) K32Srw();
    if (!p) return fail(err_msg("InitializeSRWLock: oom"));
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (eq(api, "AcquireSRWLockExclusive") || eq(api, "AcquireSRWLockShared")) {
    K32Srw* p = reinterpret_cast<K32Srw*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!p) return fail(err_msg(api));
    p->mu.lock();
    return ok("ok");
  }
  if (eq(api, "TryAcquireSRWLockExclusive") || eq(api, "TryAcquireSRWLockShared")) {
    K32Srw* p = reinterpret_cast<K32Srw*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!p) return fail(err_msg(api));
    return ok(p->mu.try_lock() ? "1" : "0");
  }
  if (eq(api, "ReleaseSRWLockExclusive") || eq(api, "ReleaseSRWLockShared")) {
    K32Srw* p = reinterpret_cast<K32Srw*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!p) return fail(err_msg(api));
    p->mu.unlock();
    return ok("ok");
  }
  if (eq(api, "InitializeConditionVariable")) {
    K32Cv* p = new (std::nothrow) K32Cv();
    if (!p) return fail(err_msg("InitializeConditionVariable: oom"));
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (eq(api, "WakeConditionVariable")) {
    K32Cv* p = reinterpret_cast<K32Cv*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!p) return fail(err_msg("WakeConditionVariable"));
    p->cv.notify_one();
    return ok("ok");
  }
  if (eq(api, "WakeAllConditionVariable")) {
    K32Cv* p = reinterpret_cast<K32Cv*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!p) return fail(err_msg("WakeAllConditionVariable"));
    p->cv.notify_all();
    return ok("ok");
  }
  if (eq(api, "SleepConditionVariableCS") || eq(api, "SleepConditionVariableSRW")) {
    std::string cvh, rest, csh, ms;
    split1f(a, &cvh, &rest);
    split1f(rest.c_str(), &csh, &ms);
    K32Cv* cv = reinterpret_cast<K32Cv*>(static_cast<uintptr_t>(std::strtoull(cvh.c_str(), nullptr, 10)));
    if (!cv) return fail(err_msg(api));
    unsigned long wait_ms = ms.empty() ? 0xfffffffful : std::strtoul(ms.c_str(), nullptr, 10);
    std::mutex* mu = nullptr;
    if (eq(api, "SleepConditionVariableCS")) {
      K32Cs* cs = reinterpret_cast<K32Cs*>(static_cast<uintptr_t>(std::strtoull(csh.c_str(), nullptr, 10)));
      if (!cs) return fail(err_msg(api));
      mu = &cs->mu;
    } else {
      K32Srw* srw = reinterpret_cast<K32Srw*>(static_cast<uintptr_t>(std::strtoull(csh.c_str(), nullptr, 10)));
      if (!srw) return fail(err_msg(api));
      mu = &srw->mu;
    }
    std::unique_lock<std::mutex> lk(*mu, std::adopt_lock);
    bool woke = true;
    if (wait_ms == 0xfffffffful)
      cv->cv.wait(lk);
    else
      woke = cv->cv.wait_for(lk, std::chrono::milliseconds(wait_ms)) == std::cv_status::no_timeout;
    lk.release();
    return ok(woke ? "1" : "0");
  }
  if (eq(api, "InitOnceInitialize")) {
    int* p = new (std::nothrow) int(0);
    if (!p) return fail(err_msg("InitOnceInitialize: oom"));
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (eq(api, "InitOnceExecuteOnce") || eq(api, "InitOnceBeginInitialize")) {
    int* p = reinterpret_cast<int*>(static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!p) return fail(err_msg(api));
    if (*p) return ok("0");
    *p = 1;
    return ok("1");
  }
  if (eq(api, "InitOnceComplete")) return ok("ok");
  if (eq(api, "QueueUserWorkItem")) {
    try {
      std::thread([]() {}).detach();
    } catch (...) {
      return fail(err_msg("QueueUserWorkItem: std::thread create failed"));
    }
    return ok("1");
  }
  if (eq(api, "HeapValidate") || eq(api, "HeapLock") || eq(api, "HeapUnlock")) return ok("1");
  if (eq(api, "HeapCompact")) return ok("0");
  if (eq(api, "VirtualLock") || eq(api, "VirtualUnlock") || eq(api, "FlushProcessWriteBuffers"))
    return ok("ok");
  if (eq(api, "GetProcessHeaps")) {
    uintptr_t h = k32_process_heap();
    return ok(std::string("1\x1f") + std::to_string(h));
  }
  if (eq(api, "GetLargePageMinimum")) return ok("2097152");
  if (eq(api, "GetPhysicallyInstalledSystemMemory")) return ok("1048576");
  if (eq(api, "QueryProcessCycleTime") || eq(api, "QueryThreadCycleTime")) return ok("0");
  if (eq(api, "GetThreadIOPendingFlag")) return ok("0");
  if (eq(api, "GetSystemDEPPolicy")) return ok("0");
  if (eq(api, "SetThreadExecutionState")) return ok("2147483648");
  if (eq(api, "CreateMemoryResourceNotification")) {
    int h = k32_alloc_vol(3, "mem");
    if (h < 0) return fail(err_msg("CreateMemoryResourceNotification: no handles"));
    return ok(std::to_string(h));
  }
  if (eq(api, "QueryMemoryResourceNotification")) return ok("0");
  if (eq(api, "CaptureStackBackTrace") || eq(api, "RtlCaptureStackBackTrace")) return ok("0");
  if (eq(api, "SetUnhandledExceptionFilter")) return ok("0");
  if (eq(api, "NeedCurrentDirectoryForExePathW") || eq(api, "NeedCurrentDirectoryForExePathA"))
    return ok("1");
  if (eq(api, "FindFirstVolumeW") || eq(api, "FindFirstVolumeA")) {
    std::string name = "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\";
    int h = k32_alloc_vol(1, name);
    if (h < 0) return fail(err_msg("FindFirstVolumeW: no handles"));
    return ok(std::to_string(h) + "\x1f" + name);
  }
  if (eq(api, "FindNextVolumeW") || eq(api, "FindNextVolumeA")) {
    K32Vol* v = k32_vol(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!v || v->kind != 1) return fail(err_msg("FindNextVolumeW: bad handle"));
    return fail(err_msg("FindNextVolumeW: ERROR_NO_MORE_FILES"));
  }
  if (eq(api, "FindVolumeClose")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (!k32_close_vol(h)) return fail(err_msg("FindVolumeClose: bad handle"));
    return ok("ok");
  }
  if (eq(api, "GetVolumeNameForVolumeMountPointW") || eq(api, "GetVolumeNameForVolumeMountPointA"))
    return ok("\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\");
  if (eq(api, "GetVolumePathNamesForVolumeNameW") || eq(api, "GetVolumePathNamesForVolumeNameA"))
    return ok("/");
  if (eq(api, "GetVolumePathNameW") || eq(api, "GetVolumePathNameA")) return ok(a[0] ? a : "/");
  if (eq(api, "DefineDosDeviceW") || eq(api, "DefineDosDeviceA")) return ok("ok");
  if (eq(api, "GetVolumeInformationByHandleW")) return ok("wasigocvm\x1f" "0\x1f" "255\x1f" "0\x1fwasifs");
  if (eq(api, "FindFirstStreamW") || eq(api, "FindFirstStreamA")) {
    int h = k32_alloc_vol(2, "::$DATA");
    if (h < 0) return fail(err_msg("FindFirstStreamW: no handles"));
    return ok(std::to_string(h) + "\x1f::$DATA");
  }
  if (eq(api, "FindNextStreamW") || eq(api, "FindNextStreamA"))
    return fail(err_msg("FindNextStreamW: ERROR_NO_MORE_FILES"));
  if (eq(api, "GetSystemPowerStatus")) return ok("1\x1f" "255\x1f-1\x1f-1");
  if (eq(api, "GetConsoleWindow")) return ok("0");
  if (eq(api, "GetConsoleOriginalTitleW") || eq(api, "GetConsoleOriginalTitleA"))
    return ok("wasigocvm");
  if (eq(api, "GetConsoleProcessList"))
    return ok(std::string("1\x1f") + std::to_string(static_cast<long>(getpid())));
  if (eq(api, "CreateConsoleScreenBuffer")) return ok("-11");
  if (eq(api, "SetConsoleActiveScreenBuffer")) return ok("ok");
  if (eq(api, "CreateDirectoryExW") || eq(api, "CreateDirectoryExA")) {
    std::string tmpl, path;
    split1f(a, &tmpl, &path);
    if (path.empty()) path = tmpl;
    if (path.empty()) return fail(err_msg("CreateDirectoryExW: empty path"));
    if (mkdir(path.c_str(), 0777) != 0) return fail(err("CreateDirectoryExW"));
    return ok("ok");
  }
  if (eq(api, "CompareStringOrdinal")) {
    std::string l, rest, r, ign;
    split1f(a, &l, &rest);
    split1f(rest.c_str(), &r, &ign);
    int c = 0;
    if (ign.size() && std::strtol(ign.c_str(), nullptr, 10) != 0) {
      for (size_t i = 0;; ++i) {
        unsigned char x = static_cast<unsigned char>(l[i]);
        unsigned char y = static_cast<unsigned char>(r[i]);
        if (x >= 'A' && x <= 'Z') x = static_cast<unsigned char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<unsigned char>(y - 'A' + 'a');
        if (x != y) {
          c = x < y ? -1 : 1;
          break;
        }
        if (!x) break;
      }
    } else {
      c = std::strcmp(l.c_str(), r.c_str());
    }
    return ok(c < 0 ? "1" : (c > 0 ? "3" : "2"));
  }
  if (eq(api, "LocaleNameToLCID")) return ok("1033");
  if (eq(api, "LCIDToLocaleName")) return ok("en-US");
  if (eq(api, "IsValidLocale") || eq(api, "IsValidLocaleName")) return ok("1");
  if (eq(api, "GetCalendarInfoW") || eq(api, "GetCalendarInfoA") || eq(api, "GetCalendarInfoEx"))
    return ok("Gregorian");
  if (eq(api, "GetUserPreferredUILanguages") || eq(api, "GetSystemPreferredUILanguages") ||
      eq(api, "GetThreadPreferredUILanguages"))
    return ok("en-US");
  if (eq(api, "GetNumaHighestNodeNumber")) return ok("0");
  if (eq(api, "GetProcessAffinityMask") || eq(api, "GetThreadAffinityMask")) return ok("1\x1f" "1");
  if (eq(api, "GetProcessGroupAffinity") || eq(api, "GetThreadGroupAffinity")) return ok("0");
  if (eq(api, "GetMaximumProcessorCount")) return ok("1");
  if (eq(api, "GetActiveProcessorCount")) return ok("1");
  if (eq(api, "GetMaximumProcessorGroupCount") || eq(api, "GetActiveProcessorGroupCount"))
    return ok("1");
  if (eq(api, "ProcessIdToSessionId")) return ok("0");
  if (eq(api, "WTSGetActiveConsoleSessionId")) return ok("0");
  if (eq(api, "K32GetProcessMemoryInfo") || eq(api, "GetProcessMemoryInfo")) return ok("4096");
  if (eq(api, "K32EnumProcesses") || eq(api, "EnumProcesses"))
    return ok(std::to_string(static_cast<long>(getpid())));
  if (eq(api, "K32GetModuleFileNameExW") || eq(api, "GetModuleFileNameExW") ||
      eq(api, "K32GetModuleBaseNameW")) {
    if (const char* p = std::getenv("_")) {
      if (p[0]) return ok(p);
    }
    return ok("main.wasm");
  }
  if (eq(api, "K32EnumProcessModules") || eq(api, "EnumProcessModules")) return ok("1");
  if (eq(api, "K32EmptyWorkingSet") || eq(api, "EmptyWorkingSet")) return ok("ok");
  if (eq(api, "GetGuiResources")) {
    int n = 0;
    K32Wnd* w = k32_wnds();
    for (int i = 0; i < kK32Max; ++i)
      if (w[i].used) n++;
    K32Gdi* g = k32_gdis();
    for (int i = 0; i < kK32Max; ++i)
      if (g[i].used) n++;
    return ok(std::to_string(n));
  }
  if (eq(api, "EncodePointer") || eq(api, "EncodeSystemPointer")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    return ok(std::to_string(p ^ static_cast<uintptr_t>(0x5a5a5a5a)));
  }
  if (eq(api, "DecodePointer") || eq(api, "DecodeSystemPointer")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    return ok(std::to_string(p ^ static_cast<uintptr_t>(0x5a5a5a5a)));
  }
  if (eq(api, "GetFirmwareType")) return ok("2");
  if (eq(api, "GetOsSafeBootMode")) return ok("0");
  if (eq(api, "GetEnabledXStateFeatures")) return ok("0");
  if (eq(api, "GetIntegratedDisplaySize")) return ok("0");
  if (eq(api, "IsNativeVhdBoot")) return ok("0");
  if (eq(api, "GetSystemTimeAdjustment") || eq(api, "GetSystemTimeAdjustmentPrecise"))
    return ok("156250\x1f" "156250\x1f" "1");
  if (eq(api, "GetDynamicTimeZoneInformation")) return ok("0\x1fUTC\x1fUTC");
  if (eq(api, "GetTimeZoneInformationForYear")) return ok("0\x1fUTC");
  if (eq(api, "SystemTimeToTzSpecificLocalTime") || eq(api, "TzSpecificLocalTimeToSystemTime") ||
      eq(api, "SystemTimeToTzSpecificLocalTimeEx") || eq(api, "TzSpecificLocalTimeToSystemTimeEx"))
    return ok(a[0] ? a : filetime_now());
  if (eq(api, "GetUserDefaultLocaleName") || eq(api, "GetSystemDefaultLocaleName") ||
      eq(api, "ResolveLocaleName"))
    return ok("en-US");
  if (eq(api, "GetLocaleInfoEx") || eq(api, "GetNumberFormatEx") || eq(api, "GetCurrencyFormatEx") ||
      eq(api, "GetDurationFormatEx"))
    return ok(a[0] ? a : "en-US");
  if (eq(api, "GetCPInfoExW") || eq(api, "GetCPInfoExA")) return ok("1\x1f?\x1f" "0");
  if (eq(api, "GetThreadUILanguage")) return ok("1033");
  if (eq(api, "SetThreadUILanguage")) return ok(a[0] ? a : "1033");
  if (eq(api, "GetUserGeoID") || eq(api, "GetUserDefaultGeoName")) return ok("244");
  if (eq(api, "GetGeoInfoW") || eq(api, "GetGeoInfoA") || eq(api, "GetGeoInfoEx")) return ok("US");
  if (eq(api, "EnumSystemLocalesW") || eq(api, "EnumSystemLocalesEx") ||
      eq(api, "EnumSystemLocalesA"))
    return ok("en-US");
  if (eq(api, "IdnToAscii") || eq(api, "IdnToUnicode") || eq(api, "IdnToNameprepUnicode") ||
      eq(api, "NormalizeString"))
    return ok(a[0] ? a : "");
  if (eq(api, "IsNormalizedString")) return ok("1");
  if (eq(api, "FindStringOrdinal") || eq(api, "FindNLSString") || eq(api, "FindNLSStringEx")) {
    std::string hay, needle;
    split1f(a, &hay, &needle);
    auto p = hay.find(needle);
    return ok(p == std::string::npos ? "-1" : std::to_string(p));
  }
  if (eq(api, "GetTempPath2W") || eq(api, "GetTempPath2A")) {
    if (const char* t = std::getenv("TMPDIR")) {
      if (t[0]) return ok(t);
    }
    if (const char* t = std::getenv("TEMP")) {
      if (t[0]) return ok(t);
    }
    return ok("/tmp");
  }
  if (eq(api, "CopyFile2")) {
    std::string src, dst;
    split1f(a, &src, &dst);
    std::string r = copy_file(src.c_str(), dst.c_str());
    if (r.rfind("error:", 0) == 0) return fail(r);
    return ok(r);
  }
  if (eq(api, "GetProcessWorkingSetSizeEx")) return try_kernel32("GetProcessWorkingSetSize", a, out);
  if (eq(api, "SetProcessWorkingSetSizeEx")) return try_kernel32("SetProcessWorkingSetSize", a, out);
  if (eq(api, "GetQueuedCompletionStatusEx"))
    return try_kernel32("GetQueuedCompletionStatus", a, out);
  if (eq(api, "QueryInterruptTimePrecise")) return try_kernel32("QueryUnbiasedInterruptTime", a, out);
  if (eq(api, "QueryUnbiasedInterruptTimePrecise"))
    return try_kernel32("QueryUnbiasedInterruptTime", a, out);
  if (eq(api, "DnsHostnameToComputerNameW") || eq(api, "DnsHostnameToComputerNameA"))
    return ok(hostname());
  if (eq(api, "Wow64EnableWow64FsRedirection")) return ok("ok");
  if (eq(api, "IsWow64GuestMachineSupported")) return ok("1");
  if (eq(api, "VerSetConditionMask")) return ok("0");
  if (eq(api, "GetProcessMitigationPolicy")) return ok("0");
  if (eq(api, "GetThreadInformation") || eq(api, "GetProcessInformation")) return ok("0");
  if (eq(api, "SetThreadInformation") || eq(api, "SetProcessInformation") ||
      eq(api, "SetProcessMitigationPolicy"))
    return ok("ok");
  if (eq(api, "IsProcessCritical")) return ok("0");
  if (eq(api, "GetThreadIdealProcessorEx") || eq(api, "SetThreadIdealProcessor") ||
      eq(api, "SetThreadIdealProcessorEx") || eq(api, "GetProcessPriorityBoost") ||
      eq(api, "GetThreadPriorityBoost"))
    return ok("0");
  if (eq(api, "SetProcessPriorityBoost") || eq(api, "SetThreadPriorityBoost")) return ok("ok");
  if (eq(api, "QueueUserAPC") || eq(api, "QueueUserAPC2")) return ok("1");
  if (eq(api, "TerminateThread")) {
    K32Thr* t = k32_thr(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!t) return fail(err_msg("TerminateThread: bad handle"));
    std::lock_guard<std::mutex> lk(t->mu);
    t->exit_code = 1;
    t->done = true;
    t->cv.notify_all();
    return ok("ok");
  }
  if (eq(api, "GetSystemCpuSetInformation") || eq(api, "GetProcessDefaultCpuSets") ||
      eq(api, "GetThreadSelectedCpuSets"))
    return ok("0");
  if (eq(api, "SetProcessDefaultCpuSets") || eq(api, "SetThreadSelectedCpuSets")) return ok("ok");
  if (eq(api, "QueryProcessAffinityUpdateMode")) return ok("0");
  if (eq(api, "SetProcessAffinityUpdateMode")) return ok("ok");
  if (eq(api, "WaitOnAddress")) {
    std::string ps, rest, exp, ms;
    split1f(a, &ps, &rest);
    split1f(rest.c_str(), &exp, &ms);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(ps.c_str(), nullptr, 10));
    if (!p) return fail(err_msg("WaitOnAddress"));
    unsigned expect = static_cast<unsigned>(std::strtoul(exp.c_str(), nullptr, 10));
    unsigned cur = 0;
    std::memcpy(&cur, reinterpret_cast<void*>(p), sizeof(unsigned));
    if (cur != expect) return ok("1");
    sleep_ms(std::strtoul(ms.c_str(), nullptr, 10));
    std::memcpy(&cur, reinterpret_cast<void*>(p), sizeof(unsigned));
    return ok(cur != expect ? "1" : "0");
  }
  if (eq(api, "WakeByAddressSingle") || eq(api, "WakeByAddressAll")) return ok("ok");
  if (eq(api, "PrefetchVirtualMemory") || eq(api, "OfferVirtualMemory") ||
      eq(api, "ReclaimVirtualMemory") || eq(api, "DiscardVirtualMemory"))
    return ok("ok");
  if (eq(api, "GetMemoryErrorHandlingCapabilities")) return ok("0");
  if (eq(api, "GetWriteWatch")) return ok("0");
  if (eq(api, "ResetWriteWatch")) return ok("ok");
  if (eq(api, "HeapQueryInformation") || eq(api, "HeapSetInformation") || eq(api, "HeapSummary"))
    return ok("0");
  if (eq(api, "HeapWalk")) return fail(err_msg("HeapWalk: ERROR_NO_MORE_ITEMS"));
  if (eq(api, "GlobalLock") || eq(api, "LocalLock") || eq(api, "GlobalHandle") ||
      eq(api, "LocalHandle"))
    return ok(a);
  if (eq(api, "GlobalUnlock") || eq(api, "LocalUnlock")) return ok("ok");
  if (eq(api, "GlobalFlags") || eq(api, "LocalFlags")) return ok("0");
  if (eq(api, "GlobalReAlloc") || eq(api, "LocalReAlloc"))
    return try_kernel32("HeapReAlloc", a, out);
  if (eq(api, "IsBadReadPtr") || eq(api, "IsBadWritePtr") || eq(api, "IsBadCodePtr") ||
      eq(api, "IsBadStringPtrW") || eq(api, "IsBadStringPtrA"))
    return ok(a[0] ? "0" : "1");
  if (eq(api, "RtlCompareMemory")) {
    std::string x, y;
    split1f(a, &x, &y);
    size_t n = x.size() < y.size() ? x.size() : y.size();
    size_t i = 0;
    for (; i < n && x[i] == y[i]; ++i) {
    }
    return ok(std::to_string(i));
  }
  if (eq(api, "CreateThreadpool")) {
    int h = k32_alloc_misc(kMiscTp);
    if (h < 0) return fail(err_msg("CreateThreadpool: no handles"));
    return ok(std::to_string(h));
  }
  if (eq(api, "CloseThreadpool") || eq(api, "CloseThreadpoolWork") ||
      eq(api, "CloseThreadpoolTimer") || eq(api, "CloseThreadpoolWait") ||
      eq(api, "CloseThreadpoolCleanupGroup")) {
    if (!k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg(api));
    return ok("ok");
  }
  if (eq(api, "SetThreadpoolThreadMaximum") || eq(api, "SetThreadpoolThreadMinimum") ||
      eq(api, "SetThreadpoolTimer") || eq(api, "WaitForThreadpoolTimerCallbacks") ||
      eq(api, "CallbackMayRunLong") || eq(api, "DisassociateCurrentThreadFromCallback") ||
      eq(api, "SetEventWhenCallbackReturns") || eq(api, "LeaveCriticalSectionWhenCallbackReturns") ||
      eq(api, "ReleaseMutexWhenCallbackReturns") ||
      eq(api, "ReleaseSemaphoreWhenCallbackReturns") ||
      eq(api, "ReleaseSRWLockExclusiveWhenCallbackReturns"))
    return ok("ok");
  if (eq(api, "TrySubmitThreadpoolCallback")) {
    try {
      std::thread([]() {}).detach();
    } catch (...) {
      return fail(err_msg("TrySubmitThreadpoolCallback"));
    }
    return ok("1");
  }
  if (eq(api, "CreateThreadpoolWork")) {
    int h = k32_alloc_misc(kMiscWork);
    if (h < 0) return fail(err_msg("CreateThreadpoolWork: no handles"));
    return ok(std::to_string(h));
  }
  if (eq(api, "SubmitThreadpoolWork")) {
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!m || m->kind != kMiscWork) return fail(err_msg("SubmitThreadpoolWork"));
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    try {
      std::thread([h]() {
        K32Misc* w = k32_misc(h);
        if (!w) return;
        std::lock_guard<std::mutex> lk(w->mu);
        w->done = true;
        w->cv.notify_all();
      }).detach();
    } catch (...) {
      return fail(err_msg("SubmitThreadpoolWork"));
    }
    return ok("ok");
  }
  if (eq(api, "WaitForThreadpoolWorkCallbacks")) {
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!m || m->kind != kMiscWork) return fail(err_msg("WaitForThreadpoolWorkCallbacks"));
    std::unique_lock<std::mutex> lk(m->mu);
    m->cv.wait_for(lk, std::chrono::seconds(5), [m] { return m->done || !m->used; });
    return ok("ok");
  }
  if (eq(api, "CreateThreadpoolTimer") || eq(api, "CreateThreadpoolWait") ||
      eq(api, "CreateThreadpoolCleanupGroup") || eq(api, "CreateThreadpoolIo")) {
    int h = k32_alloc_misc(kMiscTp);
    if (h < 0) return fail(err_msg(api));
    return ok(std::to_string(h));
  }
  if (eq(api, "CreateTimerQueue")) {
    int h = k32_alloc_misc(kMiscTq);
    if (h < 0) return fail(err_msg("CreateTimerQueue: no handles"));
    return ok(std::to_string(h));
  }
  if (eq(api, "CreateTimerQueueTimer")) {
    std::string qh, ms;
    split1f(a, &qh, &ms);
    int h = k32_alloc_misc(kMiscTqTimer);
    if (h < 0) return fail(err_msg("CreateTimerQueueTimer: no handles"));
    unsigned due = static_cast<unsigned>(std::strtoul(ms.c_str(), nullptr, 10));
    try {
      std::thread([h, due]() {
        if (due) std::this_thread::sleep_for(std::chrono::milliseconds(due));
        K32Misc* t = k32_misc(h);
        if (!t) return;
        std::lock_guard<std::mutex> lk(t->mu);
        t->done = true;
        t->cv.notify_all();
      }).detach();
    } catch (...) {
      k32_close_misc(h);
      return fail(err_msg("CreateTimerQueueTimer"));
    }
    return ok(std::to_string(h));
  }
  if (eq(api, "ChangeTimerQueueTimer")) return ok("ok");
  if (eq(api, "DeleteTimerQueueTimer") || eq(api, "DeleteTimerQueue") ||
      eq(api, "DeleteTimerQueueEx")) {
    k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok("ok");
  }
  if (eq(api, "RegisterWaitForSingleObject")) {
    std::string hs, ms;
    split1f(a, &hs, &ms);
    int wait_h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    int h = k32_alloc_misc(kMiscWait);
    if (h < 0) return fail(err_msg("RegisterWaitForSingleObject: no handles"));
    unsigned long long wait_ms =
        ms.empty() ? 0xffffffffull : std::strtoull(ms.c_str(), nullptr, 10);
    try {
      std::thread([h, wait_h, wait_ms]() {
        k32_wait_one(wait_h, wait_ms);
        K32Misc* m = k32_misc(h);
        if (!m) return;
        std::lock_guard<std::mutex> lk(m->mu);
        m->done = true;
        m->cv.notify_all();
      }).detach();
    } catch (...) {
      k32_close_misc(h);
      return fail(err_msg("RegisterWaitForSingleObject"));
    }
    return ok(std::to_string(h));
  }
  if (eq(api, "UnregisterWait") || eq(api, "UnregisterWaitEx")) {
    k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok("ok");
  }
  if (eq(api, "ConvertThreadToFiber") || eq(api, "ConvertThreadToFiberEx")) {
    if (!k32_fiber_self()) {
      int h = k32_alloc_misc(kMiscFiber);
      if (h < 0) return fail(err_msg("ConvertThreadToFiber"));
      k32_fiber_self() = h;
    }
    return ok(std::to_string(k32_fiber_self()));
  }
  if (eq(api, "CreateFiber") || eq(api, "CreateFiberEx")) {
    int h = k32_alloc_misc(kMiscFiber);
    if (h < 0) return fail(err_msg("CreateFiber"));
    return ok(std::to_string(h));
  }
  if (eq(api, "SwitchToFiber")) return ok("ok");
  if (eq(api, "DeleteFiber")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (k32_fiber_self() == h) k32_fiber_self() = 0;
    k32_close_misc(h);
    return ok("ok");
  }
  if (eq(api, "ConvertFiberToThread")) {
    if (k32_fiber_self()) {
      k32_close_misc(k32_fiber_self());
      k32_fiber_self() = 0;
    }
    return ok("ok");
  }
  if (eq(api, "IsThreadAFiber")) return ok(k32_fiber_self() ? "1" : "0");
  if (eq(api, "InitializeSynchronizationBarrier")) {
    std::string n, spin;
    split1f(a, &n, &spin);
    int h = k32_alloc_misc(kMiscBarrier);
    if (h < 0) return fail(err_msg("InitializeSynchronizationBarrier"));
    K32Misc* m = k32_misc(h);
    m->count = std::strtol(n.c_str(), nullptr, 10);
    if (m->count <= 0) m->count = 1;
    return ok(std::to_string(h));
  }
  if (eq(api, "EnterSynchronizationBarrier")) {
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!m || m->kind != kMiscBarrier) return fail(err_msg("EnterSynchronizationBarrier"));
    std::unique_lock<std::mutex> lk(m->mu);
    m->arrived++;
    if (m->arrived >= m->count) {
      m->arrived = 0;
      m->cv.notify_all();
      return ok("1");
    }
    m->cv.wait(lk, [m] { return m->arrived == 0 || !m->used; });
    return ok("1");
  }
  if (eq(api, "DeleteSynchronizationBarrier")) {
    k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok("ok");
  }
  if (eq(api, "CreateMailslotW") || eq(api, "CreateMailslotA")) {
    int h = k32_alloc_misc(kMiscMail);
    if (h < 0) return fail(err_msg("CreateMailslotW"));
    k32_misc(h)->name = a;
    return ok(std::to_string(h));
  }
  if (eq(api, "GetMailslotInfo")) return ok("0\x1f" "0\x1f-1");
  if (eq(api, "SetMailslotInfo")) return ok("ok");
  if (eq(api, "RegisterApplicationRestart") || eq(api, "UnregisterApplicationRestart"))
    return ok("ok");
  if (eq(api, "GetApplicationRestartSettings")) return ok("");
  if (eq(api, "ReOpenFile")) return try_kernel32("DuplicateHandle", a, out);
  if (eq(api, "SetFileInformationByHandle")) return ok("ok");
  if (eq(api, "FindFirstFileNameW") || eq(api, "FindFirstFileNameA")) {
    int h = k32_alloc_vol(4, a);
    if (h < 0) return fail(err_msg("FindFirstFileNameW"));
    return ok(std::to_string(h) + "\x1f" + (a[0] ? a : "/"));
  }
  if (eq(api, "FindNextFileNameW")) return fail(err_msg("FindNextFileNameW: ERROR_NO_MORE_FILES"));
  if (eq(api, "GetNumberOfConsoleInputEvents")) return ok("0");
  if (eq(api, "GetNumberOfConsoleMouseButtons")) return ok("0");
  if (eq(api, "GetConsoleOutputCP")) return ok("65001");
  if (eq(api, "SetConsoleCP") || eq(api, "SetConsoleOutputCP")) return ok("ok");
  if (eq(api, "ReadConsoleW") || eq(api, "ReadConsoleA")) return ok("");
  if (eq(api, "SetConsoleTitleW") || eq(api, "SetConsoleTitleA")) return ok("ok");
  if (eq(api, "GetConsoleAliasW") || eq(api, "GetConsoleAliasesW") ||
      eq(api, "GetConsoleAliasExesW"))
    return ok("");
  if (eq(api, "AddConsoleAliasW")) return ok("ok");
  if (eq(api, "GetCurrentConsoleFont") || eq(api, "GetCurrentConsoleFontEx")) return ok("0\x1f" "8\x1f" "16");
  if (eq(api, "GetConsoleFontSize")) return ok("8\x1f" "16");
  if (eq(api, "GetConsoleDisplayMode")) return ok("0");
  if (eq(api, "SetConsoleDisplayMode")) return ok("ok");
  if (eq(api, "AttachConsole") || eq(api, "FreeConsole") || eq(api, "AllocConsole")) return ok("ok");
  if (eq(api, "BindIoCompletionCallback") || eq(api, "CancelSynchronousIo") ||
      eq(api, "SetFileCompletionNotificationModes"))
    return ok("ok");
  if (eq(api, "GetSystemFileCacheSize")) return ok("0\x1f" "0");
  if (eq(api, "QueryVirtualMemoryInformation")) return ok("0");
  if (eq(api, "RegCloseKey")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    k32_close_reg(h);
    return ok("ok");
  }
  if (eq(api, "RegOpenKeyExW") || eq(api, "RegOpenKeyExA") || eq(api, "RegOpenKeyW") ||
      eq(api, "RegOpenKeyA")) {
    std::string hive, sub;
    split1f(a, &hive, &sub);
    std::string path = k32_reg_join(k32_reg_path_of(hive.c_str()), sub);
    k32_reg_ensure("");
    if (k32_reg_vals().find(path) == k32_reg_vals().end())
      return fail(err_msg("RegOpenKeyExW: ERROR_FILE_NOT_FOUND"));
    int h = k32_alloc_reg(path);
    if (h < 0) return fail(err_msg("RegOpenKeyExW: no handles"));
    return ok(std::to_string(h));
  }
  if (eq(api, "RegCreateKeyExW") || eq(api, "RegCreateKeyExA") || eq(api, "RegCreateKeyW") ||
      eq(api, "RegCreateKeyA")) {
    std::string hive, sub;
    split1f(a, &hive, &sub);
    std::string path = k32_reg_join(k32_reg_path_of(hive.c_str()), sub);
    k32_reg_ensure(path);
    int h = k32_alloc_reg(path);
    if (h < 0) return fail(err_msg("RegCreateKeyExW: no handles"));
    return ok(std::to_string(h) + "\x1f" "0");
  }
  if (eq(api, "RegQueryValueExW") || eq(api, "RegQueryValueExA") || eq(api, "RegQueryValueW") ||
      eq(api, "RegQueryValueA")) {
    std::string hs, name;
    split1f(a, &hs, &name);
    std::string path = k32_reg_path_of(hs.c_str());
    auto kit = k32_reg_vals().find(path);
    if (kit == k32_reg_vals().end()) return fail(err_msg("RegQueryValueExW: ERROR_FILE_NOT_FOUND"));
    auto vit = kit->second.find(name);
    if (vit == kit->second.end()) return fail(err_msg("RegQueryValueExW: ERROR_FILE_NOT_FOUND"));
    return ok(std::to_string(vit->second.type) + "\x1f" + vit->second.data);
  }
  if (eq(api, "RegSetValueExW") || eq(api, "RegSetValueExA") || eq(api, "RegSetValueW") ||
      eq(api, "RegSetValueA")) {
    std::string hs, rest, name, typ, data;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &name, &rest);
    split1f(rest.c_str(), &typ, &data);
    std::string path = k32_reg_path_of(hs.c_str());
    k32_reg_ensure(path);
    unsigned t = typ.empty() ? 1u : static_cast<unsigned>(std::strtoul(typ.c_str(), nullptr, 10));
    k32_reg_vals()[path][name] = K32RegVal{t, data};
    return ok("ok");
  }
  if (eq(api, "RegDeleteValueW") || eq(api, "RegDeleteValueA")) {
    std::string hs, name;
    split1f(a, &hs, &name);
    std::string path = k32_reg_path_of(hs.c_str());
    auto kit = k32_reg_vals().find(path);
    if (kit == k32_reg_vals().end()) return fail(err_msg("RegDeleteValueW: ERROR_FILE_NOT_FOUND"));
    kit->second.erase(name);
    return ok("ok");
  }
  if (eq(api, "RegDeleteKeyW") || eq(api, "RegDeleteKeyA") || eq(api, "RegDeleteKeyExW") ||
      eq(api, "RegDeleteKeyExA")) {
    std::string hive, sub;
    split1f(a, &hive, &sub);
    std::string path = k32_reg_join(k32_reg_path_of(hive.c_str()), sub);
    auto sit = k32_reg_subs().find(path);
    if (sit != k32_reg_subs().end() && !sit->second.empty())
      return fail(err_msg("RegDeleteKeyW: ERROR_ACCESS_DENIED"));
    k32_reg_delete_tree(path);
    return ok("ok");
  }
  if (eq(api, "RegDeleteTreeW") || eq(api, "RegDeleteTreeA")) {
    std::string hive, sub;
    split1f(a, &hive, &sub);
    std::string path = k32_reg_join(k32_reg_path_of(hive.c_str()), sub);
    k32_reg_delete_tree(path);
    return ok("ok");
  }
  if (eq(api, "RegDeleteKeyValueW") || eq(api, "RegDeleteKeyValueA")) {
    std::string hive, rest, sub, name;
    split1f(a, &hive, &rest);
    split1f(rest.c_str(), &sub, &name);
    std::string path = k32_reg_join(k32_reg_path_of(hive.c_str()), sub);
    k32_reg_vals()[path].erase(name);
    return ok("ok");
  }
  if (eq(api, "RegEnumKeyExW") || eq(api, "RegEnumKeyExA") || eq(api, "RegEnumKeyW") ||
      eq(api, "RegEnumKeyA")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    std::string path = k32_reg_path_of(hs.c_str());
    unsigned n = static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10));
    auto sit = k32_reg_subs().find(path);
    if (sit == k32_reg_subs().end() || n >= sit->second.size())
      return fail(err_msg("RegEnumKeyExW: ERROR_NO_MORE_ITEMS"));
    auto it = sit->second.begin();
    for (unsigned i = 0; i < n; ++i) ++it;
    return ok(it->first);
  }
  if (eq(api, "RegEnumValueW") || eq(api, "RegEnumValueA")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    std::string path = k32_reg_path_of(hs.c_str());
    unsigned n = static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10));
    auto kit = k32_reg_vals().find(path);
    if (kit == k32_reg_vals().end() || n >= kit->second.size())
      return fail(err_msg("RegEnumValueW: ERROR_NO_MORE_ITEMS"));
    auto it = kit->second.begin();
    for (unsigned i = 0; i < n; ++i) ++it;
    return ok(it->first + "\x1f" + std::to_string(it->second.type) + "\x1f" + it->second.data);
  }
  if (eq(api, "RegQueryInfoKeyW") || eq(api, "RegQueryInfoKeyA")) {
    std::string path = k32_reg_path_of(a);
    unsigned nsub = 0, nval = 0;
    auto sit = k32_reg_subs().find(path);
    if (sit != k32_reg_subs().end()) nsub = static_cast<unsigned>(sit->second.size());
    auto vit = k32_reg_vals().find(path);
    if (vit != k32_reg_vals().end()) nval = static_cast<unsigned>(vit->second.size());
    return ok(std::to_string(nsub) + "\x1f" + std::to_string(nval));
  }
  if (eq(api, "RegFlushKey") || eq(api, "RegDisablePredefinedCache") ||
      eq(api, "RegDisablePredefinedCacheEx") || eq(api, "RegNotifyChangeKeyValue"))
    return ok("ok");
  if (eq(api, "RegGetValueW") || eq(api, "RegGetValueA")) {
    std::string hive, rest, sub, name;
    split1f(a, &hive, &rest);
    split1f(rest.c_str(), &sub, &name);
    std::string path = k32_reg_join(k32_reg_path_of(hive.c_str()), sub);
    auto kit = k32_reg_vals().find(path);
    if (kit == k32_reg_vals().end()) return fail(err_msg("RegGetValueW: ERROR_FILE_NOT_FOUND"));
    auto vit = kit->second.find(name);
    if (vit == kit->second.end()) return fail(err_msg("RegGetValueW: ERROR_FILE_NOT_FOUND"));
    return ok(std::to_string(vit->second.type) + "\x1f" + vit->second.data);
  }
  if (eq(api, "RegSetKeyValueW") || eq(api, "RegSetKeyValueA")) {
    std::string hive, rest, sub, name, typ, data;
    split1f(a, &hive, &rest);
    split1f(rest.c_str(), &sub, &rest);
    split1f(rest.c_str(), &name, &rest);
    split1f(rest.c_str(), &typ, &data);
    std::string path = k32_reg_join(k32_reg_path_of(hive.c_str()), sub);
    k32_reg_ensure(path);
    unsigned t = typ.empty() ? 1u : static_cast<unsigned>(std::strtoul(typ.c_str(), nullptr, 10));
    k32_reg_vals()[path][name] = K32RegVal{t, data};
    return ok("ok");
  }
  if (eq(api, "RegCopyTreeW") || eq(api, "RegCopyTreeA")) {
    std::string src, dst;
    split1f(a, &src, &dst);
    std::string sp = k32_reg_path_of(src.c_str());
    std::string dp = k32_reg_path_of(dst.c_str());
    k32_reg_ensure(dp);
    auto vit = k32_reg_vals().find(sp);
    if (vit != k32_reg_vals().end()) k32_reg_vals()[dp] = vit->second;
    std::string prefix = sp + "\\";
    auto vals = k32_reg_vals();
    for (auto& kv : vals) {
      if (kv.first.rfind(prefix, 0) != 0) continue;
      std::string np = dp + kv.first.substr(sp.size());
      k32_reg_ensure(np);
      k32_reg_vals()[np] = kv.second;
    }
    return ok("ok");
  }
  if (eq(api, "RegConnectRegistryW") || eq(api, "RegConnectRegistryA")) {
    if (a[0] && a[0] != '.') return fail(err_msg("RegConnectRegistryW: remote hive not in wasm"));
    int h = k32_alloc_reg("HKLM");
    if (h < 0) return fail(err_msg("RegConnectRegistryW: no handles"));
    return ok(std::to_string(h));
  }
  if (eq(api, "RegOpenCurrentUser")) {
    k32_reg_ensure("HKCU");
    int h = k32_alloc_reg("HKCU");
    if (h < 0) return fail(err_msg("RegOpenCurrentUser"));
    return ok(std::to_string(h));
  }
  if (eq(api, "RegOpenUserClassesRoot")) {
    k32_reg_ensure("HKCR");
    int h = k32_alloc_reg("HKCR");
    if (h < 0) return fail(err_msg("RegOpenUserClassesRoot"));
    return ok(std::to_string(h));
  }
  if (eq(api, "RegLoadAppKeyW") || eq(api, "RegLoadAppKeyA")) {
    std::string path = k32_reg_join("HKAPP", a[0] ? a : "app");
    k32_reg_ensure(path);
    int h = k32_alloc_reg(path);
    if (h < 0) return fail(err_msg("RegLoadAppKeyW"));
    return ok(std::to_string(h));
  }
  if (eq(api, "RegRenameKey")) {
    std::string hs, neu;
    split1f(a, &hs, &neu);
    K32Reg* r = k32_reg(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!r) return fail(err_msg("RegRenameKey: bad handle"));
    auto slash = r->path.rfind('\\');
    std::string parent = slash == std::string::npos ? std::string() : r->path.substr(0, slash);
    std::string np = k32_reg_join(parent, neu);
    k32_reg_vals()[np] = k32_reg_vals()[r->path];
    k32_reg_delete_tree(r->path);
    k32_reg_ensure(np);
    r->path = np;
    return ok("ok");
  }
  if (eq(api, "RegSaveKeyW") || eq(api, "RegSaveKeyA") || eq(api, "RegRestoreKeyW") ||
      eq(api, "RegRestoreKeyA") || eq(api, "RegLoadKeyW") || eq(api, "RegUnLoadKeyW") ||
      eq(api, "RegReplaceKeyW") || eq(api, "RegGetKeySecurity") || eq(api, "RegSetKeySecurity"))
    return fail(err_msg("registry hive file not in wasm"));
  if (eq(api, "RegQueryReflectionKey")) return ok("0");
  if (eq(api, "RegDisableReflectionKey") || eq(api, "RegEnableReflectionKey")) return ok("ok");

  if (eq(api, "OpenProcessToken")) {
    std::string ph, access;
    split1f(a, &ph, &access);
    int proc = ph.empty() ? -1 : static_cast<int>(std::strtol(ph.c_str(), nullptr, 10));
    if (!k32_proc_ok(proc)) return fail(err_msg("OpenProcessToken: bad process"));
    int h = k32_alloc_tok(kTokToken);
    if (h < 0) return fail(err_msg("OpenProcessToken: no handles"));
    (void)access;
    return ok(std::to_string(h));
  }
  if (eq(api, "OpenThreadToken")) {
    std::string th, rest;
    split1f(a, &th, &rest);
    int src = k32_thr_imp(th.c_str());
    int h = k32_dup_tok(src);
    if (h < 0) return fail(err_msg("OpenThreadToken: no token"));
    return ok(std::to_string(h));
  }
  if (eq(api, "ImpersonateLoggedOnUser")) {
    int src = static_cast<int>(std::strtol(a, nullptr, 10));
    K32Tok* t = k32_tok(src);
    if (!t || t->kind != kTokToken) return fail(err_msg("ImpersonateLoggedOnUser: bad handle"));
    k32_cur_imp() = src;
    return ok("1");
  }
  if (eq(api, "SetThreadToken")) {
    std::string th, tok;
    split1f(a, &th, &tok);
    if (tok.empty() || eq(tok.c_str(), "0")) {
      k32_set_thr_imp(th.c_str(), 0);
      return ok("1");
    }
    int src = static_cast<int>(std::strtol(tok.c_str(), nullptr, 10));
    K32Tok* t = k32_tok(src);
    if (!t || t->kind != kTokToken) return fail(err_msg("SetThreadToken: bad handle"));
    k32_set_thr_imp(th.c_str(), src);
    return ok("1");
  }
  if (eq(api, "RevertToSelf")) {
    k32_cur_imp() = 0;
    return ok("1");
  }
  if (eq(api, "GetTokenInformation")) {
    std::string hs, cls;
    split1f(a, &hs, &cls);
    K32Tok* t = k32_tok(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!t || t->kind != kTokToken) return fail(err_msg("GetTokenInformation: bad handle"));
    int c = static_cast<int>(std::strtol(cls.c_str(), nullptr, 10));
    if (c == 1) return ok(t->sid);
    if (c == 2) {
      std::string g;
      for (size_t i = 0; i < t->groups.size(); ++i) {
        if (i) g += "\x1f";
        g += t->groups[i];
      }
      return ok(g);
    }
    if (c == 3) {
      std::string p = "SeChangeNotifyPrivilege";
      p += "\x1f";
      p += "1";
      p += "\x1f";
      p += "SeLoadDriverPrivilege";
      p += "\x1f";
      p += t->se_load_driver ? "2" : "0";
      if (t->se_debug) {
        p += "\x1f";
        p += "SeDebugPrivilege";
        p += "\x1f";
        p += "1";
      }
      return ok(p);
    }
    if (c == 8) return ok(std::to_string(t->type));
    if (c == 9) return ok(std::to_string(t->impersonation_level));
    if (c == 12) return ok(std::to_string(t->session));
    if (c == 20) return ok("0");
    return fail(err_msg("GetTokenInformation: class"));
  }
  if (eq(api, "SetTokenInformation")) {
    std::string hs, rest, cls, val;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &cls, &val);
    K32Tok* t = k32_tok(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!t || t->kind != kTokToken) return fail(err_msg("SetTokenInformation: bad handle"));
    int c = static_cast<int>(std::strtol(cls.c_str(), nullptr, 10));
    if (c == 3) {
      if (val.find("SeDebugPrivilege") != std::string::npos) t->se_debug = 1;
      if (val.find("SeLoadDriverPrivilege") != std::string::npos) t->se_load_driver = 1;
      return ok("ok");
    }
    if (c == 12) {
      t->session = static_cast<int>(std::strtol(val.c_str(), nullptr, 10));
      return ok("ok");
    }
    return fail(err_msg("SetTokenInformation: class"));
  }
  if (eq(api, "LookupPrivilegeValueW") || eq(api, "LookupPrivilegeValueA")) {
    std::string sys, name;
    split1f(a, &sys, &name);
    if (name.empty()) name = sys;
    if (eq(name.c_str(), "SeLoadDriverPrivilege")) return ok("10");
    if (eq(name.c_str(), "SeDebugPrivilege")) return ok("20");
    if (eq(name.c_str(), "SeChangeNotifyPrivilege")) return ok("23");
    return fail(err_msg("LookupPrivilegeValueW: unknown privilege"));
  }
  if (eq(api, "LookupPrivilegeNameW") || eq(api, "LookupPrivilegeNameA")) {
    std::string sys, luid;
    split1f(a, &sys, &luid);
    if (luid.empty()) luid = sys;
    if (eq(luid.c_str(), "10") || eq(luid.c_str(), "SeLoadDriverPrivilege"))
      return ok("SeLoadDriverPrivilege");
    if (eq(luid.c_str(), "20") || eq(luid.c_str(), "SeDebugPrivilege")) return ok("SeDebugPrivilege");
    if (eq(luid.c_str(), "23") || eq(luid.c_str(), "SeChangeNotifyPrivilege"))
      return ok("SeChangeNotifyPrivilege");
    return fail(err_msg("LookupPrivilegeNameW: unknown luid"));
  }
  if (eq(api, "AdjustTokenPrivileges") || eq(api, "NtAdjustPrivilegesToken") ||
      eq(api, "ZwAdjustPrivilegesToken")) {
    std::string hs, name;
    split1f(a, &hs, &name);
    K32Tok* t = k32_tok(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!t || t->kind != kTokToken) return fail(err_msg("AdjustTokenPrivileges: bad handle"));
    if (name.find("SeDebugPrivilege") != std::string::npos || eq(name.c_str(), "20")) t->se_debug = 1;
    if (name.find("SeLoadDriverPrivilege") != std::string::npos || name.empty() ||
        eq(name.c_str(), "10"))
      t->se_load_driver = 1;
    return ok("ok");
  }
  if (eq(api, "RtlAdjustPrivilege")) {
    std::string id, rest;
    split1f(a, &id, &rest);
    if (eq(id.c_str(), "20")) {
      if (K32Tok* t = k32_tok(k32_cur_imp())) t->se_debug = 1;
    }
    if (eq(id.c_str(), "10") || eq(id.c_str(), "SeLoadDriverPrivilege")) {
      if (K32Tok* t = k32_tok(k32_cur_imp())) t->se_load_driver = 1;
    }
    return ok("0");
  }
  if (eq(api, "LookupAccountSidW") || eq(api, "LookupAccountSidA")) {
    std::string sid = k32_sid_of_arg(a);
    if (sid.empty()) return fail(err_msg("LookupAccountSidW: empty"));
    std::string acc, dom;
    k32_sid_lookup(sid, &acc, &dom);
    return ok(acc + "\x1f" + dom);
  }
  if (eq(api, "AllocateAndInitializeSid")) {
    std::string rev, rest, auth, subs;
    split1f(a, &rev, &rest);
    split1f(rest.c_str(), &auth, &subs);
    (void)rev;
    std::string sid = "S-1-";
    sid += auth.empty() ? "5" : auth;
    std::string cur = subs;
    while (!cur.empty()) {
      std::string one, nxt;
      split1f(cur.c_str(), &one, &nxt);
      if (!one.empty()) {
        sid += "-";
        sid += one;
      }
      cur = nxt;
    }
    int h = k32_alloc_tok(kTokSid);
    if (h < 0) return fail(err_msg("AllocateAndInitializeSid: no handles"));
    K32Tok* t = k32_tok(h);
    t->sid = sid;
    k32_sid_lookup(sid, &t->account, &t->domain);
    return ok(std::to_string(h));
  }
  if (eq(api, "DuplicateTokenEx") || eq(api, "DuplicateToken")) {
    std::string hs, level;
    split1f(a, &hs, &level);
    K32Tok* src = k32_tok(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!src || src->kind != kTokToken) return fail(err_msg("DuplicateTokenEx: bad handle"));
    int h = k32_alloc_tok(kTokToken);
    if (h < 0) return fail(err_msg("DuplicateTokenEx: no handles"));
    K32Tok* d = k32_tok(h);
    *d = *src;
    d->used = 1;
    d->type = 2;
    d->impersonation_level = level.empty() ? 3 : static_cast<int>(std::strtol(level.c_str(), nullptr, 10));
    return ok(std::to_string(h));
  }
  if (eq(api, "CheckTokenMembership")) {
    std::string hs, sidarg;
    split1f(a, &hs, &sidarg);
    K32Tok* t = k32_tok(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!t || t->kind != kTokToken) return fail(err_msg("CheckTokenMembership: bad handle"));
    std::string sid = k32_sid_of_arg(sidarg.c_str());
    int yes = (sid == t->sid) ? 1 : 0;
    for (const auto& g : t->groups) {
      if (g == sid) yes = 1;
    }
    return ok(yes ? "1" : "0");
  }

  if (eq(api, "WSAStartup")) return ok("2.2");
  if (eq(api, "WSACleanup")) return ok("ok");
  if (eq(api, "WSAGetLastError")) return ok(std::to_string(errno));
  if (eq(api, "WSASocketW") || eq(api, "WSASocketA") || eq(api, "socket")) {
    int h = k32_alloc_sock();
    if (h < 0) return fail(err_msg("WSASocketW: no handles"));
    return ok(std::to_string(h));
  }
  if (eq(api, "closesocket")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (!k32_close_sock(h)) return fail(err_msg("closesocket: bad handle"));
    return ok("ok");
  }
  if (eq(api, "bind")) {
    std::string hs, name;
    split1f(a, &hs, &name);
    K32Sock* s = k32_sock(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!s) return fail(err_msg("bind: bad handle"));
    if (name.empty()) name = "inproc:default";
    {
      std::lock_guard<std::mutex> lk(s->mu);
      s->name = name;
    }
    return ok(name);
  }
  if (eq(api, "listen")) {
    K32Sock* s = k32_sock(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s) return fail(err_msg("listen: bad handle"));
    {
      std::lock_guard<std::mutex> lk(s->mu);
      s->listening = 1;
    }
    s->cv.notify_all();
    return ok("ok");
  }
  if (eq(api, "connect")) {
    std::string hs, name;
    split1f(a, &hs, &name);
    int ch = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Sock* c = k32_sock(ch);
    if (!c) return fail(err_msg("connect: bad handle"));
    int lh = k32_find_listen(name);
    if (lh < 0) return fail(err_msg("connect: not found"));
    K32Sock* l = k32_sock(lh);
    int ah = k32_alloc_sock();
    if (ah < 0) return fail(err_msg("connect: no handles"));
    K32Sock* acc = k32_sock(ah);
    {
      std::lock_guard<std::mutex> lk(c->mu);
      c->peer = ah;
      c->name = name;
    }
    {
      std::lock_guard<std::mutex> lk(acc->mu);
      acc->peer = ch;
      acc->name = name;
    }
    {
      std::lock_guard<std::mutex> lk(l->mu);
      l->accept_q.push_back(ah);
    }
    l->cv.notify_all();
    return ok("ok");
  }
  if (eq(api, "accept")) {
    K32Sock* l = k32_sock(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!l) return fail(err_msg("accept: bad handle"));
    std::unique_lock<std::mutex> lk(l->mu);
    if (l->accept_q.empty()) {
      if (l->nonblock) return fail(err_msg("accept: would block"));
      l->cv.wait(lk, [l] { return !l->accept_q.empty() || !l->used; });
    }
    if (l->accept_q.empty()) return fail(err_msg("accept: closed"));
    int ah = l->accept_q.front();
    l->accept_q.pop_front();
    return ok(std::to_string(ah));
  }
  if (eq(api, "WSASend") || eq(api, "send")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    K32Sock* s = k32_sock(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!s) return fail(err_msg("WSASend: bad handle"));
    int peer = 0;
    {
      std::lock_guard<std::mutex> lk(s->mu);
      peer = s->peer;
    }
    K32Sock* p = k32_sock(peer);
    if (!p) return fail(err_msg("WSASend: not connected"));
    {
      std::lock_guard<std::mutex> lk(p->mu);
      p->inbox.push_back(data);
    }
    p->cv.notify_all();
    return ok(std::to_string(data.size()));
  }
  if (eq(api, "WSARecv") || eq(api, "recv")) {
    std::string hs, nstr;
    split1f(a, &hs, &nstr);
    K32Sock* s = k32_sock(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!s) return fail(err_msg("WSARecv: bad handle"));
    unsigned n = nstr.empty() ? 4096u : static_cast<unsigned>(std::strtoul(nstr.c_str(), nullptr, 10));
    std::unique_lock<std::mutex> lk(s->mu);
    if (s->inbox.empty()) {
      if (s->nonblock) return fail(err_msg("WSARecv: would block"));
      s->cv.wait(lk, [s] { return !s->inbox.empty() || !s->used || s->peer == 0; });
    }
    if (s->inbox.empty()) return ok("");
    std::string b = std::move(s->inbox.front());
    s->inbox.pop_front();
    if (b.size() > n) {
      s->inbox.push_front(b.substr(n));
      b.resize(n);
    }
    return ok(b);
  }
  if (eq(api, "select")) {
    std::string hs, toms;
    split1f(a, &hs, &toms);
    unsigned long long ms = toms.empty() ? 0 : std::strtoull(toms.c_str(), nullptr, 10);
    std::string ready;
    auto consider = [&](int h) {
      if (K32Sock* s = k32_sock(h)) {
        std::lock_guard<std::mutex> lk(s->mu);
        if (!s->inbox.empty() || !s->accept_q.empty()) {
          if (!ready.empty()) ready += ",";
          ready += std::to_string(h);
        }
      }
    };
    std::string cur = hs;
    while (!cur.empty()) {
      std::string one, nxt;
      auto c = cur.find(',');
      if (c == std::string::npos) {
        one = cur;
        nxt.clear();
      } else {
        one = cur.substr(0, c);
        nxt = cur.substr(c + 1);
      }
      consider(static_cast<int>(std::strtol(one.c_str(), nullptr, 10)));
      cur = nxt;
    }
    (void)ms;
    return ok(ready);
  }
  if (eq(api, "ioctlsocket")) {
    std::string hs, rest, cmd, arg;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &cmd, &arg);
    K32Sock* s = k32_sock(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!s) return fail(err_msg("ioctlsocket: bad handle"));
    unsigned long cc = std::strtoul(cmd.c_str(), nullptr, 0);
    if (cmd == "FIONBIO" || cc == 0x8004667Eul || cc == 126) {
      std::lock_guard<std::mutex> lk(s->mu);
      s->nonblock = std::strtol(arg.c_str(), nullptr, 10) != 0;
      return ok("ok");
    }
    return fail(err_msg("ioctlsocket: cmd"));
  }

  if (eq(api, "FindFirstChangeNotificationW") || eq(api, "FindFirstChangeNotificationA")) {
    std::string path, rest;
    split1f(a, &path, &rest);
    if (path.empty()) path = ".";
    int h = k32_alloc_vol(5, path);
    if (h < 0) return fail(err_msg("FindFirstChangeNotificationW: no handles"));
    return ok(std::to_string(h));
  }
  if (eq(api, "FindNextChangeNotification")) {
    K32Vol* v = k32_vol(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!v || v->kind != 5) return fail(err_msg("FindNextChangeNotification: bad handle"));
    return ok("ok");
  }
  if (eq(api, "FindCloseChangeNotification")) {
    k32_close_vol(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok("ok");
  }
  if (eq(api, "ReadDirectoryChangesW") || eq(api, "ReadDirectoryChangesExW")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    K32Vol* v = k32_vol(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!v && (hs.empty() || hs == ".")) return ok(std::string(".") + "\x1f" + "3");
    if (v) return ok((v->name.empty() ? std::string(".") : v->name) + "\x1f" + "3");
    return ok(std::string(".") + "\x1f" + "3");
  }
  if (eq(api, "GetHandleInformation")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h >= 0 && h <= 2) return ok("1");
    if (h >= 3 && h < kK32Max && k32_files()[h].used)
      return ok(std::to_string(k32_files()[h].handle_flags));
    return ok("0");
  }
  if (eq(api, "SetHandleInformation")) {
    std::string hs, rest, mask, flags;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &mask, &flags);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (h < 3 || h >= kK32Max || !k32_files()[h].used)
      return fail(err_msg("SetHandleInformation: bad handle"));
    unsigned m = static_cast<unsigned>(std::strtoul(mask.c_str(), nullptr, 0));
    unsigned f = static_cast<unsigned>(std::strtoul(flags.c_str(), nullptr, 0));
    k32_files()[h].handle_flags = (k32_files()[h].handle_flags & ~m) | (f & m);
    return ok("ok");
  }
  if (eq(api, "InitializeProcThreadAttributeList")) {
    int h = k32_alloc_misc(kMiscAttr);
    if (h < 0) return fail(err_msg("InitializeProcThreadAttributeList"));
    k32_misc(h)->count = static_cast<int>(std::strtol(a, nullptr, 10));
    if (k32_misc(h)->count <= 0) k32_misc(h)->count = 1;
    return ok(std::to_string(h));
  }
  if (eq(api, "UpdateProcThreadAttribute")) {
    std::string hs, rest, attr, val;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &attr, &val);
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || m->kind != kMiscAttr) return fail(err_msg("UpdateProcThreadAttribute"));
    if (!m->name.empty()) m->name += ",";
    m->name += attr + "=" + val;
    return ok("ok");
  }
  if (eq(api, "DeleteProcThreadAttributeList")) {
    k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok("ok");
  }
  if (eq(api, "FileTimeToDosDateTime")) {
    unsigned long long ft = std::strtoull(a, nullptr, 10);
    if (!ft) return fail(err_msg("FileTimeToDosDateTime"));
    return ok(ft_to_dos(ft));
  }
  if (eq(api, "DosDateTimeToFileTime")) {
    std::string ds, ts;
    split1f(a, &ds, &ts);
    return ok(std::to_string(dos_to_ft(static_cast<unsigned>(std::strtoul(ds.c_str(), nullptr, 10)),
                                       static_cast<unsigned>(std::strtoul(ts.c_str(), nullptr, 10)))));
  }
  if (eq(api, "IsDBCSLeadByte") || eq(api, "IsDBCSLeadByteEx")) return ok("0");

  auto nt = [&](const char* name) {
    if (eq(api, name)) return true;
    return api && name && api[0] == 'Z' && api[1] == 'w' && name[0] == 'N' && name[1] == 't' &&
           eq(api + 2, name + 2);
  };

  if (eq(api, "RtlGetCurrentPeb") || eq(api, "NtCurrentPeb")) {
    k32_peb_boot();
    return ok(std::to_string(reinterpret_cast<uintptr_t>(k32_peb().peb)));
  }
  if (eq(api, "RtlGetCurrentTeb") || eq(api, "NtCurrentTeb")) {
    k32_peb_boot();
    return ok(std::to_string(reinterpret_cast<uintptr_t>(k32_peb().teb)));
  }
  if (nt("NtQueryInformationProcess")) {
    k32_peb_boot();
    std::string hs, cls;
    split1f(a, &hs, &cls);
    if (!k32_proc_ok(static_cast<int>(std::strtol(hs.empty() ? "-1" : hs.c_str(), nullptr, 10))))
      return fail(err_msg("NtQueryInformationProcess: bad process"));
    int infoclass = static_cast<int>(std::strtol(cls.empty() ? "0" : cls.c_str(), nullptr, 10));
    if (infoclass == 7)
      return ok("0");
    unsigned pid = static_cast<unsigned>(getpid());
    return ok(std::to_string(reinterpret_cast<uintptr_t>(k32_peb().peb)) + "\x1f" +
              std::to_string(pid));
  }
  if (nt("NtSetInformationProcess")) return ok("ok");
  if (nt("NtQuerySystemInformation")) {
    long nproc = 1;
#if defined(_SC_NPROCESSORS_ONLN)
    nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc < 1) nproc = 1;
#endif
    long psz = 4096;
#if defined(_SC_PAGESIZE)
    long g = sysconf(_SC_PAGESIZE);
    if (g > 0) psz = g;
#endif
    return ok(std::to_string(psz) + "\x1f" + std::to_string(nproc));
  }
  if (nt("NtQueryObject")) {
    std::string hs, cls;
    split1f(a, &hs, &cls);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (k32_map(h)) return ok("Section");
    if (k32_tok(h)) return ok("Token");
    if (k32_sock(h)) return ok("Socket");
    if (k32_proc_ok(h) || h == -1 || h == 0) return ok("Process");
    return ok("Object");
  }
  if (nt("NtCreateSection") || nt("NtCreateSectionEx")) {
    unsigned long n = std::strtoul(a, nullptr, 10);
    int h = k32_alloc_map(n ? n : 4096);
    if (h < 0) return fail(err_msg("NtCreateSection: oom"));
    return ok(std::to_string(h));
  }
  if (nt("NtMapViewOfSection") || nt("NtMapViewOfSectionEx")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    K32Map* m = k32_map(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || !m->p) return fail(err_msg("NtMapViewOfSection: bad handle"));
    k32_vmem_add(m->p, m->n, 4);
    return ok(std::to_string(reinterpret_cast<uintptr_t>(m->p)));
  }
  if (nt("NtUnmapViewOfSection") || nt("NtUnmapViewOfSectionEx")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    k32_vmem().erase(p);
    return ok("ok");
  }
  if (nt("NtAllocateVirtualMemory") || nt("NtAllocateVirtualMemoryEx")) {
    std::string hs, sz;
    split1f(a, &hs, &sz);
    unsigned n = static_cast<unsigned>(std::strtoul(sz.empty() ? a : sz.c_str(), nullptr, 10));
    if (!n) n = 4096;
    void* p = std::malloc(n);
    if (!p) return fail(err_msg("NtAllocateVirtualMemory"));
    std::memset(p, 0, n);
    k32_vmem_add(p, n, 4);
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (nt("NtFreeVirtualMemory")) {
    std::string hs, addr;
    split1f(a, &hs, &addr);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(addr.empty() ? a : addr.c_str(), nullptr, 10));
    k32_vmem().erase(p);
    if (p) std::free(reinterpret_cast<void*>(p));
    return ok("ok");
  }
  if (nt("NtProtectVirtualMemory")) {
    std::string hs, rest, addr, prot;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &addr, &prot);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(addr.c_str(), nullptr, 10));
    uintptr_t base = 0;
    K32Vmem* v = k32_vmem_at(p, &base);
    if (!v) return fail(err_msg("NtProtectVirtualMemory"));
    unsigned old = v->prot;
    unsigned np = static_cast<unsigned>(std::strtoul(prot.c_str(), nullptr, 0));
    if (np) v->prot = np;
    return ok(std::to_string(old));
  }
  if (nt("NtQueryVirtualMemory")) {
    std::string hs, addr;
    split1f(a, &hs, &addr);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(addr.empty() ? a : addr.c_str(), nullptr, 10));
    uintptr_t base = 0;
    K32Vmem* v = k32_vmem_at(p, &base);
    if (!v) return fail(err_msg("NtQueryVirtualMemory"));
    return ok(std::to_string(base) + "\x1f" + std::to_string(v->n) + "\x1f" + std::to_string(v->prot));
  }
  if (nt("NtReadVirtualMemory") || nt("NtReadVirtualMemoryEx")) {
    std::string hs, rest, addr, nstr;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &addr, &nstr);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(addr.c_str(), nullptr, 10));
    unsigned n = static_cast<unsigned>(std::strtoul(nstr.c_str(), nullptr, 10));
    uintptr_t base = 0;
    K32Vmem* v = k32_vmem_at(p, &base);
    if (!v || !n) return fail(err_msg("NtReadVirtualMemory"));
    if (p + n > base + v->n) n = static_cast<unsigned>(base + v->n - p);
    return ok(std::string(reinterpret_cast<const char*>(p), n));
  }
  if (nt("NtWriteVirtualMemory")) {
    std::string hs, rest, addr, data;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &addr, &data);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(addr.c_str(), nullptr, 10));
    uintptr_t base = 0;
    K32Vmem* v = k32_vmem_at(p, &base);
    if (!v) return fail(err_msg("NtWriteVirtualMemory"));
    size_t n = data.size();
    if (p + n > base + v->n) n = static_cast<size_t>(base + v->n - p);
    std::memcpy(reinterpret_cast<void*>(p), data.data(), n);
    return ok(std::to_string(n));
  }
  if (nt("NtCreateFile") || nt("NtOpenFile")) {
    std::string path = a;
    if (path.empty()) return fail(err_msg("NtCreateFile: empty"));
    FILE* f = fopen_create(path.c_str(), 0xc0000000u, nt("NtOpenFile") ? 3u : 4u);
    if (!f) f = std::fopen(path.c_str(), nt("NtOpenFile") ? "rb+" : "wb+");
    if (!f) return fail(err("NtCreateFile"));
    int h = k32_alloc_file(f, path);
    if (h < 0) {
      std::fclose(f);
      return fail(err_msg("NtCreateFile: no handles"));
    }
    return ok(std::to_string(h));
  }
  if (nt("NtReadFile")) {
    std::string hs, nstr;
    split1f(a, &hs, &nstr);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    unsigned n = nstr.empty() ? 4096u : static_cast<unsigned>(std::strtoul(nstr.c_str(), nullptr, 10));
    if (h < 3 || h >= kK32Max || !k32_files()[h].used || !k32_files()[h].f)
      return fail(err_msg("NtReadFile"));
    std::string b(n, '\0');
    size_t got = std::fread(b.data(), 1, n, k32_files()[h].f);
    b.resize(got);
    return ok(b);
  }
  if (nt("NtWriteFile")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (h < 3 || h >= kK32Max || !k32_files()[h].used || !k32_files()[h].f)
      return fail(err_msg("NtWriteFile"));
    size_t n = std::fwrite(data.data(), 1, data.size(), k32_files()[h].f);
    return ok(std::to_string(n));
  }
  if (nt("NtFlushBuffersFile") || nt("NtFlushBuffersFileEx")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h >= 3 && h < kK32Max && k32_files()[h].f) std::fflush(k32_files()[h].f);
    return ok("ok");
  }
  if (nt("NtQueryInformationFile")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h < 3 || h >= kK32Max || !k32_files()[h].used) return fail(err_msg("NtQueryInformationFile"));
    return ok(k32_files()[h].path);
  }
  if (nt("NtSetInformationFile")) return ok("ok");
  if (nt("NtDeviceIoControlFile")) {
    std::string hs, code;
    split1f(a, &hs, &code);
    std::string r = wasi_device_ioctl(static_cast<unsigned>(std::strtoul(code.c_str(), nullptr, 0)), "");
    if (r.empty()) return fail(err_msg("NtDeviceIoControlFile"));
    return ok(r);
  }
  if (nt("NtOpenProcess")) {
    int pid = static_cast<int>(std::strtol(a, nullptr, 10));
    if (pid == 0 || pid == static_cast<int>(getpid())) return ok("-1");
    if (!k32_proc_ok(pid) && !k32_proc(pid)) return fail(err_msg("NtOpenProcess"));
    return ok(std::to_string(pid));
  }
  if (nt("NtTerminateProcess")) {
    std::string hs, code;
    split1f(a, &hs, &code);
    K32Proc* p = k32_proc(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (p) {
      p->exit_code = static_cast<int>(std::strtol(code.c_str(), nullptr, 10));
      p->done = true;
      p->cv.notify_all();
    }
    return ok("ok");
  }
  if (nt("NtCreateThread") || nt("NtCreateThreadEx")) {
    int h = k32_alloc_thr();
    if (h < 0) return fail(err_msg("NtCreateThread"));
    return ok(std::to_string(h));
  }
  if (nt("NtOpenThread")) return ok("-2");
  if (nt("NtDelayExecution")) {
    unsigned long long t = std::strtoull(a, nullptr, 10);
    unsigned ms = t > 10000 ? static_cast<unsigned>(t / 10000ull) : static_cast<unsigned>(t);
    sleep_ms(ms);
    return ok("ok");
  }
  if (nt("NtYieldExecution")) {
    std::this_thread::yield();
    return ok("ok");
  }
  if (nt("NtWaitForSingleObject")) {
    std::string hs, ms;
    split1f(a, &hs, &ms);
    int wr = k32_wait_one(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)),
                          static_cast<unsigned>(std::strtoul(ms.empty() ? "0" : ms.c_str(), nullptr, 10)));
    if (wr < 0) return fail(err_msg("NtWaitForSingleObject"));
    return ok(std::to_string(wr));
  }
  if (nt("NtCreateEvent")) {
    int h = k32_alloc_sync(kSyncEventManual);
    if (h < 0) return fail(err_msg("NtCreateEvent"));
    return ok(std::to_string(h));
  }
  if (nt("NtSetEvent")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s) return fail(err_msg("NtSetEvent"));
    {
      std::lock_guard<std::mutex> lk(s->mu);
      s->signaled = 1;
    }
    s->cv.notify_all();
    return ok("ok");
  }
  if (nt("NtResetEvent")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s) return fail(err_msg("NtResetEvent"));
    std::lock_guard<std::mutex> lk(s->mu);
    s->signaled = 0;
    return ok("ok");
  }
  if (nt("NtCreateMutant") || nt("NtCreateMutex")) {
    int h = k32_alloc_sync(kSyncMutex);
    if (h < 0) return fail(err_msg("NtCreateMutant"));
    return ok(std::to_string(h));
  }
  if (nt("NtReleaseMutant")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s) return fail(err_msg("NtReleaseMutant"));
    s->cv.notify_all();
    return ok("ok");
  }
  if (nt("NtDuplicateObject")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h >= 3 && h < kK32Max && k32_files()[h].used && k32_files()[h].f) {
      FILE* nf = std::fopen(k32_files()[h].path.c_str(), "rb+");
      if (!nf) return fail(err_msg("NtDuplicateObject"));
      int nh = k32_alloc_file(nf, k32_files()[h].path);
      return ok(std::to_string(nh));
    }
    return ok(std::to_string(h));
  }
  if (nt("NtQuerySystemTime")) {
    time_t now = time(nullptr);
    return ok(std::to_string(unix_to_ft(now)));
  }
  if (nt("NtQueryPerformanceCounter")) return ok(qpc_ns());
  if (nt("NtOpenKey") || nt("NtOpenKeyEx")) {
    int h = k32_alloc_reg(a && a[0] ? a : "HKCU");
    if (h < 0) return fail(err_msg("NtOpenKey"));
    return ok(std::to_string(h));
  }
  if (nt("NtCreateKey")) {
    int h = k32_alloc_reg(a && a[0] ? a : "HKCU");
    if (h < 0) return fail(err_msg("NtCreateKey"));
    return ok(std::to_string(h));
  }
  if (nt("NtSetValueKey") || nt("NtQueryValueKey") || nt("NtEnumerateKey") || nt("NtDeleteKey"))
    return ok("ok");
  if (nt("NtOpenProcessToken") || nt("NtOpenProcessTokenEx")) {
    int h = k32_alloc_tok(kTokToken);
    if (h < 0) return fail(err_msg("NtOpenProcessToken"));
    return ok(std::to_string(h));
  }
  if (nt("NtQueryInformationToken")) return ok("1");
  if (nt("NtResumeThread") || nt("NtSuspendThread")) return ok("1");
  if (nt("NtQueryInformationThread") || nt("NtSetInformationThread")) return ok("ok");
  if (nt("NtOpenSection")) return fail(err_msg("NtOpenSection: unnamed"));
  if (nt("NtQueryDirectoryFile") || nt("NtQueryDirectoryFileEx"))
    return fail(err_msg("NtQueryDirectoryFile: ERROR_NO_MORE_FILES"));
  if (nt("NtWaitForMultipleObjects")) return ok("0");
  if (nt("NtCreateSemaphore")) {
    std::string init, rest, maxc;
    split1f(a, &init, &rest);
    split1f(rest.c_str(), &maxc, &rest);
    int h = k32_alloc_sync(kSyncSem);
    if (h < 0) return fail(err_msg("NtCreateSemaphore"));
    K32Sync* s = k32_sync(h);
    s->count = static_cast<int>(std::strtol(init.c_str(), nullptr, 10));
    s->maxc = maxc.empty() ? 0x7fffffff : static_cast<int>(std::strtol(maxc.c_str(), nullptr, 10));
    if (s->count < 0) s->count = 0;
    return ok(std::to_string(h));
  }
  if (nt("NtReleaseSemaphore")) {
    std::string hs, nstr;
    split1f(a, &hs, &nstr);
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!s || s->kind != kSyncSem) return fail(err_msg("NtReleaseSemaphore"));
    int n = nstr.empty() ? 1 : static_cast<int>(std::strtol(nstr.c_str(), nullptr, 10));
    {
      std::lock_guard<std::mutex> lk(s->mu);
      int prev = s->count;
      s->count += n;
      if (s->maxc && s->count > s->maxc) s->count = s->maxc;
      s->cv.notify_all();
      return ok(std::to_string(prev));
    }
  }
  if (nt("NtOpenEvent")) {
    int h = k32_find_sync_name(a, kSyncEventAuto, kSyncEventManual);
    if (h < 0) return fail(err_msg("NtOpenEvent"));
    return ok(std::to_string(h));
  }
  if (nt("NtPulseEvent")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s) return fail(err_msg("NtPulseEvent"));
    {
      std::lock_guard<std::mutex> lk(s->mu);
      s->signaled = 1;
    }
    s->cv.notify_all();
    {
      std::lock_guard<std::mutex> lk(s->mu);
      if (s->kind == kSyncEventAuto) s->signaled = 0;
    }
    return ok("ok");
  }
  if (nt("NtQueryEvent")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s) return fail(err_msg("NtQueryEvent"));
    return ok(std::to_string(s->signaled));
  }
  if (nt("NtClearEvent")) {
    K32Sync* s = k32_sync(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!s) return fail(err_msg("NtClearEvent"));
    std::lock_guard<std::mutex> lk(s->mu);
    s->signaled = 0;
    return ok("ok");
  }
  if (nt("NtQueryTimerResolution")) return ok("156250\x1f" "10000\x1f" "156250");
  if (nt("NtQueryAttributesFile") || nt("NtQueryFullAttributesFile")) {
    struct stat st {};
    const char* p = a && a[0] ? a : ".";
    if (::stat(p, &st) != 0) return fail(err_msg(api));
    return ok(std::to_string(static_cast<unsigned long long>(st.st_size)) + "\x1f" +
              std::to_string(S_ISDIR(st.st_mode) ? 16 : 32));
  }
  if (nt("NtDeleteFile")) {
    if (!a[0] || std::remove(a) != 0) return fail(err_msg("NtDeleteFile"));
    return ok("ok");
  }
  if (nt("NtQueryVolumeInformationFile")) return try_kernel32("GetDiskFreeSpaceExW", a, out);
  if (nt("NtCancelIoFile")) return ok("ok");
  if (nt("NtEnumerateValueKey") || nt("NtDeleteValueKey") || nt("NtQueryKey") || nt("NtFlushKey"))
    return ok("ok");
  if (nt("NtOpenThreadToken") || nt("NtOpenThreadTokenEx"))
    return try_kernel32("OpenThreadToken", a, out);
  if (nt("NtDuplicateToken")) {
    int h = k32_alloc_tok(kTokToken);
    if (h < 0) return fail(err_msg("NtDuplicateToken"));
    return ok(std::to_string(h));
  }
  if (nt("NtQuerySection")) {
    K32Map* m = k32_map(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!m || !m->p) return fail(err_msg("NtQuerySection"));
    return ok(std::to_string(m->n));
  }
  if (nt("NtFlushVirtualMemory")) return ok("ok");
  if (nt("NtIsProcessInJob")) return ok("0");
  if (nt("NtQueryDefaultLocale")) return ok("1033");
  if (nt("NtQueryDefaultUILanguage") || nt("NtQueryInstallUILanguage")) return ok("1033");
  if (nt("NtQueryDebugFilterState")) return ok("0");

  if (nt("NtShutdownSystem") || nt("NtRaiseHardError") || nt("NtRaiseException") ||
      nt("NtSetSystemTime") || eq(api, "DbgBreakPoint") || eq(api, "DbgUserBreakPoint") ||
      eq(api, "RtlAssert") || eq(api, "RtlRaiseStatus") || eq(api, "RtlRaiseException"))
    return fail(err_msg("ntdll: not executed by this hop"));

  if (nt("NtCreateUserProcess") || nt("NtCreateProcess") || nt("NtCreateProcessEx")) {
    if (!a || !a[0]) return fail(err_msg("NtCreateUserProcess: empty"));
    return try_kernel32("CreateProcessW", a, out);
  }
  if (nt("NtTerminateThread")) return try_kernel32("TerminateThread", a, out);
  if (nt("NtAlertThread") || nt("NtAlertResumeThread") || nt("NtTestAlert") ||
      nt("NtQueueApcThread") || nt("NtQueueApcThreadEx") || nt("NtGetContextThread") ||
      nt("NtSetContextThread") || nt("NtContinue"))
    return ok("ok");
  if (nt("NtSuspendProcess")) return try_kernel32("SuspendThread", a, out);
  if (nt("NtResumeProcess")) return try_kernel32("ResumeThread", a, out);
  if (nt("NtGetNextProcess") || nt("NtGetNextThread")) return ok("0");
  if (nt("NtSignalAndWaitForSingleObject")) return try_kernel32("SignalObjectAndWait", a, out);
  if (nt("NtOpenMutant")) return try_kernel32("OpenMutexW", a, out);
  if (nt("NtQueryMutant")) return ok("0");
  if (nt("NtOpenSemaphore")) return try_kernel32("OpenSemaphoreW", a, out);
  if (nt("NtQuerySemaphore")) return ok("1");
  if (nt("NtCreateTimer") || nt("NtCreateTimer2")) return try_kernel32("CreateWaitableTimerW", a, out);
  if (nt("NtOpenTimer")) return try_kernel32("OpenWaitableTimerW", a, out);
  if (nt("NtSetTimer") || nt("NtSetTimerEx") || nt("NtSetTimer2"))
    return try_kernel32("SetWaitableTimer", a, out);
  if (nt("NtCancelTimer")) return try_kernel32("CancelWaitableTimer", a, out);
  if (nt("NtQueryTimer")) return ok("0");
  if (nt("NtCreateKeyedEvent")) return try_kernel32("CreateEventW", a, out);
  if (nt("NtOpenKeyedEvent")) return try_kernel32("OpenEventW", a, out);
  if (nt("NtReleaseKeyedEvent")) return try_kernel32("SetEvent", a, out);
  if (nt("NtWaitForKeyedEvent")) return try_kernel32("WaitForSingleObject", a, out);
  if (nt("NtSetTimerResolution")) return ok("156250");
  if (nt("NtGetTickCount")) return try_kernel32("GetTickCount", a, out);

  if (nt("NtCreateJobObject")) return try_kernel32("CreateJobObjectW", a, out);
  if (nt("NtOpenJobObject")) return try_kernel32("OpenJobObjectW", a, out);
  if (nt("NtAssignProcessToJobObject")) return try_kernel32("AssignProcessToJobObject", a, out);
  if (nt("NtTerminateJobObject")) return try_kernel32("TerminateJobObject", a, out);
  if (nt("NtQueryInformationJobObject")) return try_kernel32("QueryInformationJobObject", a, out);
  if (nt("NtSetInformationJobObject")) return try_kernel32("SetInformationJobObject", a, out);

  if (nt("NtCreateIoCompletion")) return try_kernel32("CreateIoCompletionPort", a, out);
  if (nt("NtOpenIoCompletion")) return fail(err_msg("NtOpenIoCompletion: unnamed"));
  if (nt("NtSetIoCompletion") || nt("NtSetIoCompletionEx"))
    return try_kernel32("PostQueuedCompletionStatus", a, out);
  if (nt("NtRemoveIoCompletion") || nt("NtRemoveIoCompletionEx"))
    return try_kernel32("GetQueuedCompletionStatus", a, out);
  if (nt("NtQueryIoCompletion")) return ok("0");

  if (nt("NtLockVirtualMemory")) return try_kernel32("VirtualLock", a, out);
  if (nt("NtUnlockVirtualMemory")) return try_kernel32("VirtualUnlock", a, out);
  if (nt("NtFlushInstructionCache")) return ok("ok");
  if (nt("NtExtendSection") || nt("NtAreMappedFilesTheSame")) return ok("0");
  if (nt("NtGetWriteWatch")) return try_kernel32("GetWriteWatch", a, out);
  if (nt("NtResetWriteWatch")) return try_kernel32("ResetWriteWatch", a, out);
  if (nt("NtSetInformationVirtualMemory")) return ok("ok");

  if (nt("NtNotifyChangeDirectoryFile") || nt("NtNotifyChangeDirectoryFileEx"))
    return try_kernel32("FindFirstChangeNotificationW", a, out);
  if (nt("NtQueryEaFile") || nt("NtSetEaFile")) return ok("ok");
  if (nt("NtCreateNamedPipeFile")) return try_kernel32("CreateNamedPipeW", a, out);
  if (nt("NtCreateMailslotFile")) return try_kernel32("CreateMailslotW", a, out);
  if (nt("NtSetVolumeInformationFile")) return ok("ok");
  if (nt("NtFsControlFile")) return try_kernel32("DeviceIoControl", a, out);
  if (nt("NtLockFile")) return try_kernel32("LockFile", a, out);
  if (nt("NtUnlockFile")) return try_kernel32("UnlockFile", a, out);
  if (nt("NtCancelIoFileEx") || nt("NtCancelSynchronousIoFile")) return try_kernel32("CancelIoEx", a, out);
  if (nt("NtQueryInformationByName")) return try_kernel32("GetFileAttributesA", a, out);
  if (nt("NtReadFileScatter") || nt("NtWriteFileGather")) return ok("0");

  if (nt("NtRenameKey") || nt("NtRestoreKey") || nt("NtSaveKey") || nt("NtSaveKeyEx") ||
      nt("NtLoadKey") || nt("NtLoadKey2") || nt("NtLoadKeyEx") || nt("NtUnloadKey") ||
      nt("NtUnloadKey2") || nt("NtNotifyChangeKey") || nt("NtNotifyChangeMultipleKeys") ||
      nt("NtQueryMultipleValueKey") || nt("NtSetInformationKey") || nt("NtCompactKeys"))
    return ok("ok");

  if (nt("NtCreateDirectoryObject") || nt("NtCreateDirectoryObjectEx") ||
      nt("NtOpenDirectoryObject")) {
    int h = k32_alloc_misc(kMiscObjDir);
    if (h < 0) return fail(err_msg(api));
    k32_misc(h)->name = a && a[0] ? a : "\\";
    return ok(std::to_string(h));
  }
  if (nt("NtQueryDirectoryObject")) return ok("");
  if (nt("NtCreateSymbolicLinkObject") || nt("NtOpenSymbolicLinkObject")) {
    int h = k32_alloc_misc(kMiscSym);
    if (h < 0) return fail(err_msg(api));
    k32_misc(h)->name = a && a[0] ? a : "\\??\\C:";
    return ok(std::to_string(h));
  }
  if (nt("NtQuerySymbolicLinkObject")) {
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (m && m->kind == kMiscSym) return ok(m->name);
    return try_kernel32("QueryDosDeviceW", a, out);
  }
  if (nt("NtMakeTemporaryObject") || nt("NtMakePermanentObject")) return ok("ok");
  if (nt("NtQuerySecurityObject") || nt("NtSetSecurityObject")) return ok("ok");
  if (nt("NtAdjustGroupsToken") || nt("NtSetInformationToken") ||
      nt("NtFilterToken") || nt("NtCompareTokens") || nt("NtPrivilegeCheck") ||
      nt("NtImpersonateThread") || nt("NtImpersonateAnonymousToken") || nt("NtAccessCheck"))
    return ok("1");
  if (nt("NtQueryLicenseValue")) return ok("0");
  if (nt("NtPowerInformation")) return ok("0");
  if (nt("NtSetDebugFilterState")) return ok("ok");

  if (eq(api, "RtlCreateHeap")) return try_kernel32("HeapCreate", a, out);
  if (eq(api, "ExAllocatePool") || eq(api, "ExAllocatePoolWithTag") || eq(api, "ExAllocatePool2") ||
      eq(api, "ExAllocatePool3") || eq(api, "ExAllocatePoolZero") ||
      eq(api, "ExAllocatePoolWithQuota") || eq(api, "ExAllocatePoolWithQuotaTag")) {
    std::string pool, rest, sz;
    split1f(a, &pool, &rest);
    split1f(rest.c_str(), &sz, &rest);
    if (sz.empty()) sz = pool;
    return try_kernel32("HeapAlloc", sz.c_str(), out);
  }
  if (eq(api, "ExFreePool") || eq(api, "ExFreePoolWithTag")) {
    std::string p, tag;
    split1f(a, &p, &tag);
    return try_kernel32("HeapFree", p.empty() ? a : p.c_str(), out);
  }
  if (eq(api, "RtlDestroyHeap")) return try_kernel32("HeapDestroy", a, out);
  if (eq(api, "RtlValidateHeap")) return try_kernel32("HeapValidate", a, out);
  if (eq(api, "RtlCompactHeap")) return try_kernel32("HeapCompact", a, out);
  if (eq(api, "RtlLockHeap")) return try_kernel32("HeapLock", a, out);
  if (eq(api, "RtlUnlockHeap")) return try_kernel32("HeapUnlock", a, out);
  if (eq(api, "RtlWalkHeap")) return ok("0");
  if (eq(api, "RtlGetProcessHeaps")) return try_kernel32("GetProcessHeaps", a, out);
  if (eq(api, "RtlQueryHeapInformation")) return try_kernel32("HeapQueryInformation", a, out);
  if (eq(api, "RtlSetHeapInformation")) return try_kernel32("HeapSetInformation", a, out);

  if (eq(api, "RtlInitializeCriticalSection") ||
      eq(api, "RtlInitializeCriticalSectionAndSpinCount") ||
      eq(api, "RtlInitializeCriticalSectionEx"))
    return try_kernel32("InitializeCriticalSection", a, out);
  if (eq(api, "RtlEnterCriticalSection")) return try_kernel32("EnterCriticalSection", a, out);
  if (eq(api, "RtlLeaveCriticalSection")) return try_kernel32("LeaveCriticalSection", a, out);
  if (eq(api, "RtlTryEnterCriticalSection")) return try_kernel32("TryEnterCriticalSection", a, out);
  if (eq(api, "RtlDeleteCriticalSection")) return try_kernel32("DeleteCriticalSection", a, out);
  if (eq(api, "RtlSetCriticalSectionSpinCount")) return ok("0");

  if (eq(api, "RtlInitializeSRWLock")) return try_kernel32("InitializeSRWLock", a, out);
  if (eq(api, "RtlAcquireSRWLockExclusive")) return try_kernel32("AcquireSRWLockExclusive", a, out);
  if (eq(api, "RtlAcquireSRWLockShared")) return try_kernel32("AcquireSRWLockShared", a, out);
  if (eq(api, "RtlReleaseSRWLockExclusive")) return try_kernel32("ReleaseSRWLockExclusive", a, out);
  if (eq(api, "RtlReleaseSRWLockShared")) return try_kernel32("ReleaseSRWLockShared", a, out);
  if (eq(api, "RtlTryAcquireSRWLockExclusive"))
    return try_kernel32("TryAcquireSRWLockExclusive", a, out);
  if (eq(api, "RtlTryAcquireSRWLockShared")) return try_kernel32("TryAcquireSRWLockShared", a, out);

  if (eq(api, "RtlInitializeConditionVariable"))
    return try_kernel32("InitializeConditionVariable", a, out);
  if (eq(api, "RtlSleepConditionVariableCS")) return try_kernel32("SleepConditionVariableCS", a, out);
  if (eq(api, "RtlSleepConditionVariableSRW"))
    return try_kernel32("SleepConditionVariableSRW", a, out);
  if (eq(api, "RtlWakeConditionVariable")) return try_kernel32("WakeConditionVariable", a, out);
  if (eq(api, "RtlWakeAllConditionVariable"))
    return try_kernel32("WakeAllConditionVariable", a, out);
  if (eq(api, "RtlRunOnceInitialize")) return try_kernel32("InitOnceInitialize", a, out);
  if (eq(api, "RtlRunOnceExecuteOnce")) return try_kernel32("InitOnceExecuteOnce", a, out);
  if (eq(api, "RtlRunOnceBeginInitialize")) return try_kernel32("InitOnceBeginInitialize", a, out);
  if (eq(api, "RtlRunOnceComplete")) return try_kernel32("InitOnceComplete", a, out);
  if (eq(api, "RtlWaitOnAddress")) return try_kernel32("WaitOnAddress", a, out);
  if (eq(api, "RtlWakeAddressSingle")) return try_kernel32("WakeByAddressSingle", a, out);
  if (eq(api, "RtlWakeAddressAll")) return try_kernel32("WakeByAddressAll", a, out);

  if (eq(api, "RtlInitializeSListHead")) {
    auto* v = new (std::nothrow) std::vector<uintptr_t>();
    if (!v) return fail(err_msg("RtlInitializeSListHead"));
    return ok(std::to_string(reinterpret_cast<uintptr_t>(v)));
  }
  if (eq(api, "RtlQueryDepthSList")) {
    auto* v = reinterpret_cast<std::vector<uintptr_t>*>(
        static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    return ok(v ? std::to_string(v->size()) : "0");
  }
  if (eq(api, "RtlInterlockedPushEntrySList")) {
    std::string hs, val;
    split1f(a, &hs, &val);
    auto* v = reinterpret_cast<std::vector<uintptr_t>*>(
        static_cast<uintptr_t>(std::strtoull(hs.c_str(), nullptr, 10)));
    if (!v) return fail(err_msg(api));
    v->push_back(static_cast<uintptr_t>(std::strtoull(val.c_str(), nullptr, 10)));
    return ok(std::to_string(v->size()));
  }
  if (eq(api, "RtlInterlockedPopEntrySList")) {
    auto* v = reinterpret_cast<std::vector<uintptr_t>*>(
        static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (!v || v->empty()) return ok("0");
    uintptr_t x = v->back();
    v->pop_back();
    return ok(std::to_string(x));
  }
  if (eq(api, "RtlInterlockedFlushSList")) {
    auto* v = reinterpret_cast<std::vector<uintptr_t>*>(
        static_cast<uintptr_t>(std::strtoull(a, nullptr, 10)));
    if (v) v->clear();
    return ok("ok");
  }

  if (eq(api, "RtlQueryEnvironmentVariable_U")) {
    const char* v = std::getenv(a && a[0] ? a : "PATH");
    if (!v) return fail(err_msg("RtlQueryEnvironmentVariable_U"));
    return ok(v);
  }
  if (eq(api, "RtlSetEnvironmentVariable")) {
    std::string name, val;
    split1f(a, &name, &val);
    if (name.empty()) return fail(err_msg("RtlSetEnvironmentVariable"));
#if defined(_WIN32)
    return fail(err_msg("RtlSetEnvironmentVariable: use native hop"));
#else
    if (val.empty()) {
      if (unsetenv(name.c_str()) != 0) return fail(err("RtlSetEnvironmentVariable"));
    } else if (setenv(name.c_str(), val.c_str(), 1) != 0) {
      return fail(err("RtlSetEnvironmentVariable"));
    }
    return ok("ok");
#endif
  }
  if (eq(api, "RtlExpandEnvironmentStrings_U")) {
    std::string e = expand_env(a);
    return ok(e);
  }
  if (eq(api, "RtlCreateEnvironment")) return ok("1");
  if (eq(api, "RtlDestroyEnvironment")) return ok("ok");
  if (eq(api, "RtlGetCurrentDirectory_U")) return ok(cwd());
  if (eq(api, "RtlSetCurrentDirectory_U")) {
    if (!a[0] || chdir(a) != 0) return fail(err_msg("RtlSetCurrentDirectory_U"));
    return ok("ok");
  }
  if (eq(api, "RtlGetFullPathName_U")) {
    if (!a[0]) return fail(err_msg("RtlGetFullPathName_U"));
    if (a[0] == '/' || a[0] == '\\' || (a[0] && a[1] == ':')) return ok(a);
    std::string d = cwd();
    if (!d.empty() && d.back() != '/' && d.back() != '\\') d += '/';
    return ok(d + a);
  }
  if (eq(api, "RtlDosPathNameToNtPathName_U")) return ok(k32_dos_to_nt(a));
  if (eq(api, "RtlNtPathNameToDosPathName")) {
    std::string s = a ? a : "";
    const char* pfx = "\\??\\UNC\\";
    if (s.rfind(pfx, 0) == 0) return ok(std::string("\\\\") + s.substr(8));
    if (s.rfind("\\??\\", 0) == 0) return ok(s.substr(4));
    return ok(s);
  }
  if (eq(api, "RtlDetermineDosPathNameType_U")) {
    std::string s = a ? a : "";
    if (s.size() >= 2 && s[0] == '\\' && s[1] == '\\') return ok("1");
    if (s.size() >= 3 && s[1] == ':' && (s[2] == '\\' || s[2] == '/')) return ok("2");
    if (s.size() >= 2 && s[1] == ':') return ok("3");
    if (!s.empty() && (s[0] == '\\' || s[0] == '/')) return ok("4");
    return ok("5");
  }
  if (eq(api, "RtlIsDosDeviceName_U")) {
    std::string s = a ? a : "";
    for (char& c : s)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return ok(s == "NUL" || s == "CON" || s == "PRN" || s == "AUX" ? "1" : "0");
  }
  if (eq(api, "RtlDosSearchPath_U")) return try_kernel32("SearchPathW", a, out);

  if (eq(api, "RtlInitializeSid") || eq(api, "RtlAllocateAndInitializeSid"))
    return try_kernel32("AllocateAndInitializeSid", a && a[0] ? a : "1\x1f" "5" "\x1f" "18", out);
  if (eq(api, "RtlLengthSid") || eq(api, "RtlLengthRequiredSid")) {
    K32Tok* t = k32_tok(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok(std::to_string(t && !t->sid.empty() ? t->sid.size() : 12));
  }
  if (eq(api, "RtlCopySid")) {
    std::string dst, src;
    split1f(a, &dst, &src);
    K32Tok* s = k32_tok(static_cast<int>(std::strtol(src.c_str(), nullptr, 10)));
    K32Tok* d = k32_tok(static_cast<int>(std::strtol(dst.c_str(), nullptr, 10)));
    if (!s || !d) return fail(err_msg("RtlCopySid"));
    d->sid = s->sid;
    d->account = s->account;
    d->domain = s->domain;
    return ok("ok");
  }
  if (eq(api, "RtlEqualSid")) {
    std::string x, y;
    split1f(a, &x, &y);
    K32Tok* A = k32_tok(static_cast<int>(std::strtol(x.c_str(), nullptr, 10)));
    K32Tok* B = k32_tok(static_cast<int>(std::strtol(y.c_str(), nullptr, 10)));
    return ok(A && B && A->sid == B->sid ? "1" : "0");
  }
  if (eq(api, "RtlValidSid")) return ok(k32_tok(static_cast<int>(std::strtol(a, nullptr, 10))) ? "1" : "0");
  if (eq(api, "RtlFreeSid")) {
    k32_close_tok(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok("ok");
  }
  if (eq(api, "RtlConvertSidToUnicodeString")) {
    K32Tok* t = k32_tok(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok(t ? t->sid : "S-1-0-0");
  }
  if (eq(api, "RtlCreateWellKnownSid")) {
    int id = static_cast<int>(std::strtol(a, nullptr, 10));
    std::string sid = id == 26 ? "S-1-5-32-544" : "S-1-1-0";
    int h = k32_alloc_tok(kTokSid);
    if (h < 0) return fail(err_msg("RtlCreateWellKnownSid"));
    k32_tok(h)->sid = sid;
    k32_sid_lookup(sid, &k32_tok(h)->account, &k32_tok(h)->domain);
    return ok(std::to_string(h));
  }
  if (eq(api, "RtlCreateSecurityDescriptor") || eq(api, "RtlSetDaclSecurityDescriptor"))
    return ok("ok");
  if (eq(api, "RtlLengthSecurityDescriptor")) return ok("20");
  if (eq(api, "RtlValidSecurityDescriptor") || eq(api, "RtlValidAcl")) return ok("1");

  if (eq(api, "RtlCopyUnicodeString") || eq(api, "RtlDuplicateUnicodeString") ||
      eq(api, "RtlCreateUnicodeString")) {
    std::string dst, src;
    split1f(a, &dst, &src);
    return ok(src.empty() ? dst : src);
  }
  if (eq(api, "RtlFreeUnicodeString") || eq(api, "RtlFreeAnsiString")) return ok("ok");
  if (eq(api, "RtlAppendUnicodeToString")) {
    std::string x, y;
    split1f(a, &x, &y);
    return ok(x + y);
  }
  if (eq(api, "RtlUpcaseUnicodeString") || eq(api, "RtlUpperString")) {
    std::string s = a;
    for (char& c : s)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return ok(s);
  }
  if (eq(api, "RtlDowncaseUnicodeString")) {
    std::string s = a;
    for (char& c : s)
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return ok(s);
  }
  if (eq(api, "RtlEqualString")) {
    std::string x, y;
    split1f(a, &x, &y);
    return ok(x == y ? "1" : "0");
  }
  if (eq(api, "RtlCompareString")) {
    std::string x, y;
    split1f(a, &x, &y);
    return ok(x < y ? "-1" : (x > y ? "1" : "0"));
  }
  if (eq(api, "RtlUpperChar")) {
    unsigned c = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    if (!c && a[0]) c = static_cast<unsigned char>(a[0]);
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    return ok(std::to_string(c));
  }
  if (eq(api, "RtlMultiByteToUnicodeN") || eq(api, "RtlUTF8ToUnicodeN")) return ok(a);
  if (eq(api, "RtlUnicodeToMultiByteN") || eq(api, "RtlUnicodeToUTF8N")) return ok(a);

  if (eq(api, "RtlTimeToTimeFields") || eq(api, "RtlTimeFieldsToTime")) return ok(filetime_now());
  if (eq(api, "RtlSecondsSince1970ToTime")) {
    time_t sec = static_cast<time_t>(std::strtoull(a, nullptr, 10));
    return ok(std::to_string(unix_to_ft(sec)));
  }
  if (eq(api, "RtlTimeToSecondsSince1970")) {
    unsigned long long ft = std::strtoull(a, nullptr, 10);
    if (!ft) return ok(std::to_string(static_cast<long long>(time(nullptr))));
    return ok(std::to_string((ft / 10000000ull) - 11644473600ull));
  }
  if (eq(api, "RtlQueryTimeZoneInformation")) return ok("0");
  if (eq(api, "RtlLocalTimeToSystemTime") || eq(api, "RtlSystemTimeToLocalTime"))
    return ok(a[0] ? a : filetime_now());
  if (eq(api, "RtlIsProcessorFeaturePresent")) return try_kernel32("IsProcessorFeaturePresent", a, out);
  if (eq(api, "RtlVerifyVersionInfo")) return try_kernel32("VerifyVersionInfoW", a, out);
  if (eq(api, "RtlGetNtGlobalFlags")) return ok("0");
  if (eq(api, "RtlComputeCrc32")) {
    unsigned c = 0xffffffffu;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(a ? a : ""); *p; ++p) {
      c ^= *p;
      for (int i = 0; i < 8; ++i) c = (c >> 1) ^ (0xEDB88320u & static_cast<unsigned>(0 - (c & 1)));
    }
    return ok(std::to_string(~c));
  }
  if (eq(api, "RtlUniform")) return try_kernel32("RtlRandomEx", a, out);
  if (eq(api, "RtlAddVectoredExceptionHandler"))
    return try_kernel32("AddVectoredExceptionHandler", a, out);
  if (eq(api, "RtlRemoveVectoredExceptionHandler"))
    return try_kernel32("RemoveVectoredExceptionHandler", a, out);
  if (eq(api, "RtlAddVectoredContinueHandler"))
    return try_kernel32("AddVectoredExceptionHandler", a, out);
  if (eq(api, "RtlCaptureContext") || eq(api, "RtlCaptureStackBackTrace") ||
      eq(api, "RtlLookupFunctionEntry") || eq(api, "RtlVirtualUnwind"))
    return ok("0");

  if (eq(api, "LdrGetDllHandleEx") || eq(api, "LdrGetDllHandleByMapping"))
    return try_kernel32("LdrGetDllHandle", a, out);
  if (eq(api, "LdrGetProcedureAddressEx")) return try_kernel32("LdrGetProcedureAddress", a, out);
  if (eq(api, "LdrLockLoaderLock") || eq(api, "LdrUnlockLoaderLock") ||
      eq(api, "LdrDisableThreadCalloutsForDll"))
    return ok("ok");
  if (eq(api, "LdrFindResource_U")) return try_kernel32("FindResourceW", a, out);
  if (eq(api, "LdrAccessResource")) return try_kernel32("LoadResource", a, out);
  if (eq(api, "LdrGetDllFullName")) {
    if (const char* p = std::getenv("_")) {
      if (p[0]) return ok(p);
    }
    std::string d = cwd();
    if (!d.empty() && d.back() != '/' && d.back() != '\\') d += '/';
    return ok(d + "main.wasm");
  }

  if (eq(api, "TpAllocPool")) return try_kernel32("CreateThreadpool", a, out);
  if (eq(api, "TpReleasePool")) return try_kernel32("CloseThreadpool", a, out);
  if (eq(api, "TpSetPoolMaxThreads")) return try_kernel32("SetThreadpoolThreadMaximum", a, out);
  if (eq(api, "TpSetPoolMinThreads")) return try_kernel32("SetThreadpoolThreadMinimum", a, out);
  if (eq(api, "TpAllocWork")) return try_kernel32("CreateThreadpoolWork", a, out);
  if (eq(api, "TpPostWork")) return try_kernel32("SubmitThreadpoolWork", a, out);
  if (eq(api, "TpReleaseWork")) return try_kernel32("CloseThreadpoolWork", a, out);
  if (eq(api, "TpWaitForWork")) return try_kernel32("WaitForThreadpoolWorkCallbacks", a, out);
  if (eq(api, "TpAllocTimer")) return try_kernel32("CreateThreadpoolTimer", a, out);
  if (eq(api, "TpSetTimer")) return try_kernel32("SetThreadpoolTimer", a, out);
  if (eq(api, "TpReleaseTimer")) return try_kernel32("CloseThreadpoolTimer", a, out);
  if (eq(api, "TpSimpleTryPost")) return try_kernel32("TrySubmitThreadpoolCallback", a, out);

  if (eq(api, "DbgPrint") || eq(api, "DbgPrintEx")) return try_kernel32("OutputDebugStringA", a, out);
  if (eq(api, "DbgQueryDebugFilterState")) return ok("0");
  if (eq(api, "DbgSetDebugFilterState")) return ok("ok");

  if (eq(api, "AddVectoredExceptionHandler")) {
    unsigned c = k32_veh_next()++;
    k32_veh().push_back(c);
    return ok(std::to_string(c));
  }
  if (eq(api, "RemoveVectoredExceptionHandler")) {
    unsigned c = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    auto& v = k32_veh();
    for (auto it = v.begin(); it != v.end(); ++it) {
      if (*it == c) {
        v.erase(it);
        return ok("ok");
      }
    }
    return fail(err_msg("RemoveVectoredExceptionHandler"));
  }
  if (eq(api, "RaiseException")) {
    if (k32_veh().empty()) return fail(err_msg("RaiseException: no handler"));
    return ok("continue");
  }
  if (eq(api, "RtlUnwind") || eq(api, "RtlUnwindEx")) return ok("ok");

  if (eq(api, "BCryptOpenAlgorithmProvider")) {
    int h = k32_alloc_cng(kCngAlg);
    if (h < 0) return fail(err_msg("BCryptOpenAlgorithmProvider: no handles"));
    K32Cng* p = k32_cng(h);
    p->alg = a && a[0] ? a : "SHA256";
    for (char& c : p->alg)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return ok(std::to_string(h));
  }
  if (eq(api, "BCryptCloseAlgorithmProvider") || eq(api, "BCryptDestroyHash") ||
      eq(api, "BCryptDestroyKey")) {
    if (!k32_close_cng(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("BCryptCloseAlgorithmProvider"));
    return ok("ok");
  }
  if (eq(api, "BCryptGenRandom")) {
    unsigned n = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    if (n == 0) n = 16;
    if (n > 4096) n = 4096;
    std::vector<unsigned char> b(n);
    k32_entropy(b.data(), n);
    return ok(k32_hex(b.data(), n));
  }
  if (eq(api, "BCryptCreateHash")) {
    K32Cng* alg = k32_cng(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!alg || alg->kind != kCngAlg) return fail(err_msg("BCryptCreateHash: bad alg"));
    int h = k32_alloc_cng(kCngHash);
    if (h < 0) return fail(err_msg("BCryptCreateHash: no handles"));
    K32Cng* p = k32_cng(h);
    p->alg = alg->alg;
    return ok(std::to_string(h));
  }
  if (eq(api, "BCryptHashData")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    K32Cng* p = k32_cng(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p || p->kind != kCngHash) return fail(err_msg("BCryptHashData"));
    p->acc.append(data);
    return ok("ok");
  }
  if (eq(api, "BCryptFinishHash")) {
    K32Cng* p = k32_cng(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!p || p->kind != kCngHash) return fail(err_msg("BCryptFinishHash"));
    unsigned char dig[32];
    k32_sha256(reinterpret_cast<const unsigned char*>(p->acc.data()), p->acc.size(), dig);
    p->acc.clear();
    return ok(k32_hex(dig, 32));
  }
  if (eq(api, "BCryptGenerateSymmetricKey")) {
    std::string hs, key;
    split1f(a, &hs, &key);
    K32Cng* alg = k32_cng(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!alg || alg->kind != kCngAlg) return fail(err_msg("BCryptGenerateSymmetricKey"));
    int h = k32_alloc_cng(kCngKey);
    if (h < 0) return fail(err_msg("BCryptGenerateSymmetricKey: no handles"));
    K32Cng* p = k32_cng(h);
    p->alg = alg->alg;
    p->key = key;
    return ok(std::to_string(h));
  }
  if (eq(api, "BCryptEncrypt") || eq(api, "BCryptDecrypt")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    K32Cng* p = k32_cng(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p) return fail(err_msg("BCryptEncrypt: bad handle"));
    std::string alg = p->alg;
    std::string key = p->key;
    if (alg.find("AES") == std::string::npos)
      return fail(err_msg("BCryptEncrypt: AES only"));
    std::string out = k32_aes128(key, data, eq(api, "BCryptDecrypt") ? 1 : 0);
    if (eq(api, "BCryptDecrypt") && out.empty() && !data.empty())
      return fail(err_msg("BCryptDecrypt"));
    return ok(out);
  }

  if (eq(api, "NCryptOpenStorageProvider")) {
    int h = k32_alloc_cng(kCngProv);
    if (h < 0) return fail(err_msg("NCryptOpenStorageProvider"));
    k32_cng(h)->alg = a && a[0] ? a : "MS_KEY_STORAGE_PROVIDER";
    return ok(std::to_string(h));
  }
  if (eq(api, "NCryptFreeObject")) {
    if (!k32_close_cng(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("NCryptFreeObject"));
    return ok("ok");
  }
  if (eq(api, "NCryptGenRandom")) {
    std::string hs, nstr;
    split1f(a, &hs, &nstr);
    unsigned n = static_cast<unsigned>(std::strtoul(nstr.empty() ? hs.c_str() : nstr.c_str(), nullptr, 10));
    if (!n) n = 16;
    if (n > 4096) n = 4096;
    std::vector<unsigned char> b(n);
    k32_entropy(b.data(), n);
    return ok(k32_hex(b.data(), n));
  }
  if (eq(api, "NCryptCreatePersistedKey") || eq(api, "NCryptOpenKey")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    K32Cng* p = k32_cng(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p) return fail(err_msg("NCryptCreatePersistedKey"));
    int h = k32_alloc_cng(kCngKey);
    if (h < 0) return fail(err_msg("NCryptCreatePersistedKey"));
    k32_cng(h)->alg = p->alg;
    k32_cng(h)->key = rest;
    return ok(std::to_string(h));
  }
  if (eq(api, "NCryptFinalizeKey") || eq(api, "NCryptDeleteKey")) {
    if (!k32_cng(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg(api));
    if (eq(api, "NCryptDeleteKey")) k32_close_cng(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok("ok");
  }
  if (eq(api, "NCryptEncrypt") || eq(api, "NCryptDecrypt")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    K32Cng* p = k32_cng(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p) return fail(err_msg("NCryptEncrypt"));
    return ok(k32_aes128(p->key, data, eq(api, "NCryptDecrypt") ? 1 : 0));
  }
  if (eq(api, "NCryptSignHash")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    unsigned char dig[32];
    k32_sha256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), dig);
    return ok(k32_hex(dig, 32));
  }
  if (eq(api, "NCryptVerifySignature")) return ok("1");
  if (eq(api, "NCryptExportKey") || eq(api, "NCryptImportKey")) {
    K32Cng* p = k32_cng(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!p) return fail(err_msg(api));
    return ok(p->key.empty() ? "ok" : p->key);
  }

  if (eq(api, "RegisterClassW") || eq(api, "RegisterClassA") || eq(api, "RegisterClassExW")) {
    std::string name = a && a[0] ? a : "WASMWin32";
    auto& cs = k32_wnd_classes();
    for (auto& c : cs)
      if (c == name) return ok("1");
    cs.push_back(name);
    return ok("1");
  }
  if (eq(api, "UnregisterClassW") || eq(api, "UnregisterClassA")) {
    auto& cs = k32_wnd_classes();
    for (auto it = cs.begin(); it != cs.end(); ++it) {
      if (*it == a) {
        cs.erase(it);
        return ok("ok");
      }
    }
    return ok("ok");
  }
  if (eq(api, "CreateWindowExW") || eq(api, "CreateWindowExA") || eq(api, "CreateWindowW") ||
      eq(api, "CreateWindowA")) {
    std::string cls, rest, title, st;
    split1f(a, &cls, &rest);
    split1f(rest.c_str(), &title, &st);
    if (cls.empty()) cls = "WASMWin32";
    int h = k32_alloc_wnd();
    if (h < 0) return fail(err_msg("CreateWindowExW: no handles"));
    K32Wnd* w = k32_wnd(h);
    w->cls = cls;
    w->title = title;
    w->style = static_cast<int>(std::strtol(st.c_str(), nullptr, 10));
    w->parent = k32_desktop();
    w->msgs.push_back(std::string("1") + "\x1f" + "0" + "\x1f" + "0");
    k32_fg_wnd() = h;
    return ok(std::to_string(h));
  }
  if (eq(api, "DestroyWindow")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h == k32_desktop()) return fail(err_msg("DestroyWindow: desktop"));
    K32Wnd* w = k32_wnd(h);
    if (!w) return fail(err_msg("DestroyWindow: bad hwnd"));
    w->msgs.push_back(std::string("2") + "\x1f" + "0" + "\x1f" + "0");
    if (k32_fg_wnd() == h) k32_fg_wnd() = k32_desktop();
    k32_close_wnd(h);
    return ok("ok");
  }
  if (eq(api, "ShowWindow")) {
    std::string hs, cmd;
    split1f(a, &hs, &cmd);
    K32Wnd* w = k32_wnd(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!w) return fail(err_msg("ShowWindow: bad hwnd"));
    w->vis = cmd != "0";
    return ok("1");
  }
  if (eq(api, "GetDesktopWindow")) return ok(std::to_string(k32_desktop()));
  if (eq(api, "GetForegroundWindow")) {
    if (!k32_fg_wnd()) k32_fg_wnd() = k32_desktop();
    return ok(std::to_string(k32_fg_wnd()));
  }
  if (eq(api, "SetForegroundWindow")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (!k32_wnd(h)) return fail(err_msg("SetForegroundWindow"));
    k32_fg_wnd() = h;
    return ok("1");
  }
  if (eq(api, "GetClientRect") || eq(api, "GetWindowRect")) {
    K32Wnd* w = k32_wnd(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!w) w = k32_wnd(k32_desktop());
    return ok(std::to_string(w->x) + "\x1f" + std::to_string(w->y) + "\x1f" +
              std::to_string(w->w) + "\x1f" + std::to_string(w->h));
  }
  if (eq(api, "SetWindowPos")) {
    std::string hs, rest, x, y, wh;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &x, &rest);
    split1f(rest.c_str(), &y, &wh);
    std::string ww, hh;
    split1f(wh.c_str(), &ww, &hh);
    K32Wnd* w = k32_wnd(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!w) return fail(err_msg("SetWindowPos"));
    if (!x.empty()) w->x = static_cast<int>(std::strtol(x.c_str(), nullptr, 10));
    if (!y.empty()) w->y = static_cast<int>(std::strtol(y.c_str(), nullptr, 10));
    if (!ww.empty()) w->w = static_cast<int>(std::strtol(ww.c_str(), nullptr, 10));
    if (!hh.empty()) w->h = static_cast<int>(std::strtol(hh.c_str(), nullptr, 10));
    return ok("1");
  }
  if (eq(api, "SetWindowTextW") || eq(api, "SetWindowTextA")) {
    std::string hs, title;
    split1f(a, &hs, &title);
    K32Wnd* w = k32_wnd(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!w) return fail(err_msg("SetWindowTextW"));
    w->title = title;
    return ok("1");
  }
  if (eq(api, "GetWindowTextW") || eq(api, "GetWindowTextA") || eq(api, "GetWindowTextLengthW")) {
    K32Wnd* w = k32_wnd(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!w) return fail(err_msg("GetWindowTextW"));
    if (eq(api, "GetWindowTextLengthW")) return ok(std::to_string(w->title.size()));
    return ok(w->title);
  }
  if (eq(api, "GetWindowLongPtrW") || eq(api, "GetWindowLongW")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    K32Wnd* w = k32_wnd(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!w) return fail(err_msg("GetWindowLongPtrW"));
    int i = static_cast<int>(std::strtol(idx.c_str(), nullptr, 10));
    if (i == -4) return ok(std::to_string(w->style));
    return ok(std::to_string(w->userdata));
  }
  if (eq(api, "SetWindowLongPtrW") || eq(api, "SetWindowLongW")) {
    std::string hs, rest, idx, val;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &idx, &val);
    K32Wnd* w = k32_wnd(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!w) return fail(err_msg("SetWindowLongPtrW"));
    unsigned long old = w->userdata;
    w->userdata = static_cast<unsigned long>(std::strtoul(val.c_str(), nullptr, 10));
    return ok(std::to_string(old));
  }
  if (eq(api, "FindWindowW") || eq(api, "FindWindowA")) {
    std::string cls, title;
    split1f(a, &cls, &title);
    K32Wnd* t = k32_wnds();
    for (int i = 0; i < kK32Max; ++i) {
      if (!t[i].used) continue;
      if (!cls.empty() && t[i].cls != cls) continue;
      if (!title.empty() && t[i].title != title) continue;
      return ok(std::to_string(kK32WndBase + i));
    }
    return fail(err_msg("FindWindowW: not found"));
  }
  if (eq(api, "EnumWindows")) {
    std::string ids;
    K32Wnd* t = k32_wnds();
    for (int i = 0; i < kK32Max; ++i) {
      if (!t[i].used) continue;
      if (!ids.empty()) ids += ",";
      ids += std::to_string(kK32WndBase + i);
    }
    return ok(ids);
  }
  if (eq(api, "GetWindowThreadProcessId")) {
    if (!k32_wnd(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("GetWindowThreadProcessId"));
    return ok(std::to_string(static_cast<long>(getpid())));
  }
  if (eq(api, "PostMessageW") || eq(api, "PostMessageA") || eq(api, "SendMessageW") ||
      eq(api, "SendMessageA")) {
    std::string hs, rest, msg, wp;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &msg, &wp);
    std::string lp;
    split1f(wp.c_str(), &wp, &lp);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Wnd* w = k32_wnd(h);
    if (!w) w = k32_wnd(k32_desktop());
    std::string m = (msg.empty() ? "0" : msg) + "\x1f" + (wp.empty() ? "0" : wp) + "\x1f" +
                    (lp.empty() ? "0" : lp);
    w->msgs.push_back(m);
    return ok("1");
  }
  if (eq(api, "PostQuitMessage")) {
    K32Wnd* w = k32_wnd(k32_fg_wnd() ? k32_fg_wnd() : k32_desktop());
    if (w) w->msgs.push_back(std::string("18") + "\x1f" + (a[0] ? a : "0") + "\x1f" + "0");
    return ok("ok");
  }
  if (eq(api, "PeekMessageW") || eq(api, "PeekMessageA") || eq(api, "GetMessageW") ||
      eq(api, "GetMessageA")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Wnd* w = h ? k32_wnd(h) : nullptr;
    auto take = [&](K32Wnd* wnd) -> std::string {
      if (!wnd || wnd->msgs.empty()) return {};
      std::string m = wnd->msgs.front();
      wnd->msgs.pop_front();
      return m;
    };
    std::string m;
    if (w) m = take(w);
    if (m.empty()) {
      K32Wnd* t = k32_wnds();
      for (int i = 0; i < kK32Max && m.empty(); ++i)
        if (t[i].used) m = take(&t[i]);
    }
    if (m.empty()) {
      if (eq(api, "GetMessageW") || eq(api, "GetMessageA")) return ok("quit");
      return ok("0");
    }
    if (m.rfind("18\x1f", 0) == 0) return ok("quit");
    return ok(m);
  }
  if (eq(api, "TranslateMessage") || eq(api, "DispatchMessageW") || eq(api, "DispatchMessageA") ||
      eq(api, "DefWindowProcW") || eq(api, "DefWindowProcA"))
    return ok("0");
  if (eq(api, "InvalidateRect") || eq(api, "UpdateWindow") || eq(api, "BeginPaint") ||
      eq(api, "EndPaint")) {
    if (eq(api, "BeginPaint")) {
      int h = static_cast<int>(std::strtol(a, nullptr, 10));
      K32Wnd* w = k32_wnd(h);
      if (!w) return fail(err_msg("BeginPaint"));
      if (!w->dc) {
        w->dc = k32_alloc_gdi(kGdiDc);
        if (K32Gdi* d = k32_gdi(w->dc)) d->wnd = h;
      }
      return ok(std::to_string(w->dc));
    }
    return ok("1");
  }
  if (eq(api, "MessageBoxW") || eq(api, "MessageBoxA")) return ok("1");
  if (eq(api, "GetCursorPos")) return ok("0\x1f" "0");
  if (eq(api, "SetCursorPos")) return ok("1");
  if (eq(api, "GetSystemMetrics")) {
    int i = static_cast<int>(std::strtol(a, nullptr, 10));
    if (i == 0 || i == 1) return ok("1024");
    return ok("0");
  }
  if (eq(api, "LoadCursorW") || eq(api, "LoadCursorA") || eq(api, "LoadIconW") ||
      eq(api, "LoadIconA") || eq(api, "LoadImageW"))
    return ok("1");
  if (eq(api, "GetAsyncKeyState") || eq(api, "GetKeyState")) return ok("0");
  if (eq(api, "GetDC") || eq(api, "GetWindowDC")) {
    int h = a[0] ? static_cast<int>(std::strtol(a, nullptr, 10)) : k32_desktop();
    K32Wnd* w = k32_wnd(h);
    if (!w) w = k32_wnd(k32_desktop());
    if (!w->dc) {
      w->dc = k32_alloc_gdi(kGdiDc);
      if (K32Gdi* d = k32_gdi(w->dc)) {
        d->wnd = h;
        d->bw = w->w;
        d->bh = w->h;
      }
    }
    return ok(std::to_string(w->dc));
  }
  if (eq(api, "ReleaseDC")) {
    std::string hs, dcs;
    split1f(a, &hs, &dcs);
    return ok("1");
  }

  if (eq(api, "CreateCompatibleDC")) {
    int h = k32_alloc_gdi(kGdiDc);
    if (h < 0) return fail(err_msg("CreateCompatibleDC"));
    if (K32Gdi* d = k32_gdi(h)) {
      d->bw = 1;
      d->bh = 1;
      d->bits.assign(4, 0);
    }
    return ok(std::to_string(h));
  }
  if (eq(api, "DeleteDC")) {
    if (!k32_close_gdi(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("DeleteDC"));
    return ok("1");
  }
  if (eq(api, "CreateSolidBrush")) {
    int h = k32_alloc_gdi(kGdiBrush);
    if (h < 0) return fail(err_msg("CreateSolidBrush"));
    k32_gdi(h)->color = static_cast<unsigned>(std::strtoul(a, nullptr, 0));
    return ok(std::to_string(h));
  }
  if (eq(api, "CreatePen")) {
    std::string style, rest, w, color;
    split1f(a, &style, &rest);
    split1f(rest.c_str(), &w, &color);
    int h = k32_alloc_gdi(kGdiPen);
    if (h < 0) return fail(err_msg("CreatePen"));
    k32_gdi(h)->width = static_cast<int>(std::strtol(w.c_str(), nullptr, 10));
    k32_gdi(h)->color = static_cast<unsigned>(std::strtoul(color.c_str(), nullptr, 0));
    return ok(std::to_string(h));
  }
  if (eq(api, "CreateFontW") || eq(api, "CreateFontA")) {
    int h = k32_alloc_gdi(kGdiFont);
    if (h < 0) return fail(err_msg("CreateFontW"));
    k32_gdi(h)->face = a && a[0] ? a : "Arial";
    return ok(std::to_string(h));
  }
  if (eq(api, "CreateCompatibleBitmap")) {
    std::string dcs, rest, ww, hh;
    split1f(a, &dcs, &rest);
    split1f(rest.c_str(), &ww, &hh);
    int h = k32_alloc_gdi(kGdiBmp);
    if (h < 0) return fail(err_msg("CreateCompatibleBitmap"));
    K32Gdi* b = k32_gdi(h);
    b->bw = static_cast<int>(std::strtol(ww.c_str(), nullptr, 10));
    b->bh = static_cast<int>(std::strtol(hh.c_str(), nullptr, 10));
    if (b->bw < 1) b->bw = 1;
    if (b->bh < 1) b->bh = 1;
    b->bits.assign(static_cast<size_t>(b->bw * b->bh * 4), 0);
    return ok(std::to_string(h));
  }
  if (eq(api, "GetStockObject")) {
    int h = k32_alloc_gdi(kGdiStock);
    if (h < 0) return fail(err_msg("GetStockObject"));
    k32_gdi(h)->color = 0xffffffu;
    return ok(std::to_string(h));
  }
  if (eq(api, "SelectObject")) {
    std::string dcs, obj;
    split1f(a, &dcs, &obj);
    K32Gdi* d = k32_gdi(static_cast<int>(std::strtol(dcs.c_str(), nullptr, 10)));
    K32Gdi* o = k32_gdi(static_cast<int>(std::strtol(obj.c_str(), nullptr, 10)));
    if (!d || !o) return fail(err_msg("SelectObject"));
    int prev = 0;
    if (o->kind == kGdiBrush) {
      prev = d->sel_brush;
      d->sel_brush = static_cast<int>(std::strtol(obj.c_str(), nullptr, 10));
    } else if (o->kind == kGdiPen) {
      prev = d->sel_pen;
      d->sel_pen = static_cast<int>(std::strtol(obj.c_str(), nullptr, 10));
    } else if (o->kind == kGdiFont) {
      prev = d->sel_font;
      d->sel_font = static_cast<int>(std::strtol(obj.c_str(), nullptr, 10));
    } else if (o->kind == kGdiBmp) {
      prev = d->sel_bmp;
      d->sel_bmp = static_cast<int>(std::strtol(obj.c_str(), nullptr, 10));
    }
    return ok(std::to_string(prev));
  }
  if (eq(api, "DeleteObject")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (!k32_gdi(h)) return fail(err_msg("DeleteObject"));
    k32_close_gdi(h);
    return ok("1");
  }
  if (eq(api, "GetObjectW") || eq(api, "GetObjectA")) {
    K32Gdi* o = k32_gdi(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!o) return fail(err_msg("GetObjectW"));
    return ok(std::to_string(o->kind) + "\x1f" + std::to_string(o->color));
  }
  if (eq(api, "GetDeviceCaps")) {
    std::string dcs, idx;
    split1f(a, &dcs, &idx);
    int i = static_cast<int>(std::strtol(idx.c_str(), nullptr, 10));
    if (i == 8 || i == 10) return ok("1024");
    return ok("96");
  }
  if (eq(api, "SetBkMode") || eq(api, "SetTextColor") || eq(api, "SetBkColor")) return ok("0");
  if (eq(api, "MoveToEx")) return ok("1");
  if (eq(api, "LineTo") || eq(api, "Rectangle") || eq(api, "Ellipse") || eq(api, "TextOutW") ||
      eq(api, "TextOutA") || eq(api, "BitBlt") || eq(api, "StretchBlt") || eq(api, "SetPixel"))
    return ok("1");
  if (eq(api, "GetPixel")) return ok("0");

  if (eq(api, "CoInitialize") || eq(api, "CoInitializeEx") || eq(api, "OleInitialize")) {
    k32_com_apt() = 1;
    return ok("0");
  }
  if (eq(api, "CoUninitialize") || eq(api, "OleUninitialize")) {
    k32_com_apt() = 0;
    return ok("ok");
  }
  if (eq(api, "CoCreateGuid") || eq(api, "UuidCreate")) {
    unsigned char g[16];
    k32_entropy(g, 16);
    g[6] = static_cast<unsigned char>((g[6] & 0x0f) | 0x40);
    g[8] = static_cast<unsigned char>((g[8] & 0x3f) | 0x80);
    return ok(k32_hex(g, 16));
  }
  if (eq(api, "CoTaskMemAlloc")) {
    unsigned n = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    if (!n) n = 1;
    void* p = std::malloc(n);
    if (!p) return fail(err_msg("CoTaskMemAlloc"));
    k32_vmem_add(p, n, 4);
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (eq(api, "CoTaskMemFree")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    k32_vmem().erase(p);
    if (p) std::free(reinterpret_cast<void*>(p));
    return ok("ok");
  }
  if (eq(api, "CoCreateInstance")) {
    int h = k32_alloc_com();
    if (h < 0) return fail(err_msg("CoCreateInstance"));
    k32_com(h)->clsid = a && a[0] ? a : "00000000-0000-0000-0000-000000000000";
    return ok(std::to_string(h));
  }
  if (eq(api, "CoGetClassObject")) {
    int h = k32_alloc_com();
    if (h < 0) return fail(err_msg("CoGetClassObject"));
    k32_com(h)->clsid = a && a[0] ? a : "factory";
    return ok(std::to_string(h));
  }
  if (eq(api, "CLSIDFromString")) {
    return ok(a && a[0] ? a : "00000000-0000-0000-0000-000000000000");
  }
  if (eq(api, "StringFromCLSID") || eq(api, "StringFromGUID2")) {
    return ok(a && a[0] ? a : "{00000000-0000-0000-0000-000000000000}");
  }
  if (eq(api, "SysAllocString") || eq(api, "SysAllocStringLen")) {
    int h = k32_alloc_com();
    if (h < 0) return fail(err_msg("SysAllocString"));
    k32_com(h)->data = a ? a : "";
    return ok(std::to_string(h));
  }
  if (eq(api, "SysFreeString")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (k32_com(h)) k32_close_com(h);
    return ok("ok");
  }
  if (eq(api, "SysStringLen")) {
    K32Com* p = k32_com(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok(std::to_string(p ? p->data.size() : 0));
  }
  if (eq(api, "VariantInit") || eq(api, "VariantClear")) return ok("ok");

  if (eq(api, "CryptProtectData")) {
    std::string blob = k32_dpapi_xor(a ? a : "", 0);
    return ok(k32_hex(reinterpret_cast<const unsigned char*>(blob.data()), blob.size()));
  }
  if (eq(api, "CryptUnprotectData")) {
    std::string hex = a ? a : "";
    std::string raw;
    if (hex.size() % 2 == 0) {
      raw.resize(hex.size() / 2);
      for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        auto nyb = [](char c) -> int {
          if (c >= '0' && c <= '9') return c - '0';
          if (c >= 'a' && c <= 'f') return c - 'a' + 10;
          if (c >= 'A' && c <= 'F') return c - 'A' + 10;
          return 0;
        };
        raw[i / 2] = static_cast<char>((nyb(hex[i]) << 4) | nyb(hex[i + 1]));
      }
    }
    std::string plain = k32_dpapi_xor(raw, 1);
    if (plain.empty() && !raw.empty()) return fail(err_msg("CryptUnprotectData"));
    return ok(plain);
  }
  if (eq(api, "CryptBinaryToStringW") || eq(api, "CryptBinaryToStringA")) {
    std::string data, flags;
    split1f(a, &data, &flags);
    unsigned f = static_cast<unsigned>(std::strtoul(flags.c_str(), nullptr, 0));
    if (f == 4 || f == 0x0000000c)
      return ok(k32_hex(reinterpret_cast<const unsigned char*>(data.data()), data.size()));
    return ok(k32_b64(reinterpret_cast<const unsigned char*>(data.data()), data.size()));
  }
  if (eq(api, "CryptStringToBinaryW") || eq(api, "CryptStringToBinaryA")) {
    std::string data, flags;
    split1f(a, &data, &flags);
    unsigned f = static_cast<unsigned>(std::strtoul(flags.c_str(), nullptr, 0));
    if (f == 4 || f == 0x0000000c) {
      std::string raw(data.size() / 2, '\0');
      for (size_t i = 0; i + 1 < data.size(); i += 2) {
        auto nyb = [](char c) -> int {
          if (c >= '0' && c <= '9') return c - '0';
          if (c >= 'a' && c <= 'f') return c - 'a' + 10;
          if (c >= 'A' && c <= 'F') return c - 'A' + 10;
          return 0;
        };
        raw[i / 2] = static_cast<char>((nyb(data[i]) << 4) | nyb(data[i + 1]));
      }
      return ok(raw);
    }
    return ok(k32_unb64(data));
  }
  if (eq(api, "CertOpenStore") || eq(api, "CertOpenSystemStoreW")) {
    int h = k32_alloc_cert(kCertStore);
    if (h < 0) return fail(err_msg("CertOpenStore"));
    k32_cert(h)->name = a && a[0] ? a : "MEMORY";
    return ok(std::to_string(h));
  }
  if (eq(api, "CertCloseStore")) {
    if (!k32_close_cert(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("CertCloseStore"));
    return ok("ok");
  }
  if (eq(api, "CertEnumCertificatesInStore")) return ok("");
  if (eq(api, "CertCreateCertificateContext")) {
    int h = k32_alloc_cert(kCertCtx);
    if (h < 0) return fail(err_msg("CertCreateCertificateContext"));
    k32_cert(h)->blob = a ? a : "";
    return ok(std::to_string(h));
  }
  if (eq(api, "CertFreeCertificateContext")) {
    k32_close_cert(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok("ok");
  }
  if (eq(api, "CertAddCertificateContextToStore")) return ok("ok");
  if (eq(api, "PFXImportCertStore")) {
    int h = k32_alloc_cert(kCertStore);
    if (h < 0) return fail(err_msg("PFXImportCertStore"));
    k32_cert(h)->name = "PFX";
    k32_cert(h)->blob = a ? a : "";
    return ok(std::to_string(h));
  }

  auto folder = [&]() -> std::string {
    int id = -1;
    if (a && a[0] && ((a[0] >= '0' && a[0] <= '9') || a[0] == '-'))
      id = static_cast<int>(std::strtol(a, nullptr, 0));
    const char* home = std::getenv("HOME");
    if (!home || !home[0]) home = std::getenv("USERPROFILE");
    std::string h = home && home[0] ? home : "/tmp";
    const char* windir = std::getenv("WINDIR");
    std::string w = windir && windir[0] ? windir : "/windows";
    if (id == 36 || eq(a, "Windows") || eq(a, "FOLDERID_Windows")) return w;
    if (id == 37 || eq(a, "System") || eq(a, "FOLDERID_System")) return w + "/system32";
    if (id == 26 || eq(a, "RoamingAppData") || eq(a, "FOLDERID_RoamingAppData"))
      return h + "/AppData/Roaming";
    if (id == 5 || eq(a, "Documents") || eq(a, "FOLDERID_Documents")) return h + "/Documents";
    if (id == 0 || eq(a, "Desktop") || eq(a, "FOLDERID_Desktop")) return h + "/Desktop";
    return h;
  };
  if (eq(api, "SHGetFolderPathW") || eq(api, "SHGetFolderPathA") ||
      eq(api, "SHGetSpecialFolderPathW"))
    return ok(folder());
  if (eq(api, "SHGetKnownFolderPath")) return ok(folder());
  if (eq(api, "CommandLineToArgvW")) {
    std::string s = a && a[0] ? a : "main.wasm";
    std::string o;
    std::string cur;
    bool q = false;
    for (size_t i = 0; i <= s.size(); ++i) {
      char c = i < s.size() ? s[i] : ' ';
      if (c == '"') {
        q = !q;
        continue;
      }
      if (!q && (c == ' ' || c == '\t' || i == s.size())) {
        if (!cur.empty()) {
          if (!o.empty()) o.push_back('\x1f');
          o += cur;
          cur.clear();
        }
      } else if (i < s.size())
        cur.push_back(c);
    }
    return ok(o.empty() ? s : o);
  }
  if (eq(api, "PathFileExistsW") || eq(api, "PathFileExistsA")) {
    if (!a[0]) return ok("0");
    return ok(access(a, F_OK) == 0 ? "1" : "0");
  }
  if (eq(api, "PathCombineW") || eq(api, "PathCombineA")) {
    std::string dir, file;
    split1f(a, &dir, &file);
    if (dir.empty()) return ok(file);
    char sep = (dir.find('\\') != std::string::npos) ? '\\' : '/';
    if (dir.back() != '/' && dir.back() != '\\') dir.push_back(sep);
    return ok(dir + file);
  }
  if (eq(api, "PathFindFileNameW") || eq(api, "PathFindFileNameA") ||
      eq(api, "GetFileTitleW") || eq(api, "GetFileTitleA")) {
    std::string s = a ? a : "";
    auto p = s.find_last_of("/\\");
    return ok(p == std::string::npos ? s : s.substr(p + 1));
  }
  if (eq(api, "PathIsDirectoryW") || eq(api, "PathIsDirectoryA")) {
    if (!a[0]) return ok("0");
    struct stat st {};
    if (::stat(a, &st) != 0) return ok("0");
    return ok(S_ISDIR(st.st_mode) ? "1" : "0");
  }
  if (eq(api, "PathIsRelativeW") || eq(api, "PathIsRelativeA")) {
    std::string s = a ? a : "";
    if (s.empty()) return ok("1");
    if (s[0] == '/' || s[0] == '\\') return ok("0");
    if (s.size() >= 3 && s[1] == ':' && (s[2] == '/' || s[2] == '\\')) return ok("0");
    return ok("1");
  }
  if (eq(api, "PathCanonicalizeW") || eq(api, "PathCanonicalizeA")) {
    std::string s = a ? a : "";
    char sep = (s.find('\\') != std::string::npos) ? '\\' : '/';
    for (char& c : s)
      if (c == '/' || c == '\\') c = sep;
    std::string drive;
    if (s.size() >= 2 && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) &&
        s[1] == ':') {
      drive = s.substr(0, 2);
      s = s.substr(2);
    }
    bool abs = !s.empty() && s[0] == sep;
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i <= s.size(); ++i) {
      char c = i < s.size() ? s[i] : sep;
      if (c == sep) {
        if (cur == "..") {
          if (!parts.empty()) parts.pop_back();
        } else if (!cur.empty() && cur != ".")
          parts.push_back(cur);
        cur.clear();
      } else
        cur.push_back(c);
    }
    std::string o = drive;
    if (abs) o.push_back(sep);
    for (size_t i = 0; i < parts.size(); ++i) {
      if (i) o.push_back(sep);
      o += parts[i];
    }
    return ok(o.empty() ? "." : o);
  }
  if (eq(api, "PathAppendW") || eq(api, "PathAppendA")) {
    std::string dir, file;
    split1f(a, &dir, &file);
    if (dir.empty()) return ok(file);
    char sep = (dir.find('\\') != std::string::npos) ? '\\' : '/';
    if (dir.back() != '/' && dir.back() != '\\') dir.push_back(sep);
    return ok(dir + file);
  }
  if (eq(api, "PathRemoveFileSpecW") || eq(api, "PathRemoveFileSpecA")) {
    std::string s = a ? a : "";
    auto p = s.find_last_of("/\\");
    if (p == std::string::npos) return ok("");
    if (p == 0) return ok(s.substr(0, 1));
    return ok(s.substr(0, p));
  }
  if (eq(api, "StrCmpIW") || eq(api, "StrCmpIA")) {
    std::string x, y;
    split1f(a, &x, &y);
    auto fold = [](std::string s) {
      for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
      return s;
    };
    x = fold(x);
    y = fold(y);
    if (x == y) return ok("0");
    return ok(x < y ? "-1" : "1");
  }
  if (eq(api, "CommDlgExtendedError")) return ok("0");
  if (eq(api, "SHGetFileInfoW")) return ok("ok");

  if (eq(api, "WinHttpCrackUrl") || eq(api, "InternetCrackUrlW"))
    return ok(k32_crack_url(a));
  if (eq(api, "WinHttpCreateUrl")) {
    std::string scheme, rest, host, rest2, port, path;
    split1f(a, &scheme, &rest);
    split1f(rest.c_str(), &host, &rest2);
    split1f(rest2.c_str(), &port, &path);
    if (scheme.empty()) scheme = "http";
    if (path.empty()) path = "/";
    std::string u = scheme + "://" + host;
    if (!port.empty()) u += ":" + port;
    u += path;
    return ok(u);
  }
  if (eq(api, "WinHttpOpen") || eq(api, "InternetOpenW")) {
    int h = k32_alloc_http(kHttpSess);
    if (h < 0) return fail(err_msg("WinHttpOpen"));
    k32_http(h)->agent = a && a[0] ? a : "wasmwin32";
    return ok(std::to_string(h));
  }
  if (eq(api, "WinHttpConnect") || eq(api, "InternetConnectW")) {
    std::string hs, rest, host, port;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &host, &port);
    if (!k32_http(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10))))
      return fail(err_msg("WinHttpConnect"));
    int h = k32_alloc_http(kHttpConn);
    if (h < 0) return fail(err_msg("WinHttpConnect"));
    k32_http(h)->parent = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    k32_http(h)->host = host.empty() ? rest : host;
    k32_http(h)->port = static_cast<unsigned>(std::strtoul(port.c_str(), nullptr, 10));
    if (!k32_http(h)->port) k32_http(h)->port = 80;
    return ok(std::to_string(h));
  }
  if (eq(api, "WinHttpOpenRequest") || eq(api, "HttpOpenRequestW")) {
    std::string hs, rest, verb, path;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &verb, &path);
    if (!k32_http(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10))))
      return fail(err_msg("WinHttpOpenRequest"));
    int h = k32_alloc_http(kHttpReq);
    if (h < 0) return fail(err_msg("WinHttpOpenRequest"));
    k32_http(h)->parent = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    k32_http(h)->verb = verb.empty() ? "GET" : verb;
    k32_http(h)->path = path.empty() ? "/" : path;
    k32_http(h)->body = "ok";
    return ok(std::to_string(h));
  }
  if (eq(api, "WinHttpAddRequestHeaders") || eq(api, "HttpAddRequestHeadersW")) {
    std::string hs, hdr;
    split1f(a, &hs, &hdr);
    K32Http* p = k32_http(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p) return fail(err_msg(api));
    if (!p->headers.empty()) p->headers += "\r\n";
    p->headers += hdr;
    return ok("ok");
  }
  if (eq(api, "WinHttpSendRequest") || eq(api, "HttpSendRequestW")) {
    K32Http* p = k32_http(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!p) return fail(err_msg("WinHttpSendRequest"));
    p->sent = 1;
    p->off = 0;
    return ok("ok");
  }
  if (eq(api, "WinHttpReceiveResponse")) {
    K32Http* p = k32_http(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!p || !p->sent) return fail(err_msg("WinHttpReceiveResponse"));
    return ok("200");
  }
  if (eq(api, "WinHttpQueryHeaders") || eq(api, "HttpQueryInfoW")) {
    K32Http* p = k32_http(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!p) return fail(err_msg(api));
    return ok("HTTP/1.1 200 OK");
  }
  if (eq(api, "WinHttpReadData") || eq(api, "InternetReadFile")) {
    std::string hs, nstr;
    split1f(a, &hs, &nstr);
    K32Http* p = k32_http(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p) return fail(err_msg(api));
    unsigned n = static_cast<unsigned>(std::strtoul(nstr.c_str(), nullptr, 10));
    if (!n) n = static_cast<unsigned>(p->body.size());
    if (p->off >= p->body.size()) return ok("");
    std::string out = p->body.substr(p->off, n);
    p->off += static_cast<unsigned>(out.size());
    return ok(out);
  }
  if (eq(api, "WinHttpWriteData") || eq(api, "InternetWriteFile")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    K32Http* p = k32_http(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p) return fail(err_msg(api));
    p->body += data;
    return ok(std::to_string(data.size()));
  }
  if (eq(api, "WinHttpCloseHandle") || eq(api, "InternetCloseHandle")) {
    if (!k32_close_http(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("WinHttpCloseHandle"));
    return ok("ok");
  }
  if (eq(api, "WinHttpSetTimeouts") || eq(api, "WinHttpSetOption") ||
      eq(api, "WinHttpQueryOption"))
    return ok("ok");

  if (eq(api, "GetAdaptersAddresses") || eq(api, "GetAdaptersInfo"))
    return ok("lo" "\x1f" "127.0.0.1");
  if (eq(api, "GetNetworkParams")) {
    char name[256] = {};
    if (gethostname(name, sizeof(name) - 1) != 0) std::strcpy(name, "wasm");
    return ok(name);
  }
  if (eq(api, "GetIfTable")) return ok("1" "\x1f" "lo");
  if (eq(api, "GetBestInterface") || eq(api, "GetBestInterfaceEx")) return ok("1");

  if (eq(api, "GetFileVersionInfoSizeW") || eq(api, "GetFileVersionInfoSizeA") ||
      eq(api, "GetFileVersionInfoSizeExW")) {
    int h = k32_find_mod(k32_mod_norm(a ? a : ""));
    K32Mod* m = h >= 0 ? k32_mod(h) : nullptr;
    if (m && m->pe && m->base) {
      PeMap tmp;
      tmp.base = m->base;
      tmp.size = m->n;
      tmp.resources = m->resources;
      std::string v = pe_file_version(tmp);
      if (!v.empty()) return ok("52");
    }
    return ok("52");
  }
  if (eq(api, "GetFileVersionInfoW") || eq(api, "GetFileVersionInfoA") ||
      eq(api, "GetFileVersionInfoExW")) {
    std::string path, rest;
    split1f(a, &path, &rest);
    int h = k32_find_mod(k32_mod_norm(path.c_str()));
    K32Mod* m = h >= 0 ? k32_mod(h) : nullptr;
    if (m && m->pe && m->base) {
      PeMap tmp;
      tmp.base = m->base;
      tmp.size = m->n;
      tmp.resources = m->resources;
      std::string v = pe_file_version(tmp);
      if (!v.empty()) return ok(v);
    }
    return ok("10.0.0.1");
  }
  if (eq(api, "VerQueryValueW") || eq(api, "VerQueryValueA")) {
    std::string blob, q;
    split1f(a, &blob, &q);
    return ok(blob.empty() ? "10.0.0.1" : blob);
  }

  if (eq(api, "InitCommonControls") || eq(api, "InitCommonControlsEx")) return ok("ok");
  if (eq(api, "ImageList_Create")) {
    std::string w, rest, h;
    split1f(a, &w, &rest);
    split1f(rest.c_str(), &h, &rest);
    int id = k32_alloc_gdi(kGdiImg);
    if (id < 0) return fail(err_msg("ImageList_Create"));
    k32_gdi(id)->bw = static_cast<int>(std::strtol(w.c_str(), nullptr, 10));
    k32_gdi(id)->bh = static_cast<int>(std::strtol(h.c_str(), nullptr, 10));
    if (!k32_gdi(id)->bw) k32_gdi(id)->bw = 16;
    if (!k32_gdi(id)->bh) k32_gdi(id)->bh = 16;
    k32_gdi(id)->width = 0;
    return ok(std::to_string(id));
  }
  if (eq(api, "ImageList_Destroy")) {
    if (!k32_close_gdi(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("ImageList_Destroy"));
    return ok("ok");
  }
  if (eq(api, "ImageList_Add") || eq(api, "ImageList_AddMasked")) {
    K32Gdi* p = k32_gdi(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!p || p->kind != kGdiImg) return fail(err_msg("ImageList_Add"));
    p->width++;
    return ok(std::to_string(p->width - 1));
  }
  if (eq(api, "ImageList_GetImageCount")) {
    K32Gdi* p = k32_gdi(static_cast<int>(std::strtol(a, nullptr, 10)));
    return ok(std::to_string(p && p->kind == kGdiImg ? p->width : 0));
  }

  if (eq(api, "InternetGetConnectedState")) return ok("1");
  if (eq(api, "DnsNameCompare_A") || eq(api, "DnsNameCompare_W")) {
    std::string x, y;
    split1f(a, &x, &y);
    auto fold = [](std::string s) {
      for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
      return s;
    };
    return ok(fold(x) == fold(y) ? "1" : "0");
  }
  if (eq(api, "DnsHostnameToComputerName_W") || eq(api, "DnsHostnameToComputerName_A")) {
    if (a && a[0]) return ok(a);
    char name[256] = {};
    if (gethostname(name, sizeof(name) - 1) != 0) std::strcpy(name, "wasm");
    return ok(name);
  }
  if (eq(api, "GetUserNameExW") || eq(api, "GetUserNameExA")) {
    const char* u = std::getenv("USER");
    if (!u || !u[0]) u = std::getenv("USERNAME");
    return ok(u && u[0] ? u : "user");
  }
  if (eq(api, "SymInitialize") || eq(api, "SymInitializeW")) return ok("ok");
  if (eq(api, "SymCleanup") || eq(api, "SymSetOptions")) return ok("ok");
  if (eq(api, "SymGetOptions")) return ok("0");
  if (eq(api, "ImageNtHeader")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    K32Mod* m = k32_mod(h);
    if (m && m->pe && m->base) {
      const unsigned char* p = static_cast<const unsigned char*>(m->base);
      uint32_t lfanew = (uint32_t)p[0x3c] | ((uint32_t)p[0x3d] << 8) | ((uint32_t)p[0x3e] << 16) |
                        ((uint32_t)p[0x3f] << 24);
      if (lfanew + 4 <= m->n) return ok(std::to_string(reinterpret_cast<uintptr_t>(p + lfanew)));
    }
    return ok("PE");
  }
  if (eq(api, "WinVerifyTrust"))
    return fail(err_msg("WinVerifyTrust: no Authenticode engine in this module"));
  if (eq(api, "IsThemeActive") || eq(api, "IsAppThemed")) return ok("0");
  if (eq(api, "DwmIsCompositionEnabled")) return ok("0");

  if (eq(api, "SetupDiGetClassDevsW") || eq(api, "SetupDiGetClassDevsA") ||
      eq(api, "SetupDiGetClassDevsExW")) {
    int h = k32_alloc_misc(kMiscDev);
    if (h < 0) return fail(err_msg("SetupDiGetClassDevsW"));
    k32_misc(h)->name = "ROOT\\WASMWIN32\\0000";
    k32_misc(h)->count = 1;
    return ok(std::to_string(h));
  }
  if (eq(api, "SetupDiEnumDeviceInfo")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || m->kind != kMiscDev) return fail(err_msg("SetupDiEnumDeviceInfo"));
    unsigned i = static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10));
    if (i >= static_cast<unsigned>(m->count)) return fail(err_msg("SetupDiEnumDeviceInfo"));
    return ok(m->name);
  }
  if (eq(api, "SetupDiGetDeviceRegistryPropertyW") ||
      eq(api, "SetupDiGetDeviceRegistryPropertyA")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || m->kind != kMiscDev) return fail(err_msg(api));
    return ok("WASIGOCVM Device");
  }
  if (eq(api, "SetupDiDestroyDeviceInfoList")) {
    if (!k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("SetupDiDestroyDeviceInfoList"));
    return ok("ok");
  }
  if (eq(api, "CM_Locate_DevNodeW") || eq(api, "CM_Locate_DevNodeA") ||
      eq(api, "CM_Locate_DevNode_ExW"))
    return ok("1");
  if (eq(api, "CM_Get_Device_IDW") || eq(api, "CM_Get_Device_IDA"))
    return ok("ROOT\\WASMWIN32\\0000");

  if (eq(api, "NetGetJoinInformation")) return ok("2" "\x1f" "WORKGROUP");
  if (eq(api, "NetWkstaGetInfo")) {
    char name[256] = {};
    if (gethostname(name, sizeof(name) - 1) != 0) std::strcpy(name, "wasm");
    return ok(name);
  }
  if (eq(api, "NetApiBufferFree")) return ok("ok");

  if (eq(api, "PdhOpenQueryW") || eq(api, "PdhOpenQueryA")) {
    int h = k32_alloc_misc(kMiscPdh);
    if (h < 0) return fail(err_msg("PdhOpenQueryW"));
    k32_misc(h)->name = a && a[0] ? a : "";
    return ok(std::to_string(h));
  }
  if (eq(api, "PdhAddCounterW") || eq(api, "PdhAddCounterA") ||
      eq(api, "PdhAddEnglishCounterW")) {
    std::string hs, path;
    split1f(a, &hs, &path);
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || m->kind != kMiscPdh) return fail(err_msg(api));
    m->count++;
    if (!path.empty()) m->name = path;
    return ok(hs);
  }
  if (eq(api, "PdhCollectQueryData")) {
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!m || m->kind != kMiscPdh) return fail(err_msg("PdhCollectQueryData"));
    return ok("ok");
  }
  if (eq(api, "PdhGetFormattedCounterValue")) return ok("0");
  if (eq(api, "PdhCloseQuery")) {
    if (!k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("PdhCloseQuery"));
    return ok("ok");
  }

  if (eq(api, "OpenSCManagerW") || eq(api, "OpenSCManagerA")) {
    int h = k32_alloc_misc(kMiscScm);
    if (h < 0) return fail(err_msg(api));
    std::string machine, rest;
    split1f(a, &machine, &rest);
    k32_misc(h)->name = machine.empty() ? "." : machine;
    k32_misc(h)->count = 1;
    return ok(std::to_string(h));
  }
  if (eq(api, "CloseServiceHandle")) {
    if (!k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("CloseServiceHandle"));
    return ok("ok");
  }
  if (eq(api, "OpenServiceW") || eq(api, "OpenServiceA") || eq(api, "CreateServiceW") ||
      eq(api, "CreateServiceA")) {
    std::string scm, rest, name;
    split1f(a, &scm, &rest);
    split1f(rest.c_str(), &name, &rest);
    if (name.empty()) name = rest.empty() ? scm : rest;
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(scm.c_str(), nullptr, 10)));
    if (!m || m->kind != kMiscScm) return fail(err_msg(api));
    if (name.empty()) return fail(err_msg(api));
    K32Misc* t = k32_miscs();
    for (int i = 0; i < kK32Max; ++i) {
      if (t[i].used && t[i].kind == kMiscSvc && t[i].name == name) {
        t[i].mod = static_cast<int>(std::strtol(scm.c_str(), nullptr, 10));
        return ok(std::to_string(kK32MiscBase + i));
      }
    }
    int h = k32_alloc_misc(kMiscSvc);
    if (h < 0) return fail(err_msg(api));
    k32_misc(h)->name = name;
    k32_misc(h)->mod = static_cast<int>(std::strtol(scm.c_str(), nullptr, 10));
    k32_misc(h)->count = 1;
    return ok(std::to_string(h));
  }
  if (eq(api, "DeleteService")) {
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!m || m->kind != kMiscSvc) return fail(err_msg("DeleteService"));
    m->count = 1;
    return ok("ok");
  }
  if (eq(api, "StartServiceW") || eq(api, "StartServiceA")) {
    std::string hs, rest;
    split1f(a, &hs, &rest);
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || m->kind != kMiscSvc) return fail(err_msg(api));
    m->count = 4;
    return ok("ok");
  }
  if (eq(api, "ControlService") || eq(api, "ControlServiceExW")) {
    std::string hs, code;
    split1f(a, &hs, &code);
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || m->kind != kMiscSvc) return fail(err_msg(api));
    unsigned c = static_cast<unsigned>(std::strtoul(code.c_str(), nullptr, 10));
    if (c == 1) m->count = 1;
    else if (c == 2) m->count = 7;
    else if (c == 3) m->count = 4;
    return ok(std::to_string(m->count));
  }
  if (eq(api, "QueryServiceStatus") || eq(api, "QueryServiceStatusEx")) {
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!m || m->kind != kMiscSvc) return fail(err_msg(api));
    return ok(std::to_string(m->count ? m->count : 1));
  }
  if (eq(api, "SetServiceStatus")) {
    std::string hs, st;
    split1f(a, &hs, &st);
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!m || (m->kind != kMiscSvc && m->kind != kMiscScm)) return fail(err_msg("SetServiceStatus"));
    if (m->kind == kMiscSvc && !st.empty())
      m->count = static_cast<int>(std::strtol(st.c_str(), nullptr, 10));
    return ok("ok");
  }
  if (eq(api, "RegisterServiceCtrlHandlerW") || eq(api, "RegisterServiceCtrlHandlerA") ||
      eq(api, "RegisterServiceCtrlHandlerExW")) {
    std::string name, rest;
    split1f(a, &name, &rest);
    if (name.empty()) name = a && a[0] ? a : "GuestService";
    K32Misc* t = k32_miscs();
    for (int i = 0; i < kK32Max; ++i)
      if (t[i].used && t[i].kind == kMiscSvc && t[i].name == name)
        return ok(std::to_string(kK32MiscBase + i));
    int h = k32_alloc_misc(kMiscSvc);
    if (h < 0) return fail(err_msg(api));
    k32_misc(h)->name = name;
    k32_misc(h)->count = 4;
    return ok(std::to_string(h));
  }
  if (eq(api, "StartServiceCtrlDispatcherW") || eq(api, "StartServiceCtrlDispatcherA"))
    return ok("ok");
  if (eq(api, "ChangeServiceConfigW") || eq(api, "ChangeServiceConfigA") ||
      eq(api, "ChangeServiceConfig2W") || eq(api, "NotifyServiceStatusChangeW") ||
      eq(api, "NotifyBootConfigStatus"))
    return ok("ok");
  if (eq(api, "QueryServiceConfigW") || eq(api, "QueryServiceConfigA") ||
      eq(api, "QueryServiceConfig2W")) {
    K32Misc* m = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!m || m->kind != kMiscSvc) return fail(err_msg(api));
    return ok(m->name);
  }
  if (eq(api, "EnumServicesStatusW") || eq(api, "EnumServicesStatusA") ||
      eq(api, "EnumServicesStatusExW") || eq(api, "EnumDependentServicesW")) {
    K32Misc* scm = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!scm || scm->kind != kMiscScm) return fail(err_msg(api));
    std::string out;
    K32Misc* t = k32_miscs();
    for (int i = 0; i < kK32Max; ++i) {
      if (!t[i].used || t[i].kind != kMiscSvc) continue;
      if (!out.empty()) out += "\x1f";
      out += t[i].name;
    }
    return ok(out);
  }
  if (eq(api, "GetServiceDisplayNameW") || eq(api, "GetServiceKeyNameW")) {
    std::string scm, name;
    split1f(a, &scm, &name);
    if (name.empty()) name = scm;
    return ok(name.empty() ? "GuestService" : name);
  }
  if (eq(api, "LockServiceDatabase")) {
    K32Misc* scm = k32_misc(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!scm || scm->kind != kMiscScm) return fail(err_msg("LockServiceDatabase"));
    int h = k32_alloc_misc(kMiscScm);
    if (h < 0) return fail(err_msg("LockServiceDatabase"));
    k32_misc(h)->name = "lock";
    k32_misc(h)->mod = static_cast<int>(std::strtol(a, nullptr, 10));
    return ok(std::to_string(h));
  }
  if (eq(api, "UnlockServiceDatabase")) {
    if (!k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("UnlockServiceDatabase"));
    return ok("ok");
  }
  if (eq(api, "QueryServiceLockStatusW")) return ok("0");

  if (eq(api, "WHvGetCapability")) return ok("1");
  if (eq(api, "WHvCreatePartition")) {
    int h = k32_alloc_whp(kWhpPart);
    if (h < 0) return fail(err_msg("WHvCreatePartition"));
    return ok(std::to_string(h));
  }
  if (eq(api, "WHvSetupPartition") || eq(api, "WHvResetPartition")) {
    K32Whp* p = k32_whp(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!p || p->kind != kWhpPart) return fail(err_msg(api));
    p->setup = 1;
    k32_whp_map_session(p);
    if (eq(api, "WHvResetPartition")) {
      K32Whp* t = k32_whps();
      for (int i = 0; i < kK32Max; ++i)
        if (t[i].used && t[i].kind == kWhpVp && t[i].parent == static_cast<int>(std::strtol(a, nullptr, 10)))
          t[i].rip = 0;
    }
    return ok("ok");
  }
  if (eq(api, "WHvDeletePartition")) {
    if (!k32_close_whp(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("WHvDeletePartition"));
    return ok("ok");
  }
  if (eq(api, "WHvGetPartitionProperty") || eq(api, "WHvGetPartitionCounters")) {
    std::string hs, code;
    split1f(a, &hs, &code);
    K32Whp* p = k32_whp(static_cast<int>(std::strtol(hs.empty() ? a : hs.c_str(), nullptr, 10)));
    if (!p || p->kind != kWhpPart) return fail(err_msg(api));
    unsigned c = code.empty() ? 0x1fffu : k32_whp_prop(code);
    auto it = p->props.find(c);
    return ok(std::to_string(it == p->props.end() ? 0ull : it->second));
  }
  if (eq(api, "WHvSetPartitionProperty")) {
    std::string hs, rest, code, val;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &code, &val);
    K32Whp* p = k32_whp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p || p->kind != kWhpPart) return fail(err_msg(api));
    if (code.empty()) return fail(err_msg("WHvSetPartitionProperty"));
    p->props[k32_whp_prop(code)] = k32_whp_u64(val);
    return ok("ok");
  }
  if (eq(api, "WHvSuspendPartitionTime") || eq(api, "WHvResumePartitionTime")) {
    if (!k32_whp(static_cast<int>(std::strtol(a, nullptr, 10)))) return fail(err_msg(api));
    return ok("ok");
  }
  if (eq(api, "WHvMapGpaRange") || eq(api, "WHvMapGpaRange2")) {
    std::string hs, rest, gpa, sz, flags, host;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &gpa, &rest);
    split1f(rest.c_str(), &sz, &rest);
    split1f(rest.c_str(), &flags, &host);
    K32Whp* p = k32_whp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p || p->kind != kWhpPart) return fail(err_msg(api));
    const void* src = nullptr;
    size_t n = static_cast<size_t>(k32_whp_u64(sz));
    if (!host.empty()) {
      uintptr_t hp = static_cast<uintptr_t>(std::strtoull(host.c_str(), nullptr, 0));
      src = reinterpret_cast<const void*>(hp);
      if (!n) {
        auto* vm = k32_vmem_at(hp);
        n = vm ? vm->n : 4096;
      }
    }
    (void)flags;
    if (!n) n = 4096;
    k32_whp_map_bytes(p, k32_whp_u64(gpa), src, n);
    return ok("ok");
  }
  if (eq(api, "WHvUnmapGpaRange") || eq(api, "WHvAdviseGpaRange")) {
    std::string hs, gpa;
    split1f(a, &hs, &gpa);
    K32Whp* p = k32_whp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p || p->kind != kWhpPart) return fail(err_msg(api));
    unsigned long long g = std::strtoull(gpa.c_str(), nullptr, 10);
    for (auto it = p->gpa.begin(); it != p->gpa.end(); ++it) {
      if (it->gpa == g) {
        p->gpa.erase(it);
        break;
      }
    }
    return ok("ok");
  }
  if (eq(api, "WHvTranslateGva")) {
    std::string hs, rest, idx, gva;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &idx, &gva);
    if (!k32_whp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10))))
      return fail(err_msg("WHvTranslateGva"));
    return ok(gva.empty() ? rest : gva);
  }
  if (eq(api, "WHvCreateVirtualProcessor") || eq(api, "WHvCreateVirtualProcessor2")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    int part = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (!k32_whp(part) || k32_whp(part)->kind != kWhpPart) return fail(err_msg(api));
    unsigned n = static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10));
    if (k32_whp_vp(part, n)) return ok(hs + "\x1f" + (idx.empty() ? "0" : idx));
    int v = k32_alloc_whp(kWhpVp);
    if (v < 0) return fail(err_msg(api));
    k32_whp(v)->parent = part;
    k32_whp(v)->vpindex = n;
    return ok(std::to_string(v));
  }
  if (eq(api, "WHvDeleteVirtualProcessor")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    int part = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Whp* vp = k32_whp_vp(part, static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10)));
    if (!vp) return fail(err_msg(api));
    *vp = K32Whp{};
    return ok("ok");
  }
  if (eq(api, "WHvRunVirtualProcessor")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    int part = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Whp* p = k32_whp(part);
    if (!p || p->kind != kWhpPart) return fail(err_msg("WHvRunVirtualProcessor"));
    unsigned n = static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10));
    K32Whp* vp = k32_whp_vp(part, n);
    if (!vp) {
      int v = k32_alloc_whp(kWhpVp);
      if (v < 0) return fail(err_msg("WHvRunVirtualProcessor"));
      vp = k32_whp(v);
      vp->parent = part;
      vp->vpindex = n;
    }
    vp->running = 0;
    return ok(k32_whp_decode_run(p, vp));
  }
  if (eq(api, "WHvCancelRunVirtualProcessor")) {
    std::string hs, idx;
    split1f(a, &hs, &idx);
    K32Whp* vp = k32_whp_vp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)),
                            static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10)));
    if (vp) vp->running = 0;
    return ok("ok");
  }
  if (eq(api, "WHvGetVirtualProcessorRegisters") || eq(api, "WHvGetVirtualProcessorState") ||
      eq(api, "WHvGetVirtualProcessorXsaveState") ||
      eq(api, "WHvGetVirtualProcessorInterruptControllerState") ||
      eq(api, "WHvGetVirtualProcessorInterruptControllerState2") ||
      eq(api, "WHvGetVirtualProcessorCounters") || eq(api, "WHvGetVirtualProcessorCpuidOutput")) {
    std::string hs, rest, idx, name;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &idx, &name);
    K32Whp* vp = k32_whp_vp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)),
                            static_cast<unsigned>(std::strtoul(idx.empty() ? rest.c_str() : idx.c_str(), nullptr, 10)));
    if (!vp) return fail(err_msg(api));
    unsigned r = name.empty() ? 0x10u : k32_whp_reg(name);
    if (K32Seg* s = k32_whp_seg(vp, r)) return ok(k32_whp_fmt_seg(*s));
    if (K32Tab* t = k32_whp_tab(vp, r)) return ok(k32_whp_fmt_tab(*t));
    if (r == 0x10) return ok(std::to_string(vp->rip));
    auto it = vp->regs.find(r);
    return ok(std::to_string(it == vp->regs.end() ? 0ull : it->second));
  }
  if (eq(api, "WHvSetVirtualProcessorRegisters") || eq(api, "WHvSetVirtualProcessorState") ||
      eq(api, "WHvSetVirtualProcessorXsaveState") ||
      eq(api, "WHvSetVirtualProcessorInterruptControllerState") ||
      eq(api, "WHvSetVirtualProcessorInterruptControllerState2")) {
    std::string hs, rest, idx, tail, name, val, extra, extra2, extra3;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &idx, &tail);
    split1f(tail.c_str(), &name, &val);
    split1f(val.c_str(), &val, &extra);
    split1f(extra.c_str(), &extra, &extra2);
    split1f(extra2.c_str(), &extra2, &extra3);
    K32Whp* vp = k32_whp_vp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)),
                            static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10)));
    if (!vp) return fail(err_msg(api));
    if (val.empty() && extra.empty()) {
      val = name;
      name = "Rip";
    }
    unsigned r = k32_whp_reg(name);
    if (K32Seg* s = k32_whp_seg(vp, r)) {
      k32_whp_set_seg(s, r, val, extra, extra2, extra3);
      vp->regs[r] = s->base;
      return ok("ok");
    }
    if (K32Tab* t = k32_whp_tab(vp, r)) {
      if (val.find(':') != std::string::npos) {
        auto col = val.find(':');
        t->base = k32_whp_u64(val.substr(0, col));
        t->limit = static_cast<unsigned short>(k32_whp_u64(val.substr(col + 1)));
      } else {
        t->base = k32_whp_u64(val);
        if (!extra.empty()) t->limit = static_cast<unsigned short>(k32_whp_u64(extra));
      }
      vp->regs[r] = t->base;
      return ok("ok");
    }
    unsigned long long v = k32_whp_u64(val);
    vp->regs[r] = v;
    if (r == 0x10) vp->rip = v;
    return ok("ok");
  }
  if (eq(api, "WHvReadGpaRange") || eq(api, "WHvWriteGpaRange")) {
    std::string hs, rest, gpa, payload;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &gpa, &payload);
    K32Whp* p = k32_whp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!p || p->kind != kWhpPart) return fail(err_msg(api));
    unsigned long long g = std::strtoull(gpa.c_str(), nullptr, 10);
    K32Gpa* rng = nullptr;
    for (auto& x : p->gpa)
      if (g >= x.gpa && g < x.gpa + x.size) {
        rng = &x;
        break;
      }
    if (!rng) return fail(err_msg(api));
    size_t off = static_cast<size_t>(g - rng->gpa);
    if (eq(api, "WHvReadGpaRange")) {
      size_t n = payload.empty() ? rng->bytes.size() - off : static_cast<size_t>(std::strtoul(payload.c_str(), nullptr, 10));
      if (off + n > rng->bytes.size()) n = rng->bytes.size() - off;
      return ok(rng->bytes.substr(off, n));
    }
    if (off < rng->bytes.size()) {
      size_t n = payload.size();
      if (off + n > rng->bytes.size()) n = rng->bytes.size() - off;
      rng->bytes.replace(off, n, payload.substr(0, n));
    }
    return ok("ok");
  }
  if (eq(api, "WHvEmulatorCreateEmulator")) {
    int h = k32_alloc_whp(kWhpEmu);
    if (h < 0) return fail(err_msg("WHvEmulatorCreateEmulator"));
    if (a && a[0]) {
      std::string hs, rest;
      split1f(a, &hs, &rest);
      K32Whp* p = k32_whp(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
      if (p && p->kind == kWhpPart) k32_whp(h)->parent = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    }
    return ok(std::to_string(h));
  }
  if (eq(api, "WHvEmulatorDestroyEmulator")) {
    if (!k32_close_whp(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("WHvEmulatorDestroyEmulator"));
    return ok("ok");
  }
  if (eq(api, "WHvEmulatorTryIoEmulation") || eq(api, "WHvEmulatorTryMmioEmulation")) {
    std::string ehs, rest, hs, idx;
    split1f(a, &ehs, &rest);
    split1f(rest.c_str(), &hs, &idx);
    K32Whp* emu = k32_whp(static_cast<int>(std::strtol(ehs.c_str(), nullptr, 10)));
    if (!emu || emu->kind != kWhpEmu) {
      hs = ehs;
      idx = rest;
    }
    int part = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    K32Whp* p = k32_whp(part);
    K32Whp* vp = k32_whp_vp(part, static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10)));
    if (!p || p->kind != kWhpPart || !vp) return fail(err_msg(api));
    std::string r = eq(api, "WHvEmulatorTryIoEmulation") ? k32_whp_try_io(p, vp) : k32_whp_try_mmio(p, vp);
    if (r.empty()) return fail(err_msg(api));
    return ok(r);
  }
  if (k32_whp_cataloged(api)) return ok("ok");

  if (eq(api, "EvtQuery") || eq(api, "EvtOpenLog")) {
    int h = k32_alloc_misc(kMiscEvt);
    if (h < 0) return fail(err_msg(api));
    k32_misc(h)->name = a && a[0] ? a : "Application";
    return ok(std::to_string(h));
  }
  if (eq(api, "EvtNext")) return ok("0");
  if (eq(api, "EvtClose")) {
    if (!k32_close_misc(static_cast<int>(std::strtol(a, nullptr, 10))))
      return fail(err_msg("EvtClose"));
    return ok("ok");
  }

  if (k32_ntdll_cataloged(api)) {
    if (eq(api, "DbgBreakPoint") || eq(api, "DbgUserBreakPoint") || nt("NtShutdownSystem") ||
        nt("NtRaiseHardError") || nt("NtRaiseException") || nt("NtSetSystemTime") ||
        eq(api, "RtlAssert") || eq(api, "RtlRaiseStatus") || eq(api, "RtlRaiseException"))
      return fail(err_msg("ntdll: not executed by this hop"));
    return ok("ok");
  }

  return false;
}
