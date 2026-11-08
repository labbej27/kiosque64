#!/bin/sh
# mkpage.sh fichier.vdt -> page.s
n=$(wc -c < "$1" | tr -d ' ')
printf '\t.export _page, _page_len\n\t.rodata\n_page_len: .word %s\n_page: .incbin "%s"\n' "$n" "$1" > page.s
