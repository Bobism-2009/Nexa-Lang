#pragma once

#include <string>

namespace nexa {

// Raw TCP, both ends, on the OS's own sockets: Winsock on Windows, POSIX
// sockets everywhere else. There is no third stack because there is no reason
// for one -- connect/listen/accept/send/recv is the same call sequence on both,
// so the split is a handful of typedefs rather than a second implementation.
// Those typedefs live in SocketPlatform.hpp, which udp.* stands on too and
// which Modules.hpp emits once ahead of either.
//
// http.* is a protocol; this is the wire under it. A program reaches for tcp.*
// when it speaks something HTTP does not cover -- its own game protocol, a
// line-based control channel, someone else's binary format. udp.* is the same
// wire without the connection.
//
// Everything here is an int handle: the socket as the OS numbers it, with 0
// reserved for "no socket". That is the whole type system of this module --
// no struct, no Result -- because a handle is all a raw socket is, and
// 0-on-failure is the same shape os.spawn already uses for a process id.
//
// `needConnect` and `needListen` gate the two entry points, so a program that
// only dials carries no bind/listen/accept and a program that only serves
// carries no name resolution: the same needs-driven emission as http.* and
// std/gfx. send/recv/port/close sit under both and are always emitted.
//
// `needTimeout` is tcp.timeout and tcp.connect's third argument, the time limits:
// a program that sets none carries none of the code for them.
inline std::string tcpRuntimeCpp(bool needConnect = true, bool needListen = true, bool needTimeout = false) {
    std::string out = R"NEXA_TCP(
#include <string>
#include <cstring>
)NEXA_TCP";

    // A page has no socket to open, so the wasm build gets the same surface
    // with nothing behind it: every call fails the way a refused connection
    // does rather than pretending to have reached the network.
    std::string wasm;
    if (needConnect) {
        wasm += "[[maybe_unused]] static int __nexa_tcp_connect(const std::string& host, int port) "
                "{ (void)host; (void)port; return 0; }\n";
    }
    if (needTimeout) {
        wasm += "[[maybe_unused]] static int __nexa_tcp_timeout(int h, int ms) { (void)h; (void)ms; return 0; }\n";
        if (needConnect) {
            wasm += "[[maybe_unused]] static int __nexa_tcp_connect_ms(const std::string& host, int port, int ms) "
                    "{ (void)host; (void)port; (void)ms; return 0; }\n";
        }
    }
    if (needListen) {
        wasm += "[[maybe_unused]] static int __nexa_tcp_listen(int port) { (void)port; return 0; }\n";
        wasm += "[[maybe_unused]] static int __nexa_tcp_accept(int listener) { (void)listener; return 0; }\n";
    }
    wasm += "[[maybe_unused]] static int __nexa_tcp_send(int h, const std::string& data) "
            "{ (void)h; (void)data; return 0; }\n";
    wasm += "[[maybe_unused]] static std::string __nexa_tcp_recv(int h, int max) "
            "{ (void)h; (void)max; return std::string(); }\n";
    wasm += "[[maybe_unused]] static int __nexa_tcp_port(int h) { (void)h; return 0; }\n";
    wasm += "[[maybe_unused]] static int __nexa_tcp_close(int h) { (void)h; return 0; }\n";

    std::string real;

    if (needConnect) {
        real += R"NEXA_TCP_CONNECT(
// tcp.connect: the first address the name resolves to that answers. AF_UNSPEC
// means an IPv6-only host is reachable without the program saying so.
[[maybe_unused]] static int __nexa_tcp_connect(const std::string& host, int port) {
  if (!__nexa_net_start()) return 0;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return 0;
  __nexa_net_sock_t fd = __NEXA_NET_BAD;
  for (struct addrinfo* p = res; p; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd == __NEXA_NET_BAD) continue;
    if (connect(fd, p->ai_addr, (socklen_t)p->ai_addrlen) == 0) break;
    __nexa_net_shut(fd);
    fd = __NEXA_NET_BAD;
  }
  freeaddrinfo(res);
  if (fd != __NEXA_NET_BAD) __nexa_net_nosigpipe(fd);
  return __nexa_net_handle(fd);
}
)NEXA_TCP_CONNECT";
    }

    if (needTimeout) {
        real += R"NEXA_TCP_TIMEOUT(
#ifndef _WIN32
#include <sys/time.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#endif
// tcp.timeout: how long a tcp.recv or tcp.send on this socket waits, in
// milliseconds, before it gives up the way a closed connection does ("" or a
// short count). 0 is no limit, which is what a socket starts with.
[[maybe_unused]] static int __nexa_tcp_timeout(int h, int ms) {
  if (ms < 0) ms = 0;
#ifdef _WIN32
  DWORD t = (DWORD)ms;
#else
  struct timeval t;
  t.tv_sec = ms / 1000;
  t.tv_usec = (ms % 1000) * 1000;
#endif
  __nexa_net_sock_t s = (__nexa_net_sock_t)h;
  return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, (socklen_t)sizeof(t)) == 0 &&
         setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&t, (socklen_t)sizeof(t)) == 0;
}
)NEXA_TCP_TIMEOUT";
        if (needConnect) {
            real += R"NEXA_TCP_CMS(
// One address, waiting at most ms for it to answer: the connect is started
// without blocking and then waited on, so a host that drops the packets costs
// ms and not the OS's own limit (about 21s on Windows, minutes on Linux).
static bool __nexa_tcp_connect_wait(__nexa_net_sock_t fd, const struct sockaddr* a, socklen_t n, int ms) {
#ifdef _WIN32
  u_long on = 1;
  if (ioctlsocket(fd, FIONBIO, &on) != 0) return false;
  bool ok = connect(fd, a, n) == 0;
  if (!ok && WSAGetLastError() == WSAEWOULDBLOCK) {
    fd_set w, e;
    FD_ZERO(&w);
    FD_ZERO(&e);
    FD_SET(fd, &w);
    FD_SET(fd, &e);
    struct timeval t;
    t.tv_sec = ms / 1000;
    t.tv_usec = (ms % 1000) * 1000;
    ok = select(0, nullptr, &w, &e, &t) > 0 && FD_ISSET(fd, &w);
  }
  u_long off = 0;
  if (ioctlsocket(fd, FIONBIO, &off) != 0) ok = false;
#else
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) return false;
  bool ok = connect(fd, a, n) == 0;
  if (!ok && errno == EINPROGRESS) {
    struct pollfd p;
    p.fd = fd;
    p.events = POLLOUT;
    p.revents = 0;
    int r;
    do r = poll(&p, 1, ms); while (r < 0 && errno == EINTR);
    int err = 0;
    socklen_t len = (socklen_t)sizeof(err);
    ok = r > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&err, &len) == 0 && err == 0;
  }
  if (fcntl(fd, F_SETFL, fl) < 0) ok = false;
