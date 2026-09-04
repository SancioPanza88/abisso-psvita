/* ABISSO Vita - multiplayer LAN: UDP discovery + TCP sync host-autoritativo. */
#include "net.h"
#include "abisso.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
static bool wsa_on = false;
#define CLOSESOCK closesocket
#define SOCKERR WSAGetLastError()
#define EWOULDBLK WSAEWOULDBLOCK
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define CLOSESOCK close
#define SOCKERR errno
#ifdef ABISSO_VITA
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#endif
#endif

NetRole net_role = NET_OFF;
bool net_connected = false;
uint32_t net_host_seed = 0;
uint32_t net_join_seed = 0;
int net_join_depth = 1;
NetPeer net_peer;
char net_found_room[NET_MAX_FOUND][24];
char net_found_ip[NET_MAX_FOUND][32];
int net_found_n = 0;

static int tcp_fd = -1;      /* socket connesso (host: accept, join: connect) */
static int listen_fd = -1;   /* host listen */
static int udp_fd = -1;      /* discovery (host: send bcast, join: recv) */
static char host_room[24] = "abisso";
static bool scan_on = false;
static double bcast_t = 0, pos_t = 0, snap_t = 0, ping_t = 0;
static double rx_timeout = 0;
/* outbox/inbox TCP */
#define OBCAP 8192
static unsigned char obuf[OBCAP];
static int obuf_n = 0;
#define IBCAP 8192
static unsigned char ibuf[IBCAP];
static int ibuf_n = 0;

/* tipi messaggio */
enum {
  M_HELLO = 1, M_WELCOME = 2, M_POS = 3, M_SNAP = 4, M_HIT = 5,
  M_TAKE = 6, M_OPEN = 7, M_GIV = 8, M_STAIRS = 9, M_DEPTH = 10,
  M_REVIVE = 11, M_PING = 12, M_PONG = 13, M_BYE = 14, M_HURT = 15
};

static void put_u16(unsigned char *p, unsigned v) { p[0] = v & 255; p[1] = (v >> 8) & 255; }
static void put_u32(unsigned char *p, uint32_t v) {
  p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = (v >> 24) & 255;
}
static void put_f32(unsigned char *p, float v) {
  uint32_t u; memcpy(&u, &v, 4); put_u32(p, u);
}
static void put_s16(unsigned char *p, int v) { put_u16(p, (unsigned)(v & 0xFFFF)); }
static unsigned get_u16(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8); }
static uint32_t get_u32(const unsigned char *p) {
  return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static float get_f32(const unsigned char *p) {
  uint32_t u = get_u32(p); float f; memcpy(&f, &u, 4); return f;
}
static int get_s16(const unsigned char *p) {
  int v = (int)get_u16(p); return v >= 32768 ? v - 65536 : v;
}

static void set_nb(int fd) {
#ifdef _WIN32
  u_long m = 1;
  ioctlsocket(fd, FIONBIO, &m);
#else
  int fl = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, fl | O_NONBLOCK);
#endif
}

bool net_init(void) {
  memset(&net_peer, 0, sizeof net_peer);
#ifdef _WIN32
  if (!wsa_on) {
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) return false;
    wsa_on = true;
  }
#endif
#ifdef ABISSO_VITA
  {
    static bool netmod = false;
    static unsigned char *pool = NULL;
    if (!netmod) {
      if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0) return false;
      pool = (unsigned char *)malloc(1024 * 1024);
      if (!pool) return false;
      static SceNetInitParam p;
      memset(&p, 0, sizeof p);
      p.memory = pool;
      p.size = 1024 * 1024;
      p.flags = 0;
      if (sceNetInit(&p) < 0) return false;
      netmod = true;
    }
  }
#endif
  return true;
}

void net_quit(void) { net_leave(); }

bool net_is_on(void) { return net_role != NET_OFF; }

