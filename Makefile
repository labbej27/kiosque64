# Kiosque 64 : terminal Minitel et BBS pour le C64 Ultimate.
# make -> build/kiosque-fr.crt et build/kiosque-en.crt (et les .prg)
LANGS := fr en
SRC   := src/main.c src/vdt.c src/net.c src/uci.c src/term.c src/gfx.s src/font.s
HDR   := src/charmap.h src/vdt.h src/net.h src/uci.h src/screens.h
CFLAGS := -t c64 -O -Or -Cl -Wl -D,__HIMEM__=0xC000 -Wl -D,__STACKSIZE__=0x400

.PHONY: all clean
PAGES  = $(shell python3 -I -c "import os,sys;print((os.path.getsize(sys.argv[1])-2+255)//256)" $<)

all: $(foreach l,$(LANGS),build/kiosque-$(l).crt)

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
