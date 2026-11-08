/* Kiosque 64 : accès réseau par le Command Interface du C64 Ultimate.
   Registres en $DF1C-$DF1F (à activer dans le menu de l'Ultimate). */
#ifndef UCI_H
#define UCI_H

/* 1 si le Command Interface répond (registre d'identification = $C9). */
unsigned char uci_present(void);

/* Ouvre une connexion TCP. Retourne le numéro de socket, ou 0xFF en cas
   d'erreur ; uci_status contient alors le message de l'Ultimate. */
unsigned char uci_tcp_open(const char *host, unsigned int port);
void uci_close(unsigned char sock);

/* Lit au plus max octets (max <= 892). Retourne le nombre lu (0 = rien
   pour l'instant). uci_closed passe à 1 si le serveur a fermé. */
unsigned int uci_read(unsigned char sock, unsigned char *buf, unsigned int max);

/* Envoie n octets (n <= 890). Retourne 0 si tout est parti. */
unsigned char uci_write(unsigned char sock, const unsigned char *buf, unsigned int n);

/* Adresse IP de l'Ultimate (4 octets), 0 si inconnue. */
unsigned char uci_ipaddr(unsigned char *ip4);

extern unsigned char uci_closed;
extern char uci_status[40];

#endif
