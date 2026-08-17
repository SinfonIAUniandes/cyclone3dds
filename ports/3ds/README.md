# Nintendo 3DS Port

This port builds Eclipse Cyclone DDS as a static `libddsc.a` for the Nintendo
3DS ARM11 using devkitARM and libctru. It is consumed directly by the sibling
`ros2_3ds_interface` homebrew application.

## Build

Start with a fresh target build directory whenever changing the toolchain:

```sh
rm -rf build-3ds
cmake -S . -B build-3ds \
  -DCMAKE_TOOLCHAIN_FILE=ports/3ds/toolchain.cmake \
  -DWITH_NINTENDO_3DS=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_IDLC=OFF \
  -DBUILD_DDSPERF=OFF \
  -DBUILD_TESTING=OFF \
  -DENABLE_SECURITY=OFF \
  -DENABLE_IPV6=OFF \
  -DENABLE_SOURCE_SPECIFIC_MULTICAST=OFF
cmake --build build-3ds --target ddsc -j2
```

The resulting archive is `build-3ds/lib/libddsc.a`.

## Platform Layer

The libctru implementation is intentionally split by responsibility:

- `src/ddsrt/src/sync/3ds`: mutexes, condition variables, reader/writer locks,
  and one-time initialization over `LightLock` and `CondVar`.
- `src/ddsrt/src/threads/3ds`: lifecycle and joinable threads over `threadCreate`.
- `src/ddsrt/src/time/3ds`: clocks and sleep over `osGetTime` and `svcSleepThread`.
- `src/ddsrt/src/process/3ds`: single-process identity.
- `src/ddsrt/src/random/3ds`: seed material from system ticks and time.
- `src/ddsrt/src/ifaddrs/3ds`: the active Wi-Fi interface using `SOCU_GetIPInfo`.
- `src/ddsrt/src/sockets/3ds`: IPv4 socket wrappers over `soc:u`.

## Intentional Limits

This initial port is IPv4 and UDP only. It disables IPv6, source-specific
multicast, shared libraries, DDS Security, filesystem helpers, dynamic loading,
and process isolation.

libctru does not expose `IP_MULTICAST_IF` or `SO_DONTROUTE`; the socket backend
handles those options as explicit no-ops and relies on the active Wi-Fi
interface. Its SOC service also cannot query `SO_SNDBUF` or `SO_RCVBUF`; the
backend reports these as unsupported, which lets Cyclone DDS retain the service
defaults instead of aborting RTPS initialization. The DDS socket waitset has no
pipe/socketpair on 3DS, so it uses a 100 ms `select` timeout to observe changes
and shutdown requests.

The SOC service also rejects an IPv4 `bind(INADDR_ANY, 0)`. Cyclone uses that
combination for unicast transmit sockets, so the 3DS backend binds those sockets
to the active Wi-Fi address with a retried high UDP port in the ephemeral range.

The app currently validates participant creation. ROS 2 topic and service
types, QoS mapping, and direct interoperability tests are the next layer.