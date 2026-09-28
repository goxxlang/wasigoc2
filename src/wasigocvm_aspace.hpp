// Shared second address space: EPT / TPT / CHPT loaded as-is from
// ~/WASMSafeSpace and ~/WASMv8bindings. Exec, Win32, Nix, Droid, and
// anything else that needs a handle across the cage uses these tables
// — not a second linear memory and not a companion host.
//
// Include this from runtime.hpp at file-scope first (cppgc headers must
// not be parsed inside namespace wasigo). Then define
// WASIGO_ASPACE_WANT_TYPES and include again from wasigocvm_libc.hpp /
// wasigocvm_exec.hpp so Aspace lives in wasigo::gocvm.
#ifndef WASIGO_ASPACE_HEADERS_INCLUDED
#define WASIGO_ASPACE_HEADERS_INCLUDED

#if defined(__has_include)
#  if defined(WASIGO_HAS_WASMSAFESPACE) && WASIGO_HAS_WASMSAFESPACE
#    if __has_include("cppgc/heap.h") && \
        __has_include("src/sandbox/cppheap-pointer-table.h") && \
        __has_include("src/sandbox/external-pointer-table.h") && \
        __has_include("src/sandbox/trusted-pointer-table.h") && \
        __has_include("src/heap/internal/platform.h")
#      include "cppgc/allocation.h"
#      include "cppgc/garbage-collected.h"
#      include "cppgc/heap.h"
#      include "cppgc/persistent.h"
#      include "cppgc/platform.h"
#      include "cppgc/visitor.h"
#      include "src/heap/internal/platform.h"
#      include "src/sandbox/cppheap-pointer-table.h"
#      include "src/sandbox/external-pointer-table.h"
#      include "src/sandbox/trusted-pointer-table.h"
#      include "v8-sandbox.h"
#      define WASIGO_HAS_WASMV8 1
#undef CHECK
#undef CHECK_OP
#undef CHECK_EQ
#undef CHECK_NE
#undef CHECK_LT
#undef CHECK_LE
#undef CHECK_GT
#undef CHECK_GE
#undef CHECK_IMPLIES
#undef DCHECK
#undef DCHECK_EQ
#undef DCHECK_NE
#undef DCHECK_LT
#undef DCHECK_LE
#undef DCHECK_GT
#undef DCHECK_GE
#undef DCHECK_IMPLIES
#    endif
#  endif
#endif
#ifndef WASIGO_HAS_WASMV8
#define WASIGO_HAS_WASMV8 0
#endif

#endif  // WASIGO_ASPACE_HEADERS_INCLUDED

#if defined(WASIGO_ASPACE_WANT_TYPES) && !defined(WASIGO_ASPACE_TYPES_DEFINED)
#define WASIGO_ASPACE_TYPES_DEFINED

namespace gocvm {

// Guest process table. Not WASI emulated getpid (always 1), not the
// host Task Manager. First guest is pid 1000 / tid 1004 (Windows-like
// stride). Exec children take the next ids.
inline int& proc_next_id() {
  static int n = 1000;
  return n;
}

inline int proc_alloc_id() {
  int id = proc_next_id();
  proc_next_id() += 4;
  return id;
}

struct ProcSelf {
  int pid = 0;
  int ppid = 0;
  int tid = 0;
};

inline ProcSelf& proc_self_slot() {
  static ProcSelf p;
  if (p.pid == 0) {
    p.ppid = 0;
    p.pid = proc_alloc_id();
    p.tid = proc_alloc_id();
  }
  return p;
}

inline int proc_self() { return proc_self_slot().pid; }
inline int proc_ppid() { return proc_self_slot().ppid; }
inline int proc_tid() { return proc_self_slot().tid; }

#if defined(WASIGO_HAS_WASMSAFESPACE)
// A module that links a kernel (WASMGocOS) has one process cage, the one
// its JIT and vCPU allocate from. Everything here lives there too, so
// cppgc (which pages from Sandbox::current()) and these tables agree on
// one cage. Any other module keeps its own.
extern "C" __attribute__((weak)) v8::internal::Sandbox* wasmturbo_process_cage();

inline v8::internal::Sandbox& aspace_cage() {
  static v8::internal::Sandbox* current = [] {
    v8::internal::Sandbox* c = wasmturbo_process_cage ? wasmturbo_process_cage() : nullptr;
    if (c == nullptr) {
      static v8::internal::Sandbox own;
      own.Initialize(8 * v8::internal::MB);
      c = &own;
    }
    v8::internal::Sandbox::set_current(c);
    return c;
  }();
  return *current;
}
inline v8::internal::Sandbox& exec_cage() { return aspace_cage(); }
#endif

#if WASIGO_HAS_WASMV8
constexpr v8::CppHeapPointerTag kExecChildTag =
    v8::CppHeapPointerTag::kFirstObjectWrappableTag;
constexpr v8::CppHeapPointerTag kWin32KernelTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 1);
constexpr v8::CppHeapPointerTag kWin32TokenTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 2);
constexpr v8::CppHeapPointerTag kWin32SidTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 3);
constexpr v8::CppHeapPointerTag kWin32PebTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 4);
constexpr v8::CppHeapPointerTag kWin32TebTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 5);
constexpr v8::CppHeapPointerTag kWin32WndTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 6);
constexpr v8::CppHeapPointerTag kWin32GdiTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 7);
constexpr v8::CppHeapPointerTag kWin32ComTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 8);
constexpr v8::CppHeapPointerTag kNixSessionTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 9);
constexpr v8::CppHeapPointerTag kDroidSessionTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 10);
constexpr v8::CppHeapPointerTag kGocOSSessionTag =
    static_cast<v8::CppHeapPointerTag>(
        static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstObjectWrappableTag) + 11);

