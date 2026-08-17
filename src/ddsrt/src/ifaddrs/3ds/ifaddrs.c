#include <3ds.h>

#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"

static bool requests_ipv4(const int *families)
{
  if (families == NULL) return true;
  for (size_t index = 0; families[index] != DDSRT_AF_TERM; index++) if (families[index] == AF_INET) return true;
  return false;
}

dds_return_t ddsrt_getifaddrs(ddsrt_ifaddrs_t **interfaces, const int *families)
{
  assert(interfaces != NULL);
  if (!requests_ipv4(families)) { *interfaces = NULL; return DDS_RETCODE_OK; }

  struct in_addr ip, netmask, broadcast;
  if (SOCU_GetIPInfo(&ip, &netmask, &broadcast) != 0 || ip.s_addr == 0) { *interfaces = NULL; return DDS_RETCODE_ERROR; }

  ddsrt_ifaddrs_t *interface = ddsrt_calloc(1, sizeof(*interface));
  struct sockaddr_in *address = ddsrt_calloc(1, sizeof(*address));
  struct sockaddr_in *mask = ddsrt_calloc(1, sizeof(*mask));
  struct sockaddr_in *broad = ddsrt_calloc(1, sizeof(*broad));
  if (!interface || !address || !mask || !broad) {
    ddsrt_free(interface); ddsrt_free(address); ddsrt_free(mask); ddsrt_free(broad);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  interface->name = ddsrt_strdup("wlan0");
  if (!interface->name) {
    ddsrt_free(interface); ddsrt_free(address); ddsrt_free(mask); ddsrt_free(broad);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  address->sin_family = AF_INET; address->sin_addr = ip;
  mask->sin_family = AF_INET; mask->sin_addr = netmask;
  broad->sin_family = AF_INET; broad->sin_addr = broadcast;
  interface->index = 1;
  interface->flags = IFF_UP | IFF_BROADCAST | IFF_MULTICAST;
  interface->type = DDSRT_IFTYPE_WIFI;
  interface->addr = (struct sockaddr *)address;
  interface->netmask = (struct sockaddr *)mask;
  interface->broadaddr = (struct sockaddr *)broad;
  *interfaces = interface;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_eth_get_mac_addr(char *interface_name, unsigned char *mac_address)
{
  if (!interface_name || !mac_address || strcmp(interface_name, "wlan0") != 0) return DDS_RETCODE_ERROR;
  socklen_t size = 6;
  return SOCU_GetNetworkOpt(SOL_CONFIG, NETOPT_MAC_ADDRESS, mac_address, &size) == 0 && size >= 6 ? DDS_RETCODE_OK : DDS_RETCODE_ERROR;
}
