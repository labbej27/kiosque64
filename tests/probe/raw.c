#include "../../src/charmap.h"
#include "../../src/uci.h"
#include <string.h>
#define CTRL   (*(volatile unsigned char *)0xDF1C)
#define STAT   (*(volatile unsigned char *)0xDF1C)
#define CMD    (*(volatile unsigned char *)0xDF1D)
#define RESP   (*(volatile unsigned char *)0xDF1E)
#define SDATA  (*(volatile unsigned char *)0xDF1F)
#define JIFFY (*(volatile unsigned char *)0xA2)
#define RES ((unsigned char *)0x3F00)
#define BUF ((unsigned char *)0x5000)
int main(void)
{
    unsigned char sock, t, k;
    unsigned int n;
    memset(RES, 0, 256);
    RES[0] = 0xAA;
    sock = uci_tcp_open("go.minipavi.fr", 516);
    RES[1] = sock;
    t = JIFFY; while ((unsigned char)(JIFFY - t) < 90) ;   /* 1,5 s */
    for (k = 0; k < 3; ++k) {
        unsigned char *b = BUF + k * 0x400;
        CMD = 3; CMD = 0x10; CMD = sock; CMD = 0x00; CMD = 0x02;   /* 512 */
        CTRL = 1;
        while ((STAT & 0x20) == 0) ;
        RES[0x10 + k*8] = STAT;
        n = 0;
        while (STAT & 0x80) { b[n++] = RESP; if (n >= 0x3F0) break; }
        RES[0x11 + k*8] = n; RES[0x12 + k*8] = n >> 8;
        RES[0x13 + k*8] = STAT;
        n = 0;
        while (STAT & 0x40) { RES[0x40 + k*32 + n] = SDATA; if (++n >= 31) break; }
        RES[0x14 + k*8] = STAT;
        CTRL = 2;
        while ((STAT & 0x30) != 0) ;
        RES[0x15 + k*8] = STAT;
        t = JIFFY; while ((unsigned char)(JIFFY - t) < 30) ;
    }
    uci_close(sock);
    RES[0] = 0x55;
    return 0;
}