static void close_all(void) {
  if (tcp_fd >= 0) { CLOSESOCK(tcp_fd); tcp_fd = -1; }
  if (listen_fd >= 0) { CLOSESOCK(listen_fd); listen_fd = -1; }
  if (udp_fd >= 0) { CLOSESOCK(udp_fd); udp_fd = -1; }
  obuf_n = 0; ibuf_n = 0;
  net_connected = false;
  scan_on = false;
  memset(&net_peer, 0, sizeof net_peer);
}

/* ---------------- host ---------------- */
bool net_host_begin(const char *room) {
  net_leave();
  if (!net_init()) return false;
  strncpy(host_room, room && room[0] ? room : "abisso", 23);
  net_host_seed = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
  if (!net_host_seed) net_host_seed = 0xA551550;
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) return false;
  {
    int one = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
  }
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_port = htons(NET_TCP_PORT);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(listen_fd, (struct sockaddr *)&a, sizeof a) < 0) { close_all(); return false; }
  if (listen(listen_fd, 1) < 0) { close_all(); return false; }
  set_nb(listen_fd);
  udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd >= 0) {
    int one = 1;
    setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof one);
    set_nb(udp_fd);
  }
  net_role = NET_HOST;
  bcast_t = 0;
  return true;
}

void net_host_stop(void) { net_leave(); }

/* ---------------- join/scan ---------------- */
bool net_scan_begin(void) {
  net_leave();
  if (!net_init()) return false;
  udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd < 0) return false;
  {
    int one = 1;
    setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
  }
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_port = htons(NET_UDP_PORT);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(udp_fd, (struct sockaddr *)&a, sizeof a) < 0) { close_all(); return false; }
  set_nb(udp_fd);
  net_role = NET_JOIN;
  scan_on = true;
  net_found_n = 0;
  return true;
}

void net_scan_end(void) {
  scan_on = false;
  if (udp_fd >= 0 && tcp_fd < 0) { CLOSESOCK(udp_fd); udp_fd = -1; }
}

int net_scan_poll(void) {
  if (!scan_on || udp_fd < 0) return net_found_n;
  char buf[96];
  struct sockaddr_in from;
  socklen_t fl = sizeof from;
  for (int k = 0; k < 8; k++) {
    int n = recvfrom(udp_fd, buf, sizeof buf - 1, 0, (struct sockaddr *)&from, &fl);
    if (n <= 0) break;
    buf[n] = 0;
    if (strncmp(buf, "ABISSO1 room=", 13) != 0) continue;
    char room[24] = {0};
    strncpy(room, buf + 13, 23);
    room[strcspn(room, "\r\n")] = 0;
    char ip[32];
#ifdef _WIN32
    strncpy(ip, inet_ntoa(from.sin_addr), 31);
#else
    inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
#endif
    bool dup = false;
    for (int i = 0; i < net_found_n; i++)
      if (strcmp(net_found_ip[i], ip) == 0) { strncpy(net_found_room[i], room, 23); dup = true; }
    if (!dup && net_found_n < NET_MAX_FOUND) {
      strncpy(net_found_ip[net_found_n], ip, 31);
      strncpy(net_found_room[net_found_n], room, 23);
      net_found_n++;
    }
  }
  return net_found_n;
}

static bool tcp_connect_nb(const char *ip, int port, int fd) {
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_port = htons((unsigned short)port);
#ifdef _WIN32
  a.sin_addr.s_addr = inet_addr(ip);
  if (a.sin_addr.s_addr == INADDR_NONE) return false;
#else
  if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) return false;
#endif
  set_nb(fd);
  int r = connect(fd, (struct sockaddr *)&a, sizeof a);
  if (r == 0) return true;
#ifdef _WIN32
  if (SOCKERR != WSAEWOULDBLOCK) return false;
#else
  if (SOCKERR != EINPROGRESS) return false;
