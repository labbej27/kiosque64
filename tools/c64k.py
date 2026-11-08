#!/usr/bin/env python3
"""Pilote Kiosque 64 sur un C64 Ultimate par son API REST.

c64k.py run fichier.prg           lance le programme
c64k.py shot fichier.png          capture de l'écran bitmap en couleurs
c64k.py keys "texte"              frappes (\\xNN accepté) : ASCII -> clavier C64
c64k.py raw 85 0d                 codes PETSCII bruts (F1, RETURN...)
"""
import os, struct, sys, time, urllib.request, zlib

IP = os.environ.get("C64_IP", "192.168.1.128")
PAL = [(0, 0, 0), (255, 255, 255), (136, 57, 50), (103, 182, 189), (139, 63, 150), (85, 160, 73),
       (64, 49, 141), (191, 206, 114), (139, 84, 41), (87, 66, 0), (184, 105, 98), (80, 80, 80),
       (120, 120, 120), (148, 224, 137), (120, 105, 196), (159, 159, 159)]


def api(path, method="GET", data=None):
    req = urllib.request.Request(f"http://{IP}{path}", method=method, data=data)
    return urllib.request.urlopen(req, timeout=15).read()


def read(addr, n):
    return api(f"/v1/machine:readmem?address={addr:04X}&length={n}")


def write(addr, data):
    api(f"/v1/machine:writemem?address={addr:04X}&data={bytes(data).hex()}", "PUT")


def shot(path):
    write(0x02A7, [2])                 # Kiosque se fige, ROM masquée
    for _ in range(100):
        if read(0x02A7, 1)[0] == 3:
            break
        time.sleep(0.05)
    else:
        print("Kiosque ne répond pas au crochet de test", file=sys.stderr)
    try:
        bitmap = read(0xE000, 8000)
        colors = read(0xCC00, 1000)
    finally:
        write(0x02A7, [0])
    rows = bytearray()
    for y in range(200):
        rows.append(0)
        for col in range(40):
            b = bitmap[col * 8 + (y // 8) * 320 + y % 8]
            c = colors[(y // 8) * 40 + col]
            for x in range(8):
                rows += bytes(PAL[c >> 4] if b >> (7 - x) & 1 else PAL[c & 15])
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 320, 200, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(rows))) + chunk(b"IEND", b""))


def feed(data):
    for i in range(0, len(data), 8):
        while read(0xC6, 1)[0]:
            time.sleep(0.05)
        part = data[i:i + 8]
        write(0x0277, part)
        write(0xC6, [len(part)])
        time.sleep(0.15)


def keys(text):
    raw = text.encode("latin-1").decode("unicode_escape").encode("latin-1")
    feed(bytes(c - 0x20 if 0x61 <= c <= 0x7A else c + 0x80 if 0x41 <= c <= 0x5A else c for c in raw))


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "run":
        api("/v1/runners:run_prg", "POST", open(sys.argv[2], "rb").read())
    elif cmd == "shot":
        shot(sys.argv[2])
    elif cmd == "keys":
        keys(sys.argv[2])
    elif cmd == "raw":
        feed(bytes(int(x, 16) for x in sys.argv[2:]))


def text_screen():
    """Écran texte ($0400) en ASCII approximatif, pour le terminal BBS."""
    d = read(0x0400, 1000)
    lines = []
    for r in range(25):
        s = ""
        for c in d[r * 40:r * 40 + 40]:
            c &= 0x7F
            s += chr(c + 0x60) if 1 <= c <= 26 else chr(c) if 32 <= c < 64 else "@" if c == 0 \
                else chr(c) if 65 <= c <= 90 else "#"
        lines.append(s)
    return "\n".join(lines)


if __name__ == "__main__" and sys.argv[1] == "text":
    print(text_screen())
