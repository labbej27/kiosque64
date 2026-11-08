/* Test réseau : connexion à MiniPAVI (TCP 516), résultats en mémoire. */
#include "../../src/charmap.h"
#include "../../src/uci.h"
#include <string.h>
#define RES ((unsigned char *)0x3F00)   /* résultats */
#define BUF ((unsigned char *)0x4000)   /* données reçues */
#define JIFFY (*(volatile unsigned char *)0xA2)
int main(void)
{
    unsigned int total = 0, n;
    unsigned char sock, t0, reads = 0, empty = 0, maxdt = 0, t;
    memset(RES, 0, 256);
    RES[0] = 0xAA;
    RES[1] = uci_present();
    if (!RES[1]) { RES[0] = 0xEE; return 0; }
    uci_ipaddr(RES + 2);
    sock = uci_tcp_open("go.minipavi.fr", 516);
    RES[6] = sock;
    memcpy(RES + 0x40, uci_status, 40);
    if (sock == 0xFF) { RES[0] = 0xEF; return 0; }
    t0 = JIFFY;
    while ((unsigned char)(JIFFY - t0) < 240 && total < 0x6000) {
        t = JIFFY;
        n = uci_read(sock, BUF + total, 512);
        t = JIFFY - t;
        if (t > maxdt) maxdt = t;
        ++reads;
        if (n == 0) ++empty;
        total += n;
        if (uci_closed) break;
    }
    memcpy(RES + 0x80, uci_status, 40);
    RES[8] = (unsigned char)total; RES[9] = total >> 8;
    RES[10] = reads; RES[11] = empty; RES[12] = maxdt; RES[13] = uci_closed;
    /* envoie Connexion/Fin pour partir proprement */
    RES[0xF0] = 0x13; RES[0xF1] = 0x49;
    RES[14] = uci_write(sock, RES + 0xF0, 2);
    uci_close(sock);
    RES[0] = 0x55;
    return 0;
}
