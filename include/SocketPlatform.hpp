#pragma once

#include <string>

namespace nexa {

// The part of a socket that is the same whether the program is speaking TCP or
// UDP: which headers exist, what a socket is called, how it is closed, and the
// one call Winsock wants before any of it.
//
// std/network's tcp.* and udp.* both stand on this, and a program that calls
// both gets it once -- which is the whole reason it is here and not copied into
// each runtime. It is emitted only for the native builds; the wasm slice has no
// socket at all, so each protocol's stubs stand alone over there.
inline std::string socketPlatformCpp() {
    std::string out;
    out += "#if !defined(__EMSCRIPTEN__) && !defined(__wasi__)\n";
    out += R"NEXA_SOCK(
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET __nexa_sock_t;
#define __NEXA_SOCK_BAD INVALID_SOCKET
#define __NEXA_SOCK_SEND_FLAGS 0
[[maybe_unused]] static void __nexa_sock_shut(__nexa_sock_t s) { closesocket(s); }
// Winsock wants to be woken before its first call; every other platform does
// not have an equivalent, so this is the one thing the split is really for.
[[maybe_unused]] static int __nexa_sock_start() {
  static int started = 0;
  if (started) return 1;
  WSADATA w;
  if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 0;
  started = 1;
  return 1;
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
typedef int __nexa_sock_t;
#define __NEXA_SOCK_BAD (-1)
// A peer that hangs up mid-write must not take the process with it: Linux says
// so per-send, macOS per-socket (SO_NOSIGPIPE, below).
#ifdef MSG_NOSIGNAL
#define __NEXA_SOCK_SEND_FLAGS MSG_NOSIGNAL
#else
#define __NEXA_SOCK_SEND_FLAGS 0
#endif
[[maybe_unused]] static void __nexa_sock_shut(__nexa_sock_t s) { close(s); }
[[maybe_unused]] static int __nexa_sock_start() { return 1; }
#endif
[[maybe_unused]] static void __nexa_sock_nosigpipe(__nexa_sock_t s) {
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&one, (socklen_t)sizeof(one));
#else
  (void)s;
#endif
}
// The socket as the OS numbers it, with 0 kept back to mean "no socket". On
// POSIX a fresh descriptor can land on 0 when the program has closed its stdin
// -- a daemon does exactly that -- so the socket is moved up out of the way
// rather than handed back as the one value that reads as failure.
[[maybe_unused]] static int __nexa_sock_handle(__nexa_sock_t s) {
  if (s == __NEXA_SOCK_BAD) return 0;
#ifndef _WIN32
  if (s == 0) {
    int moved = dup(s);
    close(s);
    return moved < 0 ? 0 : moved;
  }
#endif
  return (int)s;
}
)NEXA_SOCK";
    out += "#endif\n";
    return out;
}

}  // namespace nexa
