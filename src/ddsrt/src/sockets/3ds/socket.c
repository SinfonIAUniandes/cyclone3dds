#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sockets.h"

static uint32_t socket_tx_multicast;
static uint32_t socket_tx_unicast;
static uint32_t socket_rx_total;
static uint32_t socket_rx_remote;
static int32_t socket_last_send_errno;
static int32_t socket_last_recv_errno;
static int32_t socket_multicast_if_errno;

void ddsrt_3ds_socket_stats(ddsrt_3ds_socket_stats_t *stats)
{
  assert(stats != NULL);
  stats->tx_multicast = __atomic_load_n(&socket_tx_multicast, __ATOMIC_RELAXED);
  stats->tx_unicast = __atomic_load_n(&socket_tx_unicast, __ATOMIC_RELAXED);
  stats->rx_total = __atomic_load_n(&socket_rx_total, __ATOMIC_RELAXED);
  stats->rx_remote = __atomic_load_n(&socket_rx_remote, __ATOMIC_RELAXED);
  stats->last_send_errno = __atomic_load_n(&socket_last_send_errno, __ATOMIC_RELAXED);
  stats->last_recv_errno = __atomic_load_n(&socket_last_recv_errno, __ATOMIC_RELAXED);
  stats->multicast_if_errno = __atomic_load_n(&socket_multicast_if_errno, __ATOMIC_RELAXED);
}

static void record_send_success(const ddsrt_msghdr_t *message)
{
  if (message->msg_name != NULL && message->msg_namelen >= sizeof(struct sockaddr_in)) {
    const struct sockaddr_in *destination = message->msg_name;
    if (destination->sin_family == AF_INET && IN_MULTICAST(ntohl(destination->sin_addr.s_addr))) {
      __atomic_fetch_add(&socket_tx_multicast, 1, __ATOMIC_RELAXED);
      return;
    }
  }
  __atomic_fetch_add(&socket_tx_unicast, 1, __ATOMIC_RELAXED);
}

static void record_receive_success(const ddsrt_msghdr_t *message)
{
  __atomic_fetch_add(&socket_rx_total, 1, __ATOMIC_RELAXED);
  if (message->msg_name != NULL && message->msg_namelen >= sizeof(struct sockaddr_in)) {
    const struct sockaddr_in *source = message->msg_name;
    struct in_addr local_ip;
    struct in_addr netmask;
    struct in_addr broadcast;
    if (source->sin_family == AF_INET &&
        SOCU_GetIPInfo(&local_ip, &netmask, &broadcast) == 0 &&
        source->sin_addr.s_addr != local_ip.s_addr) {
      __atomic_fetch_add(&socket_rx_remote, 1, __ATOMIC_RELAXED);
    }
  }
}

static dds_return_t socket_error(int error)
{
  if (error == EAGAIN || error == EWOULDBLOCK) return DDS_RETCODE_TRY_AGAIN;
  if (error == ENOMEM || error == ENOBUFS) return DDS_RETCODE_OUT_OF_RESOURCES;
  if (error == EADDRINUSE) return DDS_RETCODE_PRECONDITION_NOT_MET;
  if (error == EINVAL || error == EBADF || error == ENOTSOCK) return DDS_RETCODE_BAD_PARAMETER;
  if (error == ECONNREFUSED || error == ENETUNREACH || error == EHOSTUNREACH) return DDS_RETCODE_NO_CONNECTION;
  if (error == EINTR) return DDS_RETCODE_INTERRUPTED;
  if (error == EMSGSIZE) return DDS_RETCODE_NOT_ENOUGH_SPACE;
  return DDS_RETCODE_ERROR;
}

dds_return_t ddsrt_socket(ddsrt_socket_t *socket_out, int domain, int type, int protocol)
{
  assert(socket_out != NULL);
  int socket_fd = socket(domain, type, protocol);
  if (socket_fd < 0) return socket_error(errno);
  *socket_out = socket_fd;
  return DDS_RETCODE_OK;
}

void ddsrt_socket_ext_init(ddsrt_socket_ext_t *extended, ddsrt_socket_t socket_fd) { extended->sock = socket_fd; }
void ddsrt_socket_ext_fini(ddsrt_socket_ext_t *extended) { (void)extended; }
dds_return_t ddsrt_close(ddsrt_socket_t socket_fd) { return close(socket_fd) == 0 ? DDS_RETCODE_OK : socket_error(errno); }

