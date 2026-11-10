/* Kiosque 64 modem : connexion par un modem WiFi branché sur le port
   utilisateur, avec les seules commandes AT communes aux modems WiFi pour C64
   (ATDT hôte:port, CONNECT, NO CARRIER, +++ puis ATH).

   Ce fichier remplace uci.c dans l'édition modem et en garde l'interface
   (uci.h) : les couches telnet et WebSocket de net.c restent les mêmes. Le
   modem transmet les octets tels quels, comme une connexion TCP brute ; s'il
   gère lui-même le telnet, net.c reçoit simplement des données sans IAC. */
#include <string.h>
#include <cbm.h>
#include "charmap.h"
#include "uci.h"
#include "modem.h"

extern unsigned char rs_iface;
void __fastcall__ rs_start(unsigned char speed);
void rs_stop(void);
int rs_getc(void);
void __fastcall__ rs_putc(unsigned char c);
unsigned char rs_txbusy(void);

#define JIFFY (*(volatile unsigned char *)0xA2)

struct modem_cfg mcfg = { 'L', 2, "", "ATDT", IF_USER };
/* User Port : 2400 ; UP9600 : 9600 ; SwiftLink : 38 400 */
const unsigned char speed_max[IF_COUNT] = { 2, 4, 6, 6 };
const unsigned char speed_def[IF_COUNT] = { 2, 4, 4, 4 };
unsigned char uci_closed;
char uci_status[40];

static unsigned char on;                /* pilote RS-232 démarré */
static int pending = -1;                /* octet lu d'avance */
static unsigned char nc;                /* repérage de « NO CARRIER » */
static const char nocarrier[] = "NO CARRIER";
static char line[40];

static void pause(unsigned char jiffies);
static void drain(void);

static void driver_on(void)
{
    if (!on) {
        rs_iface = mcfg.iface;
        rs_start(mcfg.speed);
        on = 1;
        /* un retour chariot pour démarrer proprement la ligne (UP9600 :
           le premier caractère fixe le niveau de repos) ; la réponse
           éventuelle du modem est ignorée */
        rs_putc('\r');
        pause(20);
        drain();
    }
}

void modem_off(void)
{
    if (on) {
        while (rs_txbusy())
            ;
        rs_stop();
        on = 0;
    }
}

/* Commande AT en majuscules (les modems Hayes n'acceptent pas tous les
   minuscules ; les noms d'hôtes n'en dépendent pas). */
static void send_str(const char *s)
{
    unsigned char c;
    while ((c = *s++) != 0) {
        if (c >= 'a' && c <= 'z')
            c -= 32;
        rs_putc(c);
    }
}

static void pause(unsigned char jiffies)
{
    unsigned char t = JIFFY;
    while ((unsigned char)(JIFFY - t) < jiffies)
        ;
}

static void drain(void)
{
    pending = -1;
    while (rs_getc() >= 0)
        ;
}

/* Réponses du modem : 1 OK, 2 CONNECT, 3 échec, 0 rien de connu.
   Certains firmwares renvoient l'écho de la commande sans le retour chariot :
   la réponse arrive alors collée à l'écho (« ATOK », « ATDT...CONNECTING... »).
   On cherche donc les mots clés n'importe où dans la ligne. */
static unsigned char classify(void)
{
    static const char *const fails[] = { "NO CARRIER", "ERROR", "BUSY", "NO ANSWER", "NO DIALTONE" };
    unsigned char i, n = strlen(line);
    const char *p;
    for (i = 0; i < sizeof(fails) / sizeof(fails[0]); ++i) {
        p = strstr(line, fails[i]);
        if (p) {
            strcpy(uci_status, p);
            return 3;
        }
    }
    for (p = line; (p = strstr(p, "CONNECT")) != 0; p += 7)
        if (strncmp(p + 7, "ING", 3) != 0)
            return 2;
    if (n >= 2 && line[n - 2] == 'O' && line[n - 1] == 'K')
        return 1;
    return 0;                           /* écho seul, CONNECTING, RING, etc. */
}

/* Attend une réponse connue, au plus « jiffies » soixantièmes de seconde. */
static unsigned char wait_reply(unsigned int jiffies)
{
    unsigned char n = 0, t = JIFFY, r;
    unsigned int el = 0;
    int c;
    for (;;) {
        c = rs_getc();
        if (c < 0) {
            el += (unsigned char)(JIFFY - t);
            t = JIFFY;
            if (el > jiffies)
                return 0;
            continue;
        }
        if (c == '\r' || c == '\n') {
            line[n] = 0;
            n = 0;
            r = classify();
            if (r == 2 && c == '\r') {
                /* « CONNECT\r\n » : on retire le \n, la suite est déjà au serveur */
                pause(3);
                c = rs_getc();
                if (c >= 0 && c != '\n')
                    pending = c;
            }
            if (r)
                return r;
        } else if (n < sizeof(line) - 1) {
            line[n++] = (char)c;
        }
    }
}