constexpr v8::internal::ExternalPointerTag kExecStdoutTag =
    v8::internal::ExternalPointerTag::kFirstManagedResourceTag;
constexpr v8::internal::ExternalPointerTag kWin32CatalogTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 1);
constexpr v8::internal::ExternalPointerTag kWin32VmemTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 2);
constexpr v8::internal::ExternalPointerTag kWin32SockTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 3);
constexpr v8::internal::ExternalPointerTag kWin32CngTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 4);
constexpr v8::internal::ExternalPointerTag kWin32ModTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 5);
constexpr v8::internal::ExternalPointerTag kWin32CertTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 6);
constexpr v8::internal::ExternalPointerTag kWin32HttpTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 7);
constexpr v8::internal::ExternalPointerTag kWin32WhpTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 8);
constexpr v8::internal::ExternalPointerTag kWin32HeapTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 9);
constexpr v8::internal::ExternalPointerTag kNixCatalogTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 10);
constexpr v8::internal::ExternalPointerTag kDroidCatalogTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 11);
constexpr v8::internal::ExternalPointerTag kDroidPhoneTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 12);
constexpr v8::internal::ExternalPointerTag kGocOSCatalogTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 13);
constexpr v8::internal::ExternalPointerTag kGocOSDesktopTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 14);
constexpr v8::internal::ExternalPointerTag kGocOSVmemTag =
    static_cast<v8::internal::ExternalPointerTag>(
        static_cast<uint16_t>(
            v8::internal::ExternalPointerTag::kFirstManagedResourceTag) + 15);

struct Aspace {
  v8::internal::ExternalPointerTable& ept;
  v8::internal::TrustedPointerTable& tpt;
  cppgc::internal::CppHeapPointerTable& chpt;
};

// EPT and TPT are the Sandbox's own tables. CHPT is the one table
// registered on that same cage. This is the browser layout for a single
// cage: field access goes through Sandbox::current(), not a side static.
inline Aspace& aspace() {
  auto& cage = aspace_cage();
  static cppgc::internal::CppHeapPointerTable chpt;
  if (cage.cpp_heap_pointer_table() != &chpt) {
    cage.RegisterCppHeapPointerTable(&chpt);
  }
  static Aspace tables{cage.external_pointer_table(), cage.trusted_pointer_table(),
                       chpt};
  return tables;
}
inline Aspace& exec_aspace() { return aspace(); }

// cppgc::Platform::PostJob's JobTask/JobHandle/JobDelegate: WASMv8bindings
// declares the full interface (it's a strip-ported subset of real V8's) but
// its own Heap never calls PostJob -- Heap::Create CHECKs MarkingType::
// kAtomic/SweepingType::kAtomic only, so concurrent marking jobs can't
// exist there. Real V8's own DefaultPlatform can't stand in either: it
// forwards to v8::platform::NewDefaultPlatform, whose libplatform worker
// pool was never ported here. So this table IS the job runner: same
// bounded-handle-map shape as ExecTable (wasigocvm_exec.hpp), backing a
// real Platform that Heap::Create() actually holds instead of nullptr.
struct JobEntry {
  std::unique_ptr<cppgc::JobTask> task;
  std::mutex mu;  // guards workers
  std::vector<std::thread> workers;
  std::atomic<bool> cancelled{false};
  std::atomic<int> running{0};
};

// Starts workers until the job has as many as it asks for. Post calls
// it once; a NotifyConcurrencyIncrease (the task has more work than its
// workers) calls it again, as real V8's worker pool re-queries then.
inline void job_grow(JobEntry* e);

class TableJobDelegate final : public cppgc::JobDelegate {
 public:
  TableJobDelegate(JobEntry* entry, uint8_t id) : entry_(entry), id_(id) {}
  bool ShouldYield() override { return entry_->cancelled.load(std::memory_order_relaxed); }
  void NotifyConcurrencyIncrease() override { job_grow(entry_); }
  uint8_t GetTaskId() override { return id_; }
  // Workers are their own threads; Join only waits and runs no task work.
  bool IsJoiningThread() const override { return false; }

 private:
  JobEntry* entry_;
  uint8_t id_;
};

