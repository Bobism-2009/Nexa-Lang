#!/usr/bin/env python3
"""Non-Nexa peer for the loopback layer of Tests/tcp_cases.sh (BOB-44).

Tests/tcp_server_test.nxa already puts std/tcp on both ends of a connection,
which proves the two halves agree with each other. This script is here to
prove they agree with someone else: it is plain Python sockets, so a Nexa
program talking to it is talking real TCP and not to its own conventions.

It binds 127.0.0.1 on a free port, writes the port to argv[1] (so no layer of
the suite ever picks a port and hopes it was free), serves exactly one
connection, and exits.

The exchange, in order:

  1. The peer reads a "PING\\n" line and answers "PONG\\n".
     A line at a time, which is what a text protocol over TCP looks like.

  2. The peer reads a "SIZE <n>\\n" line, then exactly n more bytes, then
     answers "GOT <n>\\n". n is large enough that it cannot be one send and
     cannot be one recv, which is what pins tcp.send's write-until-done loop.

  3. The peer reads a "BLOB\\n" line, sends 7 bytes with a NUL in the middle,
     and hangs up. That is the binary-safety half: a Nexa program must see 7
     bytes, not the 3 before the NUL, and must see the hang-up as an empty
     read.

Every answer is asked for, so nothing the peer writes can arrive alongside
anything else it writes. That is not how a real protocol frames itself -- it is
how this test stays race-free, with one send in flight at a time, so a failure
here is std/tcp moving and never the scheduler.
"""

import socket
import sys


BLOB = b"bin\x00ary"          # 7 bytes, NUL at index 3


def read_line(conn, buf):
    """Read until buf holds a newline; return (line_without_newline, rest)."""
    while b"\n" not in buf:
        chunk = conn.recv(4096)
        if not chunk:
            return None, buf
        buf += chunk
    line, _, rest = buf.partition(b"\n")
    return line.rstrip(b"\r"), rest


def main(argv):
    if len(argv) < 2:
        sys.stderr.write("usage: tcp_peer.py <portfile>\n")
        return 2

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(1)
    port = srv.getsockname()[1]

    # Written whole and renamed into place: a reader polling for the file never
    # sees half a number.
    tmp = argv[1] + ".tmp"
    with open(tmp, "w") as f:
        f.write(str(port))
    import os
    os.replace(tmp, argv[1])

    srv.settimeout(30)
    conn, _ = srv.accept()
    srv.close()
    conn.settimeout(30)

    buf = b""
    line, buf = read_line(conn, buf)
    if line != b"PING":
        conn.sendall(b"ERR want PING, got %r\n" % (line,))
        conn.close()
        return 1
    conn.sendall(b"PONG\n")

    line, buf = read_line(conn, buf)
    if line is None or not line.startswith(b"SIZE "):
        conn.sendall(b"ERR want SIZE, got %r\n" % (line,))
        conn.close()
        return 1
    # Count exactly `want` bytes and keep whatever ran past them: the payload is
    # a stream, so the read that finishes it can already hold the next line.
    want = int(line[5:])
    take = min(len(buf), want)
    got = take
    buf = buf[take:]
    while got < want:
        chunk = conn.recv(65536)
        if not chunk:
            break
        take = min(len(chunk), want - got)
        got += take
        buf += chunk[take:]
    conn.sendall(b"GOT %d\n" % got)

    line, buf = read_line(conn, buf)
    if line != b"BLOB":
        conn.close()
        return 1
    conn.sendall(BLOB)
    conn.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
