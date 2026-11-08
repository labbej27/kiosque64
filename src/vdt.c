/* Kiosque 64 : décodeur Vidéotex.
   Écrit d'après la spécification Télétel (STUM1B) ; le modèle des zones
   d'attributs suit le comportement des Minitel :
   - en mode texte (G0), fond, soulignement et masquage sont des attributs
     « série » : ils attendent un délimiteur (une espace) pour s'appliquer,
     et valent jusqu'au délimiteur suivant sur la rangée ;
   - en mode semi-graphique (G1), le fond s'applique case par case. */
#include <string.h>
#include "charmap.h"
#include "vdt.h"

/* --- mémoire écran : 25 rangées de 40 cases ------------------------- */
#define NCELLS 1000
static unsigned char vch[NCELLS];     /* caractère G0 (0-127) ou motif G1 (0-63) */
static unsigned char vat[NCELLS];     /* couleur, inversion, clignotement, type */
static unsigned char vbg[NCELLS];     /* fond, masquage, soulignement de zone */
static unsigned char vsz[NCELLS];     /* taille et partie (drapeaux de draw_cell) */

#define A_FG    0x07
#define A_INV   0x08
#define A_BLINK 0x10
#define A_TYPE  0x60
#define A_SEP   0x80
#define T_CHAR  0x00
#define T_DELIM 0x20
#define T_MOS   0x40
#define T_G1L   0x60                   /* lettre en mode G1 */

#define B_BG    0x07
#define B_MASK  0x08
#define B_ZUL   0x10

#define F_DW    0x01
#define F_RIGHT 0x02
#define F_DH    0x04
#define F_TOP   0x08
#define F_UL    0x10

/* routines de gfx.s */
extern unsigned char dc_row, dc_col, dc_flags, dc_color;
extern const unsigned char *dc_src;
void draw_cell(void);
void gfx_clear_rows(unsigned char first, unsigned char count, unsigned char color);
void gfx_copy_row(unsigned char from, unsigned char to);
void cursor_place(unsigned char row, unsigned char col);
void cursor_show(unsigned char on);
extern const unsigned char font_g0[], font_g1[], font_g1s[];

static const unsigned int rowoff[25] = {
    0, 40, 80, 120, 160, 200, 240, 280, 320, 360, 400, 440, 480, 520,
    560, 600, 640, 680, 720, 760, 800, 840, 880, 920, 960
};

/* Couleurs Minitel (noir, rouge, vert, jaune, bleu, magenta, cyan, blanc)
   vers la palette du C64, en couleur et en niveaux de gris. */
