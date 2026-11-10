/* Kiosque 64 : terminal Minitel (Vidéotex) et BBS pour le C64 Ultimate.
   (c) 2026 LABBEJ27 - licence MIT. */
#include <string.h>
#include <cbm.h>
#include "charmap.h"
#include "vdt.h"
#include "net.h"
#include "uci.h"
#include "screens.h"
#ifdef MODEM
#include "modem.h"
#endif

void gfx_init(void);
void gfx_text(void);
void term_run(unsigned char ascii);

#define JIFFY (*(volatile unsigned char *)0xA2)
#define SIDR ((volatile unsigned char *)0xD400)

/* --- carnet d'adresses --------------------------------------------- */
struct site {
    const char *host;
    unsigned int port;
    const char *path;
    unsigned char kind;      /* NET_TCP, NET_TELNET, NET_WS */
    unsigned char mode;      /* 0 Minitel, 1 PETSCII, 2 ASCII */
};
static const struct site sites[] = {
    { "go.minipavi.fr", 516, 0, NET_TELNET, 0 },
    { "minitel.labbej.fr", 8180, 0, NET_TELNET, 0 },
    { "3615co.de", 80, "/ws", NET_WS, 0 },
    { "3611.re", 80, "/ws", NET_WS, 0 },
    { "mntl.joher.com", 2018, "/", NET_WS, 0 },
    { 0, 0, 0, 0, 0 },                               /* 6 : autre Minitel */
    { "bbs.retrocampus.com", 6510, 0, NET_TELNET, 1 },
    { "minitel.labbej.fr", 2323, 0, NET_TELNET, 2 },
    { 0, 0, 0, 0, 1 },                               /* 9 : autre BBS */
};

static char host[40];
static char path[40];
static struct site custom;

/* --- petits utilitaires -------------------------------------------- */

static void vdt_str(const char *s)
{
    while (*s)
        vdt_put(*s++);
}

/* Crochet de test : quand l'outil tools/c64k.py écrit 2 en $02A7, on
   masque la ROM et on attend, pour qu'il puisse lire l'écran en $E000. */
#define TESTBOX (*(volatile unsigned char *)0x02A7)
static void test_hook(void)
{
    if (TESTBOX == 2) {
        __asm__("sei");
        *(volatile unsigned char *)1 = 0x35;
        TESTBOX = 3;
        while (TESTBOX == 3)
            ;
        *(volatile unsigned char *)1 = 0x36;
        __asm__("cli");
    }
}

static unsigned char getkey(void)
{
    test_hook();
    return cbm_k_getin();
}

static unsigned char waitkey(void)
{
    unsigned char k;
    while ((k = getkey()) == 0)
        vdt_cursor_update();
    return k;
}

static unsigned char bell_t;

static void bell_start(void)
{
    SIDR[24] = 15;                 /* volume */
    SIDR[5] = 0x08;                /* attaque 0, déclin court */
    SIDR[6] = 0x00;
    SIDR[0] = 0x00;                /* environ 1 kHz */
    SIDR[1] = 0x44;
    SIDR[4] = 0x11;                /* triangle, porte ouverte */
    bell_t = JIFFY;
    vdt_bell = 0;
}

static void bell_poll(void)
{
    if (SIDR[4] & 1 && (unsigned char)(JIFFY - bell_t) > 8)
        SIDR[4] = 0x10;
}

/* PETSCII -> ASCII pour la saisie d'une adresse */
static unsigned char to_ascii(unsigned char k)
{
    if (k >= 0x41 && k <= 0x5A) return k + 0x20;      /* minuscules */
    if (k >= 0xC1 && k <= 0xDA) return k - 0x80;      /* majuscules */
    if (k >= 0x20 && k < 0x40) return k;
    if (k == 0x5F) return '_';
    return 0;
}

/* Saisie d'une ligne en rangée 23, affichée par le décodeur Vidéotex. */
static unsigned char read_line(char *buf, unsigned char max)
{
    unsigned char n = 0, k, c;
    for (;;) {
        k = waitkey();
        if (k == 0x0D) break;
        if (k == 0x03 || k == 0x8C) return 0;           /* RUN/STOP, F8 */
        if (k == 0x14) {
            if (n) { --n; vdt_str("\x08 \x08"); }
            continue;
        }
        c = to_ascii(k);
        if (c && n < max) {
            buf[n++] = c;
            vdt_put(c);
        }
    }
    buf[n] = 0;
    return n;
}

/* « ws://hôte:port/chemin » ou « hôte:port » */
static unsigned char parse_address(char *s, unsigned char bbs)
{
    char *p;
    unsigned int port = 0;
    custom.kind = NET_TELNET;
    custom.mode = bbs;
    custom.path = 0;
    if (strncmp(s, "ws://", 5) == 0) {
        s += 5;
        custom.kind = NET_WS;
        custom.mode = 0;
        port = 80;
    } else if (strncmp(s, "telnet://", 9) == 0) {
        s += 9;
    }
    p = strchr(s, '/');
    if (p) {
        strcpy(path, p);
        custom.path = path;
        *p = 0;
    }
    p = strchr(s, ':');
    if (p) {
        *p++ = 0;
        port = 0;
        while (*p >= '0' && *p <= '9')
            port = port * 10 + (*p++ - '0');
    }
    if (!port)
        port = bbs ? 23 : 516;
    strcpy(host, s);
    custom.host = host;
    custom.port = port;
    return *host != 0;
}

