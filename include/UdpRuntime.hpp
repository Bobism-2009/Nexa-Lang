#pragma once

#include <string>

namespace nexa {

// UDP, on the same OS sockets tcp.* uses and through the same SocketPlatform
// preamble: Winsock on Windows, POSIX everywhere else.
//
// tcp.* is a connection; this is not. There is no dial and no accept -- a
// program opens a port and then every send says where it is going and every
// read says where it came from. That is the whole difference, and it is why
// udp.send carries a host and a port that tcp.send does not need.
//
// Everything here is an int handle, exactly as in tcp.*: the socket as the OS
// numbers it, 0 kept back for "no socket", no struct and no Result.
//
// `needSender` gates udp.sender/udp.sender_port and the one-entry table behind
// them, so a program that never asks who sent a packet carries no bookkeeping
// for the answer -- the same needs-driven emission as tcp.connect/tcp.listen.
// The table is thread_local: the thread that did the read is the thread that
// asks, so an accept-loop-style program in std/thread cannot have one worker's
// packet answer another worker's question.
inline std::string udpRuntimeCpp(bool needSender = true) {
    std::string out = R"NEXA_UDP(
#include <string>
#include <cstring>
)NEXA_UDP";

    // A page has no datagram socket either, so the wasm slice is the same
    // surface over nothing -- every call fails the way an unreachable network
    // does. NexaC refuses a --wasm build that actually calls udp.* before it
    // gets this far; the stubs are what keep the emission compiling.
    std::string wasm;
    wasm += "[[maybe_unused]] static int __nexa_udp_open(int port) { (void)port; return 0; }\n";
    wasm += "[[maybe_unused]] static int __nexa_udp_port(int h) { (void)h; return 0; }\n";
    wasm += "[[maybe_unused]] static int __nexa_udp_send(int h, const std::string& host, int port, "
            "const std::string& data) { (void)h; (void)host; (void)port; (void)data; return 0; }\n";
    wasm += "[[maybe_unused]] static std::string __nexa_udp_recv(int h, int max) "
            "{ (void)h; (void)max; return std::string(); }\n";
    if (needSender) {
        wasm += "[[maybe_unused]] static std::string __nexa_udp_sender(int h) "
                "{ (void)h; return std::string(); }\n";
        wasm += "[[maybe_unused]] static int __nexa_udp_sender_port(int h) { (void)h; return 0; }\n";
    }
    wasm += "[[maybe_unused]] static int __nexa_udp_close(int h) { (void)h; return 0; }\n";

    std::string real;

    if (needSender) {
        real += R"NEXA_UDP_PEER(
#include <map>
#include <cstdlib>
// Who the last packet on this handle came from. One entry per handle, per
// thread, written by udp.recv and read by udp.sender/udp.sender_port.
struct __nexa_udp_peer {
  std::string host;
  int port;
};
[[maybe_unused]] static std::map<int, __nexa_udp_peer>& __nexa_udp_peers() {
  static thread_local std::map<int, __nexa_udp_peer> m;
  return m;
}
)NEXA_UDP_PEER";
    }

    real += R"NEXA_UDP_OPEN(
