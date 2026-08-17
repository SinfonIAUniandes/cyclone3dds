#include <errno.h>
#include <string.h>

#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/string.h"

dds_return_t ddsrt_gethostname(char *hostname, size_t size)
{
  char local_name[256];
  memset(local_name, 0, sizeof(local_name));
  if (gethostname(local_name, sizeof(local_name) - 1) != 0) return DDS_RETCODE_ERROR;
  return ddsrt_strlcpy(hostname, local_name, size) >= size ? DDS_RETCODE_NOT_ENOUGH_SPACE : DDS_RETCODE_OK;
}
