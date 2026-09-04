/* ABISSO Vita - multiplayer LAN (2 Vita/PSTV sulla stessa rete).
 * Un host autoritativo + un joiner. Stesso seed => stesso dungeon statico;
 * si sincronizzano stati giocatore, mostri, oggetti, forzieri, boss, depth.
 * Socket BSD non bloccanti (sceNetInit su Vita, WSA su Windows). */
#ifndef AB_NET_H
#define AB_NET_H

#include <stdbool.h>
#include <stdint.h>

#define NET_TCP_PORT 26882
#define NET_UDP_PORT 26881

typedef enum { NET_OFF = 0, NET_HOST, NET_JOIN } NetRole;

extern NetRole net_role;
extern bool net_connected;   /* TCP attivo col compagno */
extern uint32_t net_host_seed;
extern uint32_t net_join_seed;
extern int net_join_depth;

/* compagno (posizione/stato dall'ultimo POS) */
typedef struct {
  bool active;
  char name[24];
  int cls;
  double x, y, fx, fy, rx, ry;
  int hp, max_hp, mp, max_mp;
  bool downed, dead;
  bool atk;
  double iframes;
  double timeout;
} NetPeer;
extern NetPeer net_peer;

/* risultati scansione */
#define NET_MAX_FOUND 8
extern char net_found_room[NET_MAX_FOUND][24];
extern char net_found_ip[NET_MAX_FOUND][32];
extern int net_found_n;

bool net_init(void);
void net_quit(void);
bool net_is_on(void);

/* host */
bool net_host_begin(const char *room);
void net_host_stop(void);
/* join */
bool net_scan_begin(void);
void net_scan_end(void);
/* ritorna n stanze trovate (colleziona per ~4s chiamando ogni frame) */
int net_scan_poll(void);
bool net_join(const char *ip, const char *name, int cls, const char *room);
void net_leave(void);

/* tick di rete ogni frame (solo se ruolo != OFF) */
void net_tick(double dt);

/* eventi gameplay */
void net_send_hit(int slot, int dmg);
void net_send_take(int slot);
void net_send_open(int idx);
void net_send_revive(void);
void net_send_stairs(void);
void net_send_bye(void);
void net_send_hurt_to_peer(int dmg, bool poison, bool web);
void net_send_giv(int kind, int amount, int buff, int rar, int slot,
                  int s0, int s1, int s2, int s3);
void net_send_depth(int depth);

#endif