#endif
  /* attesa completamento fino a 3s */
  fd_set wf;
  FD_ZERO(&wf);
  FD_SET((unsigned)fd, &wf);
  struct timeval tv;
  tv.tv_sec = 3; tv.tv_usec = 0;
#ifdef _WIN32
  r = select(0, NULL, &wf, NULL, &tv);
#else
  r = select(fd + 1, NULL, &wf, NULL, &tv);
#endif
  if (r <= 0) return false;
  int err = 0;
  socklen_t el = sizeof err;
  getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &el);
  return err == 0;
}

static bool send_all_nb(int fd, const unsigned char *p, int n) {
  int sent = 0;
  while (sent < n) {
    int r = send(fd, (const char *)p + sent, n - sent, 0);
    if (r <= 0) return false;
    sent += r;
  }
  return true;
}

static bool recv_all_blk(int fd, unsigned char *p, int n, int ms) {
  int got = 0;
  int waited = 0;
  while (got < n && waited < ms) {
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET((unsigned)fd, &rf);
    struct timeval tv;
    tv.tv_sec = 0; tv.tv_usec = 50000;
#ifdef _WIN32
    int r = select(0, &rf, NULL, NULL, &tv);
#else
    int r = select(fd + 1, &rf, NULL, NULL, &tv);
#endif
    if (r > 0) {
      int k = recv(fd, (char *)p + got, n - got, 0);
      if (k <= 0) return false;
      got += k;
    } else if (r < 0) {
      return false;
    }
    waited += 50;
  }
  return got == n;
}

bool net_join(const char *ip, const char *name, int cls, const char *room) {
  net_scan_end();
  if (tcp_fd >= 0) { CLOSESOCK(tcp_fd); tcp_fd = -1; }
  tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (tcp_fd < 0) { net_role = NET_OFF; return false; }
  if (!tcp_connect_nb(ip, NET_TCP_PORT, tcp_fd)) {
    CLOSESOCK(tcp_fd); tcp_fd = -1;
    net_role = NET_OFF;
    return false;
  }
  /* handshake: HELLO poi WELCOME */
  unsigned char msg[3 + 49];
  msg[0] = M_HELLO; put_u16(msg + 1, 49);
  memset(msg + 3, 0, 49);
  strncpy((char *)msg + 3, name, 23);
  msg[3 + 24] = (unsigned char)cls;
  strncpy((char *)msg + 3 + 25, room, 23);
  if (!send_all_nb(tcp_fd, msg, sizeof msg)) {
    CLOSESOCK(tcp_fd); tcp_fd = -1;
    net_role = NET_OFF;
    return false;
  }
  unsigned char hdr[3];
  if (!recv_all_blk(tcp_fd, hdr, 3, 5000) || hdr[0] != M_WELCOME || get_u16(hdr + 1) != 5) {
    CLOSESOCK(tcp_fd); tcp_fd = -1;
    net_role = NET_OFF;
    return false;
  }
  unsigned char pl[5];
  if (!recv_all_blk(tcp_fd, pl, 5, 3000)) {
    CLOSESOCK(tcp_fd); tcp_fd = -1;
    net_role = NET_OFF;
    return false;
  }
  net_join_seed = get_u32(pl);
  net_join_depth = pl[4];
  if (!net_join_seed) net_join_seed = 0xBEEF;
  if (net_join_depth < 1) net_join_depth = 1;
  if (net_join_depth > 30) net_join_depth = 30;
  net_role = NET_JOIN;
  net_connected = true;
  rx_timeout = 0;
  obuf_n = 0; ibuf_n = 0;
  pos_t = 0; ping_t = 0;
  return true;
}

void net_leave(void) {
  if (tcp_fd >= 0 && net_connected) {
    unsigned char bye[3] = {M_BYE, 0, 0};
    send(tcp_fd, (const char *)bye, 3, 0);
  }
  close_all();
  net_role = NET_OFF;
}

