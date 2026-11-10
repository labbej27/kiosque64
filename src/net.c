/* Kiosque 64 : couches telnet et WebSocket au-dessus du pilote uci.c.
   WebSocket d'après la RFC 6455, telnet d'après la RFC 854. */
#include <string.h>
#include "charmap.h"
#include "uci.h"
#include "net.h"

#define JIFFY (*(volatile unsigned char *)0xA2)

void (*net_sink)(unsigned char c);

static unsigned char sock = 0xFF;
static unsigned char kind;
static unsigned char rbuf[512];
static unsigned char sbuf[136];

/* --- telnet ---------------------------------------------------------- */
#define IAC  255
#define DONT 254
#define DO   253
#define WONT 252
#define WILL 251
#define SB   250
#define SE   240
static unsigned char t_state, t_cmd;
/* Une négociation complète (6 réponses + taille d'écran) fait 27 octets :
   avec 24 octets (v0.1), la taille d'écran se perdait quand toute la
   négociation arrivait d'un coup. */
static unsigned char t_reply[48], t_reply_len;

static void telnet_byte(unsigned char c)
{
    switch (t_state) {
    case 0:
        if (c == IAC) t_state = 1;
        else net_sink(c);
        return;
    case 1:                                 /* après IAC */
        if (c == IAC) { t_state = 0; net_sink(c); return; }
        if (c >= WILL && c <= DONT) { t_cmd = c; t_state = 2; return; }
        t_state = (c == SB) ? 3 : 0;
        return;
    case 2: {                               /* option négociée */
        /* On accepte « binaire » (0), « écho » (1, côté serveur),
           « suppression du go-ahead » (3) et la taille d'écran (31,
           NAWS : 40 x 25) ; on refuse le reste. */
        unsigned char ok = (c == 0 || c == 3 || (c == 1 && t_cmd == WILL)
                            || (c == 31 && t_cmd == DO));
        unsigned char r;
        if (t_cmd == DO) r = ok ? WILL : WONT;
        else if (t_cmd == WILL) r = ok ? DO : DONT;
        else r = 0;
        if (r && t_reply_len + 3 <= sizeof(t_reply)) {
            t_reply[t_reply_len++] = IAC;
            t_reply[t_reply_len++] = r;
            t_reply[t_reply_len++] = c;
        }
        if (c == 31 && t_cmd == DO && t_reply_len + 9 <= sizeof(t_reply)) {
            static const unsigned char naws[9] = { IAC, SB, 31, 0, 40, 0, 25, IAC, SE };
            memcpy(t_reply + t_reply_len, naws, 9);
            t_reply_len += 9;
        }
        t_state = 0;
        return;
    }
    case 3:                                 /* sous-négociation : ignorée */
        if (c == IAC) t_state = 4;
        return;
    case 4:
        t_state = (c == SE) ? 0 : 3;
        return;
    }
}

/* --- WebSocket ------------------------------------------------------- */
static unsigned char w_state, w_op, w_hdr, w_utf;
static unsigned long w_len;
static unsigned char w_ping[125], w_ping_len, w_pong;
static unsigned char w_closed;

static void ws_frame(unsigned char op, const unsigned char *p, unsigned char n)
{
    /* trame client : masque obligatoire, clé nulle (contenu inchangé) */
    sbuf[0] = 0x80 | op;
    sbuf[1] = 0x80 | n;
    sbuf[2] = sbuf[3] = sbuf[4] = sbuf[5] = 0;
    memcpy(sbuf + 6, p, n);
    uci_write(sock, sbuf, n + 6);
}

static void ws_payload(unsigned char c)
{
    if (w_op == 9) {                        /* ping : on garde pour le pong */
        if (w_ping_len < sizeof(w_ping))
            w_ping[w_ping_len++] = c;
        return;
    }
    if (w_op == 8)
        return;
    /* trames texte en UTF-8 : on ramène les caractères à un octet */
    if (w_utf) {
        net_sink(((w_utf & 3) << 6) | (c & 0x3F));
        w_utf = 0;
    } else if (c >= 0xC0 && c < 0xE0) {
        w_utf = c;
    } else {
        net_sink(c);
    }
}

static void ws_end(void)
{
    if (w_op == 9)
        w_pong = 1;
    else if (w_op == 8)
        w_closed = 1;
}

static void ws_byte(unsigned char c)
{
    switch (w_state) {
    case 0:                                 /* FIN, opcode */
        if ((c & 0x0F) != 0)                /* 0 = suite de la trame précédente */
            w_op = c & 0x0F;
        if (w_op == 9)
            w_ping_len = 0;
        w_state = 1;
        return;
    case 1:                                 /* longueur */
        c &= 0x7F;                          /* le serveur ne masque pas */
        if (c == 126) { w_hdr = 2; w_len = 0; w_state = 2; return; }
        if (c == 127) { w_hdr = 8; w_len = 0; w_state = 2; return; }
        w_len = c;
        break;
    case 2:                                 /* longueur étendue */
        w_len = (w_len << 8) | c;
        if (--w_hdr)
            return;
        break;
    case 3:                                 /* contenu */
        ws_payload(c);
        if (--w_len)
            return;
        ws_end();
        w_state = 0;
        return;
    }
    if (w_len == 0) {
        ws_end();
        w_state = 0;
    } else {
        w_state = 3;
    }
}