inline void job_grow(JobEntry* e) {
  if (e->cancelled.load(std::memory_order_relaxed)) return;
  std::lock_guard<std::mutex> lk(e->mu);
  size_t have = static_cast<size_t>(e->running.load(std::memory_order_relaxed));
  size_t want = e->task->GetMaxConcurrency(have);
  if (want == 0 && e->workers.empty()) want = 1;
  while (have < want) {
    uint8_t id = static_cast<uint8_t>(e->workers.size());
    ++e->running;
    e->workers.emplace_back([e, id]() {
      TableJobDelegate delegate(e, id);
      e->task->Run(&delegate);
      --e->running;
    });
    ++have;
  }
}

class JobTable {
 public:
  uint64_t post(std::unique_ptr<cppgc::JobTask> task) {
    std::lock_guard<std::mutex> lk(mu_);
    uint64_t h = ++next_handle_;
    auto entry = std::make_unique<JobEntry>();
    JobEntry* raw = entry.get();
    raw->task = std::move(task);
    jobs_[h] = std::move(entry);
    job_grow(raw);
    return h;
  }

  JobEntry* get(uint64_t h) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = jobs_.find(h);
    return it == jobs_.end() ? nullptr : it->second.get();
  }

  void join(uint64_t h) {
    JobEntry* e = get(h);
    if (!e) return;
    // A worker may start another while this waits; wait for those too.
    for (;;) {
      std::thread t;
      {
        std::lock_guard<std::mutex> lk(e->mu);
        for (auto& w : e->workers) {
          if (w.joinable()) {
            t = std::move(w);
            break;
          }
        }
      }
      if (!t.joinable()) return;
      t.join();
    }
  }

  void cancel(uint64_t h) {
    JobEntry* e = get(h);
    if (e) e->cancelled.store(true, std::memory_order_relaxed);
  }

  void grow(uint64_t h) {
    if (JobEntry* e = get(h)) job_grow(e);
  }

  // Cancel, and let the workers end on their own.
  void detach(uint64_t h) {
    JobEntry* e = get(h);
    if (!e) return;
    e->cancelled.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(e->mu);
    for (auto& w : e->workers)
      if (w.joinable()) w.detach();
  }

  bool running(uint64_t h) {
    JobEntry* e = get(h);
    return e && e->running.load(std::memory_order_relaxed) > 0;
  }

 private:
  std::mutex mu_;
  uint64_t next_handle_ = 0;
  std::unordered_map<uint64_t, std::unique_ptr<JobEntry>> jobs_;
};

inline JobTable& job_table() {
  static JobTable table;
  return table;
}

class TableJobHandle final : public cppgc::JobHandle {
 public:
  explicit TableJobHandle(uint64_t h) : handle_(h) {}
  ~TableJobHandle() override {
    if (valid_) Join();
  }
  void NotifyConcurrencyIncrease() override {
    if (valid_) job_table().grow(handle_);
  }
  void Join() override {
    job_table().join(handle_);
    valid_ = false;
  }
  void Cancel() override {
    job_table().cancel(handle_);
    Join();
  }
  // Cancelled; the workers finish on their own, and this does not wait.
  void CancelAndDetach() override {
    job_table().detach(handle_);
    valid_ = false;
  }
  bool IsActive() override { return job_table().running(handle_); }
  bool IsValid() override { return valid_; }

 private:
  uint64_t handle_;
  bool valid_ = true;
};

class Platform final : public cppgc::Platform {
 public:
  cppgc::PageAllocator* GetPageAllocator() override {
    return &cppgc::internal::GetGlobalPageAllocator();
  }

  double MonotonicallyIncreasingTime() override {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  std::unique_ptr<cppgc::JobHandle> PostJob(
      cppgc::TaskPriority, std::unique_ptr<cppgc::JobTask> job_task) override {
    return std::make_unique<TableJobHandle>(job_table().post(std::move(job_task)));
  }
};

inline std::shared_ptr<cppgc::Platform>& aspace_platform() {
  static std::shared_ptr<cppgc::Platform> platform = std::make_shared<Platform>();
  return platform;
}

inline cppgc::Heap& aspace_heap() {
  static cppgc::Heap* heap = nullptr;
  if (!heap) {
#if defined(WASIGO_HAS_WASMSAFESPACE)
    (void)aspace_cage();
#endif
    // Process-lifetime heap: wasm atexit must not run cppgc ~Heap
    // (indirect-table trap on Finalize). Same as a V8 Isolate that
    // outlives the last handle.
    heap = cppgc::Heap::Create(aspace_platform()).release();
  }
  return *heap;
}
inline cppgc::Heap& exec_cppgc_heap() { return aspace_heap(); }

inline int aspace_boot() {
  (void)aspace_heap();
  (void)aspace();
  return 1;
}
[[maybe_unused]] static const int kAspaceBoot = aspace_boot();
#endif

}  // namespace gocvm

#endif  // WASIGO_ASPACE_WANT_TYPES
