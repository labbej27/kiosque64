; Kiosque 64 modem : pilote RS-232 du port utilisateur (câblage standard du
; C64, niveaux TTL non inversés), 300 à 2400 bauds, par interruptions NMI.
;
;   réception : broche B (FLAG2) détecte le bit de départ, broche C (PB0)
;               est lue au milieu de chaque bit grâce au timer B du CIA 2 ;
;   émission  : broche M (PA2), un bit à chaque passage à zéro du timer A ;
;   PB1 (RTS) et PB2 (DTR) sont mis à 1 pour les modems qui les surveillent.
;
; Réception dans un tampon de 8 Ko en $8000-$9FFF (une page Minitel entière :
; à 1 MHz, le dessin de certaines pages est plus lent que 2400 bauds, et les
; modems WiFi n'ont en général pas de contrôle de flux). Émission dans un
; tampon de 256 octets. Écrit pour Kiosque 64, sans reprise de code d'un autre
; pilote.

        .export _rs_start, _rs_stop, _rs_getc, _rs_putc, _rs_txbusy, _rs_lost

CIA2    = $DD00
PRA     = CIA2
PRB     = CIA2 + 1
DDRB    = CIA2 + 3
TALO    = CIA2 + 4
TAHI    = CIA2 + 5
TBLO    = CIA2 + 6
TBHI    = CIA2 + 7
ICR     = CIA2 + 13
CRA     = CIA2 + 14
CRB     = CIA2 + 15

NMIVEC  = $0318         ; vecteur NMI du KERNAL
HWNMI   = $FFFA         ; vecteur du 6502, lu quand la ROM KERNAL est masquée
PALFLAG = $02A6
RXBUF   = $8000         ; 8 Ko, libres au-dessus du programme (vérifié ci-dessous)
RXEND   = $A000

        .import __BSS_RUN__, __BSS_SIZE__
.assert __BSS_RUN__ + __BSS_SIZE__ <= RXBUF, lderror, "le programme déborde sur le tampon RS-232"

        .bss
txbuf:  .res 256
rxtail: .res 2          ; écrit par le programme (la tête est dans le code de la NMI)
txhead: .res 1          ; écrit par le programme
txtail: .res 1          ; écrit par la NMI
txact:  .res 1          ; 1 tant que le timer A émet
txcnt:  .res 1          ; bits restant à émettre pour l'octet en cours
txsh:   .res 1
rxbits: .res 1          ; 0 = en attente d'un bit de départ
rxsh:   .res 1
_rs_lost: .res 1        ; octets perdus (tampon plein), plafonné à 255
bitlo:  .res 1          ; durée d'un bit, en cycles - 1
bithi:  .res 1
stlo:   .res 1          ; 1,5 bit moins le temps de réaction de la NMI
sthi:   .res 1
oldnmi: .res 2
oldhw:  .res 2

        .rodata
; PAL 300, 1200, 2400 puis NTSC : durée d'un bit, puis délai jusqu'au milieu
; du premier bit de données (1,5 bit - 70 cycles de réaction).
bittab: .word 3283, 820, 410, 3408, 851, 425
sttab:  .word 4855, 1161, 545, 5043, 1207, 568

        .code
rxhead  = rxstore + 1           ; la tête est l'opérande du « sta » de la NMI

; void __fastcall__ rs_start(unsigned char speed) : 0 = 300, 1 = 1200, 2 = 2400
.proc _rs_start
        ldx PALFLAG
        bne :+
        clc
        adc #3                  ; NTSC
:       asl a
        tax
        lda bittab,x
        sta bitlo
        lda bittab+1,x
        sta bithi
        lda sttab,x
        sta stlo
        lda sttab+1,x
        sta sthi
        lda #$7F                ; aucune NMI du CIA 2 pendant l'installation
        sta ICR
        lda #0
        sta CRA
        sta CRB
        sta rxtail
        sta rxhead
        sta txhead
        sta txtail
        sta txact
        sta rxbits
        sta _rs_lost
        lda #>RXBUF
        sta rxtail+1
        sta rxhead+1
        lda ICR
        lda NMIVEC
        sta oldnmi
        lda NMIVEC+1
        sta oldnmi+1
        lda HWNMI
        sta oldhw
        lda HWNMI+1
        sta oldhw+1
        lda #<nmi
        ldx #>nmi
        sta NMIVEC
        stx NMIVEC+1
        sta HWNMI               ; (écrit dans la RAM sous la ROM)
        stx HWNMI+1
        lda PRA
        ora #$04                ; ligne d'émission au repos (1)
        sta PRA
        lda #$06                ; PB1 RTS et PB2 DTR en sortie, PB0 en entrée
        sta DDRB
        sta PRB
        lda #$90                ; NMI sur le bit de départ (FLAG)
        sta ICR
        rts
.endproc

; void rs_stop(void)
.proc _rs_stop
        lda #$7F
        sta ICR
        lda #0
        sta CRA
        sta CRB
        sta txact
        sta DDRB
        lda ICR
        lda oldnmi+1
        beq @x                  ; jamais démarré
        sta NMIVEC+1
        lda oldnmi
        sta NMIVEC
        lda oldhw
        sta HWNMI
        lda oldhw+1
        sta HWNMI+1
        lda #0
        sta oldnmi+1
@x:     rts
.endproc

; int rs_getc(void) : octet reçu, ou -1 si le tampon est vide
.proc _rs_getc
@head:  lda rxhead+1            ; tête sur 2 octets, modifiée par la NMI :
        ldx rxhead              ; on relit si elle a changé de page entre-temps
        cmp rxhead+1
        bne @head
        cpx rxtail
        bne @have
        cmp rxtail+1
        beq @none
@have:  lda rxtail
        sta @ld+1
        lda rxtail+1
        sta @ld+2
@ld:    lda RXBUF
        pha
        ldy rxtail              ; queue avancée octet bas d'abord : si la NMI
        iny                     ; lit entre les deux, elle voit moins de place,
        sty rxtail              ; jamais plus
        bne @done
        ldx rxtail+1
        inx
        cpx #>RXEND
        bne :+
        ldx #>RXBUF
:       stx rxtail+1
@done:  pla
        ldx #0
        rts
@none:  lda #$FF
        tax
        rts
.endproc

; void __fastcall__ rs_putc(unsigned char c) : attend s'il n'y a plus de place
.proc _rs_putc
        ldy txhead
        sta txbuf,y
        iny
@full:  cpy txtail
        beq @full
        sty txhead              ; la NMI voit l'octet à partir d'ici
        lda txact
        bne @x
        lda #1                  ; émetteur arrêté : on relance le timer A
        sta txact
        lda #0
        sta txcnt
        lda bitlo
        sta TALO
        lda bithi
        sta TAHI
        lda #$11                ; chargement forcé, marche continue
        sta CRA
        lda #$81
        sta ICR
@x:     rts
.endproc

; unsigned char rs_txbusy(void) : 1 tant que des octets restent à émettre
.proc _rs_txbusy
        lda txact
        ldx #0
        rts
.endproc

; --- interruption NMI -------------------------------------------------
; Le registre ICR renvoie aussi les événements masqués : chaque source n'est
; traitée que dans l'état où elle a un sens (réception en cours ou non,
; émission active ou non).
nmi:    pha
        txa
        pha
        tya
        pha
        lda $01
        pha
        lda #$35                ; E/S visibles, notre code reste en RAM basse
        sta $01
        ldx ICR                 ; lit et efface les événements

        lda rxbits
        bne n_rx
        jmp n_wait
