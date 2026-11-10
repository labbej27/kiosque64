/* Test : comment le Command Interface signale-t-il une connexion fermée par
   le serveur ? Passerelle LABBEJ27, Connexion/Fin, puis lectures et écriture.
   Résultats en $3F00 (lus par l'API REST). */
#include "../../src/charmap.h"
#include "../../src/uci.h"
#include <string.h>
#define RES ((unsigned char *)0x3F00)
#define BUF ((unsigned char *)0x4000)
#define JIFFY (*(volatile unsigned char *)0xA2)
static unsigned char k = 0;
static void note(void)
{
    /* garde les statuts différents, 40 octets chacun, à partir de $3F40 */
    unsigned char i;
    for (i = 0; i < k; ++i)
        if (strcmp((char *)RES + 0x40 + i * 40, uci_status) == 0) return;
    if (k < 4) { strcpy((char *)RES + 0x40 + k * 40, uci_status); ++k; }
}
int main(void)
{
    unsigned char sock, t0, cnt = 0;
    unsigned int n;
    memset(RES, 0, 256);
    RES[0] = 0xAA;
    sock = uci_tcp_open("minitel.labbej.fr", 8180);
    RES[1] = sock;
    if (sock == 0xFF) { RES[0] = 0xEF; return 0; }
    t0 = JIFFY;
    while ((unsigned char)(JIFFY - t0) < 180) uci_read(sock, BUF, 512);
    RES[2] = 0x13; RES[3] = 0x49;
    RES[4] = uci_write(sock, RES + 2, 2);      /* Connexion/Fin */
    t0 = JIFFY;
    while ((unsigned char)(JIFFY - t0) < 240) {
        n = uci_read(sock, BUF, 512);
        if (n) ++cnt;
        note();
        if (uci_closed) { RES[5] = 1; break; }
    }
    RES[6] = cnt;
    RES[8] = 0x13; RES[9] = 0x44;
    RES[7] = uci_write(sock, RES + 8, 2);      /* Guide après fermeture */
    note();
    n = uci_read(sock, BUF, 512);
    note();
    RES[10] = uci_closed;
    uci_close(sock);
    RES[0] = 0x55;
    return 0;
}
