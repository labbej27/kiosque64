/* Kiosque 64 : pilote du Command Interface du C64 Ultimate (cible réseau 3).
   Écrit d'après la documentation publique « Ultimate-II Command Interface ». */
#include "charmap.h"
#include "uci.h"

#define CTRL   (*(volatile unsigned char *)0xDF1C)  /* écriture : contrôle */
#define STAT   (*(volatile unsigned char *)0xDF1C)  /* lecture : état */
#define CMD    (*(volatile unsigned char *)0xDF1D)  /* écriture : commande */
#define IDREG  (*(volatile unsigned char *)0xDF1D)  /* lecture : $C9 */
#define RESP   (*(volatile unsigned char *)0xDF1E)  /* données de réponse */
#define SDATA  (*(volatile unsigned char *)0xDF1F)  /* texte d'état */

#define PUSH_CMD 0x01
#define DATA_ACC 0x02
#define ABORT    0x04
#define CLR_ERR  0x08

#define ST_STATE 0x30
#define ST_IDLE  0x00
#define ST_BUSY  0x10
#define ST_ERROR 0x08
#define STAT_AV  0x40
#define DATA_AV  0x80

#define TARGET_NET 0x03
#define NET_GET_IPADDR 0x05
#define NET_OPEN_TCP   0x07
#define NET_CLOSE      0x09
#define NET_READ       0x10
#define NET_WRITE      0x11

#define JIFFY (*(volatile unsigned char *)0xA2)

unsigned char uci_closed;
static unsigned char sink;
char uci_status[40];

unsigned char uci_present(void)
{
    return IDREG == 0xC9;
}

/* Remet l'interface au repos si un échange précédent a été interrompu. */
static void uci_idle(void)
{
    unsigned char t;
    if (STAT & ST_ERROR)
        CTRL = CLR_ERR;
    if ((STAT & ST_STATE) == ST_IDLE)
        return;
    CTRL = ABORT;
    t = JIFFY;
    while ((STAT & ST_STATE) != ST_IDLE) {
        if ((unsigned char)(JIFFY - t) > 120)
            break;
    }
}

/* Pousse la commande déjà écrite, attend la réponse (2 s au plus). */
static unsigned char uci_exec(void)
{
    unsigned char t = JIFFY;
    CTRL = PUSH_CMD;
    /* Attend l'état « Data Last » ($20) ou « Data More » ($30). */
    while ((STAT & 0x20) == 0) {
        if ((unsigned char)(JIFFY - t) > 120) {
            CTRL = ABORT;
            return 1;
        }
    }
    return 0;
}

/* Lit le texte d'état (« 00,OK ») puis libère l'interface. */
static void uci_finish(void)
{
    unsigned char i = 0;
    while (STAT & DATA_AV)
        sink = RESP;
    while (STAT & STAT_AV) {
        char c = SDATA;
        if (i < sizeof(uci_status) - 1)
            uci_status[i++] = c;
    }
    uci_status[i] = 0;
    CTRL = DATA_ACC;
}

static unsigned char status_ok(void)
{
    return uci_status[0] == '0' && uci_status[1] == '0';
}

unsigned char uci_ipaddr(unsigned char *ip4)
{
    unsigned char i;
    uci_idle();
    CMD = TARGET_NET;
    CMD = NET_GET_IPADDR;
    CMD = 0;
    if (uci_exec())
        return 0;
    for (i = 0; i < 4 && (STAT & DATA_AV); ++i)
        ip4[i] = RESP;
    uci_finish();
    return i == 4 && status_ok();
}

unsigned char uci_tcp_open(const char *host, unsigned int port)
{
    unsigned char sock = 0xFF;
    uci_idle();
    uci_closed = 0;
    CMD = TARGET_NET;
    CMD = NET_OPEN_TCP;
    CMD = (unsigned char)port;
    CMD = (unsigned char)(port >> 8);
    while (*host)
        CMD = *host++;
    CMD = 0;
    if (uci_exec()) {
        uci_status[0] = 0;
        return 0xFF;
    }
    if (STAT & DATA_AV)
        sock = RESP;
    uci_finish();
    return status_ok() ? sock : 0xFF;
}

void uci_close(unsigned char sock)
{
    uci_idle();
    CMD = TARGET_NET;
    CMD = NET_CLOSE;
    CMD = sock;
    if (!uci_exec())
        uci_finish();
}

/* La connexion est-elle perdue ? L'Ultimate répond « 01,CONNECTION CLOSED
   BY HOST » quand le serveur ferme proprement, mais selon la façon dont la
   connexion se termine, il peut aussi répéter « 02,NO DATA: n » avec n le
   code d'erreur du réseau : 11 (EAGAIN) veut dire « rien pour l'instant »,
   tout autre code (128 ENOTCONN, 9 EBADF...) une connexion perdue. */
static void check_closed(void)
{
    const char *p;
    if (uci_status[0] != '0')
        return;
    if (uci_status[1] == '1') {
        uci_closed = 1;
        return;
    }
    if (uci_status[1] != '2')
        return;
    for (p = uci_status; *p && *p != ':'; ++p)
        ;
    if (*p != ':')
        return;                       /* pas de code : rien à conclure */
    while (*++p == ' ')
        ;
    if (*p >= '0' && *p <= '9' && !(p[0] == '1' && p[1] == '1' && p[2] == 0))
        uci_closed = 1;
}

unsigned int uci_read(unsigned char sock, unsigned char *buf, unsigned int max)
{
    unsigned int n = 0, len;
    uci_idle();
    CMD = TARGET_NET;
    CMD = NET_READ;
    CMD = sock;
    CMD = (unsigned char)max;
    CMD = (unsigned char)(max >> 8);
    if (uci_exec())
        return 0;
    if (STAT & DATA_AV) {
        unsigned char lo, hi;
        lo = RESP;                /* une lecture par octet : chaque */
        hi = RESP;                /* lecture avance dans la file */
        len = lo | ((unsigned int)hi << 8);
        while (n < len && (STAT & DATA_AV))
            buf[n++] = RESP;
    }
    uci_finish();
    check_closed();
    return n;
}

unsigned char uci_write(unsigned char sock, const unsigned char *buf, unsigned int n)
{
    uci_idle();
    CMD = TARGET_NET;
    CMD = NET_WRITE;
    CMD = sock;
    while (n--)
        CMD = *buf++;
    if (uci_exec())
        return 1;
    uci_finish();
    if (status_ok())
        return 0;
    uci_closed = 1;               /* « 12,SEND ERROR » : connexion perdue */
    return 1;
}
