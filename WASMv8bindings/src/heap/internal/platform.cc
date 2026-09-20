#include "src/heap/internal/platform.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "cppgc/default-platform.h"
#include "src/heap/internal/default-page-allocator.h"
#include "src/heap/internal/gc-info-table.h"

namespace cppgc {
namespace {

bool g_process_initialized = false;

class ImmediateTaskRunner final : public TaskRunner {
 public:
  explicit ImmediateTaskRunner(TaskPriority priority) : priority_(priority) {}

  void PostTask(std::unique_ptr<Task> task) override {
    if (!task) return;
    task->Run();
  }

  bool IdleTasksEnabled() override {
    return priority_ == TaskPriority::kBestEffort;
  }

 private:
  TaskPriority priority_;
};

class DefaultJobDelegate final : public JobDelegate {
 public:
  bool ShouldYield() override { return cancelled_; }
  void Cancel() { cancelled_ = true; }

 private:
  bool cancelled_ = false;
};

class DefaultJobHandle final : public JobHandle {
 public:
  explicit DefaultJobHandle(std::unique_ptr<JobTask> job_task)
      : job_task_(std::move(job_task)) {}

  ~DefaultJobHandle() override {
    if (job_task_ && !completed_) Join();
  }

  void Join() override { RunWorkers(); }

  void Cancel() override {
    delegate_.Cancel();
    completed_ = true;
    running_ = false;
    job_task_.reset();
  }

  bool IsRunning() override { return running_; }

 private:
  void RunWorkers() {
    if (completed_ || !job_task_) return;
    running_ = true;
    size_t worker_count = 0;
    while (job_task_->GetMaxConcurrency(worker_count) > worker_count &&
           !delegate_.ShouldYield()) {
      job_task_->Run(&delegate_);
      ++worker_count;
    }
    running_ = false;
    completed_ = true;
    job_task_.reset();
  }

  std::unique_ptr<JobTask> job_task_;
  DefaultJobDelegate delegate_;
  bool running_ = false;
  bool completed_ = false;
};

}  // namespace

bool IsInitialized() { return g_process_initialized; }

void InitializeProcess(PageAllocator* page_allocator, size_t) {
  PageAllocator& allocator =
      page_allocator ? *page_allocator : internal::GetGlobalPageAllocator();
  internal::GlobalGCInfoTable::Initialize(allocator);
  g_process_initialized = true;
}

void ShutdownProcess() { g_process_initialized = false; }

std::shared_ptr<TaskRunner> Platform::GetForegroundTaskRunner(
    TaskPriority priority) {
  static std::array<std::shared_ptr<TaskRunner>, 3> runners = {
      std::make_shared<ImmediateTaskRunner>(TaskPriority::kBestEffort),
      std::make_shared<ImmediateTaskRunner>(TaskPriority::kUserVisible),
      std::make_shared<ImmediateTaskRunner>(TaskPriority::kUserBlocking),
  };
  switch (priority) {
    case TaskPriority::kBestEffort:
      return runners[0];
    case TaskPriority::kUserVisible:
      return runners[1];
    case TaskPriority::kUserBlocking:
      return runners[2];
  }
  return runners[2];
}

std::unique_ptr<JobHandle> Platform::PostJob(
    TaskPriority priority, std::unique_ptr<JobTask> job_task) {
  if (!job_task) return nullptr;
  auto handle = std::make_unique<DefaultJobHandle>(std::move(job_task));
  // Use priority: blocking/visible jobs run on post; best-effort waits
  // for Join() (or the handle destructor).
  if (priority != TaskPriority::kBestEffort) handle->Join();
  return handle;
}

TracingController* Platform::GetTracingController() {
  static TracingController controller;
  return &controller;
}

DefaultPlatform::DefaultPlatform() = default;
DefaultPlatform::~DefaultPlatform() = default;

PageAllocator* DefaultPlatform::GetPageAllocator() {
  return &internal::GetGlobalPageAllocator();
}

double DefaultPlatform::MonotonicallyIncreasingTime() {
  using clock = std::chrono::steady_clock;
  static const auto origin = clock::now();
  return std::chrono::duration<double>(clock::now() - origin).count();
}

namespace internal {

void FatalOutOfMemoryHandler::operator()(const std::string& reason,
                                         SourceLocation loc) const {
  if (custom_handler_) {
    (*custom_handler_)(reason, loc, heap_);
    std::fprintf(stderr,
                 "WASMv8bindings: custom OOM handler returned (%s)\n",
                 reason.c_str());
    std::abort();
  }
  const size_t pages = heap_ ? heap_->normal_page_count() : 0;
  const size_t large = heap_ ? heap_->large_object_count() : 0;
  std::fprintf(stderr,
               "WASMv8bindings: Oilpan fatal out of memory: %s (%s:%zu) "
               "heap=%p pages=%zu large=%zu\n",
               reason.c_str(), loc.FileName() ? loc.FileName() : "?",
               loc.Line(), static_cast<void*>(heap_), pages, large);
  std::abort();
}

FatalOutOfMemoryHandler& GetGlobalOOMHandler() {
  static FatalOutOfMemoryHandler handler;
  return handler;
}

PageAllocator& GetGlobalPageAllocator() {
  static DefaultPageAllocator allocator;
  return allocator;
}

void Fatal(const std::string& reason, SourceLocation loc) {
  std::fprintf(stderr, "WASMv8bindings: cppgc fatal: %s (%s:%zu)\n",
               reason.c_str(), loc.FileName() ? loc.FileName() : "?",
               loc.Line());
  std::abort();
}

}  // namespace internal
}  // namespace cppgc
