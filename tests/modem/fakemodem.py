#!/usr/bin/env python3
"""Faux modem WiFi pour tester Kiosque 64 modem dans VICE.

VICE relie son RS-232 du port utilisateur à ce serveur (-rsdev1 127.0.0.1:25232).
Le faux modem répond comme les firmwares ESP8266 courants : AT, ATDThote:port
(CONNECT / NO CARRIER), +++ entouré d'une seconde de silence, ATH, ATO.
Tout le trafic est journalisé sur la sortie d'erreur.

fakemodem.py [port]
"""
import select, socket, sys, time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 25232
GUARD = 1.0
DEBUG = "-v" in sys.argv


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, file=sys.stderr, flush=True)


srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", PORT))
srv.listen(1)
log("en attente de VICE sur le port", PORT)

remote = None
cmd_mode = True
line = b""
last_rx = time.time()
plus = 0
plus_t = 0.0


def out(s):
    c64.sendall(s.encode() if isinstance(s, str) else s)


def command(cmd):
    global remote, cmd_mode
    up = cmd.strip().upper()
    log("commande", repr(up))
    if not up:
        return
    if up in ("AT", "ATZ", "ATE0", "ATE1"):
        out("OK\r\n")
    elif up == "ATI":
        out("\r\nC64 WIFI MODEM ESP8266\r\nOK\r\n")
    elif up.startswith("ATD"):
        target = up[4:] if up.startswith(("ATDT", "ATDP")) else up[3:]
        host, _, port = target.strip().strip('"').rpartition(":")
        out(f"CONNECTING {target}\r\n")
        try:
            remote = socket.create_connection((host.lower(), int(port or 23)), timeout=10)
            remote.setblocking(False)
            out("CONNECT\r\n")
            cmd_mode = False
            log("connecté à", host, port)
        except OSError as e:
            log("échec", e)
            remote = None
            out("NO CARRIER\r\n")
    elif up == "ATO" and remote:
        cmd_mode = False
        out("CONNECT\r\n")
    elif up in ("ATH", "ATH0"):
        if remote:
            remote.close()
            remote = None
        out("OK\r\n")
    else:
        out("ERROR\r\n")


while True:   # VICE se reconnecte quand le programme réinitialise l'ACIA
    c64, _ = srv.accept()
    log("VICE connecté")
    cmd_mode, line, plus = True, b"", 0
    sent = recv = 0
    while True:
        socks = [c64] + ([remote] if remote and not cmd_mode else [])
        r, _, _ = select.select(socks, [], [], 0.05)
        now = time.time()
        if c64 in r:
            data = c64.recv(4096)
            if not data:
                log("VICE déconnecté")
                break
            for b in data:
                if cmd_mode:
                    if b in (13, 10):
                        command(line.decode("latin1"))
                        line = b""
                    else:
                        line += bytes([b])
                        out(bytes([b]))                 # écho
                else:
                    if b == 0x2B and plus < 3 and (plus or now - last_rx >= GUARD):
                        plus += 1
                        plus_t = now
                    else:
                        if plus:
                            remote.sendall(b"+" * plus)
                            plus = 0
                        remote.sendall(bytes([b])); log("->", hex(b)) if DEBUG else None
                        sent += 1
                    last_rx = now
        if not cmd_mode and plus == 3 and now - plus_t >= GUARD:
            plus = 0
            cmd_mode = True
            log("+++ : mode commande")
            out("\r\nOK\r\n")
        if not cmd_mode and 0 < plus < 3 and now - plus_t >= GUARD:
            remote.sendall(b"+" * plus)
            plus = 0
        if remote and not cmd_mode and remote in r:
            try:
                data = remote.recv(4096)
            except OSError:
                data = b""
            if data:
                recv += len(data)
                out(data)
            else:
                log(f"serveur fermé ({recv} octets reçus, {sent} envoyés)")
                remote.close()
                remote = None
                cmd_mode = True
                out("\r\nNO CARRIER\r\n")