static dds_return_t bind_ephemeral_ipv4(ddsrt_socket_t socket_fd, const struct sockaddr_in *address)
{
  struct in_addr local_ip;
  struct in_addr netmask;
  struct in_addr broadcast;
  if (SOCU_GetIPInfo(&local_ip, &netmask, &broadcast) != 0 || local_ip.s_addr == 0) {
    return DDS_RETCODE_ERROR;
  }

  struct sockaddr_in local_address = *address;
  local_address.sin_addr = local_ip;
  u32 seed = (u32)svcGetSystemTick();
  for (u32 attempt = 0; attempt < 32; attempt++) {
    local_address.sin_port = htons((u16)(49152 + ((seed + attempt) % 16384)));
    if (bind(socket_fd, (const struct sockaddr *)&local_address, sizeof(local_address)) == 0) {
      return DDS_RETCODE_OK;
    }
    if (errno != EADDRINUSE) {
      return socket_error(errno);
    }
  }
  return DDS_RETCODE_PRECONDITION_NOT_MET;
}

dds_return_t ddsrt_bind(ddsrt_socket_t socket_fd, const struct sockaddr *address, socklen_t length)
{
  if (address->sa_family == AF_INET && length == sizeof(struct sockaddr_in)) {
    const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)address;
    if (ipv4->sin_addr.s_addr == htonl(INADDR_ANY) && ipv4->sin_port == 0) {
      return bind_ephemeral_ipv4(socket_fd, ipv4);
    }
  }
  return bind(socket_fd, address, length) == 0 ? DDS_RETCODE_OK : socket_error(errno);
}
dds_return_t ddsrt_connect(ddsrt_socket_t socket_fd, const struct sockaddr *address, socklen_t length) { return connect(socket_fd, address, length) == 0 ? DDS_RETCODE_OK : socket_error(errno); }
dds_return_t ddsrt_listen(ddsrt_socket_t socket_fd, int backlog) { return listen(socket_fd, backlog) == 0 ? DDS_RETCODE_OK : socket_error(errno); }
dds_return_t ddsrt_getsockname(ddsrt_socket_t socket_fd, struct sockaddr *address, socklen_t *length) { return getsockname(socket_fd, address, length) == 0 ? DDS_RETCODE_OK : socket_error(errno); }

dds_return_t ddsrt_accept(ddsrt_socket_t socket_fd, struct sockaddr *address, socklen_t *length, ddsrt_socket_t *connection)
{
  int accepted = accept(socket_fd, address, length);
  if (accepted < 0) return socket_error(errno);
  *connection = accepted;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_getsockopt(ddsrt_socket_t socket_fd, int32_t level, int32_t option, void *value, socklen_t *length)
{
  if (level == SOL_SOCKET && (option == SO_SNDBUF || option == SO_RCVBUF)) {
    /* libctru exposes these option values but the SOC service cannot query them. */
    (void)socket_fd;
    (void)value;
    (void)length;
    return DDS_RETCODE_UNSUPPORTED;
  }
  return getsockopt(socket_fd, level, option, value, length) == 0 ? DDS_RETCODE_OK : socket_error(errno);
}

dds_return_t ddsrt_setsockopt(ddsrt_socket_t socket_fd, int32_t level, int32_t option, const void *value, socklen_t length)
{
  if (level == SOL_SOCKET && option == SO_DONTROUTE) {
    return DDS_RETCODE_OK;
  }
  if (level == IPPROTO_IP && option == IP_MULTICAST_IF) {
    if (setsockopt(socket_fd, IPPROTO_IP, 32, value, length) != 0) {
      __atomic_store_n(&socket_multicast_if_errno, errno, __ATOMIC_RELAXED);
    }
    return DDS_RETCODE_OK;
  }
  return setsockopt(socket_fd, level, option, value, length) == 0 ? DDS_RETCODE_OK : socket_error(errno);
}

dds_return_t ddsrt_setsocknonblocking(ddsrt_socket_t socket_fd, bool nonblocking)
{
  int flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags < 0) return socket_error(errno);
  flags = nonblocking ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
  return fcntl(socket_fd, F_SETFL, flags) == 0 ? DDS_RETCODE_OK : socket_error(errno);
}

