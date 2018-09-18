#include <sys/mman.h>
#include <unistd.h>
#include <deque>
#include <map>
#include <stack>
#include "co_routine.h"
#include "co_routine_inner.h"
#include "worker_pool.h"

static const int kPageSize = 4 * 1024;

class CWorker {
 public:
  CWorker(stCoRoutine_t* co) : co_(co), func_(NULL){};
  stCoRoutine_t* co_;
  std::function<void(void)> func_;
};

class CWorkerPool {
 public:
  CWorkerPool() {
    soft_limit_stack_size_ = kDefaultSoftLimitStackSize;
    hard_limit_stack_size_ = kDefaultHardLimitStackSize;
    gc_stack_size_ = kDefaultHardLimitStackSize - kDefaultSoftLimitStackSize;
  }
  std::stack<CWorker*> workers_;
  int soft_limit_stack_size_;
  int hard_limit_stack_size_;
  int gc_stack_size_;
};

static CWorkerPool g_worker_pool;

int InitWorkerPool(int soft_limit_stack_size, int hard_limit_stack_size) {
  static bool has_init = false;
  if (has_init) {
    return 0;
  }
  has_init = true;

  soft_limit_stack_size =
      (soft_limit_stack_size + kPageSize - 1) / kPageSize * kPageSize;
  hard_limit_stack_size =
      (hard_limit_stack_size + kPageSize - 1) / kPageSize * kPageSize;
  if (soft_limit_stack_size < kDefaultSoftLimitStackSize) {
    soft_limit_stack_size = kDefaultSoftLimitStackSize;
  }
  if (hard_limit_stack_size > kDefaultHardLimitStackSize) {
    hard_limit_stack_size = kDefaultHardLimitStackSize;
  }

  if (hard_limit_stack_size < soft_limit_stack_size) {
    hard_limit_stack_size = soft_limit_stack_size;
  }

  // init g_worker_pool
  g_worker_pool.soft_limit_stack_size_ = soft_limit_stack_size;
  g_worker_pool.hard_limit_stack_size_ = hard_limit_stack_size;
  g_worker_pool.gc_stack_size_ = hard_limit_stack_size - soft_limit_stack_size;

  return 0;
}

static void FreeStackRss(void* stack_buffer) {
  int ret = 0;
  unsigned char* p1 = (unsigned char*)stack_buffer;
  p1 = p1 + g_worker_pool.gc_stack_size_ - kPageSize;
  unsigned char vec[1] = {0};
  ret = mincore((void*)p1, kPageSize, vec);
  if (ret != 0) {
    return;
  }

  // no need to free
  if ((vec[0] & 0x1) == 0) {
    return;
  }

  madvise(stack_buffer, g_worker_pool.gc_stack_size_, MADV_DONTNEED);
  return;
}

static void* worker_main(void* arg) {
  CWorker* worker = (CWorker*)arg;
  while (true) {
    if (worker->func_) {
      worker->func_();
    }
    worker->func_ = NULL;

    if (g_worker_pool.gc_stack_size_ > 0) {
      FreeStackRss(worker->co_->stack_mem->stack_buffer);
    }

    g_worker_pool.workers_.push(worker);
    co_yield_ct();
  }
  return NULL;
}

void ProcessByWorker(std::function<void(void)> func1) {
  CWorker* worker = NULL;
  if (g_worker_pool.workers_.empty()) {
    stCoRoutineAttr_t attr;
    attr.stack_size = g_worker_pool.hard_limit_stack_size_;
    stCoRoutine_t* worker_routine = NULL;
    worker = new CWorker(worker_routine);
    co_create(&worker_routine, &attr, worker_main, worker);
    worker->co_ = worker_routine;
  } else {
    worker = g_worker_pool.workers_.top();
    g_worker_pool.workers_.pop();
  }
  if (worker == NULL) {
    return;
  }
  worker->func_ = func1;
  co_resume(worker->co_);
}
