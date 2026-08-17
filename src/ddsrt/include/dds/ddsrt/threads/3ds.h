#ifndef DDSRT_THREADS_3DS_H
#define DDSRT_THREADS_3DS_H

#include <3ds.h>
#include <inttypes.h>

#define DDSRT_HAVE_THREAD_SETNAME 0
#define DDSRT_HAVE_THREAD_LIST 0

typedef struct { Thread handle; } ddsrt_thread_t;
typedef uintptr_t ddsrt_tid_t;
typedef uintptr_t ddsrt_thread_list_id_t;
#define PRIdTID PRIuPTR

#endif
