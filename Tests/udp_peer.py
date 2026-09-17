#!/usr/bin/env python3
"""Non-Nexa peer for the loopback layer of Tests/network_cases.sh (BOB-45).

Tests/udp_server_test.nxa already puts udp.* on both ends of an exchange,
which proves the two halves agree with each other. This script is here to
prove they agree with someone else: it is plain Python sockets, so a Nexa
program talking to it is talking real UDP and not to its own conventions.

It binds 127.0.0.1 on a free port, writes the port to argv[1] (so no layer of
the suite ever picks a port and hopes it was free), answers exactly four
datagrams, and exits.

The exchange, in order:

  1. "PING" in, "PONG" out.
     The smallest thing a datagram socket can be asked to do.

  2. A 40000-byte payload in, "GOT 40000" out. One datagram, not a stream:
     the whole thing arrives in one recvfrom or the count comes back wrong,
     which is the difference between udp.recv and tcp.recv in one number.

  3. Seven bytes with a NUL in the middle in, "GOT 7" out. The binary-safety
     half -- a peer that treated the packet as C text would say 3.

  4. An empty datagram in, "GOT 0" out. A zero-length packet is a real packet
     on the wire, and this is the one exchange that says so from the outside:
     "" out of udp.recv is not proof of anything by itself, since a failed
     read says "" too.

Every answer is asked for and nothing is sent unprompted, so there is one
datagram in flight at a time. Loopback does not drop a packet that nothing is
racing, so a failure here is udp.* moving and never the network.
"""

import os
import socket
import sys


def main(argv):
    if len(argv) < 2:
        sys.stderr.write("usage: udp_peer.py <portfile>\n")
        return 2

    srv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    port = srv.getsockname()[1]

    # Written whole and renamed into place: a reader polling for the file never
    # sees half a number.
    tmp = argv[1] + ".tmp"
    with open(tmp, "w") as f:
        f.write(str(port))
    os.replace(tmp, argv[1])

    srv.settimeout(30)

    data, addr = srv.recvfrom(65536)
    if data != b"PING":
        srv.sendto(b"ERR want PING, got %r" % (data,), addr)
        return 1
    srv.sendto(b"PONG", addr)

    # Three sizes, answered the same way: what came back is a count, so the
    # test reads as one number per packet rather than one shape per packet.
    for _ in range(3):
        data, addr = srv.recvfrom(65536)
        srv.sendto(b"GOT %d" % len(data), addr)

    srv.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