static const char ws_key[] = "Sec-WebSocket-Key: QzY0QzY0QzY0QzY0QzY0IQ==\r\n";

static unsigned char ws_handshake(const char *host, unsigned int port, const char *path)
{
    unsigned int n, i;
    unsigned char t, crlf = 0, ok = 0, pos = 0;
    char num[6];
    char *q = (char *)rbuf;

    /* « GET /chemin HTTP/1.1 » et les en-têtes de la RFC 6455 */
    strcpy(q, "GET ");
    strcat(q, (path && *path) ? path : (const char *)"/");
    strcat(q, " HTTP/1.1\r\nHost: ");
    strcat(q, host);
    if (port != 80) {
        i = 0;
        n = port;
        do { num[i++] = '0' + n % 10; n /= 10; } while (n);
        q += strlen(q);
        *q++ = ':';
        while (i) *q++ = num[--i];
        *q = 0;
        q = (char *)rbuf;
    }
    strcat(q, "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n");
    strcat(q, ws_key);
    strcat(q, "Sec-WebSocket-Version: 13\r\n\r\n");
    if (uci_write(sock, rbuf, strlen((char *)rbuf)))
        return 0;

    /* Réponse : « HTTP/1.1 101 ... » puis une ligne vide. Les octets qui
       suivent sont déjà des trames. */
    t = JIFFY;
    for (;;) {
        n = uci_read(sock, rbuf, sizeof(rbuf));
        for (i = 0; i < n; ++i) {
            unsigned char c = rbuf[i];
            if (crlf == 4) {
                ws_byte(c);
                continue;
            }
            if (pos < 12) {                 /* « HTTP/1.1 101 » : 101 en 9-11 */
                if (pos == 9 && c == '1') ok = 1;
                if ((pos == 10 && c != '0') || (pos == 11 && c != '1')) ok = 0;
                ++pos;
            }
            if (c == '\r' || c == '\n') ++crlf;
            else crlf = 0;
        }
        if (crlf == 4 || uci_closed)
            break;
        if ((unsigned char)(JIFFY - t) > 240)  /* 4 s */
            return 0;
    }
    return ok && crlf == 4;
}

/* --- interface ------------------------------------------------------- */

unsigned char net_open(const char *host, unsigned int port, const char *path,
                       unsigned char k)
{
    if (!uci_present())
        return NET_ERR_NOUCI;
    kind = k;
    t_state = 0; t_reply_len = 0;
    w_state = 0; w_op = 1; w_utf = 0; w_pong = 0; w_closed = 0;
    sock = uci_tcp_open(host, port);
    if (sock == 0xFF)
        return NET_ERR_CONNECT;
    if (kind == NET_WS && !ws_handshake(host, port, path)) {
        net_close();
        return NET_ERR_WS;
    }
    return 0;
}

void net_close(void)
{
    if (sock != 0xFF) {
        if (kind == NET_WS && !uci_closed)
            ws_frame(8, (const unsigned char *)"\x03\xE8", 2);  /* fermeture normale */
        uci_close(sock);
        sock = 0xFF;
    }
}

unsigned char net_poll(void)
{
    unsigned int n, i;
    if (sock == 0xFF)
        return 1;
    n = uci_read(sock, rbuf, sizeof(rbuf));
    for (i = 0; i < n; ++i) {
        if (kind == NET_WS) ws_byte(rbuf[i]);
        else if (kind == NET_TELNET) telnet_byte(rbuf[i]);
        else net_sink(rbuf[i]);
    }
    if (t_reply_len) {
        uci_write(sock, t_reply, t_reply_len);
        t_reply_len = 0;
    }
    if (w_pong) {
        ws_frame(10, w_ping, w_ping_len);
        w_pong = 0;
    }
    return uci_closed || w_closed;
}

void net_send(const unsigned char *p, unsigned char n)
{
    if (sock == 0xFF || n == 0)
        return;
    if (kind == NET_WS) {
        ws_frame(1, p, n);
    } else if (kind == NET_TELNET) {
        unsigned char i, k = 0;
        for (i = 0; i < n && k < sizeof(sbuf) - 1; ++i) {
            if (p[i] == IAC) sbuf[k++] = IAC;
            sbuf[k++] = p[i];
        }
        uci_write(sock, sbuf, k);
    } else {
        uci_write(sock, p, n);
    }
}
