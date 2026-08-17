#include <assert.h>
#include <limits.h>
#include <string.h>

#include "dds/ddsrt/sync.h"

#define ONCE_NOT_STARTED (1u << 0)
#define ONCE_IN_PROGRESS (1u << 1)
#define ONCE_FINISHED (1u << 2)

static s64 duration_until(int64_t now, int64_t deadline)
{
  if (deadline == DDS_NEVER) return INT64_MAX;
  return deadline <= now ? 0 : deadline - now;
}

static bool condition_wait(ddsrt_cond_t *condition, ddsrt_mutex_t *mutex, s64 timeout)
{
  if (timeout == 0) return false;
  if (timeout == INT64_MAX) {
    CondVar_Wait(&condition->native, &mutex->lock);
    return true;
  }
  return CondVar_WaitTimeout(&condition->native, &mutex->lock, timeout) == 0;
}

void ddsrt_mutex_init(ddsrt_mutex_t *mutex) { LightLock_Init(&mutex->lock); }
void ddsrt_mutex_destroy(ddsrt_mutex_t *mutex) { memset(mutex, 0, sizeof(*mutex)); }
void ddsrt_mutex_lock(ddsrt_mutex_t *mutex) { LightLock_Lock(&mutex->lock); }
bool ddsrt_mutex_trylock(ddsrt_mutex_t *mutex) { return LightLock_TryLock(&mutex->lock) == 0; }
void ddsrt_mutex_unlock(ddsrt_mutex_t *mutex) { LightLock_Unlock(&mutex->lock); }

void ddsrt_cond_init(ddsrt_cond_t *condition) { CondVar_Init(&condition->native); }
void ddsrt_cond_destroy(ddsrt_cond_t *condition) { memset(condition, 0, sizeof(*condition)); }
void ddsrt_cond_wctime_init(ddsrt_cond_wctime_t *condition) { ddsrt_cond_init(&condition->condition); }
void ddsrt_cond_mtime_init(ddsrt_cond_mtime_t *condition) { ddsrt_cond_init(&condition->condition); }
void ddsrt_cond_etime_init(ddsrt_cond_etime_t *condition) { ddsrt_cond_init(&condition->condition); }
void ddsrt_cond_wctime_destroy(ddsrt_cond_wctime_t *condition) { ddsrt_cond_destroy(&condition->condition); }
void ddsrt_cond_mtime_destroy(ddsrt_cond_mtime_t *condition) { ddsrt_cond_destroy(&condition->condition); }
void ddsrt_cond_etime_destroy(ddsrt_cond_etime_t *condition) { ddsrt_cond_destroy(&condition->condition); }
void ddsrt_cond_wait(ddsrt_cond_t *condition, ddsrt_mutex_t *mutex) { (void)condition_wait(condition, mutex, INT64_MAX); }
void ddsrt_cond_wctime_wait(ddsrt_cond_wctime_t *condition, ddsrt_mutex_t *mutex) { ddsrt_cond_wait(&condition->condition, mutex); }
void ddsrt_cond_mtime_wait(ddsrt_cond_mtime_t *condition, ddsrt_mutex_t *mutex) { ddsrt_cond_wait(&condition->condition, mutex); }
void ddsrt_cond_etime_wait(ddsrt_cond_etime_t *condition, ddsrt_mutex_t *mutex) { ddsrt_cond_wait(&condition->condition, mutex); }
bool ddsrt_cond_wctime_waituntil(ddsrt_cond_wctime_t *condition, ddsrt_mutex_t *mutex, ddsrt_wctime_t deadline) { return condition_wait(&condition->condition, mutex, duration_until(ddsrt_time_wallclock().v, deadline.v)); }
bool ddsrt_cond_mtime_waituntil(ddsrt_cond_mtime_t *condition, ddsrt_mutex_t *mutex, ddsrt_mtime_t deadline) { return condition_wait(&condition->condition, mutex, duration_until(ddsrt_time_monotonic().v, deadline.v)); }
bool ddsrt_cond_etime_waituntil(ddsrt_cond_etime_t *condition, ddsrt_mutex_t *mutex, ddsrt_etime_t deadline) { return condition_wait(&condition->condition, mutex, duration_until(ddsrt_time_elapsed().v, deadline.v)); }
void ddsrt_cond_signal(ddsrt_cond_t *condition) { CondVar_Signal(&condition->native); }
void ddsrt_cond_wctime_signal(ddsrt_cond_wctime_t *condition) { ddsrt_cond_signal(&condition->condition); }
void ddsrt_cond_mtime_signal(ddsrt_cond_mtime_t *condition) { ddsrt_cond_signal(&condition->condition); }
void ddsrt_cond_etime_signal(ddsrt_cond_etime_t *condition) { ddsrt_cond_signal(&condition->condition); }
void ddsrt_cond_broadcast(ddsrt_cond_t *condition) { CondVar_Broadcast(&condition->native); }
void ddsrt_cond_wctime_broadcast(ddsrt_cond_wctime_t *condition) { ddsrt_cond_broadcast(&condition->condition); }
void ddsrt_cond_mtime_broadcast(ddsrt_cond_mtime_t *condition) { ddsrt_cond_broadcast(&condition->condition); }
void ddsrt_cond_etime_broadcast(ddsrt_cond_etime_t *condition) { ddsrt_cond_broadcast(&condition->condition); }