/* --- clavier Minitel ----------------------------------------------- */

static unsigned char out[8], outn;

static void key_fn(unsigned char f)
{
    out[0] = 0x13;                /* SEP + lettre de la touche de fonction */
    out[1] = f;
    outn = 2;
}

static void key_ss2(unsigned char acc, unsigned char c)
{
    out[0] = 0x19;
    out[1] = acc;
    out[2] = c;
    outn = c ? 3 : 2;
}

/* Lettres accentuées avec C= (comme dans Plume) */
static const unsigned char cbm_keys[] = { 0xB1, 0xB2, 0xA3, 0xB0, 0xAB, 0xBC, 0xB8, 0xB7, 0xB9, 0xA2, 0xA1, 0xB6, 0xAF };
static const unsigned char cbm_acc[] = { 0x42, 0x41, 0x43, 0x41, 0x43, 0x4B, 0x41, 0x43, 0x43, 0x43, 0x48, 0x48, 0x7A };
static const unsigned char cbm_base[] = { 'e', 'e', 'e', 'a', 'a', 'c', 'u', 'u', 'o', 'i', 'i', 'e', 0 };

/* Retourne 1 si la touche demande le retour au menu. */
static unsigned char minitel_key(unsigned char k)
{
    unsigned char i;
    outn = 0;
    switch (k) {
    case 0x0D: key_fn('A'); break;            /* Envoi */
    case 0x14: key_fn('G'); break;            /* Correction */
    case 0x85: key_fn('F'); break;            /* F1 Sommaire */
    case 0x89: key_fn('D'); break;            /* F2 Guide */
    case 0x86: key_fn('B'); break;            /* F3 Retour */
    case 0x8A: key_fn('C'); break;            /* F4 Répétition */
    case 0x87: key_fn('H'); break;            /* F5 Suite */
    case 0x8B: key_fn('E'); break;            /* F6 Annulation */
    case 0x88: key_fn('I'); break;            /* F7 Connexion/Fin */
    case 0x8C: return 1;                      /* F8 : menu */
    case 0x11: key_fn('H'); break;            /* curseur bas : Suite */
    case 0x91: key_fn('B'); break;            /* curseur haut : Retour */
    case 0x9D: key_fn('F'); break;            /* curseur gauche : Sommaire */
    case 0x1D: key_fn('C'); break;            /* curseur droite : Répétition */
    case 0x5C: key_ss2(0x23, 0); break;       /* £ */
    case 0xA0: out[0] = ' '; outn = 1; break;
    default:
        for (i = 0; i < sizeof(cbm_keys); ++i) {
            if (k == cbm_keys[i]) {
                key_ss2(cbm_acc[i], cbm_base[i]);
                return 0;
            }
        }
        if (k >= 0x41 && k <= 0x5A) {         /* lettre sans Shift */
            out[0] = vdt_lower ? k + 0x20 : k;
            outn = 1;
        } else if (k >= 0xC1 && k <= 0xDA) {  /* lettre avec Shift */
            out[0] = vdt_lower ? k - 0x80 : k - 0x60;
            outn = 1;
        } else if (k >= 0x20 && k < 0x40) {
            out[0] = k; outn = 1;
        } else if (k == 0x40 || k == 0x5B || k == 0x5D) {
            out[0] = k; outn = 1;
        } else if (k == 0x5E) {
            out[0] = '^'; outn = 1;
        } else if (k == 0x5F) {
            out[0] = '_'; outn = 1;
        }
    }
    return 0;
}

/* --- session Minitel ----------------------------------------------- */

static void minitel_session(void)
{
    unsigned char k, t_cur = JIFFY, t_blink = JIFFY, cur_vis = 1, closed;
    for (;;) {
        closed = net_poll();
        if (vdt_reply_len) {
            net_send(vdt_reply, vdt_reply_len);
            vdt_reply_len = 0;
        }
        if (vdt_bell)
            bell_start();
        bell_poll();
        if (closed) {
            vdt_str(MSG_CLOSED);
            waitkey();
            return;
        }
        k = getkey();
        if (k) {
            if (minitel_key(k))
                return;
            if (outn) {
                net_send(out, outn);
                if (vdt_echo && out[0] != 0x13)
                    vdt_write(out, outn);
            }
        }
        /* curseur clignotant, attributs clignotants */
        if ((unsigned char)(JIFFY - t_cur) >= 20) {
            t_cur = JIFFY;
            cur_vis ^= 1;
            vdt_cursor_update();
            if (!cur_vis)
                *(volatile unsigned char *)0xD015 &= 0xFE;
        }
        if ((unsigned char)(JIFFY - t_blink) >= 30) {
            t_blink = JIFFY;
            vdt_blink_tick();
        }
    }
}

