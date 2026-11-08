/* Kiosque 64 : connexion TCP brute, telnet ou WebSocket (ws://). */
#ifndef NET_H
#define NET_H

#define NET_TCP    0    /* octets bruts */
#define NET_TELNET 1    /* TCP avec négociation telnet (IAC) */
#define NET_WS     2    /* WebSocket non chiffré */

/* Ouvre la connexion. path n'est utilisé qu'en WebSocket. Retourne 0 si
   c'est bon, sinon un code d'erreur (voir net_error). */
unsigned char net_open(const char *host, unsigned int port, const char *path,
                       unsigned char kind);
void net_close(void);

/* Lit ce qui est arrivé et le passe octet par octet à net_sink.
   Retourne 0 tant que la connexion est ouverte, 1 si elle est fermée. */
unsigned char net_poll(void);

/* Envoie des octets (une frappe, une réponse au serveur). */
void net_send(const unsigned char *p, unsigned char n);

extern void (*net_sink)(unsigned char c);

#define NET_ERR_NOUCI   1   /* Command Interface absent */
#define NET_ERR_CONNECT 2   /* hôte injoignable */
#define NET_ERR_WS      3   /* le serveur a refusé le WebSocket */

#endif
