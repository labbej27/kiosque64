#!/usr/bin/env python3
"""Lance un .prg dans VICE, attend, fait une capture d'écran.
vice.py PROG.prg SORTIE.png [secondes] [--nowarp]"""
import os, socket, subprocess, sys, time
prg, png = map(os.path.abspath, sys.argv[1:3])
wait = float(sys.argv[3]) if len(sys.argv) > 3 and not sys.argv[3].startswith("--") else 3
port = int(os.environ.get("VICE_PORT", "6512"))
args = ["x64sc", "-console", "-default", "-sounddev", "dummy",
        *([] if "--nowarp" in sys.argv else ["-warp"]),
        "-remotemonitor", "-remotemonitoraddress", f"ip4://127.0.0.1:{port}",
        *(["-cartcrt", prg] if prg.endswith(".crt") else ["-autostartprgmode", "1", "-autostart", prg])]
vice = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def mon(cmd):
    for _ in range(50):
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=10) as s:
                s.sendall((cmd + "\n").encode()); time.sleep(0.3); s.settimeout(0.5); r = b""
                try:
                    while True:
                        d = s.recv(65536)
                        if not d: break
                        r += d
                except OSError: pass
                return r.decode("latin1")
        except OSError:
            time.sleep(0.2)
keys = [a[7:] for a in sys.argv if a.startswith("--keys=")]
try:
    time.sleep(wait)
    for seq in keys:                 # --keys=36,0d : codes PETSCII en hexa
        codes = [int(x, 16) for x in seq.split(",")]
        for i in range(0, len(codes), 8):
            part = codes[i:i + 8]
            mon("> 0277 " + " ".join(f"{c:02x}" for c in part))
            mon(f"> 00c6 {len(part):02x}")
            time.sleep(1)
        time.sleep(1)
    if os.path.exists(png): os.remove(png)
    mon(f'screenshot "{png}" 2')
    print(mon("m 3ff0 3ff2"))
    for _ in range(20):
        if os.path.exists(png): break
        time.sleep(0.2)
    mon("x"); time.sleep(0.3)
finally:
    vice.kill()