static const unsigned char c64col[8] = { 0, 2, 5, 7, 6, 4, 3, 1 };
static const unsigned char c64grey[8] = { 0, 11, 12, 15, 11, 12, 15, 1 };
static const unsigned char blank[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

unsigned char vdt_reply[8];
unsigned char vdt_reply_len;
unsigned char vdt_lower;
unsigned char vdt_echo;
unsigned char vdt_cursor_on;
unsigned char vdt_grey;
unsigned char vdt_bell;

/* --- état courant ---------------------------------------------------- */
static unsigned char row, col;        /* curseur : rangée 0-24, colonne 0-39 */
static unsigned char fg, bg, inv, blink, size, sep, g1, ul, mask;
static unsigned char w_bg, w_ul, w_mask;   /* attributs série en attente, 0xFF = aucun */
static unsigned char last_char;
static unsigned char page_mode;
static unsigned char global_mask;
static unsigned char blink_phase;

static struct {                       /* sauvegarde en entrant en rangée 0 */
    unsigned char row, col, fg, bg, inv, blink, size, sep, g1, ul, mask;
    unsigned char w_bg, w_ul, w_mask;
} saved;

static void reset_attr(void)
{
    fg = 7; bg = 0; inv = 0; blink = 0; size = 0; sep = 0; g1 = 0;
    ul = 0; mask = 0;
    w_bg = w_ul = w_mask = 0xFF;
}

static void save_state(void)
{
    saved.row = row; saved.col = col; saved.fg = fg; saved.bg = bg;
    saved.inv = inv; saved.blink = blink; saved.size = size; saved.sep = sep;
    saved.g1 = g1; saved.ul = ul; saved.mask = mask;
    saved.w_bg = w_bg; saved.w_ul = w_ul; saved.w_mask = w_mask;
}

static void restore_state(void)
{
    row = saved.row; col = saved.col; fg = saved.fg; bg = saved.bg;
    inv = saved.inv; blink = saved.blink; size = saved.size; sep = saved.sep;
    g1 = saved.g1; ul = saved.ul; mask = saved.mask;
    w_bg = saved.w_bg; w_ul = saved.w_ul; w_mask = saved.w_mask;
}

/* --- rendu ---------------------------------------------------------- */

/* Zone en vigueur juste avant la case zc_idx : fond, soulignement, masque.
   Gardée en cache pour l'écriture séquentielle. */
static unsigned int zc_idx = 0xFFFF;
static unsigned char z_bg, z_ul, z_mask;

static void zone_scan(unsigned int idx, unsigned char c)
{
    unsigned int j = idx;
    unsigned int start = idx - c;
    unsigned char t, found = 0;
    z_bg = 0; z_ul = 0; z_mask = 0;
    while (j > start) {
        --j;
        t = vat[j] & A_TYPE;
        if (t == T_CHAR)
            continue;
        if (!found) {
            found = 1;
            z_bg = vbg[j] & B_BG;
            if (t != T_DELIM)
                continue;          /* semi-graphique : chercher le masque plus loin */
            z_ul = vbg[j] & B_ZUL;
        }
        if (t == T_DELIM) {
            z_mask = vbg[j] & B_MASK;
            break;
        }
    }
    zc_idx = idx;
}

static unsigned char r_row, r_col;
static unsigned int r_idx;

/* Dessine la case (r_row, r_col), d'indice r_idx. */
static void render(void)
{
    unsigned char at = vat[r_idx], b = vbg[r_idx], t = at & A_TYPE;
    unsigned char f, k;
    const unsigned char *pal = vdt_grey ? c64grey : c64col;

    if (zc_idx != r_idx)
        zone_scan(r_idx, r_col);
    dc_row = r_row;
    dc_col = r_col;
    f = at & A_FG;
    if (t == T_MOS) {
        dc_src = ((at & A_SEP) ? font_g1s : font_g1) + (vch[r_idx] << 3);
        dc_flags = 0;
        k = b & B_BG;
    } else if (t == T_G1L) {
        dc_src = font_g0 + (vch[r_idx] << 3);
        dc_flags = 0;
        k = b & B_BG;
    } else {
        k = (t == T_DELIM) ? (b & B_BG) : z_bg;
        dc_src = font_g0 + (vch[r_idx] << 3);
        dc_flags = vsz[r_idx] | (z_ul ? F_UL : 0);
        if (z_mask && global_mask)
            dc_src = blank;
        if (at & A_INV) {
            unsigned char x = f; f = k; k = x;
        }
    }
    if ((at & A_BLINK) && blink_phase)
        dc_src = blank;
    dc_color = (pal[f] << 4) | pal[k];
    draw_cell();

    /* zone pour la case suivante */
    if (t == T_DELIM) {
        z_bg = b & B_BG; z_ul = b & B_ZUL; z_mask = b & B_MASK;
    } else if (t != T_CHAR) {
        z_bg = b & B_BG; z_ul = 0;
    }
    zc_idx = (r_col == 39) ? 0xFFFF : r_idx + 1;
}

/* Effet d'une case sur la zone qui suit (0 = transparente). */
static unsigned char zone_key(unsigned int idx)
{
    unsigned char t = vat[idx] & A_TYPE;
    if (t == T_CHAR)
        return 0;
    if (t == T_DELIM)
        return 0x80 | (vbg[idx] & (B_BG | B_MASK | B_ZUL));
    return 0x40 | (vbg[idx] & B_BG);
}

/* Redessine les caractères qui suivent (r_row, r_col) jusqu'à la prochaine
   case qui définit une zone. */
static void refresh_after(void)
{
    unsigned char rr = r_row, cc = r_col;
    unsigned int ii = r_idx;
    while (r_col < 39) {
        ++r_col;
        ++r_idx;
        if ((vat[r_idx] & A_TYPE) != T_CHAR)
            break;
        render();
    }
    r_row = rr; r_col = cc; r_idx = ii;
}

/* Pose une case et la dessine, avec mise à jour des zones voisines. */
static void put_cell(unsigned char rr, unsigned char cc, unsigned char ch,
                     unsigned char at, unsigned char b, unsigned char sz)
{
    unsigned char before;
    if (rr > 24 || cc > 39)
        return;
    r_row = rr; r_col = cc; r_idx = rowoff[rr] + cc;
    before = zone_key(r_idx);
    vch[r_idx] = ch; vat[r_idx] = at; vbg[r_idx] = b; vsz[r_idx] = sz;
    render();
    if (zone_key(r_idx) != before)
        refresh_after();
}

static void clear_cells(unsigned int from, unsigned int n)
{
    memset(vch + from, 0, n);
    memset(vat + from, 7 | T_MOS, n);
    memset(vbg + from, 0, n);
    memset(vsz + from, 0, n);
    zc_idx = 0xFFFF;
}

static void clear_rows(unsigned char first, unsigned char count)
{
    clear_cells(rowoff[first], count * 40);
    gfx_clear_rows(first, count, (vdt_grey ? c64grey : c64col)[7] << 4);
}

/* Efface les cases [from, to] d'une même rangée (redessinées). */
static void clear_span(unsigned char rr, unsigned char c0, unsigned char c1)
{
    unsigned int i = rowoff[rr] + c0;
    clear_cells(i, c1 - c0 + 1);
    for (r_row = rr, r_col = c0, r_idx = i; r_col <= c1; ++r_col, ++r_idx)
        render();
    zc_idx = 0xFFFF;
    r_col = c1;
    r_idx = rowoff[rr] + c1;
    refresh_after();
}

static void redraw_row(unsigned char rr)
{
    zc_idx = 0xFFFF;
    for (r_row = rr, r_col = 0, r_idx = rowoff[rr]; r_col < 40; ++r_col, ++r_idx)
        render();
}

/* Défilement des rangées first..last d'une rangée vers le haut (up=1) ou
   vers le bas. */
static void scroll(unsigned char first, unsigned char last, unsigned char up)
{
    unsigned int a = rowoff[first], n = rowoff[last] - a;
    unsigned char r;
    if (up) {
        memmove(vch + a, vch + a + 40, n);
        memmove(vat + a, vat + a + 40, n);
        memmove(vbg + a, vbg + a + 40, n);
        memmove(vsz + a, vsz + a + 40, n);
        for (r = first; r < last; ++r)
            gfx_copy_row(r + 1, r);
        clear_rows(last, 1);
    } else {
        memmove(vch + a + 40, vch + a, n);
        memmove(vat + a + 40, vat + a, n);
        memmove(vbg + a + 40, vbg + a, n);
        memmove(vsz + a + 40, vsz + a, n);
        for (r = last; r > first; --r)
            gfx_copy_row(r - 1, r);
        clear_rows(first, 1);
    }
    zc_idx = 0xFFFF;
}

/* --- curseur -------------------------------------------------------- */

static void cursor_down(void)
{
    if (row == 0) {
        restore_state();
        return;
    }
    if (++row > 24) {
        if (page_mode) {
            row = 1;
        } else {
            row = 24;
            scroll(1, 24, 1);
        }
    }
}

static void cursor_up(void)
{
    if (row == 0)
        return;
    if (--row == 0) {
        if (page_mode) {
            row = 24;
        } else {
            row = 1;
            scroll(1, 24, 0);
        }
    }
}

static void cursor_left(void)
{
    if (col == 0) {
        col = 39;
        cursor_up();
    } else {
        --col;
    }
}

static void cursor_right(void)
{
    if (++col > 39) {
        col = 0;
        cursor_down();
    }
}

/* Avance après un caractère de largeur w (1 ou 2), hauteur h. */
static void advance(unsigned char w, unsigned char h)
{
    col += w;
    if (col > 39) {
        if (row == 0) {
            col = 39;
        } else {
            col = 0;
            cursor_down();
            if (h == 2)
                cursor_down();
        }
    }
}

static void locate(unsigned char y, unsigned char x)
{
    if (y >= 0x30 && y <= 0x32) {     /* forme ancienne : rangée en décimal */
        y = 10 * (y - 0x30) + (x - 0x30);
        x = 1;
    } else {
        x -= 0x40;
        y -= 0x40;
    }
    if (x < 1 || x > 40 || y > 24)
        return;
    if (y == 0) {
        save_state();
        vdt_cursor_on = 0;
    }
    row = y;
    col = x - 1;
    reset_attr();
}

/* --- affichage d'un caractère --------------------------------------- */

static void print(unsigned char c)
{
    unsigned char w = 1, h = 1, at, sz;
    if (g1) {
        /* pavés : bits 0-4 et bit 6 (le bit 5 ne compte pas) */
        c = (c & 0x1F) | ((c & 0x40) >> 1);
        put_cell(row, col, c, fg | (blink ? A_BLINK : 0) | T_MOS | (sep ? A_SEP : 0), bg, 0);
    } else {
        unsigned char delim = (c == 0x20) && (w_bg != 0xFF || w_ul != 0xFF || w_mask != 0xFF);
        if (size & F_DW) w = 2;
        if (size & F_DH) h = 2;
        at = fg | (inv ? A_INV : 0);
        if (delim) {
            if (w_bg != 0xFF) { bg = w_bg; w_bg = 0xFF; }
            if (w_ul != 0xFF) { ul = w_ul; w_ul = 0xFF; }
            if (w_mask != 0xFF) { mask = w_mask; w_mask = 0xFF; }
            at |= T_DELIM;
        } else {
            at |= T_CHAR | (blink ? A_BLINK : 0);
        }
        /* cases occupées : la rangée du curseur est la moitié basse */
        if (h == 2) {
            sz = (size & (F_DW | F_DH)) | F_TOP;
            put_cell(row - 1, col, c, at, bg | (mask ? B_MASK : 0) | (ul ? B_ZUL : 0), sz);
            if (w == 2)
                put_cell(row - 1, col + 1, c, at, bg | (mask ? B_MASK : 0) | (ul ? B_ZUL : 0), sz | F_RIGHT);
        }
        sz = size & (F_DW | F_DH);
        put_cell(row, col, c, at, bg | (mask ? B_MASK : 0) | (ul ? B_ZUL : 0), sz);
        if (w == 2)
            put_cell(row, col + 1, c, at, bg | (mask ? B_MASK : 0) | (ul ? B_ZUL : 0), sz | F_RIGHT);
    }
    advance(w, h);
}

/* --- automate de décodage ------------------------------------------- */

enum {
    S_NORMAL, S_ESC, S_US1, S_US2, S_REP, S_SS2, S_SS2ACC, S_SKIP1,
    S_PRO, S_CSI, S_HASH, S_HASH2, S_DESIG, S_DESIG2, S_DRCS
};
static unsigned char state;
static unsigned char arg, pro_n, pro_i, pro[3];
static unsigned char csi_p[2], csi_n;

static void set_reply(const char *s, unsigned char n)
{
    if (vdt_reply_len + n <= sizeof(vdt_reply)) {
        memcpy(vdt_reply + vdt_reply_len, s, n);
        vdt_reply_len += n;
    }
}

static void do_pro(void)
{
    if (pro_n == 1) {
        if (pro[0] == 0x7B)                       /* ENQROM : identification */
            set_reply("\x01" "Cu;" "\x04", 5);    /* Minitel 1B */
    } else if (pro_n == 2) {
        if (pro[0] == 0x69 || pro[0] == 0x6A) {   /* début / fin de fonction */
            unsigned char on = pro[0] == 0x69;
            if (pro[1] == 0x43)
                page_mode = !on;                  /* mode rouleau */
            else if (pro[1] == 0x45)
                vdt_lower = on;                   /* clavier minuscules */
        }
    } else {
        if ((pro[0] == 0x60 || pro[0] == 0x61) && pro[1] == 0x58 && pro[2] == 0x51)
            vdt_echo = pro[0] == 0x61;            /* aiguillage clavier -> écran */
    }
}

/* Séquences CSI (ESC [) : déplacements, effacements, insertions. */
static void do_csi(unsigned char f)
{
    unsigned char n = csi_p[0], i;
    unsigned int a;
    if (row == 0)
        return;
    switch (f) {
    case 'A': if (!n) n = 1; row = (row > n) ? row - n : 1; break;
    case 'B': if (!n) n = 1; row = (row + n > 24) ? 24 : row + n; break;
    case 'C': if (!n) n = 1; col = (col + n > 39) ? 39 : col + n; break;
    case 'D': if (!n) n = 1; col = (col > n) ? col - n : 0; break;
    case 'H':
    case 'f':
        row = csi_p[0] ? csi_p[0] : 1;
        col = csi_p[1] ? csi_p[1] - 1 : 0;
        if (row > 24) row = 24;
        if (col > 39) col = 39;
        break;
    case 'J':
        if (n == 0) {
            clear_span(row, col, 39);
            if (row < 24) clear_rows(row + 1, 24 - row);
        } else if (n == 1) {
            if (row > 1) clear_rows(1, row - 1);
            clear_span(row, 0, col);
        } else {
            clear_rows(1, 24);
        }
        break;
    case 'K':
        if (n == 0) clear_span(row, col, 39);
        else if (n == 1) clear_span(row, 0, col);
        else clear_span(row, 0, 39);
        break;
    case 'P':                                     /* supprime n caractères */
    case '@':                                     /* insère n espaces */
        if (!n) n = 1;
        if (n > 40 - col) n = 40 - col;
        a = rowoff[row];
        if (f == 'P') {
            i = 40 - col - n;
            memmove(vch + a + col, vch + a + col + n, i);
            memmove(vat + a + col, vat + a + col + n, i);
            memmove(vbg + a + col, vbg + a + col + n, i);
            memmove(vsz + a + col, vsz + a + col + n, i);
            clear_cells(a + 40 - n, n);
        } else {
            i = 40 - col - n;
            memmove(vch + a + col + n, vch + a + col, i);
            memmove(vat + a + col + n, vat + a + col, i);
            memmove(vbg + a + col + n, vbg + a + col, i);
            memmove(vsz + a + col + n, vsz + a + col, i);
            clear_cells(a + col, n);
        }
        redraw_row(row);
        break;
    case 'M':                                     /* supprime n rangées */
    case 'L':                                     /* insère n rangées */
        if (!n) n = 1;
        for (i = 0; i < n && i <= 24 - row; ++i)
            scroll(row, 24, f == 'M');
        break;
    }
}

/* Caractères obtenus par SS2 (G2) : code interne de la police. */
static unsigned char g2_simple(unsigned char c)
{
    switch (c) {
    case 0x23: return 0x03;   /* £ */
    case 0x24: return 0x24;   /* $ */
    case 0x26: return 0x23;   /* # */
    case 0x2C: return 0x0C;   /* ← */
    case 0x2D: return 0x5E;   /* ↑ */
    case 0x2E: return 0x0E;   /* → */
    case 0x2F: return 0x0F;   /* ↓ */
    case 0x30: return 0x10;   /* ° */
    case 0x31: return 0x11;   /* ± */
    case 0x38: return 0x18;   /* ÷ */
    case 0x3C: return 0x1C;   /* ¼ */
    case 0x3D: return 0x1D;   /* ½ */
    case 0x3E: return 0x1E;   /* ¾ */
    case 0x6A: return 0x0A;   /* Œ */
    case 0x7A: return 0x1A;   /* œ */
    }
    return 0x5F;
}

static unsigned char g2_accent(unsigned char acc, unsigned char c)
{
    switch (acc) {
    case 0x41:                /* grave */
        if (c == 'a') return 0x17;
        if (c == 'e') return 0x19;
        if (c == 'u') return 0x08;
        if (c == 'A') return 0x07;
        if (c == 'E') return 0x09;
        break;
    case 0x42:                /* aigu */
        if (c == 'e') return 0x12;
        if (c == 'E') return 0x02;
        break;
    case 0x43:                /* circonflexe */
        if (c == 'a') return 0x04;
        if (c == 'e') return 0x1B;
        if (c == 'i') return 0x0D;
        if (c == 'o') return 0x1F;
        if (c == 'u') return 0x16;
        if (c == 'A') return 0x01;
        if (c == 'E') return 0x0B;
        break;
    case 0x48:                /* tréma */
        if (c == 'e') return 0x13;
        if (c == 'i') return 0x14;
        if (c == 'E') return 0x06;
        break;
    case 0x4B:                /* cédille */
        if (c == 'c') return 0x15;
        if (c == 'C') return 0x05;
        break;
    }
    return c;                 /* lettre sans accent */
}

static void control(unsigned char c)
{
    switch (c) {
    case 0x07: vdt_bell = 1; break;
    case 0x08: cursor_left(); break;
    case 0x09: cursor_right(); break;
    case 0x0A: cursor_down(); break;
    case 0x0B: cursor_up(); break;
    case 0x0C:
        clear_rows(1, 24);
        row = 1; col = 0;
        reset_attr();
        break;
    case 0x0D: col = 0; break;
    case 0x0E:                                        /* SO : G1 */
        g1 = 1; sep = 0; inv = 0; size = 0; ul = 0;
        if (w_bg != 0xFF) { bg = w_bg; w_bg = 0xFF; }
        break;
    case 0x0F:                                        /* SI : G0 */
        g1 = 0; sep = 0; inv = 0; size = 0;
        break;
    case 0x11: vdt_cursor_on = 1; break;
    case 0x12: state = S_REP; break;
    case 0x13: state = S_SKIP1; break;
    case 0x14: vdt_cursor_on = 0; break;
    case 0x16:
    case 0x19: state = S_SS2; break;
    case 0x18: {                                      /* CAN : fin de rangée */
        unsigned char r = row, k = col, sz = size, pm = page_mode, n = 40 - col;
        size = 0;
        page_mode = 1;
        while (n--)
            print(0x20);
        size = sz;
        page_mode = pm;
        row = r; col = k;
        break;
    }
    case 0x1A: print(0x7F); break;
    case 0x1B: state = S_ESC; break;
    case 0x1E:
        row = 1; col = 0;
        reset_attr();
        break;
    case 0x1F: state = S_US1; break;
    }
}

static void esc(unsigned char c)
{
    state = S_NORMAL;
    if (c >= 0x40 && c <= 0x47) {
        fg = c & 7;
    } else if (c >= 0x50 && c <= 0x57) {
        if (g1) bg = c & 7; else w_bg = c & 7;
    } else switch (c) {
    case 0x48: blink = 1; break;
    case 0x49: blink = 0; break;
    case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        if (row != 0 && !g1) {
            unsigned char s = 0;
            if (c == 0x4D || c == 0x4F) s |= F_DH;
            if (c == 0x4E || c == 0x4F) s |= F_DW;
            if (!((s & F_DH) && row == 1))
                size = s;
        }
        break;
    case 0x58: w_mask = 1; break;
    case 0x5F: w_mask = 0; break;
    case 0x59: if (g1) sep = 0; else w_ul = 0; break;
    case 0x5A: if (g1) sep = 1; else w_ul = 1; break;
    case 0x5C: if (!g1) inv = 0; break;
    case 0x5D: if (!g1) inv = 1; break;
    case 0x5B: state = S_CSI; csi_p[0] = csi_p[1] = 0; csi_n = 0; break;
    case 0x39: pro_n = 1; pro_i = 0; state = S_PRO; break;
    case 0x3A: pro_n = 2; pro_i = 0; state = S_PRO; break;
    case 0x3B: pro_n = 3; pro_i = 0; state = S_PRO; break;
    case 0x23: state = S_HASH; break;
    case 0x28: case 0x29: state = S_DESIG; break;
    case 0x61: {                                       /* position du curseur */
        unsigned char r[3];
        r[0] = 0x1F; r[1] = 0x40 + row; r[2] = 0x41 + col;
        set_reply((const char *)r, 3);
        break;
    }
    case 0x1B: state = S_ESC; break;
    }
}

void vdt_put(unsigned char c)
{
    c &= 0x7F;
    switch (state) {
    case S_NORMAL:
        if (c < 0x20) control(c);
        else { print(c); last_char = c; }
        return;
    case S_ESC:
        esc(c);
        return;
    case S_US1:
        if (c == 0x23) { state = S_DRCS; return; }
        arg = c; state = S_US2;
        return;
    case S_US2:
        state = S_NORMAL;
        locate(arg, c);
        return;
    case S_REP:
        state = S_NORMAL;
        if (c >= 0x40) {
            c -= 0x40;
            while (c--)
                print(last_char);
        }
        return;
    case S_SS2:
        if (c == 0x41 || c == 0x42 || c == 0x43 || c == 0x48 || c == 0x4B) {
            arg = c; state = S_SS2ACC;
            return;
        }
        state = S_NORMAL;
        if (!g1) { last_char = g2_simple(c); print(last_char); }
        return;
    case S_SS2ACC:
        state = S_NORMAL;
        if (!g1) { last_char = g2_accent(arg, c); print(last_char); }
        return;
    case S_SKIP1:
        state = S_NORMAL;
        return;
    case S_PRO:
        pro[pro_i++] = c;
        if (pro_i == pro_n) { state = S_NORMAL; do_pro(); }
        return;
    case S_CSI:
        if (c >= '0' && c <= '9') {
            csi_p[csi_n] = csi_p[csi_n] * 10 + (c - '0');
        } else if (c == ';') {
            if (csi_n == 0) csi_n = 1;
        } else if (c >= 0x40) {
            state = S_NORMAL;
            do_csi(c);
        } else if (c < 0x20) {
            state = S_NORMAL;
            control(c);
        }
        return;
    case S_HASH:
        state = (c == 0x20) ? S_HASH2 : S_NORMAL;
        return;
    case S_HASH2:
        state = S_NORMAL;
        if (c == 0x58 || c == 0x5F) {
            unsigned char r;
            global_mask = c == 0x58;
            for (r = 0; r < 25; ++r)
                redraw_row(r);
        }
        return;
    case S_DESIG:
        state = (c == 0x20) ? S_DESIG2 : S_NORMAL;
        return;
    case S_DESIG2:
        state = S_NORMAL;
        return;
    case S_DRCS:                       /* jeux redéfinis : ignorés pour l'instant */
        if (c == 0x1F) state = S_US1;
        return;
    }
}

void vdt_write(const unsigned char *p, unsigned int n)
{
    while (n--)
        vdt_put(*p++);
}

void vdt_reset(void)
{
    state = S_NORMAL;
    page_mode = 1;
    global_mask = 1;
    blink_phase = 0;
    vdt_lower = 0;
    vdt_echo = 1;                     /* comme un Minitel : écho local tant que
                                         le serveur ne le coupe pas (PRO3) */
    vdt_cursor_on = 0;
    vdt_reply_len = 0;
    last_char = 0x20;
    reset_attr();
    save_state();
    clear_rows(0, 25);
    row = 1; col = 0;
}

void vdt_cursor_update(void)
{
    cursor_place(row, col);
    cursor_show(vdt_cursor_on);
}

void vdt_blink_tick(void)
{
    unsigned int i;
    unsigned char rr, cc;
    blink_phase ^= 1;
    zc_idx = 0xFFFF;
    for (i = 0, rr = 0, cc = 0; i < NCELLS; ++i) {
        if (vat[i] & A_BLINK) {
            r_row = rr; r_col = cc; r_idx = i;
            render();
        }
        if (++cc == 40) { cc = 0; ++rr; }
    }
}
