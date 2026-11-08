/* Banc d'essai : affiche une page Vidéotex incluse (page.s). */
#include "../../src/charmap.h"
#include "../../src/vdt.h"
void gfx_init(void);
extern const unsigned char page[];
extern const unsigned int page_len;
int main(void)
{
    gfx_init();
    vdt_reset();
    *(unsigned char *)0x3FF0 = 1;
    *(unsigned char *)0x3FF1 = *(volatile unsigned char *)0xA2;
    vdt_write(page, page_len);
    *(unsigned char *)0x3FF2 = *(volatile unsigned char *)0xA2;
    vdt_cursor_update();
    *(unsigned char *)0x3FF0 = 2;
    for (;;) ;
    return 0;
}