/* ---------------- framing ---------------- */
static void qmsg(unsigned char type, const unsigned char *pl, int n) {
  if (obuf_n + 3 + n > OBCAP) return;
  obuf[obuf_n++] = type;
  put_u16(obuf + obuf_n, (unsigned)n);
  obuf_n += 2;
  if (n > 0) { memcpy(obuf + obuf_n, pl, (size_t)n); obuf_n += n; }
}
static void flush_out(void) {
  while (obuf_n > 0 && tcp_fd >= 0) {
    int r = send(tcp_fd, (const char *)obuf, obuf_n, 0);
    if (r <= 0) break;
    memmove(obuf, obuf + r, (size_t)(obuf_n - r));
    obuf_n -= r;
  }
  if (obuf_n >= OBCAP - 64) obuf_n = 0; /* mai bloccare il gioco */
}

/* forward */
static void on_msg(unsigned char type, const unsigned char *pl, int n);

static void pump_in(void) {
  if (tcp_fd < 0) return;
  for (int k = 0; k < 16; k++) {
    unsigned char tmp[1024];
    int r = recv(tcp_fd, (char *)tmp, sizeof tmp, 0);
    if (r <= 0) break;
    if (ibuf_n + r > IBCAP) { ibuf_n = 0; break; }
    memcpy(ibuf + ibuf_n, tmp, (size_t)r);
    ibuf_n += r;
  }
  while (ibuf_n >= 3) {
    unsigned char type = ibuf[0];
    int n = (int)get_u16(ibuf + 1);
    if (n < 0 || n > 4096) { ibuf_n = 0; break; }
    if (ibuf_n < 3 + n) break;
    rx_timeout = 0;
    on_msg(type, ibuf + 3, n);
    memmove(ibuf, ibuf + 3 + n, (size_t)(ibuf_n - 3 - n));
    ibuf_n -= 3 + n;
  }
}

/* ---------------- gameplay glue (definiti in game.c) ---------------- */
extern void net_apply_hurt(int dmg, bool poison, bool web);
extern void net_apply_giv(int kind, int amount, int buff, int rar, int slot,
                          int s0, int s1, int s2, int s3);
extern void net_apply_depth(int depth);
extern void net_apply_revive(void);
extern void net_host_on_hit(int slot, int dmg);
extern void net_host_on_take(int slot);
extern void net_host_on_open(int idx);
extern void net_host_on_stairs(void);
extern void net_host_on_revive(void);
extern void net_adopt_snapshot(const unsigned char *pl, int n);

/* ---------------- messaggi in arrivo ---------------- */
static void apply_pos(const unsigned char *pl, int n) {
  if (n < 49) return;
  net_peer.active = true;
  net_peer.timeout = 0;
  net_peer.x = get_f32(pl); net_peer.y = get_f32(pl + 4);
  net_peer.fx = get_f32(pl + 8); net_peer.fy = get_f32(pl + 12);
  net_peer.hp = get_s16(pl + 16); net_peer.max_hp = get_s16(pl + 18);
  net_peer.mp = pl[20]; net_peer.max_mp = pl[21];
  net_peer.cls = pl[22];
  unsigned fl = pl[23];
  net_peer.downed = (fl & 1) != 0;
  net_peer.dead = (fl & 2) != 0;
  net_peer.atk = (fl & 4) != 0;
  memcpy(net_peer.name, pl + 25, 23);
  net_peer.name[23] = 0;
}

