; Cartouche Magic Desk de Kiosque 64 (8 Ko par banque en $8000).
; Ce format n'utilise que le registre $DE00 : la zone $DF00-$DFFF reste libre
; pour le Command Interface du C64 Ultimate ($DF1C-$DF1F).
;
; Banque 0 : démarrage CBM80 et chargeur. Banques 1 et suivantes : le
; programme, sans son adresse de chargement, recopié en $0801 puis lancé.
; PRG_PAGES vient de la ligne de commande.
        .setcpu "6502"

MD_BANK    = $DE00        ; numéro de banque ; bit 7 = cartouche coupée
MD_OFF     = $80
FIRST_BANK = 1
LOAD       = $0801
ENTRY      = $080D        ; après la ligne BASIC « SYS 2061 » de cc65
RAM_CODE   = $0334        ; tampon cassette, libre
SRC        = $FB
DST        = $FD
PAGE       = $02

        .segment "ROML"
        .word cold, cold
        .byte $C3, $C2, $CD, $38, $30   ; signature CBM80
cold:   sei
        stx $D016
        jsr $FDA3               ; IOINIT
        jsr $FD50               ; RAMTAS
        jsr $FD15               ; RESTOR
        jsr $FF5B               ; CINT
        ldx #loader_end - loader - 1
@c:     lda loader_image,x
        sta RAM_CODE,x
        dex
        bpl @c
        jmp RAM_CODE

loader_image:
        .org RAM_CODE
loader:
        lda #0
        sta PAGE
        lda #<LOAD
        sta DST
        lda #>LOAD
        sta DST+1
@page:  lda PAGE                ; banque = 1 + page / 32, adresse = $8000 + (page % 32) * 256
        lsr
        lsr
        lsr
        lsr
        lsr
        clc
        adc #FIRST_BANK
        sta MD_BANK
        lda PAGE
        and #$1F
        ora #$80
        sta SRC+1
        lda #0
        sta SRC
        ldy #0
@byte:  lda (SRC),y
        sta (DST),y
        iny
        bne @byte
        inc DST+1
        inc PAGE
        lda PAGE
        cmp #PRG_PAGES
        bne @page
        lda #MD_OFF             ; la cartouche disparaît
        sta MD_BANK
        lda #$37
        sta $01
        ldx #$FF
        txs
        cli
        jmp ENTRY
loader_end:
.assert loader_end <= $03FC, error, "chargeur trop grand"
        .reloc
