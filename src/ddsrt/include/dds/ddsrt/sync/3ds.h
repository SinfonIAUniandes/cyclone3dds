#ifndef DDSRT_SYNC_3DS_H
#define DDSRT_SYNC_3DS_H

#include <3ds.h>

#include "dds/ddsrt/atomics.h"

typedef struct { LightLock lock; } ddsrt_mutex_t;
typedef struct { CondVar native; } ddsrt_cond_t;
typedef struct { ddsrt_cond_t condition; } ddsrt_cond_wctime_t;
typedef struct { ddsrt_cond_t condition; } ddsrt_cond_mtime_t;
typedef struct { ddsrt_cond_t condition; } ddsrt_cond_etime_t;

typedef struct {
  LightLock lock;
  CondVar readers_ready;
  CondVar writers_ready;
  uint32_t active_readers;
  uint32_t waiting_writers;
  bool writer_active;
} ddsrt_rwlock_t;

typedef ddsrt_atomic_uint32_t ddsrt_once_t;
#define DDSRT_ONCE_INIT { .v = (1u << 0) }

#endif
