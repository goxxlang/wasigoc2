// Golden test: Platform::PostJob / GetForegroundTaskRunner actually run
// work. The unused-jobs bug was dropping JobTask/Task on the floor.
#include <cassert>
#include <cstdio>
#include <memory>

#include "cppgc/default-platform.h"
#include "cppgc/heap.h"
#include "cppgc/platform.h"

namespace {

class CountingJob final : public cppgc::JobTask {
 public:
  explicit CountingJob(int* runs) : runs_(runs) {}

  void Run(cppgc::JobDelegate* delegate) override {
    if (delegate && delegate->ShouldYield()) return;
    ++*runs_;
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    return worker_count < 1 ? 1 : 0;
  }

 private:
  int* runs_;
};

class CountingTask final : public cppgc::Task {
 public:
  explicit CountingTask(int* runs) : runs_(runs) {}
  void Run() override { ++*runs_; }

 private:
  int* runs_;
};

}  // namespace

int main() {
  cppgc::DefaultPlatform platform;

  int blocking_runs = 0;
  auto blocking = platform.PostJob(
      cppgc::TaskPriority::kUserBlocking,
      std::make_unique<CountingJob>(&blocking_runs));
  assert(blocking);
  assert(blocking_runs == 1);  // ran on PostJob
  blocking->Join();
  assert(blocking_runs == 1);  // Join is idempotent

  int best_effort_runs = 0;
  auto best = platform.PostJob(
      cppgc::TaskPriority::kBestEffort,
      std::make_unique<CountingJob>(&best_effort_runs));
  assert(best);
  assert(best_effort_runs == 0);  // deferred until Join
  best->Join();
  assert(best_effort_runs == 1);

  auto blocking_runner =
      platform.GetForegroundTaskRunner(cppgc::TaskPriority::kUserBlocking);
  auto best_runner =
      platform.GetForegroundTaskRunner(cppgc::TaskPriority::kBestEffort);
  assert(blocking_runner);
  assert(best_runner);
  assert(blocking_runner != best_runner);
  assert(!blocking_runner->IdleTasksEnabled());
  assert(best_runner->IdleTasksEnabled());

  int task_runs = 0;
  blocking_runner->PostTask(std::make_unique<CountingTask>(&task_runs));
  assert(task_runs == 1);

  // Heap::Create(nullptr) installs DefaultPlatform; GC posts mark+sweep jobs.
  auto heap = cppgc::Heap::Create(nullptr);
  heap->ForceGarbageCollectionSlow("test", "jobs-backed atomic GC");
  heap.reset();

  std::printf("cppgc_jobs_test: OK\n");
  return 0;
}