n_rx:
        txa                     ; réception : un bit à lire ?
        and #$02
        bne n_rxbit
        jmp n_tx
n_rxbit:
        lda PRB
        lsr a                   ; PB0 -> retenue
        ror rxsh                ; bit de poids faible reçu en premier
        dec rxbits
        beq n_byte
        jmp n_tx
n_byte:
        lda #0                  ; 8 bits reçus : timer B arrêté
        sta CRB
        lda #$02
        sta ICR
        lda #$90                ; on attend le prochain bit de départ
        sta ICR
        lda rxsh
rxstore:
        sta RXBUF               ; adresse = tête du tampon (modifiée ici même)
        ldy rxhead
        lda rxhead+1
        iny
        bne n_nw
        clc
        adc #1
        cmp #>RXEND
        bne n_nw
        lda #>RXBUF
n_nw:    cpy rxtail
        bne n_ok
        cmp rxtail+1
        beq n_lost               ; tampon plein : octet perdu
n_ok:    sty rxhead
        sta rxhead+1
        jmp n_tx

n_lost:  inc _rs_lost
        bne n_tx
        dec _rs_lost
        jmp n_tx

n_wait:  txa                     ; bit de départ ?
        and #$10
        beq n_tx
        lda #$10                ; FLAG masqué pendant l'octet
        sta ICR
        lda stlo                ; premier échantillon au milieu du bit 0
        sta TBLO
        lda sthi
        sta TBHI
        lda #$11
        sta CRB
        lda bitlo               ; puis un échantillon par bit
        sta TBLO
        lda bithi
        sta TBHI
        lda #8
        sta rxbits
        lda #$82
        sta ICR

n_tx:    txa                     ; émission : bit suivant ?
        and #$01
        beq n_done
        lda txact
        beq n_done
        lda txcnt
        bne n_bit
        ldy txtail              ; octet suivant
        cpy txhead
        beq n_stop
        lda txbuf,y
        sta txsh
        inc txtail
        lda #9
        sta txcnt
        clc                     ; bit de départ
        bcc n_out
n_bit:   dec txcnt
        beq n_stopbit
        lsr txsh                ; bit de données -> retenue
        jmp n_out
n_stopbit:
        sec                     ; bit d'arrêt
n_out:   lda PRA
        and #$FB
        bcc :+
        ora #$04
:       sta PRA
        jmp n_done
n_stop:  lda #0                  ; plus rien à émettre
        sta CRA
        sta txact
        lda #$01
        sta ICR

n_done:  pla
        sta $01
        pla
        tay
        pla
        tax
        pla
        rti
