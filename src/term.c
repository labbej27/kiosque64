/* Kiosque 64 : terminal texte pour les BBS, en PETSCII ou en ASCII.
   L'affichage passe par l'écran texte et la routine CHROUT du KERNAL. */
#include <cbm.h>
#include "charmap.h"
#include "net.h"

void gfx_text(void);

static unsigned char ascii, a_esc;
const char *term_title;                        /* ligne d'en-tête (ASCII), ou 0 */

static void put_petscii(unsigned char c)
{
    cbm_k_bsout(c);
}

/* ASCII -> PETSCII ; les séquences ANSI (ESC [ ... lettre) sont ignorées. */
static void put_ascii(unsigned char c)
{
    if (a_esc) {
        if (a_esc == 1) a_esc = (c == '[') ? 2 : 0;
        else if (c >= 0x40 && c <= 0x7E) a_esc = 0;
        return;
    }
    if (c == 0x1B) { a_esc = 1; return; }
    if (c >= 'a' && c <= 'z') c -= 0x20;
    else if (c >= 'A' && c <= 'Z') c += 0x80;
    else if (c == 0x08) c = 0x9D;              /* recule sans effacer */
    else if (c == 0x0C) c = 0x93;              /* page blanche */
    else if (c == 0x0A || c == 0x07 || c == 0 || c >= 0x80) return;
    cbm_k_bsout(c);
}

static unsigned char key_ascii(unsigned char k)
{
    if (k >= 0x41 && k <= 0x5A) return k + 0x20;
    if (k >= 0xC1 && k <= 0xDA) return k - 0x80;
    if (k == 0x14) return 0x08;
    if (k == 0x0D || (k >= 0x20 && k < 0x60)) return k;
    return 0;
}

void term_run(unsigned char use_ascii)
{
    unsigned char k, c;
    ascii = use_ascii;
    a_esc = 0;
    gfx_text();
    *(unsigned char *)0xD020 = 0;
    *(unsigned char *)0xD021 = 0;
    cbm_k_bsout(0x0E);                         /* minuscules */
    cbm_k_bsout(0x93);
    cbm_k_bsout(0x05);                         /* blanc */
    net_sink = ascii ? put_ascii : put_petscii;
    if (term_title) {
        const char *t = term_title;
        while (*t)
            put_ascii((unsigned char)*t++);
        put_ascii(0x0D);
        put_ascii(0x0D);
    }
    for (;;) {
        if (net_poll())
            break;
        k = cbm_k_getin();
        if (!k)
            continue;
        if (k == 0x8C)                         /* F8 : retour au menu */
            break;
        c = ascii ? key_ascii(k) : k;
        if (c)
            net_send(&c, 1);
    }
}
