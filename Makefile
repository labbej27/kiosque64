# Kiosque 64 : terminal Minitel et BBS pour le C64 Ultimate.
# make -> build/kiosque-fr.crt et build/kiosque-en.crt (et les .prg)
LANGS := fr en
SRC   := src/main.c src/vdt.c src/net.c src/uci.c src/term.c src/gfx.s src/font.s
HDR   := src/charmap.h src/vdt.h src/net.h src/uci.h src/screens.h
CFLAGS := -t c64 -O -Or -Cl -Wl -D,__HIMEM__=0xC000 -Wl -D,__STACKSIZE__=0x400

.PHONY: all clean
PAGES  = $(shell python3 -I -c "import os,sys;print((os.path.getsize(sys.argv[1])-2+255)//256)" $<)

all: $(foreach l,$(LANGS),build/kiosque-$(l).crt)

# Édition modem (C64 d'origine, modem WiFi sur le port utilisateur) :
# make modem -> build/kiosque-modem-fr.crt et build/kiosque-modem-en.crt
SRC_MODEM := src/main.c src/vdt.c src/net.c src/modem.c src/term.c src/gfx.s src/font.s src/rs232.s
HDR_MODEM := $(HDR) src/modem.h
.PHONY: modem
modem: $(foreach l,$(LANGS),build/kiosque-modem-$(l).crt)

build/kiosque-modem-fr.prg: $(SRC_MODEM) $(HDR_MODEM)
	@mkdir -p build/modem-fr
	cl65 $(CFLAGS) -DMODEM -o $@ -m build/kiosque-modem-fr.map -Ln build/kiosque-modem-fr.lbl --obj-path build/modem-fr $(SRC_MODEM)

build/kiosque-modem-en.prg: $(SRC_MODEM) $(HDR_MODEM)
	@mkdir -p build/modem-en
	cl65 $(CFLAGS) -DMODEM -DLANG_EN -o $@ -m build/kiosque-modem-en.map -Ln build/kiosque-modem-en.lbl --obj-path build/modem-en $(SRC_MODEM)

# Cartouche EasyFlash (celle de Plume) : pas de Command Interface à ménager,
# et ce format fonctionne sur Kung Fu Flash.
build/kiosque-modem-%.crt: build/kiosque-modem-%.prg crt-ef.s crt-ef.cfg tools/mkcrt-ef.py
	ca65 -D PRG_PAGES=$(PAGES) -o build/crt-modem-$*.o crt-ef.s
	ld65 -C crt-ef.cfg -o build/crt-modem-$*.bin build/crt-modem-$*.o
	python3 -I tools/mkcrt-ef.py build/crt-modem-$*.bin $< $@ "KIOSQUE MODEM $*"

src/font.s: fonts/unscii-8.hex tools/mkfont.py
	python3 -I tools/mkfont.py $< $@

src/screens.h: tools/mkscreens.py
	python3 -I tools/mkscreens.py $@

build/kiosque-fr.prg: $(SRC) $(HDR)
	@mkdir -p build/fr
	cl65 $(CFLAGS) -o $@ -m build/kiosque-fr.map -Ln build/kiosque-fr.lbl --obj-path build/fr $(SRC)

build/kiosque-en.prg: $(SRC) $(HDR)
	@mkdir -p build/en
	cl65 $(CFLAGS) -DLANG_EN -o $@ -m build/kiosque-en.map -Ln build/kiosque-en.lbl --obj-path build/en $(SRC)

build/kiosque-%.crt: build/kiosque-%.prg crt.s crt.cfg tools/mkcrt.py
	ca65 -D PRG_PAGES=$(PAGES) -o build/crt-$*.o crt.s
	ld65 -C crt.cfg -o build/crt-$*.bin build/crt-$*.o
	python3 -I tools/mkcrt.py build/crt-$*.bin $< $@ "KIOSQUE 64 $*"

clean:
	rm -rf build
