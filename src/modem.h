/* Kiosque 64 modem : réglages du modem WiFi (commandes AT). */
#ifndef MODEM_H
#define MODEM_H

#define MODEM_300  0
#define MODEM_1200 1
#define MODEM_2400 2

struct modem_cfg {
    unsigned char magic;     /* 'K' : fichier de réglages valide */
    unsigned char speed;     /* MODEM_300, MODEM_1200, MODEM_2400 */
    char init[31];           /* envoyée avant chaque appel, vide = rien */
    char dial[9];            /* préfixe d'appel, « ATDT » par défaut */
};
extern struct modem_cfg mcfg;

/* Lit / écrit KIOSQUE.CFG sur le lecteur 8. Retournent 0 si c'est bon. */
unsigned char modem_load(void);
unsigned char modem_save(void);

/* Arrête le pilote RS-232 (avant un accès disque). */
void modem_off(void);

#endif