static void on_msg(unsigned char type, const unsigned char *pl, int n) {
  switch (type) {
    case M_POS: apply_pos(pl, n); break;
    case M_HIT:
      if (n >= 3 && net_role == NET_HOST) net_host_on_hit(pl[0], get_s16(pl + 1));
      break;
    case M_TAKE:
      if (n >= 1 && net_role == NET_HOST) net_host_on_take(pl[0]);
      break;
    case M_OPEN:
      if (n >= 1 && net_role == NET_HOST) net_host_on_open(pl[0]);
      break;
    case M_GIV:
      if (n >= 13) net_apply_giv(pl[0], get_s16(pl + 1), pl[3], pl[4], pl[5],
                                 get_s16(pl + 6), get_s16(pl + 8), get_s16(pl + 10), get_s16(pl + 12));
      break;
    case M_SNAP:
      if (net_role == NET_JOIN) net_adopt_snapshot(pl, n);
      break;
    case M_DEPTH:
      if (n >= 1) net_apply_depth(pl[0]);
      break;
    case M_REVIVE: net_apply_revive(); break;
    case M_STAIRS:
      if (net_role == NET_HOST) net_host_on_stairs();
      break;
    case M_HURT:
      if (n >= 4) net_apply_hurt(get_s16(pl), pl[2] ? true : false, pl[3] ? true : false);
      else if (n >= 3) net_apply_hurt(get_s16(pl), pl[2] ? true : false, false);
      break;
    case M_PING: qmsg(M_PONG, NULL, 0); break;
    case M_PONG: break;
    case M_BYE:
      net_connected = false;
      memset(&net_peer, 0, sizeof net_peer);
      ab_toast("Compagno disconnesso.");
      break;
    default: break;
  }
}

/* host: accetta connessioni + handshake */
static void host_accept_tick(void) {
  if (tcp_fd >= 0 || listen_fd < 0) return;
  struct sockaddr_in from;
  socklen_t fl = sizeof from;
  int fd = accept(listen_fd, (struct sockaddr *)&from, &fl);
  if (fd < 0) return;
  set_nb(fd);
  /* attende HELLO (bloccante breve) */
  unsigned char hdr[3];
  /* usa select per non appendersi */
  fd_set rf;
  FD_ZERO(&rf);
  FD_SET((unsigned)fd, &rf);
  struct timeval tv;
  tv.tv_sec = 4; tv.tv_usec = 0;
#ifdef _WIN32
  int r = select(0, &rf, NULL, NULL, &tv);
#else
  int r = select(fd + 1, &rf, NULL, NULL, &tv);
#endif
  bool ok = false;
  if (r > 0) {
    if (recv_all_blk(fd, hdr, 3, 100) && hdr[0] == M_HELLO && get_u16(hdr + 1) == 49) {
      unsigned char pl[49];
      if (recv_all_blk(fd, pl, 49, 2000)) {
        char nm[24] = {0}, rm[24] = {0};
        memcpy(nm, pl, 23); memcpy(rm, pl + 25, 23);
        (void)rm;
        memset(&net_peer, 0, sizeof net_peer);
        memcpy(net_peer.name, nm, 23);
        net_peer.cls = pl[24];
        net_peer.active = true;
        net_peer.hp = 10; net_peer.max_hp = 10;
        unsigned char w[3 + 5];
        w[0] = M_WELCOME; put_u16(w + 1, 5);
        put_u32(w + 3, G.world_seed ? G.world_seed : net_host_seed);
        w[7] = (unsigned char)(G.depth > 0 ? G.depth : 1);
        if (send_all_nb(fd, w, sizeof w)) ok = true;
      }
    }
  }
  if (ok) {
    tcp_fd = fd;
    net_connected = true;
    rx_timeout = 0;
    obuf_n = 0; ibuf_n = 0;
    pos_t = 0; snap_t = 0; ping_t = 0;
    char b[64];
    snprintf(b, sizeof b, "%s si e unito!", net_peer.name[0] ? net_peer.name : "Compagno");
    ab_toast(b);
    ab_add_log(b);
  } else {
    CLOSESOCK(fd);
  }
}

