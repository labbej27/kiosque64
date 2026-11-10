/* Kiosque 64 modem : réglages du modem WiFi (commandes AT). */
#ifndef MODEM_H
#define MODEM_H

/* Interfaces */
#define IF_USER   0          /* User Port, 300 à 2400 bauds */
#define IF_UP9600 1          /* UP9600, jusqu'à 9600 bauds */
#define IF_SL_DE  2          /* SwiftLink en $DE00 (version .prg) */
#define IF_SL_DF  3          /* SwiftLink en $DF00 (version .prg) */
#define IF_COUNT  4

/* Vitesses : index 0 à 6 = 300, 1200, 2400, 4800, 9600, 19200, 38400 bauds */
#define SPEED_COUNT 7

struct modem_cfg {
    unsigned char magic;     /* 'L' : réglages de la version 0.2 */
    unsigned char speed;     /* index de vitesse */
    char init[31];           /* envoyée avant chaque appel, vide = rien */
    char dial[9];            /* préfixe d'appel, « ATDT » par défaut */
    unsigned char iface;     /* IF_USER, IF_UP9600, IF_SL_DE, IF_SL_DF */
};
extern struct modem_cfg mcfg;

/* Vitesse la plus haute de chaque interface, et vitesse proposée par défaut. */
extern const unsigned char speed_max[IF_COUNT];
extern const unsigned char speed_def[IF_COUNT];

extern struct modem_cfg mcfg;

/* Lit / écrit KIOSQUE.CFG sur le lecteur 8. Retournent 0 si c'est bon. */
unsigned char modem_load(void);
unsigned char modem_save(void);

/* Arrête le pilote RS-232 (avant un accès disque). */
void modem_off(void);

#endif
