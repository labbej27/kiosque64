; Kiosque 64 : écran graphique 320x200, 40x25 cases de 8x8.
; Banque VIC 3 : bitmap en $E000 (sous la ROM KERNAL), couleurs en $CC00,
; sprite du curseur en $C000.

        .export _gfx_init, _gfx_text, _draw_cell, _gfx_clear_rows
        .export _gfx_copy_row, _cursor_place, _cursor_show
        .export _dc_row, _dc_col, _dc_src, _dc_flags, _dc_color
        .importzp ptr1, ptr2, ptr3, tmp1, tmp2, tmp3, tmp4

BITMAP  = $E000
SCREEN  = $CC00
SPRITE  = $C000

; drapeaux de _dc_flags
F_DW    = $01           ; double largeur
F_RIGHT = $02           ; moitié droite (si double largeur)
F_DH    = $04           ; double hauteur
F_TOP   = $08           ; moitié haute (si double hauteur)
F_UL    = $10           ; souligné

        .bss
_dc_row:   .res 1
_dc_col:   .res 1
_dc_src:   .res 2
_dc_flags: .res 1
_dc_color: .res 1
glyph:     .res 8

        .rodata
rowlo:  .repeat 25, i
        .byte <(BITMAP + i * 320)
        .endrep
rowhi:  .repeat 25, i
        .byte >(BITMAP + i * 320)
        .endrep
scrlo:  .repeat 25, i
        .byte <(SCREEN + i * 40)
        .endrep
scrhi:  .repeat 25, i
        .byte >(SCREEN + i * 40)
        .endrep
; un quartet -> un octet, chaque pixel doublé (double largeur)
dbl:    .repeat 16, n
        .byte ((n & 8) * 24) | ((n & 4) * 12) | ((n & 2) * 6) | ((n & 1) * 3)
        .endrep

        .code

; void gfx_init(void) : mode bitmap, écran noir, sprite du curseur prêt.
.proc _gfx_init
        lda #$00
        sta $D020
        sta $D021
        ; sprite du curseur : pavé 8x8 en haut à gauche
        ldx #63
        lda #0
@spc:   sta SPRITE,x
        dex
        bpl @spc
        ldx #7*3
@sp:    lda #$FF
        sta SPRITE,x
        dex
        dex
        dex
        bpl @sp
        lda #(SPRITE & $3FFF) / 64
        sta SCREEN + $3F8
        lda #1
        sta $D027               ; blanc
        lda #0
        sta $D015               ; caché
        sta $D017
        sta $D01D
        sta $D01C
        sta $D01B               ; devant le texte
        ; effacement complet
        lda #0
        ldx #0
        ldy #25
        jsr clear_rows
        ; banque VIC 3, écran $CC00, bitmap $E000
        lda $DD02
        ora #$03
        sta $DD02
        lda $DD00
        and #$FC                ; %00 = banque 3
        sta $DD00
        lda #$38                ; écran $0C00, bitmap $2000 dans la banque
        sta $D018
        lda #$3B                ; bitmap, 25 lignes, écran allumé
        sta $D011
        lda #$08                ; 40 colonnes, pas de multicolore
        sta $D016
        rts
.endproc

; void gfx_text(void) : retour au mode texte normal (pour le terminal PETSCII).
.proc _gfx_text
        lda #0
        sta $D015
        lda $DD00
        ora #$03                ; banque 0
        sta $DD00
        lda #$17                ; écran $0400, jeu minuscules
        sta $D018
        lda #$1B
        sta $D011
        lda #$08
        sta $D016
        rts
.endproc

; void draw_cell(void) : dessine 8 octets depuis _dc_src en case
; (_dc_row, _dc_col), selon _dc_flags, et pose la couleur _dc_color.
.proc _draw_cell
        lda _dc_src
        sta ptr1
        lda _dc_src+1
        sta ptr1+1
        lda _dc_flags
        and #F_DH
        bne @dh
        ldy #7                  ; taille normale : copie directe
@cp:    lda (ptr1),y
        sta glyph,y
        dey
        bpl @cp
        bmi @width
@dh:    ldy #0                  ; double hauteur : lignes 0-3 ou 4-7 doublées
        lda _dc_flags
        and #F_TOP
        bne :+
        ldy #4
:       ldx #0
@dhl:   lda (ptr1),y
        sta glyph,x
        sta glyph+1,x
        iny
        inx
        inx
        cpx #8
        bne @dhl
@width: lda _dc_flags
        and #F_DW
        beq @ul
        lda _dc_flags
        and #F_RIGHT
        sta tmp1
        ldx #7