/* ---------------- invio periodico ---------------- */
static void send_pos(void) {
  unsigned char pl[49];
  put_f32(pl, (float)G.p.x); put_f32(pl + 4, (float)G.p.y);
  put_f32(pl + 8, (float)G.p.fx); put_f32(pl + 12, (float)G.p.fy);
  put_s16(pl + 16, G.p.hp); put_s16(pl + 18, G.p.max_hp);
  pl[20] = (unsigned char)(G.p.mp > 255 ? 255 : (int)G.p.mp);
  pl[21] = (unsigned char)(G.p.max_mp > 255 ? 255 : (int)G.p.max_mp);
  pl[22] = (unsigned char)G.p.cls;
  pl[23] = (unsigned char)((G.p.downed ? 1 : 0) | (G.p.dead ? 2 : 0) | (G.p.atk_cd > 0.2 ? 4 : 0));
  pl[24] = 0;
  memset(pl + 25, 0, 24);
  strncpy((char *)pl + 25, G.p.name, 23);
  qmsg(M_POS, pl, sizeof pl);
}

static void send_snap(void) {
  /* stima: 5 + 96*11 + 64*16 + 5 */
  static unsigned char pl[2200];
  int o = 0;
  pl[o++] = (unsigned char)G.depth;
  pl[o++] = G.boss_active ? 1 : 0;
  pl[o++] = G.boss_dead ? 1 : 0;
  put_s16(pl + o, G.boss_hp); o += 2;
  int nm = 0;
  for (int i = 0; i < MAX_MONSTERS; i++) if (G.mons[i].active) nm++;
  if (nm > 96) nm = 96;
  pl[o++] = (unsigned char)nm;
  int c = 0;
  for (int i = 0; i < MAX_MONSTERS && c < nm; i++) {
    if (!G.mons[i].active) continue;
    pl[o++] = 1;
    pl[o++] = (unsigned char)G.mons[i].type;
    put_f32(pl + o, (float)G.mons[i].x); o += 4;
    put_f32(pl + o, (float)G.mons[i].y); o += 4;
    put_s16(pl + o, G.mons[i].hp); o += 2;
    put_s16(pl + o, G.mons[i].dmg); o += 2;
    c++;
  }
  int ni = 0;
  for (int i = 0; i < MAX_ITEMS; i++) if (G.items[i].active) ni++;
  if (ni > 64) ni = 64;
  pl[o++] = (unsigned char)ni;
  c = 0;
  for (int i = 0; i < MAX_ITEMS && c < ni; i++) {
    if (!G.items[i].active) continue;
    AbItem *it = &G.items[i];
    pl[o++] = 1;
    pl[o++] = (unsigned char)it->kind;
    put_f32(pl + o, (float)it->x); o += 4;
    put_f32(pl + o, (float)it->y); o += 4;
    put_s16(pl + o, it->amount); o += 2;
    pl[o++] = (unsigned char)it->buff;
    pl[o++] = (unsigned char)it->rarity;
    pl[o++] = (unsigned char)it->slot;
    put_s16(pl + o, it->st_hp); o += 2;
    put_s16(pl + o, it->st_dmg); o += 2;
    put_s16(pl + o, it->st_spd); o += 2;
    put_s16(pl + o, it->st_arm); o += 2;
    c++;
  }
  uint32_t mask = 0;
  for (int i = 0; i < G.chest_count && i < 24; i++)
    if (G.chests[i].open) mask |= (1u << i);
  put_u32(pl + o, mask); o += 4;
  pl[o++] = G.map.gates_closed ? 1 : 0;
  qmsg(M_SNAP, pl, o);
}

