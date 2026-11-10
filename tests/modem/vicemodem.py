#!/usr/bin/env python3
"""Essai de Kiosque 64 modem dans VICE, avec le faux modem fakemodem.py.

vicemodem.py CARTOUCHE.crt|PROGRAMME.prg DOSSIER_SORTIE [baud] [if=user|up9600|sl] etape...
Etapes : wait:S  keys:4d,34 (codes PETSCII en hexa)  shot:NOM
"""
import os, socket, subprocess, sys, time

crt, out = map(os.path.abspath, sys.argv[1:3])
args = sys.argv[3:]
baud = args.pop(0) if args and args[0].isdigit() else "2400"
iface = args.pop(0)[3:] if args and args[0].startswith("if=") else "user"
link = {"user": ["-userportdevice", "2", "-rsuserbaud", baud, "-rsuserdev", "0"],
        "up9600": ["-userportdevice", "2", "-rsuserup9600", "-rsuserbaud", baud, "-rsuserdev", "0"],
        "sl": ["-userportdevice", "0", "-acia1", "-acia1base", "0xDE00", "-acia1mode", "1",
               "-acia1irq", "1", "-myaciadev", "0"]}[iface]
start = ["-cartcrt", crt] if crt.endswith(".crt") else ["-autostartprgmode", "1", "-autostart", crt]
here = os.path.dirname(os.path.abspath(__file__))
MON, MDM = 6518, 25232
d64 = os.path.join(out, "config.d64")
if not os.path.exists(d64):
    subprocess.run(["c1541", "-format", "kiosque,km", "d64", d64], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
modem = subprocess.Popen([sys.executable, "-I", os.path.join(here, "fakemodem.py"), str(MDM)] + (["-v"] if os.environ.get("MODEM_DEBUG") else []),
                         stderr=open(os.path.join(out, "modem.log"), "w"))
time.sleep(0.5)
vice = subprocess.Popen(["x64sc", "-console", "-default", "-sounddev", "dummy",
                         *link, "-rsdev1", f"127.0.0.1:{MDM}", "-rsdev1baud", baud,
                         "-8", d64, "-remotemonitor", "-remotemonitoraddress", f"ip4://127.0.0.1:{MON}",
                         *start], stdout=open(os.path.join(out, "vice.log"), "w"), stderr=subprocess.STDOUT)


def mon(cmd):
    for _ in range(100):
        try:
            with socket.create_connection(("127.0.0.1", MON), timeout=10) as s:
                s.sendall((cmd + "\n").encode())
                time.sleep(0.3)
                s.settimeout(0.5)
                r = b""
                try:
                    while True:
                        d = s.recv(65536)
                        if not d:
                            break
                        r += d
                except OSError:
                    pass
                s.sendall(b"x\n")
                return r.decode("latin1")
        except OSError:
            time.sleep(0.2)


try:
    for st in args:
        k, _, a = st.partition(":")
        if k == "wait":
            time.sleep(float(a))
        elif k == "keys":
            codes = [int(x, 16) for x in a.split(",")]
            mon("> 0277 " + " ".join(f"{c:02x}" for c in codes))
            mon(f"> 00c6 {len(codes):02x}")
            time.sleep(0.5)
        elif k == "mem":
            print(mon("m " + a.replace(",", " ")), flush=True)
        elif k == "shot":
            p = os.path.join(out, a)
            if os.path.exists(p):
                os.remove(p)
            mon(f'screenshot "{p}" 2')
            time.sleep(1)
finally:
    try:
        with socket.create_connection(("127.0.0.1", MON), timeout=5) as s:
            s.sendall(b"quit\n")       # VICE réécrit l'image disque en quittant
            time.sleep(2)
    except OSError:
        pass
    vice.kill()
    modem.kill()
