#!/usr/bin/env python3
"""Assemble la cartouche Magic Desk de Kiosque 64 :
mkcrt.py banque0.bin programme.prg sortie.crt NOM"""
import struct, sys

BANK = 0x2000
boot, prg_path, out, name = sys.argv[1:5]
rom = open(boot, "rb").read()
if len(rom) != BANK:
    sys.exit(f"banque 0 : taille inattendue ({len(rom)})")
prg = open(prg_path, "rb").read()
if prg[:2] != b"\x01\x08":
    sys.exit("le programme doit se charger en $0801")
body = prg[2:]
if 0x0801 + len(body) > 0xC000:
    sys.exit("programme trop grand")

# Type 19 (Magic Desk), EXROM actif (0), GAME inactif (1) : mode 8 Ko.
header = (b"C64 CARTRIDGE   " + struct.pack(">IHHBB", 0x40, 0x0100, 19, 0, 1) + bytes(6)
          + name.upper().encode()[:32].ljust(32, b"\0"))


def chip(bank, data):
    return b"CHIP" + struct.pack(">IHHHH", 0x10 + BANK, 0, bank, 0x8000, BANK) + data.ljust(BANK, b"\xFF")


crt = bytearray(header) + chip(0, rom)
for n, i in enumerate(range(0, len(body), BANK)):
    crt += chip(1 + n, body[i:i + BANK])
open(out, "wb").write(crt)
print(f"{out} : {1 + (len(body) + BANK - 1) // BANK} banques, programme de {len(body)} octets")