void net_tick(double dt) {
  if (net_role == NET_OFF) return;
  /* broadcast host */
  if (net_role == NET_HOST && udp_fd >= 0 && G.state == ST_GAME) {
    bcast_t -= dt;
    if (bcast_t <= 0) {
      bcast_t = 1.0;
      char msg[64];
      snprintf(msg, sizeof msg, "ABISSO1 room=%s", host_room);
      struct sockaddr_in a;
      memset(&a, 0, sizeof a);
      a.sin_family = AF_INET;
      a.sin_port = htons(NET_UDP_PORT);
      a.sin_addr.s_addr = htonl(INADDR_BROADCAST);
      sendto(udp_fd, msg, (int)strlen(msg), 0, (struct sockaddr *)&a, sizeof a);
    }
    host_accept_tick();
  }
  if (tcp_fd < 0) return;
  pump_in();
  /* timeout */
  rx_timeout += dt;
  if (rx_timeout > 5.0 && net_connected) {
    net_connected = false;
    memset(&net_peer, 0, sizeof net_peer);
    ab_toast("Connessione persa.");
  }
  /* ping */
  ping_t -= dt;
  if (ping_t <= 0) {
    ping_t = 2.0;
    qmsg(M_PING, NULL, 0);
  }
  /* POS entrambi i versi */
  if (net_connected && G.state == ST_GAME) {
    pos_t -= dt;
    if (pos_t <= 0) {
      pos_t = 1.0 / 15.0;
      send_pos();
    }
    if (net_role == NET_HOST) {
      snap_t -= dt;
      if (snap_t <= 0) {
        snap_t = 0.1;
        send_snap();
      }
    }
  }
  /* peer lerp */
  if (net_peer.active) {
    net_peer.rx += (net_peer.x - net_peer.rx) * fmin(1, dt * 10);
    net_peer.ry += (net_peer.y - net_peer.ry) * fmin(1, dt * 10);
    if (net_peer.iframes > 0) net_peer.iframes -= dt;
  }
  flush_out();
}

/* ---------------- eventi in uscita ---------------- */
void net_send_hit(int slot, int dmg) {
  if (!net_connected || net_role != NET_JOIN) return;
  unsigned char pl[3];
  pl[0] = (unsigned char)slot;
  put_s16(pl + 1, dmg);
  qmsg(M_HIT, pl, 3);
}
void net_send_take(int slot) {
  if (!net_connected || net_role != NET_JOIN) return;
  unsigned char pl[1] = {(unsigned char)slot};
  qmsg(M_TAKE, pl, 1);
}
void net_send_open(int idx) {
  if (!net_connected || net_role != NET_JOIN) return;
  unsigned char pl[1] = {(unsigned char)idx};
  qmsg(M_OPEN, pl, 1);
}
void net_send_revive(void) {
  if (!net_connected) return;
  qmsg(M_REVIVE, NULL, 0);
}
void net_send_stairs(void) {
  if (!net_connected || net_role != NET_JOIN) return;
  qmsg(M_STAIRS, NULL, 0);
}
void net_send_bye(void) {
  if (tcp_fd >= 0) {
    unsigned char bye[3] = {M_BYE, 0, 0};
    send(tcp_fd, (const char *)bye, 3, 0);
  }
}
void net_send_hurt_to_peer(int dmg, bool poison, bool web) {
  if (!net_connected || net_role != NET_HOST) return;
  if (net_peer.iframes > 0) return;
  net_peer.iframes = 0.45;
  unsigned char pl[4];
  put_s16(pl, dmg);
  pl[2] = poison ? 1 : 0;
  pl[3] = web ? 1 : 0;
  qmsg(M_HURT, pl, 4);
}
void net_send_giv(int kind, int amount, int buff, int rar, int slot,
                  int s0, int s1, int s2, int s3) {
  if (!net_connected || net_role != NET_HOST) return;
  unsigned char pl[13];
  pl[0] = (unsigned char)kind;
  put_s16(pl + 1, amount);
  pl[3] = (unsigned char)buff;
  pl[4] = (unsigned char)rar;
  pl[5] = (unsigned char)slot;
  put_s16(pl + 6, s0); put_s16(pl + 8, s1);
  put_s16(pl + 10, s2); put_s16(pl + 12, s3);
  qmsg(M_GIV, pl, sizeof pl);
}
void net_send_depth(int depth) {
  if (!net_connected || net_role != NET_HOST) return;
  unsigned char pl[1] = {(unsigned char)depth};
  qmsg(M_DEPTH, pl, 1);
}