#endif
  return ok;
}
// tcp.connect(host, port, ms): tcp.connect with a time limit, which the socket
// keeps for its reads and writes (tcp.timeout changes it). ms is per address
// the name resolves to.
[[maybe_unused]] static int __nexa_tcp_connect_ms(const std::string& host, int port, int ms) {
  if (ms <= 0) return __nexa_tcp_connect(host, port);
  if (!__nexa_net_start()) return 0;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return 0;
  __nexa_net_sock_t fd = __NEXA_NET_BAD;
  for (struct addrinfo* p = res; p; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd == __NEXA_NET_BAD) continue;
    if (__nexa_tcp_connect_wait(fd, p->ai_addr, (socklen_t)p->ai_addrlen, ms)) break;
    __nexa_net_shut(fd);
    fd = __NEXA_NET_BAD;
  }
  freeaddrinfo(res);
  if (fd == __NEXA_NET_BAD) return 0;
  __nexa_net_nosigpipe(fd);
  int h = __nexa_net_handle(fd);
  if (h) __nexa_tcp_timeout(h, ms);
  return h;
}
)NEXA_TCP_CMS";
        }
    }

    if (needListen) {
        real += R"NEXA_TCP_LISTEN(
// tcp.listen: every interface, not just loopback. http.localhost() binds
// 127.0.0.1 because a local HTTP helper has no business being reachable; a
// game server is the opposite case, and a program that wants loopback-only
// here gets it by refusing the connections it does not want.
[[maybe_unused]] static int __nexa_tcp_listen(int port) {
  if (!__nexa_net_start()) return 0;
  __nexa_net_sock_t fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == __NEXA_NET_BAD) return 0;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, (socklen_t)sizeof(one));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, (struct sockaddr*)&addr, (socklen_t)sizeof(addr)) != 0) {
    __nexa_net_shut(fd);
    return 0;
  }
  if (listen(fd, 64) != 0) {
    __nexa_net_shut(fd);
    return 0;
  }
  return __nexa_net_handle(fd);
}
[[maybe_unused]] static int __nexa_tcp_accept(int listener) {
  __nexa_net_sock_t fd = accept((__nexa_net_sock_t)listener, nullptr, nullptr);
  if (fd == __NEXA_NET_BAD) return 0;
  __nexa_net_nosigpipe(fd);
  return __nexa_net_handle(fd);
}
)NEXA_TCP_LISTEN";
    }

    real += R"NEXA_TCP_IO(
// tcp.send: a full write, so the count it hands back is the whole string on
// success. A short count is what a broken connection looks like -- the kernel
// takes what it has room for, and the write that follows is the one that
// fails.
[[maybe_unused]] static int __nexa_tcp_send(int h, const std::string& data) {
  __nexa_net_sock_t s = (__nexa_net_sock_t)h;
  const char* p = data.data();
  size_t n = data.size();
  size_t sent = 0;
  while (sent < n) {
    size_t left = n - sent;
    int chunk = left > 0x7fffffff ? 0x7fffffff : (int)left;
    int k = (int)send(s, p + sent, chunk, __NEXA_NET_SEND_FLAGS);
    if (k <= 0) break;
    sent += (size_t)k;
  }
  return (int)sent;
}
// tcp.recv: one read, not a full one. TCP is a stream and a send does not
// arrive as a send, so this hands back whatever had turned up -- up to `max`
// bytes, binary-safe the way os.load is, and "" once the peer has hung up.
[[maybe_unused]] static std::string __nexa_tcp_recv(int h, int max) {
  std::string out;
  out.resize((size_t)max);
  int n = (int)recv((__nexa_net_sock_t)h, &out[0], max, 0);
  if (n <= 0) return std::string();
  out.resize((size_t)n);
  return out;
}
// tcp.port: the port this end is actually on, which is how a program that
// asked for port 0 finds out which one the OS picked.
[[maybe_unused]] static int __nexa_tcp_port(int h) {
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  socklen_t len = (socklen_t)sizeof(a);
  if (getsockname((__nexa_net_sock_t)h, (struct sockaddr*)&a, &len) != 0) return 0;
  return (int)ntohs(a.sin_port);
}
[[maybe_unused]] static int __nexa_tcp_close(int h) {
  __nexa_net_shut((__nexa_net_sock_t)h);
  return 1;
}
)NEXA_TCP_IO";

    out += "#if defined(__EMSCRIPTEN__) || defined(__wasi__)\n";
    out += wasm;
    out += "#else\n";
    out += real;
    out += "#endif\n";
    return out;
}

}  // namespace nexa