dds_return_t ddsrt_send(ddsrt_socket_t socket_fd, const void *buffer, size_t size, int flags, size_t *written)
{
  ssize_t result = send(socket_fd, buffer, size, flags);
  if (result < 0) return socket_error(errno);
  if (written) *written = (size_t)result;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_recv(ddsrt_socket_t socket_fd, void *buffer, size_t size, int flags, size_t *received)
{
  ssize_t result = recv(socket_fd, buffer, size, flags);
  if (result < 0) { *received = 0; return socket_error(errno); }
  *received = (size_t)result;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_sendmsg(ddsrt_socket_t socket_fd, const ddsrt_msghdr_t *message, int flags, size_t *written)
{
  if (message->msg_iovlen == 0 || message->msg_controllen != 0) return DDS_RETCODE_UNSUPPORTED;

  if (message->msg_iovlen == 1) {
    ssize_t result = sendto(socket_fd, message->msg_iov[0].iov_base, message->msg_iov[0].iov_len, flags,
                            message->msg_name, message->msg_namelen);
    if (result < 0) {
      int error = errno;
      __atomic_store_n(&socket_last_send_errno, error, __ATOMIC_RELAXED);
      return socket_error(error);
    }
    record_send_success(message);
    if (written) *written = (size_t)result;
    return DDS_RETCODE_OK;
  }

  size_t size = 0;
  for (size_t index = 0; index < message->msg_iovlen; index++) {
    if (message->msg_iov[index].iov_len > SIZE_MAX - size) return DDS_RETCODE_BAD_PARAMETER;
    size += message->msg_iov[index].iov_len;
  }

  void *buffer = ddsrt_malloc_s(size);
  if (buffer == NULL) return DDS_RETCODE_OUT_OF_RESOURCES;

  char *destination = buffer;
  for (size_t index = 0; index < message->msg_iovlen; index++) {
    memcpy(destination, message->msg_iov[index].iov_base, message->msg_iov[index].iov_len);
    destination += message->msg_iov[index].iov_len;
  }

  ssize_t result = sendto(socket_fd, buffer, size, flags, message->msg_name, message->msg_namelen);
  int error = result < 0 ? errno : 0;
  ddsrt_free(buffer);
  if (result < 0) {
    __atomic_store_n(&socket_last_send_errno, error, __ATOMIC_RELAXED);
    return socket_error(error);
  }
  record_send_success(message);
  if (written) *written = (size_t)result;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_recvmsg(const ddsrt_socket_ext_t *extended, ddsrt_msghdr_t *message, int flags, size_t *received)
{
  if (message->msg_iovlen != 1 || message->msg_controllen != 0) return DDS_RETCODE_UNSUPPORTED;
  socklen_t address_length = message->msg_namelen;
  ssize_t result = recvfrom(extended->sock, message->msg_iov[0].iov_base, message->msg_iov[0].iov_len, flags,
                            message->msg_name, &address_length);
  if (result < 0) {
    int error = errno;
    *received = 0;
    if (error != EAGAIN && error != EWOULDBLOCK) {
      __atomic_store_n(&socket_last_recv_errno, error, __ATOMIC_RELAXED);
    }
    return socket_error(error);
  }
  message->msg_namelen = address_length;
  message->msg_flags = 0;
  *received = (size_t)result;
  record_receive_success(message);
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_select(int32_t nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, dds_duration_t duration)
{
  struct timeval timeout;
  timeout.tv_sec = duration == DDS_INFINITY ? 0 : (long)(duration / DDS_NSECS_IN_SEC);
  timeout.tv_usec = duration == DDS_INFINITY ? 0 : (long)((duration % DDS_NSECS_IN_SEC) / DDS_NSECS_IN_USEC);
  struct timeval *timeout_pointer = duration == DDS_INFINITY ? NULL : &timeout;
  int result = select(nfds, readfds, writefds, errorfds, timeout_pointer);
  if (result > 0) return result;
  if (result == 0) return DDS_RETCODE_TIMEOUT;
  return socket_error(errno);
}

dds_return_t ddsrt_shutdown(ddsrt_socket_t socket_fd, enum ddsrt_shutdown_how how)
{
  int native_how = how == DDSRT_SHUTDOWN_READ ? SHUT_RD : how == DDSRT_SHUTDOWN_WRITE ? SHUT_WR : SHUT_RDWR;
  return shutdown(socket_fd, native_how) == 0 ? DDS_RETCODE_OK : socket_error(errno);
}
