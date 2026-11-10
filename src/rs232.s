; Kiosque 64 modem : pilotes RS-232 pour trois interfaces, choisies par
; _rs_iface avant rs_start :
;
; 0 User Port (câblage standard du C64, niveaux TTL non inversés), 300 à
;   2400 bauds, par interruptions NMI :
;     réception : broche B (FLAG2) détecte le bit de départ, broche C (PB0)
;                 est lue au milieu de chaque bit grâce au timer B du CIA 2 ;
;     émission  : broche M (PA2), un bit à chaque passage à zéro du timer A.
;
; 1 UP9600 (câblage de Daniel Dallmann, 1997), jusqu'à 9600 bauds. Les
;   registres à décalage des CIA font le travail fin :
;     réception : broches B et C comme ci-dessus, plus broche 7 (SP2) ; le
;                 timer B du CIA 2 sort des impulsions sur PB7 (broche L,
;                 reliée à la broche 6, CNT2) qui cadencent le registre à
;                 décalage ; une NMI au bit de départ, une NMI par octet ;
;     émission  : broche 5 (SP1), registre à décalage du CIA 1 cadencé par
;                 son timer A. L'interruption à 60 Hz du KERNAL passe donc
;                 sur le timer B du CIA 1 tant que le pilote tourne.
;     Chaque octet part en deux moitiés de 8 bits : six bits de repos, le
;     bit de départ et le bit 0, puis les bits 1 à 7 et le bit d'arrêt. Sur
;     le fil, c'est une trame normale précédée d'un peu de repos ; chaque
;     octet fait juste 16 bits, ce qui convient aussi à l'émulation de VICE.
;     Le timer A du CIA 2 mesure la trame.
;
; 2 et 3 SwiftLink / Turbo232 (ACIA 6551, quartz 3,6864 MHz) en $DE00 ou
;   $DF00, 300 à 38 400 bauds. Une NMI par octet reçu ; contrôle de flux
;   RTS quand le tampon se remplit (CTS est géré par l'ACIA elle-même).
;   Seulement avec la version .prg : la cartouche occupe le même port.
;
; Réception dans un tampon de 8 Ko en $8000-$9FFF : une page Minitel entière
; tient dedans même quand le dessin à 1 MHz prend du retard. Écrit pour
; Kiosque 64 d'après les fiches techniques du 6526 et du 6551 et la
; description publique d'UP9600, sans reprise de code d'un autre pilote.

        .export _rs_start, _rs_stop, _rs_getc, _rs_putc, _rs_txbusy
        .export _rs_lost, _rs_iface

CIA1    = $DC00
CIA2    = $DD00
PRA     = CIA2
PRB     = CIA2 + 1
DDRB    = CIA2 + 3
TALO    = CIA2 + 4
TAHI    = CIA2 + 5
TBLO    = CIA2 + 6
TBHI    = CIA2 + 7
SDR     = CIA2 + 12
ICR     = CIA2 + 13
CRA     = CIA2 + 14
CRB     = CIA2 + 15

NMIVEC  = $0318         ; vecteur NMI du KERNAL
HWNMI   = $FFFA         ; vecteur du 6502, lu quand la ROM KERNAL est masquée
PALFLAG = $02A6
RXBUF   = $8000         ; 8 Ko, libres au-dessus du programme (vérifié ci-dessous)
RXEND   = $A000

IF_USER = 0
IF_U96  = 1

        .import __BSS_RUN__, __BSS_SIZE__
.assert __BSS_RUN__ + __BSS_SIZE__ <= RXBUF, lderror, "le programme déborde sur le tampon RS-232"

        .bss
_rs_iface: .res 1       ; interface choisie (voir plus haut)
txbuf:  .res 256
rxtail: .res 2          ; écrit par le programme (la tête est dans rxput)
txhead: .res 1          ; écrit par le programme
txtail: .res 1          ; écrit par la NMI
txact:  .res 1          ; 1 tant que le timer A émet (User Port)
txcnt:  .res 1          ; bits restant à émettre pour l'octet en cours
txsh:   .res 1
rxbits: .res 1          ; 0 = en attente d'un bit de départ
rxsh:   .res 1
u96rx:  .res 1          ; UP9600 : $80 pendant la réception d'un octet
slflow: .res 1          ; SwiftLink : $80 si le contrôle de flux est actif
rtsoff: .res 1          ; SwiftLink : 1 quand RTS est relâché (tampon plein)
_rs_lost: .res 1        ; octets perdus (tampon plein), plafonné à 255
bitlo:  .res 1          ; durée d'un bit, en cycles - 1
bithi:  .res 1
stlo:   .res 1          ; délai jusqu'au milieu du premier bit de données
sthi:   .res 1
frlo:   .res 1          ; UP9600 : durée des 15 derniers bits d'une trame
frhi:   .res 1
oldnmi: .res 2
oldhw:  .res 2
running: .res 1         ; interface en marche + 1, 0 = arrêté
speed:  .res 1          ; index de vitesse
halflo: .res 1          ; UP9600 : demi-bit
halfhi: .res 1

        .rodata
; Index de vitesse : 300, 1200, 2400, 4800, 9600, 19200, 38400 bauds.
; PAL puis NTSC. Durée d'un bit (cycles - 1) :
bittab: .word 3283, 820, 410, 204, 102, 50, 25
        .word 3408, 851, 425, 212, 106, 52, 26
; User Port : 1,5 bit moins 70 cycles de réaction de la NMI
sttab:  .word 4855, 1161, 545, 237, 83, 6, 0
        .word 5043, 1207, 568, 249, 89, 9, 0
; UP9600 : 1,5 bit moins 50 cycles (sa NMI de départ est plus courte)
usttab: .word 4875, 1181, 565, 257, 103, 26, 0
        .word 5063, 1227, 588, 269, 109, 29, 0
; UP9600 : demi-bit pour le timer A du CIA 1 (deux passages par bit)
halftab: .word 1641, 410, 204, 102, 50, 25, 12
        .word 1704, 425, 212, 106, 52, 26, 12
; UP9600 : 15 bits, la fin d'une trame de 16 bits
frtab:  .word 49263, 12316, 6158, 3079, 1539, 770, 385
        .word 51136, 12784, 6392, 3196, 1598, 799, 400
; SwiftLink : code de vitesse de l'ACIA (quartz doublé : 150 -> 300, etc.)
slcode: .byte $05, $07, $08, $0A, $0C, $0E, $0F
; 256 octets, bits en ordre inverse (le registre à décalage reçoit le bit 0
; en premier, en position 7)
revtab:
        .repeat 256, n
        .byte ((n & 1) << 7) | ((n & 2) << 5) | ((n & 4) << 3) | ((n & 8) << 1) | ((n & 16) >> 1) | ((n & 32) >> 3) | ((n & 64) >> 5) | ((n & 128) >> 7)
        .endrep

        .code
rxhead  = rxstore + 1           ; la tête est l'opérande du « sta » de rxput

; ---------------------------------------------------------------------
; void __fastcall__ rs_start(unsigned char speed) : index de vitesse
.proc _rs_start
        pha
        jsr _rs_stop
        pla
        sta speed               ; vitesse 0-6
        ldx PALFLAG
        bne :+
        clc
        adc #7                  ; NTSC
:       asl a
        tax
        lda bittab,x
        sta bitlo
        lda bittab+1,x
        sta bithi
        lda halftab,x
        sta halflo
        lda halftab+1,x
        sta halfhi
        lda frtab,x
        sta frlo
        lda frtab+1,x
        sta frhi
        lda _rs_iface
        cmp #IF_U96
        beq :+
        lda sttab,x
        sta stlo
        lda sttab+1,x
        sta sthi
        jmp :++
:       lda usttab,x
        sta stlo
        lda usttab+1,x
        sta sthi
:
        lda #0
        sta rxtail
        sta rxhead
        sta txhead
        sta txtail
        sta txact
        sta rxbits
        sta u96rx
        sta slflow
        sta rtsoff
        sta _rs_lost
        lda #>RXBUF
        sta rxtail+1
        sta rxhead+1
        lda NMIVEC              ; vecteurs NMI d'origine, remis par rs_stop
        sta oldnmi
        lda NMIVEC+1
        sta oldnmi+1
        lda HWNMI
        sta oldhw
        lda HWNMI+1
        sta oldhw+1
        lda _rs_iface
        clc
        adc #1
        sta running
        lda _rs_iface
        beq start_user
        cmp #IF_U96
        bne :+
        jmp start_u96
:       jmp start_sl
.endproc

; installe le gestionnaire de NMI A (bas) / X (haut)
setnmi: sta NMIVEC
        stx NMIVEC+1
        sta HWNMI               ; (écrit dans la RAM sous la ROM)
        stx HWNMI+1
        rts

; --- User Port --------------------------------------------------------
start_user:
        lda #$7F                ; aucune NMI du CIA 2 pendant l'installation
        sta ICR
        lda #0
        sta CRA
        sta CRB
        lda ICR
        lda #<nmi_user
        ldx #>nmi_user
        jsr setnmi
        lda PRA
        ora #$04                ; ligne d'émission au repos (1)
        sta PRA
        lda #$06                ; PB1 RTS et PB2 DTR en sortie, PB0 en entrée
        sta DDRB
        sta PRB
        lda #$90                ; NMI sur le bit de départ (FLAG)
        sta ICR
        rts

; --- UP9600 -----------------------------------------------------------
start_u96:
        lda #$7F
        sta ICR
        lda #0
        sta CRA                 ; registre à décalage du CIA 2 en entrée
        sta CRB
        lda ICR
        lda #<nmi_u96
        ldx #>nmi_u96
        jsr setnmi
        lda PRA
        ora #$04                ; PA2 à 1 : ne gêne pas SP1 si M et 5 sont reliées
        sta PRA
        lda #$06                ; RTS, DTR ; PB7 piloté par le timer B
        sta DDRB
        sta PRB
        php
        sei
        lda #$7F                ; CIA 1 : 60 Hz du KERNAL sur le timer B
        sta CIA1 + 13
        ldx PALFLAG
        lda #$25                ; $4025 en PAL, $4295 en NTSC (valeurs du KERNAL)
        ldy #$40
        cpx #0
        bne :+
        lda #$95
        ldy #$42
:       sta CIA1 + 6
        sty CIA1 + 7
        lda #$11
        sta CIA1 + 15
        lda halflo              ; timer A : cadence du registre à décalage
        sta CIA1 + 4
        lda halfhi
        sta CIA1 + 5
        lda #$51                ; marche continue, chargement forcé, SP en sortie
        sta CIA1 + 14
        lda CIA1 + 13
        lda #$82                ; interruption du timer B seulement
        sta CIA1 + 13
        plp
        bit PRB                 ; (voir nmi_u96)
        lda #$90                ; NMI sur le bit de départ
        sta ICR
        rts

; --- SwiftLink --------------------------------------------------------
; Les adresses de l'ACIA ($DE00 ou $DF00) sont écrites dans les instructions
; qui y accèdent : octet haut de chaque opérande.

start_sl:
        lda _rs_iface           ; 2 -> $DE, 3 -> $DF
        clc
        adc #$DC
        ldx #SLPATCHN * 2 - 2
:       ldy slpatch,x
        sty start_sl_p+1
        ldy slpatch+1,x
        sty start_sl_p+2
start_sl_p:     sta $FFFF
        dex
        dex
        bpl :-
        lda #<nmi_sl
        ldx #>nmi_sl
        jsr setnmi
sl5:    sta $DE01               ; remise à zéro de l'ACIA
        ldx speed
        lda slcode,x
        ora #$10                ; 8 bits, 1 bit d'arrêt, horloge interne
sl6:    sta $DE03
        lda #$09                ; DTR, RTS, interruption de réception
sl7:    sta $DE02
sl8:    lda $DE01
sl9:    lda $DE00               ; vide l'éventuel octet en attente
        lda #$80
        sta slflow
        rts

; ---------------------------------------------------------------------
; void rs_stop(void)
.proc _rs_stop
        lda running
        bne :+
        rts
:       ldx #0
        stx running
        cmp #IF_USER + 1
        beq rs_stop_cia2
        cmp #IF_U96 + 1
        beq rs_stop_u96
        lda #$0B                ; SwiftLink : DTR gardé, interruptions coupées
sl10:   sta $DE02
        lda #0
        sta slflow
        jmp rs_stop_vec
rs_stop_u96:   php                     ; CIA 1 : le 60 Hz revient sur le timer A
        sei
        lda #$7F
        sta CIA1 + 13
        lda #0
        sta CIA1 + 15
        ldx PALFLAG
        lda #$25
        ldy #$40
        cpx #0
        bne :+
        lda #$95
        ldy #$42
:       sta CIA1 + 4
        sty CIA1 + 5
        lda #$11
        sta CIA1 + 14
        lda CIA1 + 13
        lda #$81
        sta CIA1 + 13
        plp
rs_stop_cia2:  lda #$7F
        sta ICR
        lda #0
        sta CRA
        sta CRB
        sta txact
        sta DDRB
        lda ICR
rs_stop_vec:   lda oldnmi
        ldx oldnmi+1
        sta NMIVEC
        stx NMIVEC+1
        lda oldhw
        sta HWNMI
        lda oldhw+1
        sta HWNMI+1
        rts
.endproc

; ---------------------------------------------------------------------
; int rs_getc(void) : octet reçu, ou -1 si le tampon est vide
.proc _rs_getc
rs_getc_head:  lda rxhead+1            ; tête sur 2 octets, modifiée par la NMI :
        ldx rxhead              ; on relit si elle a changé de page entre-temps
        cmp rxhead+1
        bne rs_getc_head
        cpx rxtail
        bne rs_getc_have
        cmp rxtail+1
        beq rs_getc_none
rs_getc_have:  lda rxtail
        sta rs_getc_ld+1
        lda rxtail+1
        sta rs_getc_ld+2
rs_getc_ld:    lda RXBUF
        pha
        ldy rxtail              ; queue avancée octet bas d'abord : si la NMI
        iny                     ; lit entre les deux, elle voit moins de place,
        sty rxtail              ; jamais plus
        bne rs_getc_done
        ldx rxtail+1
        inx
        cpx #>RXEND
        bne :+
        ldx #>RXBUF
:       stx rxtail+1
        lda rtsoff              ; SwiftLink : tampon assez vidé, RTS rendu
        beq rs_getc_done
        lda rxhead+1
        sec
        sbc rxtail+1
        and #$1F                ; pages occupées
        cmp #16
        bcs rs_getc_done
        lda #0
        sta rtsoff
        lda #$09
sl11:   sta $DE02
rs_getc_done:  pla
        ldx #0
        rts
rs_getc_none:  lda #$FF
        tax
        rts
.endproc

; ---------------------------------------------------------------------
; void __fastcall__ rs_putc(unsigned char c) : attend s'il n'y a plus de place
.proc _rs_putc
        ldx _rs_iface
        beq putc_user
        cpx #IF_U96
        beq putc_u96
        pha                     ; SwiftLink : registre d'émission libre ?
sl3:    lda $DE01
        and #$10
        beq sl3
        pla
sl4:    sta $DE00
        rts
.endproc

putc_user:
        ldy txhead
        sta txbuf,y
        iny
putc_user_full:  cpy txtail
        beq putc_user_full
        sty txhead              ; la NMI voit l'octet à partir d'ici
        lda txact
        bne putc_user_x
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
putc_user_x:     rts

; UP9600 : %111111 départ bit0, puis bits 1-7 et arrêt, envoyés poids fort
; en tête par le registre à décalage du CIA 1.
putc_u96:
        tax
putc_u96_wait:  lda CRA                 ; trame précédente encore en cours ?
        and #$01
        bne putc_u96_wait
        php
        sei
        txa
        and #$01                ; bit 0 en dernier
        ora #$FC                ; six bits de repos, puis le départ (0)
        sta CIA1 + 12
        lda bitlo               ; un bit de temps : la première moitié est
        sta TALO                ; passée dans le registre à décalage
        lda bithi
        sta TAHI
        lda #$19                ; timer A du CIA 2 : un coup, chargement forcé
        sta CRA
:       lda CRA
        and #$01
        bne :-
        lda revtab,x            ; bits 0..7 en positions 7..0
        sec
        rol a                   ; bits 1 à 7, puis l'arrêt (1)
        sta CIA1 + 12
        lda frlo                ; fin de trame mesurée par le même timer
        sta TALO
        lda frhi
        sta TAHI
        lda #$19
        sta CRA
        plp
        rts

; unsigned char rs_txbusy(void) : 1 tant que des octets restent à émettre
.proc _rs_txbusy
        ldx #0
        lda _rs_iface
        beq rs_txbusy_user
        cmp #IF_U96
        beq rs_txbusy_u96
sl1:    lda $DE01               ; SwiftLink : registre d'émission occupé ?
        and #$10
        eor #$10
        rts
rs_txbusy_u96:   lda CRA
        and #$01
        rts
rs_txbusy_user:  lda txact
        rts
.endproc

; ---------------------------------------------------------------------
; rxput : range l'octet A dans le tampon (appelé par les NMI ; modifie Y)
rxput:
rxstore:
        sta RXBUF               ; adresse = tête du tampon (modifiée ici même)
        ldy rxhead
        lda rxhead+1
        iny
        bne rxput_nw
        clc
        adc #1
        cmp #>RXEND
        bne rxput_nw
        lda #>RXBUF
rxput_nw:    cpy rxtail
        bne rxput_ok
        cmp rxtail+1
        beq rxput_lost               ; tampon plein : octet perdu
rxput_ok:    sty rxhead
        sta rxhead+1
        bit slflow              ; SwiftLink : RTS relâché aux trois quarts
        bpl rxput_x
        sec
        sbc rxtail+1
        and #$1F
        cmp #28
        bcc rxput_x
        lda #1
        sta rtsoff
        lda #$01                ; DTR, interruption de réception, RTS relâché
sl2:    sta $DE02
rxput_x:     rts
rxput_lost:  inc _rs_lost
        bne rxput_x
        dec _rs_lost
        rts

; --- NMI SwiftLink ----------------------------------------------------
nmi_sl: pha
sl12:   lda $DE01               ; lire l'état efface la demande d'interruption
        and #$08
        beq nmi_sl_out
        tya
        pha
sl13:   lda $DE00
        jsr rxput
        pla
        tay
nmi_sl_out:   pla
        rti

; --- NMI UP9600 -------------------------------------------------------
nmi_u96:
        pha
        lda ICR
        bit u96rx
        bmi nmi_u96_data
        and #$10                ; bit de départ ?
        beq nmi_u96_out
        bit PRB                 ; (sans effet sur le matériel ; pour VICE, qui
                                ; compte les impulsions de PB7 entre deux lectures)
        lda stlo                ; première impulsion au milieu du bit 0
        sta TBLO
        lda sthi
        sta TBHI
        lda #$13                ; impulsions sur PB7, marche continue
        sta CRB
        lda bitlo               ; puis une par bit
        sta TBLO
        lda bithi
        sta TBHI
        lda #$10                ; FLAG masqué, registre à décalage démasqué
        sta ICR
        lda #$88
        sta ICR
        lda #$80
        sta u96rx
nmi_u96_out:   pla
        rti
nmi_u96_data:  and #$08                ; 8 bits reçus ?
        beq nmi_u96_out
        lda #0
        sta CRB
        sta u96rx
        lda #$08
        sta ICR
        lda #$90
        sta ICR
        tya
        pha
        ldy SDR
        lda revtab,y
        jsr rxput
        pla
        tay
        pla
        rti

; --- NMI User Port ----------------------------------------------------
; Le registre ICR renvoie aussi les événements masqués : chaque source n'est
; traitée que dans l'état où elle a un sens (réception en cours ou non,
; émission active ou non).
nmi_user:
        pha
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
        jsr rxput
        jmp n_tx

n_wait: txa                     ; bit de départ ?
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

n_tx:   txa                     ; émission : bit suivant ?
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
n_bit:  dec txcnt
        beq n_stopbit
        lsr txsh                ; bit de données -> retenue
        jmp n_out
n_stopbit:
        sec                     ; bit d'arrêt
n_out:  lda PRA
        and #$FB
        bcc :+
        ora #$04
:       sta PRA
        jmp n_done
n_stop: lda #0                  ; plus rien à émettre
        sta CRA
        sta txact
        lda #$01
        sta ICR

n_done: pla
        sta $01
        pla
        tay
        pla
        tax
        pla
        rti

; Instructions qui accèdent à l'ACIA (placé ici : les portées .proc doivent
; être définies avant d'y faire référence).
        .rodata
slpatch: .word _rs_txbusy::sl1+2, sl2+2, _rs_putc::sl3+2, _rs_putc::sl4+2, sl5+2, sl6+2
        .word sl7+2, sl8+2, sl9+2, _rs_stop::sl10+2, _rs_getc::sl11+2, sl12+2, sl13+2
SLPATCHN = 13