static unsigned char at(const char *cmd, unsigned int jiffies)
{
    send_str(cmd);
    rs_putc('\r');
    return wait_reply(jiffies);
}

/* Surveille « NO CARRIER » dans ce qui arrive (fin de connexion). */
static void watch(unsigned char c)
{
    if (c == nocarrier[nc]) {
        if (++nc == sizeof(nocarrier) - 1) {
            uci_closed = 1;
            nc = 0;
        }
    } else {
        nc = (c == 'N');
    }
}

/* Repasse en mode commande si le modem était resté connecté. Retourne 0 si
   le serveur a raccroché pendant la seconde de silence : inutile alors. */
static unsigned char escape(void)
{
    unsigned char t;
    int c;
    while (rs_txbusy())
        ;
    t = JIFFY;                          /* 1 s de silence avant... */
    while ((unsigned char)(JIFFY - t) < 70) {
        c = rs_getc();
        if (c >= 0)
            watch((unsigned char)c);
    }
    if (uci_closed)
        return 0;
    send_str("+++");
    wait_reply(150);                    /* ... et après ; le modem répond OK */
    return 1;
}

/* Le modem répond-il à « AT » ? */
unsigned char uci_present(void)
{
    driver_on();
    drain();
    if (at("AT", 90) == 1)
        return 1;
    uci_closed = 0;
    if (escape())                       /* peut-être encore en ligne */
        at("ATH", 90);
    drain();
    if (at("AT", 90) == 1)
        return 1;
    modem_off();
    return 0;
}

unsigned char uci_tcp_open(const char *host, unsigned int port)
{
    char num[6];
    unsigned char i = 0, r;
    uci_status[0] = 0;
    if (mcfg.init[0])
        at(mcfg.init, 120);
    drain();
    send_str(mcfg.dial);
    send_str(host);
    rs_putc(':');
    do { num[i++] = '0' + port % 10; port /= 10; } while (port);
    while (i) rs_putc(num[--i]);
    rs_putc('\r');
    r = wait_reply(30 * 60);
    if (r == 2) {
        uci_closed = 0;
        nc = 0;
        return 0;
    }
    if (!uci_status[0])
        strcpy(uci_status, "NO ANSWER");
    modem_off();
    return 0xFF;
}

unsigned int uci_read(unsigned char sock, unsigned char *buf, unsigned int max)
{
    unsigned int n = 0;
    int c;
    (void)sock;
    /* Petits lots : entre deux lots, net.c envoie ses réponses (négociation
       telnet, pong WebSocket) et main.c lit le clavier. Sinon, à 38 400
       bauds, un lot de 512 octets de texte à afficher retarde la réponse
       telnet au-delà de ce qu'attend le serveur. */
    if (max > 64)
        max = 64;
    if (pending >= 0 && max) {
        buf[n++] = (unsigned char)pending;
        pending = -1;
    }
    while (n < max && (c = rs_getc()) >= 0) {
        buf[n++] = (unsigned char)c;
        watch((unsigned char)c);        /* « NO CARRIER » : fin de connexion */
    }
    return n;
}

unsigned char uci_write(unsigned char sock, const unsigned char *buf, unsigned int n)
{
    (void)sock;
    while (n--)
        rs_putc(*buf++);
    return 0;
}

void uci_close(unsigned char sock)
{
    (void)sock;
    if (!on)
        return;
    if (!uci_closed && escape())
        at("ATH", 120);
    drain();
    modem_off();
}

unsigned char uci_ipaddr(unsigned char *ip4)
{
    (void)ip4;
    return 0;
}

/* --- réglages sur disque ------------------------------------------- */

unsigned char modem_load(void)
{
    struct modem_cfg c;
    int n;
    modem_off();
    if (cbm_open(2, 8, 2, "KIOSQUE.CFG,S,R"))
        return 1;
    n = cbm_read(2, &c, sizeof(c));
    cbm_close(2);
    if (n == sizeof(c) - 1 && c.magic == 'K') {
        /* réglages de la version 0.1 : User Port, mêmes index de vitesse */
        c.magic = 'L';
        c.iface = IF_USER;
    } else if (n != sizeof(c) || c.magic != 'L') {
        return 1;
    }
    if (c.iface >= IF_COUNT || c.speed > speed_max[c.iface])
        return 1;
    c.init[sizeof(c.init) - 1] = 0;
    c.dial[sizeof(c.dial) - 1] = 0;
    mcfg = c;
    return 0;
}

unsigned char modem_save(void)
{
    int n;
    modem_off();
    if (cbm_open(2, 8, 2, "@0:KIOSQUE.CFG,S,W"))
        return 1;
    n = cbm_write(2, &mcfg, sizeof(mcfg));
    cbm_close(2);
    return n != sizeof(mcfg);
}
