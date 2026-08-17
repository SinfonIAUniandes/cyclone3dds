#ifndef DDSRT_SOCKETS_3DS_H
#define DDSRT_SOCKETS_3DS_H

#include <stdint.h>

#include <3ds.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "dds/ddsrt/iovec.h"

#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_LOOPBACK 0x8
#define IFF_POINTOPOINT 0x10
#define IFF_MULTICAST 0x1000

#ifndef MSG_TRUNC
#define MSG_TRUNC 0
#endif
#ifndef SO_DONTROUTE
#define SO_DONTROUTE 0
#endif
#ifndef IP_MULTICAST_IF
#define IP_MULTICAST_IF 32
#endif
#ifndef IN_MULTICAST
#define IN_MULTICAST(address) (((address) & 0xf0000000u) == 0xe0000000u)
#endif

typedef int ddsrt_socket_t;
#define DDSRT_INVALID_SOCKET (-1)
#define PRIdSOCK "d"

typedef struct ddsrt_3ds_socket_stats {
  uint32_t tx_multicast;
  uint32_t tx_unicast;
  uint32_t rx_total;
  uint32_t rx_remote;
  int32_t last_send_errno;
  int32_t last_recv_errno;
  int32_t multicast_if_errno;
} ddsrt_3ds_socket_stats_t;

void ddsrt_3ds_socket_stats(ddsrt_3ds_socket_stats_t *stats);

typedef struct ddsrt_socket_ext { ddsrt_socket_t sock; } ddsrt_socket_ext_t;

typedef struct ddsrt_msghdr {
  void *msg_name;
  socklen_t msg_namelen;
  ddsrt_iovec_t *msg_iov;
  ddsrt_msg_iovlen_t msg_iovlen;
  void *msg_control;
  size_t msg_controllen;
  int msg_flags;
} ddsrt_msghdr_t;

#define DDSRT_MSGHDR_FLAGS 1

#endif
