#include <3ds.h>

#include "dds/ddsrt/time.h"

#define NINTENDO_EPOCH_OFFSET_MS INT64_C(2208988800000)

static dds_time_t milliseconds_to_nanoseconds(u64 milliseconds)
{
  return (dds_time_t)milliseconds * DDS_NSECS_IN_MSEC;
}

dds_time_t dds_time(void)
{
  u64 milliseconds = osGetTime();
  return milliseconds < NINTENDO_EPOCH_OFFSET_MS ? 0 : milliseconds_to_nanoseconds(milliseconds - NINTENDO_EPOCH_OFFSET_MS);
}

ddsrt_wctime_t ddsrt_time_wallclock(void) { return (ddsrt_wctime_t){ dds_time() }; }
ddsrt_mtime_t ddsrt_time_monotonic(void) { return (ddsrt_mtime_t){ milliseconds_to_nanoseconds(osGetTime()) }; }
ddsrt_etime_t ddsrt_time_elapsed(void) { return (ddsrt_etime_t){ milliseconds_to_nanoseconds(osGetTime()) }; }
ddsrt_hrtime_t ddsrt_time_highres(void) { return (ddsrt_hrtime_t){ (uint64_t)ddsrt_time_monotonic().v }; }
void dds_sleepfor(dds_duration_t duration) { if (duration > 0) svcSleepThread(duration); }