@dwl:   lda glyph,x
        ldy tmp1
        bne @r
        lsr a
        lsr a
        lsr a
        lsr a
        jmp @dw2
@r:     and #$0F
@dw2:   tay
        lda dbl,y
        sta glyph,x
        dex
        bpl @dwl
@ul:    lda _dc_flags
        and #F_UL
        beq @out
        lda _dc_flags           ; en double hauteur, seulement la moitié basse
        and #F_DH|F_TOP
        cmp #F_DH|F_TOP
        beq @out
        lda #$FF
        sta glyph+7
@out:   ; adresse bitmap : ligne + colonne * 8
        ldx _dc_row
        lda _dc_col
        asl a
        asl a
        asl a
        php
        clc
        adc rowlo,x
        sta ptr2
        lda rowhi,x
        adc #0
        plp
        bcc :+
        adc #0                  ; retenue de colonne*8 (colonnes >= 32)
:       sta ptr2+1
        ldy #7
@wr:    lda glyph,y
        sta (ptr2),y
        dey
        bpl @wr
        lda scrlo,x
        sta ptr3
        lda scrhi,x
        sta ptr3+1
        ldy _dc_col
        lda _dc_color
        sta (ptr3),y
        rts
.endproc

; void gfx_clear_rows(unsigned char first, unsigned char count, unsigned char color)
; appelée en C : color dans A, count et first sur la pile (fastcall cc65).
        .import popa
.proc _gfx_clear_rows
        sta tmp3                ; couleur
        jsr popa                ; (popa modifie Y)
        sta tmp4                ; nombre de rangées
        jsr popa
        tax                     ; première rangée
        ldy tmp4
        lda tmp3
        ; continue dans clear_rows
.endproc

; X = première rangée, Y = nombre, A = couleur
.proc clear_rows
        sta tmp3
        sty tmp4
        cpy #0
        beq @done
@row:   lda rowlo,x
        sta ptr1
        lda rowhi,x
        sta ptr1+1
        lda #0
        ldy #0
@b1:    sta (ptr1),y            ; 320 octets : 256 + 64
        iny
        bne @b1
        inc ptr1+1
        ldy #63
@b2:    sta (ptr1),y
        dey
        bpl @b2
        lda scrlo,x
        sta ptr1
        lda scrhi,x
        sta ptr1+1
        lda tmp3
        ldy #39
@c:     sta (ptr1),y
        dey
        bpl @c
        inx
        dec tmp4
        bne @row
@done:  rts
.endproc

; void gfx_copy_row(unsigned char from, unsigned char to) : copie une rangée
; entière (bitmap et couleurs). La ROM est masquée pendant la lecture.
.proc _gfx_copy_row
        tax                     ; to
        jsr popa
        tay                     ; from
        lda rowlo,y
        sta ptr1
        lda rowhi,y
        sta ptr1+1
        lda rowlo,x
        sta ptr2
        lda rowhi,x
        sta ptr2+1
        lda scrlo,y
        sta ptr3
        lda scrhi,y
        sta ptr3+1
        lda scrlo,x
        sta tmp1
        lda scrhi,x
        sta tmp2
        php
        sei
        lda $01
        pha
        lda #$35                ; RAM sous la KERNAL visible, E/S gardées
        sta $01
        ldy #0
@b1:    lda (ptr1),y
        sta (ptr2),y
        iny
        bne @b1
        inc ptr1+1
        inc ptr2+1
        ldy #63
@b2:    lda (ptr1),y
        sta (ptr2),y
        dey
        bpl @b2
        pla
        sta $01
        plp
        lda tmp1
        sta ptr2
        lda tmp2
        sta ptr2+1
        ldy #39
@c:     lda (ptr3),y
        sta (ptr2),y
        dey
        bpl @c
        rts
.endproc

; void cursor_place(unsigned char row, unsigned char col)
.proc _cursor_place
        asl a                   ; col * 8 + 24 (sur 9 bits)
        asl a
        asl a
        php
        clc
        adc #24
        sta $D000
        lda #0
        rol a                   ; retenue de l'addition
        plp
        adc #0                  ; retenue de col * 8
        and #1
        sta tmp1
        lda $D010
        and #$FE
        ora tmp1
        sta $D010
        jsr popa
        asl a
        asl a
        asl a
        clc
        adc #50
        sta $D001
        rts
.endproc

; void cursor_show(unsigned char on)
.proc _cursor_show
        and #1
        sta tmp1
        lda $D015
        and #$FE
        ora tmp1
        sta $D015
        rts
.endproc
