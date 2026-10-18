#!/usr/bin/env python3
"""Transparent TCP proxy that frames and logs the Minima P2P exchange.

Usage: tcp_proxy.py <listen_port> <remote_host:port> <logfile>

Point a node at 127.0.0.1:<listen_port> to see exactly what the remote peer
sends back (framed as [4-byte big-endian length][1-byte type + body]).
"""
import socket
import sys
import threading
import time

LISTEN = int(sys.argv[1])
RHOST, RPORT = sys.argv[2].rsplit(":", 1)
RPORT = int(RPORT)
LOG = sys.argv[3]

TYPE = {0: "GREETING", 1: "IBD", 2: "TXPOWID", 3: "TXPOWREQ", 4: "TXPOW",
        5: "GENMSG", 6: "PULSE", 7: "P2P", 8: "PING", 9: "MAXIMA_CTRL",
        10: "MAXIMA_TXPOW", 11: "SINGLE_PING", 12: "SINGLE_PONG", 13: "IBD_REQ",
        14: "IBD_RESP", 15: "ARCHIVE_REQ", 16: "ARCHIVE_DATA", 17: "ARCHIVE_SREQ",
        18: "TXBLOCKID", 19: "TXBLOCKREQ", 20: "TXBLOCK", 21: "TXBLOCKMINE",
        22: "MEGAMMR_REQ", 23: "MEGAMMR_RESP"}


def now():
    return time.strftime("%H:%M:%S") + (".%03d" % int((time.time() % 1) * 1000))


def log(*a):
    line = now() + " " + " ".join(str(x) for x in a)
    print(line, flush=True)
    with open(LOG, "a") as f:
        f.write(line + "\n")


def pump(src, dst, tag):
    buf = b""
    while True:
        try:
            data = src.recv(65536)
        except OSError:
            break
        if not data:
            break
        try:
            dst.sendall(data)
        except OSError:
            break
        buf += data
        while len(buf) >= 4:
            l = int.from_bytes(buf[:4], "big")
            if l < 0 or l > 300 * 1024 * 1024:
                log(tag, "BADLEN", l)
                buf = b""
                break
            if len(buf) < 4 + l:
                break
            payload = buf[4:4 + l]
            buf = buf[4 + l:]
            t = payload[0] if payload else -1
            log(tag, "FRAME len=%d type=%d(%s)" % (l, t, TYPE.get(t, "?")))
    try:
        dst.shutdown(socket.SHUT_WR)
    except OSError:
        pass


def main():
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", LISTEN))
    srv.listen(5)
    log("PROXY listening", LISTEN, "->", RHOST, RPORT)
    while True:
        c, _ = srv.accept()
        r = socket.socket()
        r.connect((RHOST, RPORT))
        log("PROXY connected to", RHOST, RPORT)
        threading.Thread(target=pump, args=(c, r, "C->P"), daemon=True).start()
        threading.Thread(target=pump, args=(r, c, "P->C"), daemon=True).start()


if __name__ == "__main__":
    main()
