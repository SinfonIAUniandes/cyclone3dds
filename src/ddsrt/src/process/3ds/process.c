#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"

ddsrt_pid_t ddsrt_getpid(void) { return 1; }
char *ddsrt_getprocessname(void) { return ddsrt_strdup("cyclonedds-3ds"); }
