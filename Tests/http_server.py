#!/usr/bin/env python3
"""Loopback server for the http_cases.sh loopback layer.

Speaks the handful of responses a client has to get right and nothing else:
chunked framing, a two-hop redirect with a relative Location, a body longer
than its own Content-Length, a 404, and an echo of the method, body and a
couple of request headers. Binds 127.0.0.1 on a free port and writes the port
to argv[1] so the caller does not have to guess one.

Not a general-purpose server -- it reads one request per connection, answers,
and closes.
"""

import socket
import sys
import threading


def handle(conn):
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            break
        data += chunk
    if not data:
        conn.close()
        return
    head, _, rest = data.partition(b"\r\n\r\n")
    lines = head.decode("latin1").split("\r\n")
    method, path = lines[0].split(" ")[:2]
    hdrs = {}
    for line in lines[1:]:
        if ":" in line:
            k, v = line.split(":", 1)
            hdrs[k.strip().lower()] = v.strip()
    want = int(hdrs.get("content-length", "0"))
    body = rest
    while len(body) < want:
        more = conn.recv(4096)
        if not more:
            break
        body += more
    body = body[:want]

    def send(status, reason, headers, payload=b""):
        out = "HTTP/1.1 %d %s\r\n" % (status, reason)
        for k, v in headers:
            out += "%s: %s\r\n" % (k, v)
        out += "Connection: close\r\n\r\n"
        conn.sendall(out.encode("latin1") + payload)

    def sized(payload, extra=()):
        send(200, "OK", [("Content-Length", str(len(payload)))] + list(extra), payload)

    if path == "/chunked":
        # Three chunks, the middle one carrying a chunk extension.
        sys.stdout.flush()
        send(200, "OK", [("Transfer-Encoding", "chunked")],
             b"5\r\nhello\r\n2;x=y\r\n, \r\n5\r\nworld\r\n0\r\n\r\n")
    elif path == "/redir":
        send(302, "Found", [("Location", "/deep/one")])
    elif path == "/deep/one":
        send(302, "Found", [("Location", "two")])       # relative, against /deep/
    elif path == "/deep/two":
        sized(("%s|%s" % (method, body.decode("latin1"))).encode())
    elif path == "/loop":
        send(302, "Found", [("Location", "/loop")])
    elif path == "/echo":
        sized(("%s|%s|%s|%s" % (method, body.decode("latin1"),
                                hdrs.get("x-token", "-"),
                                hdrs.get("content-type", "-"))).encode(),
              [("X-Echo", "1")])
    elif path == "/404":
        send(404, "Not Found", [("Content-Length", "7")], b"missing")
    elif path == "/extra":
        # More bytes on the wire than Content-Length claims.
        send(200, "OK", [("Content-Length", "2")], b"okTRAILING-GARBAGE")
    else:
        sized(b"hi")
    conn.close()


def main():
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(16)
    with open(sys.argv[1], "w") as f:
        f.write(str(srv.getsockname()[1]))
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=handle, args=(conn,), daemon=True).start()


main()
