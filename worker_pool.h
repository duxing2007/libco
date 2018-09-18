#ifndef _WORKER_POOL_H_
#define _WORKER_POOL_H_

#include <functional>
#include <string>

static const int kDefaultSoftLimitStackSize = 128 * 1024;
static const int kDefaultHardLimitStackSize = 8 * 1024 * 1024;

/*
 * suggest value:
 * develoment environment:
 *  hard_limit_stack_size =  soft_limit_stack_size,
 *  to trigger coredump when use large stack.
 * online environment:
 *  hard_limit_stack_size > soft_limit_stack_size
 *  to avoid coredump when use large stack.
 */
int InitWorkerPool(int soft_limit_stack_size = kDefaultSoftLimitStackSize,
                   int hard_limit_stack_size = kDefaultHardLimitStackSize);
/*
 * execute func1 in a coroutine.
 *
 * caution:
 * func1's arg should  use reference very carefully.
 * you need to ensure func1's arg still valid when func1 execute.
 */
void ProcessByWorker(std::function<void(void)> func1);

#endif
