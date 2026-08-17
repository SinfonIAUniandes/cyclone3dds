#include <3ds.h>

#include <assert.h>
#include <string.h>

#include "threads_priv.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"

typedef struct {
  ddsrt_thread_routine_t routine;
  void *argument;
} thread_start_data_t;

static ddsrt_thread_local thread_cleanup_t *cleanup_stack;

static void run_cleanup_handlers(void)
{
  while (cleanup_stack != NULL) {
    thread_cleanup_t *cleanup = cleanup_stack;
    cleanup_stack = cleanup->prev;
    cleanup->routine(cleanup->arg);
    ddsrt_free(cleanup);
  }
}

static void thread_entry(void *argument)
{
  thread_start_data_t *start = argument;
  ddsrt_thread_routine_t routine = start->routine;
  void *routine_argument = start->argument;
  ddsrt_free(start);

  uint32_t result = routine(routine_argument);
  run_cleanup_handlers();
  threadExit((int)result);
}

dds_return_t ddsrt_thread_create(ddsrt_thread_t *thread, const char *name,
                                 const ddsrt_threadattr_t *attributes,
                                 ddsrt_thread_routine_t routine, void *argument)
{
  (void)name;
  if (!thread || !attributes || !routine || attributes->schedAffinityN != 0) return DDS_RETCODE_BAD_PARAMETER;

  thread_start_data_t *start = ddsrt_malloc(sizeof(*start));
  if (!start) return DDS_RETCODE_OUT_OF_RESOURCES;
  start->routine = routine;
  start->argument = argument;

  int priority = attributes->schedPriority == 0 ? 0x30 : attributes->schedPriority;
  if (priority < 0x18 || priority > 0x3f) {
    ddsrt_free(start);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  size_t stack_size = attributes->stackSize == 0 ? 16 * 1024 : attributes->stackSize;
  thread->handle = threadCreate(thread_entry, start, stack_size, priority, -2, false);
  if (!thread->handle) {
    ddsrt_free(start);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  return DDS_RETCODE_OK;
}

ddsrt_tid_t ddsrt_gettid(void)
{
  return (ddsrt_tid_t)(uintptr_t)threadGetCurrent();
}

ddsrt_tid_t ddsrt_gettid_for_thread(ddsrt_thread_t thread)
{
  return (ddsrt_tid_t)(uintptr_t)thread.handle;
}

ddsrt_thread_t ddsrt_thread_self(void)
{
  return (ddsrt_thread_t){ .handle = threadGetCurrent() };
}

bool ddsrt_thread_equal(ddsrt_thread_t left, ddsrt_thread_t right)
{
  return left.handle == right.handle;
}

dds_return_t ddsrt_thread_join(ddsrt_thread_t thread, uint32_t *result)
{
  if (!thread.handle || threadJoin(thread.handle, UINT64_MAX) != 0) return DDS_RETCODE_ERROR;
  if (result) *result = (uint32_t)threadGetExitCode(thread.handle);
  threadFree(thread.handle);
  return DDS_RETCODE_OK;
}

size_t ddsrt_thread_getname(char *name, size_t size)
{
  assert(name != NULL && size > 0);
  return ddsrt_strlcpy(name, threadGetCurrent() ? "cyclonedds" : "main", size);
}

dds_return_t ddsrt_thread_cleanup_push(void (*routine)(void *), void *argument)
{
  if (!routine) return DDS_RETCODE_BAD_PARAMETER;
  thread_cleanup_t *cleanup = ddsrt_malloc(sizeof(*cleanup));
  if (!cleanup) return DDS_RETCODE_OUT_OF_RESOURCES;
  cleanup->routine = routine;
  cleanup->arg = argument;
  cleanup->prev = cleanup_stack;
  cleanup_stack = cleanup;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_thread_cleanup_pop(int execute)
{
  if (!cleanup_stack) return DDS_RETCODE_PRECONDITION_NOT_MET;
  thread_cleanup_t *cleanup = cleanup_stack;
  cleanup_stack = cleanup->prev;
  if (execute) cleanup->routine(cleanup->arg);
  ddsrt_free(cleanup);
  return DDS_RETCODE_OK;
}

void ddsrt_thread_init(uint32_t reason) { (void)reason; }
void ddsrt_thread_fini(uint32_t reason) { (void)reason; run_cleanup_handlers(); }