static void connect_to(const struct site *s)
{
    unsigned char e;
    vdt_reset();
    vdt_str(MSG_CONNECTING);
    vdt_str(s->host);
    net_sink = vdt_put;
    if (s->mode != 0)
        net_sink = 0;                         /* le terminal choisit le sien */
    e = net_open(s->host, s->port, s->path, s->kind);
    if (e) {
        vdt_str(e == NET_ERR_NOUCI ? MSG_NOUCI : e == NET_ERR_WS ? MSG_WS : MSG_CONNECT);
        waitkey();
        return;
    }
    if (s->mode == 0) {
        vdt_reset();
        minitel_session();
    } else {
        term_run(s->mode == 2);
        gfx_init();
    }
    net_close();
}

#ifdef MODEM
/* --- réglages du modem --------------------------------------------- */

static void vdt_at(unsigned char row, unsigned char col, unsigned char color)
{
    vdt_put(0x1F);
    vdt_put(0x40 + row);
    vdt_put(0x40 + col);
    vdt_put(0x1B);
    vdt_put(0x40 + color);
}

static void show_val(unsigned char row, unsigned char col, const char *s)
{
    vdt_at(row, col, 7);
    vdt_str(*s ? s : MSG_M_EMPTY);
    vdt_put(0x18);                      /* efface la fin de la rangée */
}

/* Saisie d'une valeur ; RUN/STOP ou F8 la laisse inchangée. */
static void edit_val(unsigned char row, unsigned char col, char *dst, unsigned char max)
{
    char buf[31];
    vdt_at(row, col, 7);
    vdt_put(0x18);
    vdt_put(0x11);                      /* curseur visible */
    buf[0] = 1;
    read_line(buf, max);
    vdt_put(0x14);
    if (buf[0] != 1)
        strcpy(dst, buf);
}

static const char *const ifaces[IF_COUNT] = { "User Port", "UP9600", "SwiftLink $DE00", "SwiftLink $DF00" };
static const char *const speeds[SPEED_COUNT] = { "300", "1200", "2400", "4800", "9600", "19200", "38400" };
#ifdef LANG_EN
#define BAUD " baud"
#else
#define BAUD " bauds"
#endif

static void modem_settings(void)
{
    unsigned char k;
    vdt_reset();
    vdt_str(SCR_MODEM);
    for (;;) {
        show_val(5, 20, ifaces[mcfg.iface]);
        vdt_at(7, 20, 7);
        vdt_str(speeds[mcfg.speed]);
        vdt_str(BAUD);
        vdt_put(0x18);
        show_val(10, 5, mcfg.init);
        show_val(12, 20, mcfg.dial);
        k = waitkey();
        if (k == 0x8C || k == 0x03 || k == 0x5F)
            return;
        if (k == '1') {                         /* interface suivante */
            mcfg.iface = mcfg.iface + 1 == IF_COUNT ? IF_USER : mcfg.iface + 1;
            mcfg.speed = speed_def[mcfg.iface];
        } else if (k == '2') {                  /* vitesse suivante */
            mcfg.speed = mcfg.speed >= speed_max[mcfg.iface] ? 0 : mcfg.speed + 1;
        } else if (k == '3') {
            edit_val(10, 5, mcfg.init, sizeof(mcfg.init) - 1);
        } else if (k == '4') {
            edit_val(12, 20, mcfg.dial, sizeof(mcfg.dial) - 1);
            if (!mcfg.dial[0])
                strcpy(mcfg.dial, "ATDT");
        } else if (k == '5') {
            vdt_at(15, 5, 7);
            vdt_str("...\x18");
            k = uci_present();
            modem_off();
            vdt_at(15, 5, k ? 2 : 1);
            vdt_str(k ? MSG_M_OK : MSG_M_NO);
            vdt_put(0x18);
        } else if (k == 'E' || k == 'e' || k == 0xC5) {
            k = modem_save();
            vdt_at(18, 5, k ? 1 : 2);
            vdt_str(k ? MSG_M_NOSAVE : MSG_M_SAVED);
            vdt_put(0x18);
        }
    }
}
#endif

int main(void)
{
    unsigned char k;
    TESTBOX = 0;
#ifdef MODEM
    modem_load();
#endif
    gfx_init();
    for (;;) {
        vdt_reset();
        vdt_str(SCR_MENU);
        k = waitkey();
        if (k >= '1' && k <= '9') {
            const struct site *s = &sites[k - '1'];
            if (!s->host) {
                vdt_str(s->mode ? MSG_ASK_BBS : MSG_ASK_MT);
                vdt_str(MSG_ASK);
                if (!read_line(host, 30) || !parse_address(host, s->mode))
                    continue;
                s = &custom;
            }
            connect_to(s);
        } else if (k == 'G' || k == 'g' || k == 0xC7) {
            vdt_grey ^= 1;
        }
#ifdef MODEM
        else if (k == 'M' || k == 'm' || k == 0xCD) {
            modem_settings();
        }
#endif
    }
    return 0;
}
