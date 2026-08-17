#include <3ds.h>

#include "dds/ddsrt/random.h"

bool ddsrt_prng_makeseed(struct ddsrt_prng_seed *seed)
{
  uint64_t tick = svcGetSystemTick();
  uint64_t time = osGetTime();
  uint32_t state = (uint32_t)(tick ^ (tick >> 32) ^ time ^ (time >> 32) ^ (uintptr_t)&seed);
  for (size_t index = 0; index < sizeof(seed->key) / sizeof(seed->key[0]); index++) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    seed->key[index] = state;
  }
  return true;
}
