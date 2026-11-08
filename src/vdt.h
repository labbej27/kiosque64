/* Kiosque 64 : décodeur Vidéotex (norme Télétel, profil CEPT-2). */
#ifndef VDT_H
#define VDT_H

void vdt_reset(void);                 /* écran effacé, attributs par défaut */
void vdt_put(unsigned char c);        /* un octet reçu du serveur */
void vdt_write(const unsigned char *p, unsigned int n);
void vdt_cursor_update(void);         /* place le sprite du curseur */
void vdt_blink_tick(void);            /* à appeler ~ toutes les 0,5 s */

/* Réponses du terminal (identification, position du curseur) : le
   décodeur les dépose ici, la boucle principale les envoie. */
extern unsigned char vdt_reply[8];
extern unsigned char vdt_reply_len;

extern unsigned char vdt_lower;       /* clavier en minuscules (PRO2 iE) */
extern unsigned char vdt_echo;        /* écho local du clavier */
extern unsigned char vdt_cursor_on;
extern unsigned char vdt_grey;        /* 1 = rendu en niveaux de gris */
extern unsigned char vdt_bell;        /* BEL reçu : à faire sonner */

#endif