// udp.open: a bound port and nothing else -- there is no listen and no accept
// on a datagram socket, so this is both halves at once. Port 0 asks the OS for
// a free one, and udp.port says which it picked. INADDR_ANY, like tcp.listen:
// a packet has to be able to arrive from off the machine for this to be UDP
// and not a pipe.
[[maybe_unused]] static int __nexa_udp_open(int port) {
  if (!__nexa_sock_start()) return 0;
  __nexa_sock_t fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == __NEXA_SOCK_BAD) return 0;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, (socklen_t)sizeof(one));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, (struct sockaddr*)&addr, (socklen_t)sizeof(addr)) != 0) {
    __nexa_sock_shut(fd);
    return 0;
  }
  return __nexa_sock_handle(fd);
}
// udp.port: the port this socket is on, which is how a program that asked for
// 0 tells its peer where to write.
[[maybe_unused]] static int __nexa_udp_port(int h) {
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  socklen_t len = (socklen_t)sizeof(a);
  if (getsockname((__nexa_sock_t)h, (struct sockaddr*)&a, &len) != 0) return 0;
  return (int)ntohs(a.sin_port);
}
// udp.send: one datagram, one call. A UDP write is all-or-nothing -- there is
// no short write to resume, so the count handed back is the whole string or 0.
// The destination is resolved per send, which is what lets a server answer a
// different peer each time round its loop without opening anything new.
[[maybe_unused]] static int __nexa_udp_send(int h, const std::string& host, int port,
                                            const std::string& data) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  struct addrinfo* res = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return 0;
  size_t n = data.size();
  int chunk = n > 0x7fffffff ? 0x7fffffff : (int)n;
  int k = (int)sendto((__nexa_sock_t)h, data.data(), chunk, __NEXA_SOCK_SEND_FLAGS,
                      res->ai_addr, (socklen_t)res->ai_addrlen);
  freeaddrinfo(res);
  return k < 0 ? 0 : k;
}
)NEXA_UDP_OPEN";

    // The read, with or without the source of it. Both shapes are one
    // recvfrom; the difference is whether the address it fills in is kept.
    if (needSender) {
        real += R"NEXA_UDP_RP(
// udp.recv: one datagram, whole. UDP is not a stream, so what arrives is what
// was sent -- up to `max` bytes, binary-safe the way os.load is. "" is an
// error or an empty datagram; there is no third answer, and no partial one.
[[maybe_unused]] static std::string __nexa_udp_recv(int h, int max) {
  std::string out;
  out.resize((size_t)max);
  struct sockaddr_storage from;
  memset(&from, 0, sizeof(from));
  socklen_t fromLen = (socklen_t)sizeof(from);
  int n = (int)recvfrom((__nexa_sock_t)h, &out[0], max, 0, (struct sockaddr*)&from, &fromLen);
  if (n < 0) return std::string();
  char hostBuf[NI_MAXHOST];
  char portBuf[NI_MAXSERV];
  hostBuf[0] = '\0';
  portBuf[0] = '\0';
  __nexa_udp_peer& p = __nexa_udp_peers()[h];
  if (getnameinfo((struct sockaddr*)&from, fromLen, hostBuf, (socklen_t)sizeof(hostBuf),
                  portBuf, (socklen_t)sizeof(portBuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    p.host = hostBuf;
    p.port = std::atoi(portBuf);
  } else {
    p.host.clear();
    p.port = 0;
  }
  out.resize((size_t)n);
  return out;
}
// udp.sender / udp.sender_port: where the last packet this thread read on this
// handle came from. Before the first read, "" and 0.
[[maybe_unused]] static std::string __nexa_udp_sender(int h) {
  auto& m = __nexa_udp_peers();
  auto it = m.find(h);
  return it == m.end() ? std::string() : it->second.host;
}
[[maybe_unused]] static int __nexa_udp_sender_port(int h) {
  auto& m = __nexa_udp_peers();
  auto it = m.find(h);
  return it == m.end() ? 0 : it->second.port;
}
)NEXA_UDP_RP";
    } else {
        real += R"NEXA_UDP_RECV(
// udp.recv: one datagram, whole. UDP is not a stream, so what arrives is what
// was sent -- up to `max` bytes, binary-safe the way os.load is. "" is an
// error or an empty datagram; there is no third answer, and no partial one.
// This program never asks who sent it, so the address is read and dropped.
[[maybe_unused]] static std::string __nexa_udp_recv(int h, int max) {
  std::string out;
  out.resize((size_t)max);
  int n = (int)recvfrom((__nexa_sock_t)h, &out[0], max, 0, nullptr, nullptr);
  if (n < 0) return std::string();
  out.resize((size_t)n);
  return out;
}
)NEXA_UDP_RECV";
    }

    real += R"NEXA_UDP_CLOSE(
[[maybe_unused]] static int __nexa_udp_close(int h) {
  __nexa_sock_shut((__nexa_sock_t)h);
  return 1;
}
)NEXA_UDP_CLOSE";

    out += "#if defined(__EMSCRIPTEN__) || defined(__wasi__)\n";
    out += wasm;
    out += "#else\n";
    out += real;
    out += "#endif\n";
    return out;
}

}  // namespace nexa