void ddsrt_rwlock_init(ddsrt_rwlock_t *lock)
{
  LightLock_Init(&lock->lock);
  CondVar_Init(&lock->readers_ready);
  CondVar_Init(&lock->writers_ready);
  lock->active_readers = 0;
  lock->waiting_writers = 0;
  lock->writer_active = false;
}

void ddsrt_rwlock_destroy(ddsrt_rwlock_t *lock) { memset(lock, 0, sizeof(*lock)); }

void ddsrt_rwlock_read(ddsrt_rwlock_t *lock)
{
  LightLock_Lock(&lock->lock);
  while (lock->writer_active || lock->waiting_writers != 0) CondVar_Wait(&lock->readers_ready, &lock->lock);
  lock->active_readers++;
  LightLock_Unlock(&lock->lock);
}

void ddsrt_rwlock_write(ddsrt_rwlock_t *lock)
{
  LightLock_Lock(&lock->lock);
  lock->waiting_writers++;
  while (lock->writer_active || lock->active_readers != 0) CondVar_Wait(&lock->writers_ready, &lock->lock);
  lock->waiting_writers--;
  lock->writer_active = true;
  LightLock_Unlock(&lock->lock);
}

bool ddsrt_rwlock_tryread(ddsrt_rwlock_t *lock)
{
  if (LightLock_TryLock(&lock->lock) != 0) return false;
  bool acquired = !lock->writer_active && lock->waiting_writers == 0;
  if (acquired) lock->active_readers++;
  LightLock_Unlock(&lock->lock);
  return acquired;
}

bool ddsrt_rwlock_trywrite(ddsrt_rwlock_t *lock)
{
  if (LightLock_TryLock(&lock->lock) != 0) return false;
  bool acquired = !lock->writer_active && lock->active_readers == 0;
  if (acquired) lock->writer_active = true;
  LightLock_Unlock(&lock->lock);
  return acquired;
}

void ddsrt_rwlock_unlock(ddsrt_rwlock_t *lock)
{
  LightLock_Lock(&lock->lock);
  if (lock->writer_active) lock->writer_active = false;
  else { assert(lock->active_readers != 0); lock->active_readers--; }
  if (lock->waiting_writers != 0) CondVar_Signal(&lock->writers_ready);
  else CondVar_Broadcast(&lock->readers_ready);
  LightLock_Unlock(&lock->lock);
}

void ddsrt_once(ddsrt_once_t *control, ddsrt_once_fn init_fn)
{
  for (;;) {
    uint32_t state = ddsrt_atomic_ld32(control);
    assert(state == ONCE_NOT_STARTED || state == ONCE_IN_PROGRESS || state == ONCE_FINISHED);
    if (state == ONCE_FINISHED) return;
    if (state == ONCE_NOT_STARTED && ddsrt_atomic_cas32(control, ONCE_NOT_STARTED, ONCE_IN_PROGRESS) != 0) {
      init_fn();
      (void)ddsrt_atomic_cas32(control, ONCE_IN_PROGRESS, ONCE_FINISHED);
      return;
    }
    svcSleepThread(DDS_MSECS(1));
  }
}
