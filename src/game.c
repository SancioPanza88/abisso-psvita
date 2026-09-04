/* ABISSO Vita - logica di gioco 1:1 da index.html.
 * RNG, scaling, pesi spawn, dungeon, FOV, danni, equip, mercante, boss.
 */
#include "abisso.h"
#include "audio.h"
#include "net.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

AbGame G;

/* ---------------- util ---------------- */
double ab_clamp(double v, double lo, double hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
double ab_dist(double x0, double y0, double x1, double y1) {
  double dx = x1 - x0, dy = y1 - y0;
  return sqrt(dx*dx + dy*dy);
}

/* ---------------- RNG (identico al JS) ---------------- */
uint32_t ab_hash_str(const char *s) {
  uint32_t h = 0x811c9dc5u;
  if (!s) return h;
  while (*s) {
    h ^= (uint8_t)(*s);
    h *= 0x01000193u;
    s++;
  }
  return h;
}
static uint32_t hash2(const char *a, const char *b, const char *c, int d) {
  char buf[128];
  snprintf(buf, sizeof buf, "%s::%s::%s::%d", a, b, c, d);
  return ab_hash_str(buf);
}
void ab_rng_seed(AbRng *r, uint32_t seed) { r->a = seed; }
double ab_rng_next(AbRng *r) {
  uint32_t a = r->a;
  a += 0x6D2B79F5u;
  r->a = a;
  uint32_t t = (a ^ (a >> 15)) * (1u | a);
  t += ((t ^ (t >> 7)) * (61u | t)) ^ t;
  t ^= (t >> 14);
  return (double)t / 4294967296.0;
}
int ab_rng_range(AbRng *r, int lo, int hi) {
  if (hi < lo) { int t = lo; lo = hi; hi = t; }
  double f = ab_rng_next(r);
  return lo + (int)(f * (double)(hi - lo + 1));
}
double ab_rng_frange(AbRng *r, double lo, double hi) {
  return lo + ab_rng_next(r) * (hi - lo);
}
int ab_scaled_stat(int base, int depth, double factor) {
  return (int)floor((double)base * (1.0 + factor * (double)(depth - 1)) + 0.5);
}

/* ---------------- log/toast/float/part ---------------- */
void ab_add_log(const char *s) {
  if (!s || !s[0]) return;
  int i = G.log_head % MAX_LOGLINES;
  strncpy(G.loglines[i], s, 95);
  G.loglines[i][95] = '\0';
  G.log_t[i] = 5.0;
  G.log_head++;
}
void ab_toast(const char *s) {
  if (!s) return;
  strncpy(G.toast, s, 127);
  G.toast[127] = '\0';
  G.toast_t = 2.6;
}
void ab_loot_banner(const char *s, double r, double g, double b) {
  if (!s) return;
  strncpy(G.loot, s, 95);
  G.loot[95] = '\0';
  G.loot_t = 2.6;
  G.loot_r = r; G.loot_g = g; G.loot_b = b;
}
void ab_float_text(double x, double y, const char *s, double r, double g, double b) {
  for (int i = 0; i < MAX_FLOATS; i++) {
    if (!G.floats[i].active) {
      G.floats[i].active = true;
      G.floats[i].x = x; G.floats[i].y = y;
      G.floats[i].life = 1.1;
      strncpy(G.floats[i].text, s ? s : "", 47);
      G.floats[i].text[47] = '\0';
      G.floats[i].r = r; G.floats[i].g = g; G.floats[i].b = b;
      return;
    }
  }
}
void ab_burst(double x, double y, int n, double r, double g, double b, double spd) {
  AbRng rr; ab_rng_seed(&rr, (uint32_t)(G.time * 1000) + (uint32_t)(x * 131) + (uint32_t)(y * 17) + 7);
  for (int k = 0; k < n; k++) {
    for (int i = 0; i < MAX_PARTS; i++) {
      if (!G.parts[i].active) {
        double a = ab_rng_frange(&rr, 0, 6.28318);
        double s = ab_rng_frange(&rr, spd * 0.3, spd);
        G.parts[i].active = true;
        G.parts[i].x = x; G.parts[i].y = y;
        G.parts[i].vx = cos(a) * s; G.parts[i].vy = sin(a) * s;
        G.parts[i].max_life = G.parts[i].life = ab_rng_frange(&rr, 0.25, 0.7);
        G.parts[i].r = r; G.parts[i].g = g; G.parts[i].b = b;
        G.parts[i].size = ab_rng_frange(&rr, 0.03, 0.09);
        break;
      }
    }
  }
}

static bool in_safe(int tx, int ty) {
  return tx >= G.map.safe_x && ty >= G.map.safe_y &&
         tx < G.map.safe_x + G.map.safe_w && ty < G.map.safe_y + G.map.safe_h;
}

bool ab_is_walkable(int tx, int ty) {
  if (tx < 0 || ty < 0 || tx >= G.map.w || ty >= G.map.h) return false;
  if (G.map.tiles[ty][tx] == T_WALL) return false;
  if (G.map.gates_closed) {
    for (int i = 0; i < G.map.gate_count; i++)
      if (G.map.gates[i][0] == tx && G.map.gates[i][1] == ty) return false;
  }
  return true;
}

/* movimento con slide + nudge come tryMoveEntity */
static bool try_move(double *px, double *py, double dx, double dy) {
  double nx = *px + dx, ny = *py + dy;
  int tx = (int)floor(nx), ty = (int)floor(ny);
  if (ab_is_walkable(tx, ty)) { *px = nx; *py = ny; return true; }
  bool mx = false, my = false;
  if (ab_is_walkable((int)floor(nx), (int)floor(*py))) { *px = nx; mx = true; }
  if (ab_is_walkable((int)floor(*px), (int)floor(ny))) { *py = ny; my = true; }
  if (mx || my) return true;
  if (dx != 0 && dy != 0) {
    double cy = floor(*py) + 0.5;
    double diff = cy - *py;
    if (fabs(diff) > 0.02) {
      double nudge = (diff > 0 ? 1 : -1) * fmin(fabs(diff), fabs(dx) * 0.5);
      if (ab_is_walkable((int)floor(*px), (int)floor(*py + nudge))) { *py += nudge; return true; }
    }
  }
  return false;
}

/* ---------------- save/record ---------------- */
static void ab_save_file(char *out, size_t n) {
#ifdef ABISSO_VITA
  snprintf(out, n, "ux0:data/ABISSO/save.txt");
#else
  snprintf(out, n, "abisso_save.txt");
#endif
}
void ab_save_record(void) {
  char file[160];
  ab_save_file(file, sizeof file);
#ifdef ABISSO_VITA
#ifdef _WIN32
  _mkdir("ux0:data/ABISSO");
#else
  mkdir("ux0:data/ABISSO", 0777);
#endif
#endif
  FILE *f = fopen(file, "w");
  if (!f) return;
  fprintf(f, "%s\n%s\n%d\n%d\n%d\n%.2f\n%d\n",
    G.p.name, G.room, G.p.cls, G.best_depth, G.best_gold, G.zoom, G.mute ? 1 : 0);
  fclose(f);
}
void ab_load_record(void) {
  char file[160];
  ab_save_file(file, sizeof file);
  FILE *f = fopen(file, "r");
  G.best_depth = 0; G.best_gold = 0;
  if (!f) return;
  char nm[64], rm[64];
  int cls = 0, bd = 0, bg = 0, mu = 0;
  double zm = 1.0;
  if (fgets(nm, sizeof nm, f) && fgets(rm, sizeof rm, f) &&
      fscanf(f, "%d\n%d\n%d\n%lf\n%d\n", &cls, &bd, &bg, &zm, &mu) == 5) {
    nm[strcspn(nm, "\r\n")] = 0;
    rm[strcspn(rm, "\r\n")] = 0;
    G.best_depth = bd; G.best_gold = bg;
    G.zoom = ab_clamp(zm, 0.6, 2.0);
    G.mute = mu ? true : false;
  }
  fclose(f);
}

/* ---------------- init/run ---------------- */
void ab_game_init(void) {
  memset(&G, 0, sizeof G);
  G.state = ST_LOGIN;
  G.depth = 1;
  G.zoom = 1.0;
  G.view = 0;
  strncpy(G.p.name, "Viandante", MAX_NAME - 1);
  strncpy(G.room, "abisso", MAX_ROOM - 1);
  G.p.cls = CLS_GUERRIERO;
  ab_load_record();
}

static bool room_is_c64(const char *room) {
  if (!room) return false;
  char tmp[32];
  size_t n = strlen(room);
  if (n >= sizeof tmp) n = sizeof tmp - 1;
  for (size_t i = 0; i < n; i++) {
    char c = room[i];
    if (c >= 'A' && c <= 'Z') c += 32;
    tmp[i] = c;
  }
  tmp[n] = 0;
  return strcmp(tmp, "64") == 0 || strcmp(tmp, "c64") == 0;
}

void ab_new_run(const char *name, int cls, const char *room) {
  if (name && name[0]) { strncpy(G.p.name, name, MAX_NAME - 1); G.p.name[MAX_NAME-1] = 0; }
  if (room && room[0]) { strncpy(G.room, room, MAX_ROOM - 1); G.room[MAX_ROOM-1] = 0; }
  if (cls >= 0 && cls < CLS_COUNT) G.p.cls = cls;
  G.c64 = room_is_c64(G.room);
  /* mondo fresco come bootstrapFreshWorld: seed casuale (host: quello
   * annunciato; joiner: quello ricevuto) */
  if (net_role == NET_HOST && net_host_seed) G.world_seed = net_host_seed;
  else if (net_role == NET_JOIN && net_join_seed) G.world_seed = net_join_seed;
  else {
    G.world_seed = ((uint32_t)rand() << 16) ^ (uint32_t)rand() ^ (uint32_t)(G.time * 1000 + 1);
    if (G.world_seed == 0) G.world_seed = 0x12345678;
  }
  G.state = ST_GAME;
  G.minimap = false; G.help = false; G.merchant_open = false;
  G.boss_active = false; G.boss_dead = false;
  memset(G.p.equip, 0, sizeof G.p.equip);
  for (int i = 0; i < BUFF_COUNT; i++) G.p.buffs[i] = 0;
  G.p.gold = 0; G.p.potions = 1; G.p.mana_potions = 0;
  G.p.poison_t = 0; G.p.web_t = 0;
  G.p.dead = false; G.p.downed = false;
  ab_gen_depth((net_role == NET_JOIN && net_join_depth > 1) ? net_join_depth : 1);
  const AbClassDef *c = ab_class_def(G.p.cls);
  char b[96];
  snprintf(b, sizeof b, "Benvenuto, %s %s! Stanza '%s'", c->name, G.p.name, G.room);
  ab_add_log(b);
  ab_toast(G.c64 ? "MODALITA' C64!" : "L'abisso ti attende...");
  if (G.c64) ab_add_log("Easter egg C64 attivo.");
  if (net_role == NET_OFF) ab_add_log("OFFLINE Vita: single-player, mondo fresco casuale.");
  else ab_add_log(net_role == NET_HOST ? "Partita LAN: tu sei l'host." : "Partita LAN: ti sei unito.");
}

/* ---------------- dungeon 1:1 ---------------- */
typedef struct { int x, y, w, h, cx, cy; } Room;

static void carve_h(AbMap *m, int x0, int x1, int y) {
  if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
  for (int x = x0; x <= x1; x++)
    if (x > 0 && y > 0 && x < m->w - 1 && y < m->h - 1) m->tiles[y][x] = T_FLOOR;
}
static void carve_v(AbMap *m, int y0, int y1, int x) {
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
  for (int y = y0; y <= y1; y++)
    if (x > 0 && y > 0 && x < m->w - 1 && y < m->h - 1) m->tiles[y][x] = T_FLOOR;
}
static void carve_corr(AbMap *m, AbRng *rng, int ax, int ay, int bx, int by) {
  if (ab_rng_next(rng) < 0.5) { carve_h(m, ax, bx, ay); carve_v(m, ay, by, bx); }
  else { carve_v(m, ay, by, ax); carve_h(m, ax, bx, by); }
}

/* stanze temporanee della gen (max 40) */
#define GEN_MAX_ROOMS 40
static Room gen_rooms[GEN_MAX_ROOMS];
static int gen_nr;
/* spot temporanei */
static int mon_spots[128][2]; static int mon_spot_n;
static int tre_spots[64][3]; static int tre_spot_n;
static int pow_spots[16][2]; static int pow_spot_n;
static int pot_spots[32][2]; static int pot_spot_n; static int pot_mana[32];
static int chest_x[24], chest_y[24], chest_boss[24]; static int chest_spot_n;

static void bfs_fill(AbMap *m, int sx, int sy, int16_t *dist) {
  int W = m->w, H = m->h;
  for (int i = 0; i < W * H; i++) dist[i] = -1;
  if (sx < 0 || sy < 0 || sx >= W || sy >= H) return;
  if (m->tiles[sy][sx] == T_WALL) return;
  int qx[MAP_MAX_W * MAP_MAX_H], qy[MAP_MAX_W * MAP_MAX_H];
  int head = 0, tail = 0;
  qx[tail] = sx; qy[tail] = sy; tail++;
  dist[sy * W + sx] = 0;
  while (head < tail) {
    int cx = qx[head], cy = qy[head]; head++;
    int d = dist[cy * W + cx];
    const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
    for (int k = 0; k < 4; k++) {
      int nx = cx + dx[k], ny = cy + dy[k];
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
      if (m->tiles[ny][nx] == T_WALL) continue;
      if (dist[ny * W + nx] != -1) continue;
      dist[ny * W + nx] = (int16_t)(d + 1);
      qx[tail] = nx; qy[tail] = ny; tail++;
    }
  }
}

void ab_gen_depth(int depth) {
  G.depth = depth;
  AbMap *m = &G.map;
  memset(m, 0, sizeof *m);
  char seedstr[32];
  snprintf(seedstr, sizeof seedstr, "%u", G.world_seed);
  AbRng rng;
  ab_rng_seed(&rng, hash2(G.room, seedstr, "layout", depth));

  int W = 78 + depth * 3; if (W < 78) W = 78; if (W > 130) W = 130;
  int H = 44 + depth * 2; if (H < 44) H = 44; if (H > 74) H = 74;
  m->w = W; m->h = H;

  gen_nr = 0;
  int maxRooms = 11 + (depth * 7) / 10;
  int attempts = 0;
  while (gen_nr < maxRooms && attempts < 500 && gen_nr < GEN_MAX_ROOMS) {
    attempts++;
    int rw = 4 + ab_rng_range(&rng, 0, 6);
    int rh = 3 + ab_rng_range(&rng, 0, 4);
    int rx = 1 + ab_rng_range(&rng, 0, W - rw - 3);
    int ry = 1 + ab_rng_range(&rng, 0, H - rh - 3);
    bool ok = true;
    for (int i = 0; i < gen_nr; i++) {
      Room *r = &gen_rooms[i];
      if (rx - 1 < r->x + r->w + 1 && rx + rw + 1 > r->x - 1 &&
          ry - 1 < r->y + r->h + 1 && ry + rh + 1 > r->y - 1) { ok = false; break; }
    }
    if (!ok) continue;
    gen_rooms[gen_nr].x = rx; gen_rooms[gen_nr].y = ry;
    gen_rooms[gen_nr].w = rw; gen_rooms[gen_nr].h = rh;
    gen_rooms[gen_nr].cx = rx + rw / 2; gen_rooms[gen_nr].cy = ry + rh / 2;
    gen_nr++;
    for (int y = ry; y < ry + rh; y++)
      for (int x = rx; x < rx + rw; x++) m->tiles[y][x] = T_FLOOR;
  }
  if (gen_nr == 0) {
    int rw = 8, rh = 6, rx = (W >> 1) - 4, ry = (H >> 1) - 3;
    gen_rooms[0].x = rx; gen_rooms[0].y = ry;
    gen_rooms[0].w = rw; gen_rooms[0].h = rh;
    gen_rooms[0].cx = rx + rw / 2; gen_rooms[0].cy = ry + rh / 2;
    gen_nr = 1;
    for (int y = ry; y < ry + rh; y++)
      for (int x = rx; x < rx + rw; x++) m->tiles[y][x] = T_FLOOR;
  }
  for (int i = 1; i < gen_nr; i++)
    carve_corr(m, &rng, gen_rooms[i-1].cx, gen_rooms[i-1].cy, gen_rooms[i].cx, gen_rooms[i].cy);
  int extraLoops = (gen_nr * 3) / 10;
  for (int k = 0; k < extraLoops; k++) {
    int a = ab_rng_range(&rng, 0, gen_nr - 1), b = ab_rng_range(&rng, 0, gen_nr - 1);
    if (a != b) carve_corr(m, &rng, gen_rooms[a].cx, gen_rooms[a].cy, gen_rooms[b].cx, gen_rooms[b].cy);
  }

  /* arena boss isolata */
  m->has_arena = false;
  int arena_cx = 0, arena_cy = 0;
  if (ab_is_boss_floor(depth)) {
    for (int tries = 0; tries < 100 && !m->has_arena; tries++) {
      int bw = 12 + ab_rng_range(&rng, 0, 5), bh = 8 + ab_rng_range(&rng, 0, 4);
      int bx = 3 + ab_rng_range(&rng, 0, W - bw - 7);
      int by = 3 + ab_rng_range(&rng, 0, H - bh - 7);
      bool ok = true;
      for (int i = 0; i < gen_nr && ok; i++) {
        Room *r = &gen_rooms[i];
        if (bx - 2 < r->x + r->w + 2 && bx + bw + 2 > r->x - 2 &&
            by - 2 < r->y + r->h + 2 && by + bh + 2 > r->y - 2) ok = false;
      }
      for (int y = by; y < by + bh && ok; y++)
        for (int x = bx; x < bx + bw; x++)
          if (m->tiles[y][x] != T_WALL) ok = false;
      for (int y = by - 1; y <= by + bh && ok; y++)
        for (int x = bx - 1; x <= bx + bw; x++) {
          bool inside = x >= bx && x < bx + bw && y >= by && y < by + bh;
          if (x < 0 || y < 0 || x >= W || y >= H) continue;
          if (!inside && m->tiles[y][x] != T_WALL) ok = false;
        }
      if (!ok) continue;
      for (int y = by; y < by + bh; y++)
        for (int x = bx; x < bx + bw; x++) m->tiles[y][x] = T_FLOOR;
      arena_cx = bx + (bw >> 1); arena_cy = by + (bh >> 1);
      m->arena_cx = arena_cx; m->arena_cy = arena_cy;
      m->arena_w = bw; m->arena_h = bh;
      m->arena_boss = ab_boss_for_depth(depth);
      m->has_arena = true;
      int ni = 0, nd = 1 << 30;
      for (int i = 0; i < gen_nr; i++) {
        int d = abs(gen_rooms[i].cx - arena_cx) + abs(gen_rooms[i].cy - arena_cy);
        if (d < nd) { nd = d; ni = i; }
      }
      carve_corr(m, &rng, arena_cx, arena_cy, gen_rooms[ni].cx, gen_rooms[ni].cy);
      m->gate_count = 0;
      for (int y = by - 1; y <= by + bh && m->gate_count < MAX_GATES; y++)
        for (int x = bx - 1; x <= bx + bw && m->gate_count < MAX_GATES; x++) {
          bool inside = x >= bx && x < bx + bw && y >= by && y < by + bh;
          if (inside || x < 0 || y < 0 || x >= W || y >= H) continue;
          if (m->tiles[y][x] == T_FLOOR) {
            m->gates[m->gate_count][0] = x;
            m->gates[m->gate_count][1] = y;
            m->gate_count++;
          }
        }
      /* forziere boss contro parete */
      int fx = bx + 1 + (bw > 2 ? ab_rng_range(&rng, 0, bw - 3) : 0);
      int fy = (by + 4 >= arena_cy) ? by + bh - 2 : by + 1;
      chest_x[0] = fx; chest_y[0] = fy; chest_boss[0] = 1;
      chest_spot_n = 1;
    }
    if (!m->has_arena) chest_spot_n = 0;
  } else {
    chest_spot_n = 0;
  }

  Room *r0 = &gen_rooms[0];
  m->spawn_x = r0->cx; m->spawn_y = r0->cy;
  m->safe_x = r0->x; m->safe_y = r0->y; m->safe_w = r0->w; m->safe_h = r0->h;
  m->merch_x = r0->x + (r0->w - 2 > 1 ? r0->w - 2 : 1);
  m->merch_y = r0->y + (r0->h / 2);
  if (m->merch_x == m->spawn_x && m->merch_y == m->spawn_y) m->merch_x = r0->x + 1;

  static int16_t distmap[MAP_MAX_W * MAP_MAX_H];
  bfs_fill(m, m->spawn_x, m->spawn_y, distmap);
  int stairs_i = gen_nr - 1, best = -1;
  for (int i = 0; i < gen_nr; i++) {
    int c = gen_rooms[i].cy * W + gen_rooms[i].cx;
    int d = (gen_rooms[i].cx >= 0 && gen_rooms[i].cy >= 0) ? distmap[c] : -1;
    if (d > best) { best = d; stairs_i = i; }
  }
  m->stairs_x = gen_rooms[stairs_i].cx;
  m->stairs_y = gen_rooms[stairs_i].cy;
  m->tiles[m->stairs_y][m->stairs_x] = T_STAIRS;

  /* forzieri normali */
  int numChests = 3 + depth / 2, ct = 0;
  while (chest_spot_n < numChests && ct < 300 && gen_nr > 1) {
    ct++;
    int ri = 1 + ab_rng_range(&rng, 0, gen_nr - 2);
    Room *r = &gen_rooms[ri];
    int x = r->x + 1 + ab_rng_range(&rng, 0, (r->w - 2 > 1 ? r->w - 2 : 1) - 1);
    int y = r->y + 1 + ab_rng_range(&rng, 0, (r->h - 2 > 1 ? r->h - 2 : 1) - 1);
    if (m->tiles[y][x] != T_FLOOR) continue;
    if (x == m->spawn_x && y == m->spawn_y) continue;
    if (x == m->merch_x && y == m->merch_y) continue;
    bool dup = false;
    for (int i = 0; i < chest_spot_n; i++)
      if (chest_x[i] == x && chest_y[i] == y) { dup = true; break; }
    if (dup) continue;
    if (chest_spot_n >= 24) break;
    chest_x[chest_spot_n] = x; chest_y[chest_spot_n] = y; chest_boss[chest_spot_n] = 0;
    chest_spot_n++;
  }

  /* spot mostri: stanze 1.., 1+floor(rng*3) per stanza */
  mon_spot_n = 0;
  for (int ri = 1; ri < gen_nr && mon_spot_n < 128; ri++) {
    Room *r = &gen_rooms[ri];
    int count = 1 + ab_rng_range(&rng, 0, 2);
    for (int k = 0; k < count && mon_spot_n < 128; k++) {
      int x = r->x + 1 + ab_rng_range(&rng, 0, (r->w - 2 > 1 ? r->w - 2 : 1) - 1);
      int y = r->y + 1 + ab_rng_range(&rng, 0, (r->h - 2 > 1 ? r->h - 2 : 1) - 1);
      if (m->tiles[y][x] != T_FLOOR) continue;
      mon_spots[mon_spot_n][0] = x; mon_spots[mon_spot_n][1] = y;
      mon_spot_n++;
    }
  }

  /* tesori */
  tre_spot_n = 0;
  int numTre = 4 + depth / 2, tt = 0;
  while (tre_spot_n < numTre && tt < 300) {
    tt++;
    Room *r = &gen_rooms[ab_rng_range(&rng, 0, gen_nr - 1)];
    int x = r->x + 1 + ab_rng_range(&rng, 0, (r->w - 2 > 1 ? r->w - 2 : 1) - 1);
    int y = r->y + 1 + ab_rng_range(&rng, 0, (r->h - 2 > 1 ? r->h - 2 : 1) - 1);
    if (m->tiles[y][x] != T_FLOOR) continue;
    if (x == m->spawn_x && y == m->spawn_y) continue;
    if (x == m->merch_x && y == m->merch_y) continue;
    tre_spots[tre_spot_n][0] = x; tre_spots[tre_spot_n][1] = y;
    tre_spots[tre_spot_n][2] = ab_rng_next(&rng) < 0.18 ? 1 : 0;
    tre_spot_n++;
  }

  /* powerup spots (3) */
  pow_spot_n = 0;
  int pt = 0;
  while (pow_spot_n < 3 && pt < 200 && gen_nr > 1) {
    pt++;
    Room *r = &gen_rooms[1 + ab_rng_range(&rng, 0, gen_nr - 2)];
    int x = r->x + 1 + ab_rng_range(&rng, 0, (r->w - 2 > 1 ? r->w - 2 : 1) - 1);
    int y = r->y + 1 + ab_rng_range(&rng, 0, (r->h - 2 > 1 ? r->h - 2 : 1) - 1);
    if (m->tiles[y][x] != T_FLOOR) continue;
    pow_spots[pow_spot_n][0] = x; pow_spots[pow_spot_n][1] = y;
    pow_spot_n++;
  }

  /* pozioni a terra */
  pot_spot_n = 0;
  int numPot = 3 + depth / 3, pot = 0;
  while (pot_spot_n < numPot && pot < 250) {
    pot++;
    Room *r = &gen_rooms[ab_rng_range(&rng, 0, gen_nr - 1)];
    int x = r->x + 1 + ab_rng_range(&rng, 0, (r->w - 2 > 1 ? r->w - 2 : 1) - 1);
    int y = r->y + 1 + ab_rng_range(&rng, 0, (r->h - 2 > 1 ? r->h - 2 : 1) - 1);
    if (m->tiles[y][x] != T_FLOOR) continue;
    if (x == m->merch_x && y == m->merch_y) continue;
    pot_spots[pot_spot_n][0] = x; pot_spots[pot_spot_n][1] = y;
    pot_mana[pot_spot_n] = ab_rng_next(&rng) < 0.45 ? 1 : 0;
    pot_spot_n++;
  }

  /* torce sull'anello di muri attorno alle stanze */
  G.torch_count = 0;
  for (int ri = 0; ri < gen_nr && G.torch_count < MAX_TORCHES; ri++) {
    Room *r = &gen_rooms[ri];
    int n = ri == 0 ? 3 : 1 + ab_rng_range(&rng, 0, 2);
    int x0 = r->x - 1, x1 = r->x + r->w, y0 = r->y - 1, y1 = r->y + r->h;
    int placed = 0, tr = 0;
    while (placed < n && tr < 60) {
      tr++;
      int edge = ab_rng_range(&rng, 0, 3), x, y;
      if (edge == 0) { x = x0; y = y0 + 1 + ab_rng_range(&rng, 0, (r->h - 2 > 1 ? r->h - 2 : 1) - 1); }
      else if (edge == 1) { x = x1; y = y0 + 1 + ab_rng_range(&rng, 0, (r->h - 2 > 1 ? r->h - 2 : 1) - 1); }
      else if (edge == 2) { y = y0; x = x0 + 1 + ab_rng_range(&rng, 0, (r->w - 2 > 1 ? r->w - 2 : 1) - 1); }
      else { y = y1; x = x0 + 1 + ab_rng_range(&rng, 0, (r->w - 2 > 1 ? r->w - 2 : 1) - 1); }
      if (x < 1 || y < 1 || x >= W - 1 || y >= H - 1) continue;
      if (m->tiles[y][x] != T_WALL) continue;
      bool dup = false;
      for (int i = 0; i < G.torch_count; i++)
        if (G.torches[i].tx == x && G.torches[i].ty == y) { dup = true; break; }
      if (dup) continue;
      G.torches[G.torch_count].tx = x;
      G.torches[G.torch_count].ty = y;
      G.torch_count++;
      placed++;
    }
  }

  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) { m->visited[y][x] = 0; m->visible[y][x] = 0; }

  /* giocatore (tiene % hp a cambio piano) */
  const AbClassDef *cd = ab_class_def(G.p.cls);
  int eq_hp, eq_dp, eq_sp, eq_ap;
  ab_equip_bonus(&eq_hp, &eq_dp, &eq_sp, &eq_ap);
  double keep = 1.0;
  if (depth > 1 && G.p.max_hp > 0 && G.p.hp > 0) keep = (double)G.p.hp / (double)G.p.max_hp;
  G.p.max_hp = cd->hp + eq_hp;
  G.p.hp = (int)(G.p.max_hp * keep + 0.5);
  if (G.p.hp < 1) G.p.hp = 1;
  G.p.max_mp = cd->max_mp; G.p.mp = cd->max_mp;
  G.p.x = G.p.rx = m->spawn_x + 0.5;
  G.p.y = G.p.ry = m->spawn_y + 0.5;
  G.p.fx = 0; G.p.fy = 1;
  G.p.atk_cd = 0; G.p.ability_cd = 0; G.p.iframes = 0; G.p.anim_t = 0; G.p.charge_t = 0;
  G.p.poison_t = 0; G.p.web_t = 0;
  G.p.downed = false; G.p.dead = false;

  for (int i = 0; i < MAX_MONSTERS; i++) G.mons[i].active = false;
  for (int i = 0; i < MAX_PROJS; i++) G.projs[i].active = false;
  for (int i = 0; i < MAX_PARTS; i++) G.parts[i].active = false;
  for (int i = 0; i < MAX_FLOATS; i++) G.floats[i].active = false;
  for (int i = 0; i < MAX_ITEMS; i++) G.items[i].active = false;
  G.item_count = 0;

  /* spawn dinamico: rng dyn + affix solo al piano 1 */
  AbRng drng;
  ab_rng_seed(&drng, hash2(G.room, seedstr, "dyn", depth));
  int mi = 0;
  for (int i = 0; i < mon_spot_n && mi < MAX_MONSTERS - 8; i++) {
    /* pesi 1:1 */
    static const char pool[] = {'r','b','g','j','J','s','o','z','S','W','k','h','C','c','m','q','G'};
    int tot = 0;
    for (size_t k = 0; k < sizeof pool; k++) tot += ab_mon_weight(pool[k], depth);
    if (tot <= 0) break;
    int roll = 1 + (int)(ab_rng_next(&drng) * tot);
    char tk = 'g';
    for (size_t k = 0; k < sizeof pool; k++) {
      roll -= ab_mon_weight(pool[k], depth);
      if (roll <= 0) { tk = pool[k]; break; }
    }
    const AbMonDef *td = ab_mon_def(tk);
    if (!td) continue;
    int affix = 0;
    if (depth == 1) {
      char ab[32];
      snprintf(ab, sizeof ab, "%d_%d::affix", depth, i);
      AbRng arng; ab_rng_seed(&arng, ab_hash_str(ab));
      double chance = 0.1 + depth * 0.018;
      if (chance > 0.4) chance = 0.4;
      if (ab_rng_next(&arng) < chance) affix = 1 + (int)(ab_rng_next(&arng) * 3);
    }
    AbMonster *mm = &G.mons[mi++];
    mm->active = true;
    mm->type = tk;
    mm->x = mm->rx = mon_spots[i][0] + 0.5;
    mm->y = mm->ry = mon_spots[i][1] + 0.5;    mm->max_hp = mm->hp = ab_scaled_stat(td->hp, depth, 0.16);
    mm->dmg = ab_scaled_stat(td->dmg, depth, 0.11);
    mm->speed = td->speed;
    mm->aggro = td->aggro;
    mm->facing_x = 0; mm->facing_y = 1;
    mm->atk_cd = 0; mm->wander_t = 0; mm->wx = 0; mm->wy = 0;
    mm->dot_t = 0; mm->dot_acc = 0; mm->regen_acc = 0;
    mm->affix = affix;
    if (affix == 1) mm->speed *= 1.6;
    mm->is_boss = false;
    mm->spec_cd = 0; mm->breath_cd = 0; mm->fb_cd = 0; mm->fly_cd = 0;
    mm->fly_t = 0; mm->dive_t = 0; mm->summon_left = 0; mm->flying = false;
    mm->fx_mode = 0; mm->fx_t = 0; mm->fx_dur = 0; mm->fx_hit = false; mm->dent_t = 0;
    mm->atk_anim = 0; mm->atk_dur = (tk == 'o') ? 0.5 : 0.3;
    mm->phase = ((mi * 37) % 100) / 100.0;
  }
  /* boss */
  G.boss_active = false; G.boss_dead = false;
  m->gates_closed = false;
  if (m->has_arena) {
    const AbMonDef *td = ab_mon_def(m->arena_boss);
    if (td && mi < MAX_MONSTERS) {
      AbMonster *mm = &G.mons[mi++];
      mm->active = true;
      mm->type = m->arena_boss;
      mm->x = mm->rx = arena_cx + 0.5;
      mm->y = mm->ry = arena_cy + 0.5;
      mm->max_hp = mm->hp = ab_scaled_stat(td->hp, depth, 0.10);
      mm->dmg = ab_scaled_stat(td->dmg, depth, 0.08);
      mm->speed = td->speed;
      mm->aggro = 99;
      mm->is_boss = true;
      mm->affix = 0;
      mm->atk_cd = 0;
      mm->atk_anim = 0; mm->atk_dur = 0.3; mm->phase = 0.5;
      mm->spec_cd = 2.5; mm->breath_cd = 2.0; mm->fb_cd = 3.0; mm->fly_cd = 8.5;
      mm->fly_t = 0; mm->dive_t = 0; mm->flying = false;
      mm->summon_left = td->summon ? td->summon_n : 0;
      mm->fx_mode = 0; mm->dent_t = 0;
      strncpy(G.boss_name, td->name, sizeof G.boss_name - 1);
      G.boss_hp = mm->hp; G.boss_max = mm->max_hp;
    }
  }

  /* forzieri */
  G.chest_count = 0;
  for (int i = 0; i < chest_spot_n && G.chest_count < MAX_CHESTS; i++) {
    AbChest *c = &G.chests[G.chest_count++];
    c->active = true;
    c->tx = chest_x[i]; c->ty = chest_y[i];
    c->open = false;
    c->boss_chest = chest_boss[i] ? true : false;
    snprintf(c->id, sizeof c->id, "c%d_%d_%d", depth, c->tx, c->ty);
  }

  /* oggetti iniziali 1:1 */
  for (int i = 0; i < tre_spot_n && G.item_count < MAX_ITEMS; i++) {
    AbItem *it = &G.items[G.item_count++];
    it->active = true;
    it->x = tre_spots[i][0] + 0.5; it->y = tre_spots[i][1] + 0.5;
    if (tre_spots[i][2]) { it->kind = 1; it->amount = 15 + ab_rng_range(&drng, 0, 10 * depth - 1 > 0 ? 10 * depth - 1 : 0); }
    else { it->kind = 0; it->amount = 3 + ab_rng_range(&drng, 0, 6 * depth - 1 > 0 ? 6 * depth - 1 : 0); }
  }
  if (pow_spot_n > 0 && G.item_count < MAX_ITEMS) {
    int si = ab_rng_range(&drng, 0, pow_spot_n - 1);
    AbItem *it = &G.items[G.item_count++];
    it->active = true;
    it->x = pow_spots[si][0] + 0.5; it->y = pow_spots[si][1] + 0.5;
    it->kind = 2; it->buff = ab_rng_range(&drng, 0, BUFF_COUNT - 1);
  }
  for (int i = 0; i < pot_spot_n && G.item_count < MAX_ITEMS; i++) {
    AbItem *it = &G.items[G.item_count++];
    it->active = true;
    it->x = pot_spots[i][0] + 0.5; it->y = pot_spots[i][1] + 0.5;
    it->kind = pot_mana[i] ? 4 : 3; it->amount = 1;
  }
  ab_update_fov();
}

/* ---------------- FOV Bresenham R 7.4 ---------------- */
static void cast_ray(AbMap *m, int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, x = x0, y = y0;
  int W = m->w, H = m->h;
  while (true) {
    if (x >= 0 && y >= 0 && x < W && y < H) {
      m->visible[y][x] = 1;
      if (m->tiles[y][x] == T_WALL) break;
    } else break;
    if (x == x1 && y == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x += sx; }
    if (e2 <= dx) { err += dx; y += sy; }
  }
}
void ab_update_fov(void) {
  AbMap *m = &G.map;
  for (int y = 0; y < m->h; y++)
    for (int x = 0; x < m->w; x++) m->visible[y][x] = 0;
  int tx = (int)floor(G.p.x), ty = (int)floor(G.p.y);
  int R = 8;
  for (int yy = ty - R; yy <= ty + R; yy++) {
    for (int xx = tx - R; xx <= tx + R; xx++) {
      if (xx < 0 || yy < 0 || xx >= m->w || yy >= m->h) continue;
      double ddx = xx - tx, ddy = yy - ty;
      if (ddx * ddx + ddy * ddy > FOV_RADIUS * FOV_RADIUS) continue;
      cast_ray(m, tx, ty, xx, yy);
    }
  }
  for (int y = 0; y < m->h; y++)
    for (int x = 0; x < m->w; x++)
      if (m->visible[y][x]) m->visited[y][x] = 1;
}

/* ---------------- equip ---------------- */
static int rarity_roll(AbRng *r, int depth) {
  /* pesi: comune 100, raro 32, epico 9, leggendario 2; alte crescono */
  double w[4] = {100.0, 32.0 * (1 + depth * 0.045), 9.0 * (1 + depth * 0.045), 2.0 * (1 + depth * 0.045)};
  double tot = w[0] + w[1] + w[2] + w[3];
  double roll = ab_rng_next(r) * tot;
  for (int i = 0; i < 4; i++) { roll -= w[i]; if (roll <= 0) return i; }
  return 0;
}
static int stat_calc(int key, int depth) {
  /* 0 hp,1 dmgPct,2 speedPct,3 armorPct */
  if (key == 0) return 4 + (depth * 16) / 10;
  if (key == 1) return 5 + (depth * 11) / 10;
  if (key == 2) return 3 + (depth * 5) / 10;
  return 4 + (depth * 9) / 10;
}
static void make_equip(AbRng *r, int depth, int slot, int rarity, AbEquip *e) {
  static const double mult[4] = {1.0, 1.35, 1.6, 2.2};
  static const int scount[4] = {1, 1, 2, 3};
  e->filled = true;
  e->rarity = rarity;
  e->hp = e->dmg_pct = e->spd_pct = e->arm_pct = 0;
  int pool[4] = {0, 1, 2, 3}, pn = 4;
  for (int i = 0; i < scount[rarity] && pn > 0; i++) {
    int idx = (int)(ab_rng_next(r) * pn);
    int key = pool[idx];
    pool[idx] = pool[--pn];
    int v = (int)(stat_calc(key, depth) * mult[rarity] + 0.5);
    if (key == 0) e->hp = v;
    else if (key == 1) e->dmg_pct = v;
    else if (key == 2) e->spd_pct = v;
    else e->arm_pct = v;
  }
  snprintf(e->name, sizeof e->name, "%s %s", ab_rarity_name(rarity), EQUIP_SLOT_NAMES[slot]);
}
static int equip_power(AbEquip *e) {
  if (!e->filled) return -1;
  return e->hp + e->dmg_pct + e->spd_pct + e->arm_pct;
}
void ab_equip_bonus(int *hp, int *dmg_pct, int *spd_pct, int *arm_pct) {
  *hp = *dmg_pct = *spd_pct = *arm_pct = 0;
  for (int i = 0; i < 5; i++) {
    if (!G.p.equip[i].filled) continue;
    *hp += G.p.equip[i].hp;
    *dmg_pct += G.p.equip[i].dmg_pct;
    *spd_pct += G.p.equip[i].spd_pct;
    *arm_pct += G.p.equip[i].arm_pct;
  }
}
static void equip_or_keep(int slot, int rarity, int hp, int dp, int sp, int ap) {
  AbEquip cur = G.p.equip[slot];
  AbEquip nw;
  nw.filled = true; nw.rarity = rarity;
  nw.hp = hp; nw.dmg_pct = dp; nw.spd_pct = sp; nw.arm_pct = ap;
  snprintf(nw.name, sizeof nw.name, "%s %s", ab_rarity_name(rarity), EQUIP_SLOT_NAMES[slot]);
  if (!cur.filled || equip_power(&nw) >= equip_power(&cur)) {
    int ohp, odp, osp, oap;
    ab_equip_bonus(&ohp, &odp, &osp, &oap);
    G.p.equip[slot] = nw;
    int nhp, ndp, nsp, nap;
    ab_equip_bonus(&nhp, &ndp, &nsp, &nap);
    int dh = nhp - ohp;
    G.p.max_hp += dh;
    if (dh > 0) G.p.hp += dh;
    if (G.p.hp > G.p.max_hp) G.p.hp = G.p.max_hp;
    double cr, cg, cb;
    ab_rarity_color(rarity, &cr, &cg, &cb);
    char b[64];
    snprintf(b, sizeof b, "+%s %s", EQUIP_SLOT_NAMES[slot], ab_rarity_name(rarity));
    ab_float_text(G.p.x, G.p.y - 0.7, b, cr, cg, cb);
    char l[96];
    snprintf(l, sizeof l, "Equipaggiato: %s.", nw.name);
    ab_add_log(l);
    if (rarity >= R_EPICO) {
      char t[64];
      snprintf(t, sizeof t, "%s - %s", ab_rarity_name(rarity), EQUIP_SLOT_NAMES[slot]);
      for (size_t i = 0; t[i]; i++) if (t[i] >= 'a' && t[i] <= 'z') t[i] -= 32;
      ab_loot_banner(t, cr, cg, cb);
      ab_burst(G.p.x, G.p.y, 12, cr, cg, cb, 4);
    }
  } else {
    ab_add_log("Gia possiedi di meglio.");
  }
}

/* ---------------- mercante ---------------- */
int ab_merchant_price(int idx) {
  int d = G.depth;
  if (idx == 0) return 12 + d * 2;
  if (idx == 1) return 12 + d * 2;
  if (idx == 2) return 20 + d * 4;
  if (idx == 3) return 45 + d * 8;
  return -1;
}
bool ab_merchant_buy(int idx) {
  int price = ab_merchant_price(idx);
  if (price < 0) return false;
  if (G.p.gold < price) { ab_toast("Non hai abbastanza oro"); return false; }
  G.p.gold -= price;
  if (idx == 0) { G.p.potions++; ab_add_log("Pozione di salute comprata."); }
  else if (idx == 1) { G.p.mana_potions++; ab_add_log("Pozione di mana comprata."); }
  else if (idx == 2) {
    int b = ab_rng_range(&(AbRng){(uint32_t)(G.time * 1000 + 5)}, 0, BUFF_COUNT - 1);
    G.p.buffs[b] = BUFF_DURATION[b];
    char l[96];
    snprintf(l, sizeof l, "Mercante: %s!", BUFF_NAMES[b]);
    ab_add_log(l);
    ab_float_text(G.p.x, G.p.y - 0.7, BUFF_NAMES[b], 0.5, 1, 0.5);
  } else {
    AbRng r; ab_rng_seed(&r, (uint32_t)(G.time * 977 + 3));
    int slot = ab_rng_range(&r, 0, 4);
    int rar = rarity_roll(&r, G.depth);
    AbEquip e; make_equip(&r, G.depth, slot, rar, &e);
    G.p.equip[slot].filled = false; /* forza confronto */
    equip_or_keep(slot, e.rarity, e.hp, e.dmg_pct, e.spd_pct, e.arm_pct);
  }
  ab_burst(G.p.x, G.p.y, 8, 1, 0.85, 0.3, 3);
  return true;
}

/* ---------------- pozioni / interact ---------------- */
void ab_drink_potion(void) {
  if (G.p.dead || G.p.downed) return;
  if (G.p.potions <= 0) return;
  if (G.p.hp >= G.p.max_hp) { ab_toast("Sei gia in piena salute"); return; }
  G.p.potions--;
  int heal = (int)(G.p.max_hp * 0.55 + 0.5) + 2;
  G.p.hp += heal;
  if (G.p.hp > G.p.max_hp) G.p.hp = G.p.max_hp;
  ab_burst(G.p.x, G.p.y, 10, 0.5, 1, 0.5, 3);
  char b[32]; snprintf(b, sizeof b, "+%d", heal);
  ab_float_text(G.p.x, G.p.y - 0.7, b, 0.5, 1, 0.5);
}
void ab_drink_mana(void) {
  if (G.p.dead || G.p.downed) return;
  const AbClassDef *c = ab_class_def(G.p.cls);
  if (c->max_mp <= 0) { ab_toast("La tua classe non usa mana"); return; }
  if (G.p.mana_potions <= 0) return;
  if (G.p.mp >= G.p.max_mp) { ab_toast("Mana gia pieno"); return; }
  G.p.mana_potions--;
  int restore = (int)(G.p.max_mp * 0.6 + 0.5) + 1;
  G.p.mp += restore;
  if (G.p.mp > G.p.max_mp) G.p.mp = G.p.max_mp;
  ab_float_text(G.p.x, G.p.y - 0.7, "MANA", 0.4, 0.7, 1);
}

void ab_try_interact(void) {
  if (G.p.dead || G.p.downed) return;
  bool joined = (net_role == NET_JOIN && net_connected);
  int px = (int)floor(G.p.x), py = (int)floor(G.p.y);
  if (px >= 0 && py >= 0 && px < G.map.w && py < G.map.h &&
      G.map.tiles[py][px] == T_STAIRS) {
    if (joined) { net_send_stairs(); ab_toast("Segnalato all'host..."); }
    else ab_descend();
    return;
  }
  /* rianima il compagno caduto (come la web) */
  if (net_connected && net_peer.active && net_peer.downed && !net_peer.dead) {
    double dx = G.p.x - net_peer.x, dy = G.p.y - net_peer.y;
    if (dx * dx + dy * dy < 2.1 * 2.1) {
      net_send_revive();
      sfx_revive();
      ab_add_log("Hai rianimato il compagno!");
      return;
    }
  }
  double mdx = G.p.x - (G.map.merch_x + 0.5), mdy = G.p.y - (G.map.merch_y + 0.5);
  if (mdx * mdx + mdy * mdy < 2.1 * 2.1) {
    if (!G.merchant_open) G.merchant_open = true;
    return;
  }
  for (int i = 0; i < G.chest_count; i++) {
    AbChest *c = &G.chests[i];
    if (!c->active || c->open) continue;
    double dx = G.p.x - (c->tx + 0.5), dy = G.p.y - (c->ty + 0.5);
    if (dx * dx + dy * dy > 2.1 * 2.1) continue;
    if (joined) { net_send_open(i); return; }
    if (c->boss_chest && !G.boss_dead) { ab_toast("Sigillato: uccidi il boss!"); return; }
    c->open = true;
    sfx_chest();
    ab_burst(c->tx + 0.5, c->ty + 0.5, 12, 1, 0.85, 0.3, 3.5);
    AbRng r; ab_rng_seed(&r, (uint32_t)(G.time * 131 + i * 17 + G.depth));
    if (c->boss_chest) {
      int gold = 60 + ab_rng_range(&r, 0, 25 * G.depth - 1 > 0 ? 25 * G.depth - 1 : 0);
      G.p.gold += gold;
      G.p.potions++; G.p.mana_potions++;
      int rar = ab_rng_next(&r) < 0.45 ? R_LEGGENDARIO : R_EPICO;
      int slot = ab_rng_range(&r, 0, 4);
      AbEquip e; make_equip(&r, G.depth, slot, rar, &e);
      char b[48]; snprintf(b, sizeof b, "+%d ORO (tana)", gold);
      ab_add_log(b);
      equip_or_keep(slot, e.rarity, e.hp, e.dmg_pct, e.spd_pct, e.arm_pct);
    } else {
      int gold = 6 + ab_rng_range(&r, 0, 9 * G.depth - 1 > 0 ? 9 * G.depth - 1 : 0);
      G.p.gold += gold;
      char b[48]; snprintf(b, sizeof b, "+%d ORO", gold);
      ab_float_text(c->tx + 0.5, c->ty, b, 1, 0.85, 0.3);
      if (ab_rng_next(&r) < 0.32) { G.p.potions++; ab_add_log("Pozione trovata."); }
      if (ab_rng_next(&r) < 0.24) { G.p.mana_potions++; ab_add_log("Pozione di mana trovata."); }
      if (ab_rng_next(&r) < 0.2) {
        int slot = ab_rng_range(&r, 0, 4);
        int rar = rarity_roll(&r, G.depth);
        AbEquip e; make_equip(&r, G.depth, slot, rar, &e);
        equip_or_keep(slot, e.rarity, e.hp, e.dmg_pct, e.spd_pct, e.arm_pct);
      }
    }
    return;
  }
}

void ab_descend(void) {
  int nd = G.depth + 1;
  ab_add_log("Scendi piu' a fondo...");
  sfx_stair();
  ab_gen_depth(nd);
  if (net_role == NET_HOST && net_connected) net_send_depth(nd);
  char b[64];
  snprintf(b, sizeof b, "Piano %d", nd);
  ab_toast(b);
  if (ab_is_boss_floor(nd)) {
    char bb[96];
    snprintf(bb, sizeof bb, "TANA: %s!", ab_boss_name(ab_boss_for_depth(nd)));
    ab_add_log(bb);
    ab_toast(bb);
  }
}

/* ---------------- combattimento ---------------- */
static double roll_amount(int cls, bool *crit) {
  const AbClassDef *c = ab_class_def(cls);
  AbRng r;
  ab_rng_seed(&r, (uint32_t)(G.time * 977) + (uint32_t)(G.p.x * 57) + (uint32_t)(G.p.y * 131) + (uint32_t)rand());
  double dmg = c->dmg_min + ab_rng_next(&r) * (c->dmg_max - c->dmg_min);
  *crit = false;
  if (c->crit > 0 && ab_rng_next(&r) < c->crit) { dmg *= 1.7; *crit = true; }
  int hp, dp, sp, ap;
  ab_equip_bonus(&hp, &dp, &sp, &ap);
  dmg *= (1 + dp / 100.0);
  return dmg;
}

static AbMonster *nearest_mon(double range, bool use_rx) {
  AbMonster *best = NULL;
  double bd = range * range;
  for (int i = 0; i < MAX_MONSTERS; i++) {
    if (!G.mons[i].active) continue;
    double mx = use_rx ? G.mons[i].rx : G.mons[i].x;
    double my = use_rx ? G.mons[i].ry : G.mons[i].y;
    double dx = mx - G.p.x, dy = my - G.p.y;
    double d2 = dx * dx + dy * dy;
    if (d2 < range * range && d2 < bd) { bd = d2; best = &G.mons[i]; }
  }
  return best;
}

static void spawn_proj(double x, double y, double dx, double dy, double spd, double range, int dmg, bool friendly, double r, double g, double b, double hit_r, bool poison, bool web, double life, bool pierce) {
  for (int i = 0; i < MAX_PROJS; i++) {
    if (G.projs[i].active) continue;
    AbProj *p = &G.projs[i];
    static unsigned seq = 1;
    p->active = true;
    p->x = x; p->y = y;
    double l = sqrt(dx*dx + dy*dy);
    if (l < 0.001) { dx = 0; dy = 1; l = 1; }
    p->vx = dx / l * spd; p->vy = dy / l * spd;
    p->life = life; p->range_left = range;
    p->dmg = dmg;
    p->friendly = friendly;
    p->poison = poison; p->web = web; p->pierce = pierce;
    p->hit_r = hit_r > 0 ? hit_r : 0.45;
    p->r = r; p->g = g; p->b = b;
    p->size = 0.09;
    p->id_mark = seq++;
    if (seq == 0) seq = 1;
    return;
  }
}

static void hurt_player(int dmg, bool poison) {
  if (G.god) return;
  if (G.p.dead || G.p.iframes > 0) return;
  G.p.iframes = 0.45;
  if (G.p.downed) {
    G.p.dead = true;
    G.state = ST_DEAD;
    sfx_death();
    ab_add_log("Colpito a terra: morte.");
    G.p.gold = 0; G.p.potions = 0; G.p.mana_potions = 0;
    memset(G.p.equip, 0, sizeof G.p.equip);
    if (G.depth > G.best_depth) G.best_depth = G.depth;
    ab_save_record();
    return;
  }
  double fa = G.p.buffs[BUFF_SHIELD] > 0 ? dmg * 0.5 : dmg;
  int hp, dp, sp, ap;
  ab_equip_bonus(&hp, &dp, &sp, &ap);
  double m = 1 - ap / 100.0;
  if (m < 0.2) m = 0.2;
  fa *= m;
  int fin = (int)(fa + 0.5);
  if (fin < 1) fin = 1;
  G.p.hp -= fin;
  if (G.p.hp < 0) G.p.hp = 0;
  G.shake = 0.3;
  sfx_hurt();
  char b[16]; snprintf(b, sizeof b, "-%d", fin);
  ab_float_text(G.p.x, G.p.y - 0.6, b, 1, 0.3, 0.3);
  ab_burst(G.p.x, G.p.y, 6, 1, 0.2, 0.2, 3);
  if (poison) G.p.poison_t = 3;
  if (G.p.hp <= 0) {
    G.p.downed = true; G.p.downed_t = DOWNED_BLEEDOUT; G.p.hp = 0;
    ab_toast("A TERRA! Nessuno puo rianimarti...");
    ab_add_log("Sei a terra! Senza compagni, l'abisso ti reclama.");
  }
}

static void kill_monster(int i) {
  AbMonster *m = &G.mons[i];
  const AbMonDef *td = ab_mon_def(m->type);
  AbRng r;
  ab_rng_seed(&r, (uint32_t)(G.time * 131) + (uint32_t)i * 17 + 3);
  int gold = td ? td->gold_min + ab_rng_range(&r, 0, td->gold_max - td->gold_min) : 2;
  G.p.gold += gold;
  char b[24]; snprintf(b, sizeof b, "+%d", gold);
  ab_float_text(m->x, m->y - 0.5, b, 1, 0.85, 0.3);
  ab_burst(m->x, m->y, m->is_boss ? 30 : 10, 1, 0.5, 0.2, 4);
  ab_burst(m->x, m->y, 13, 0.5, 0.4, 0.3, 3);
  if (m->affix == 2) {
    sfx_boom();
    for (int k = 0; k < MAX_MONSTERS; k++) {
      if (k != i && G.mons[k].active && ab_dist(m->x, m->y, G.mons[k].x, G.mons[k].y) < 1.8)
        G.mons[k].hp -= 6;
    }
    if (ab_dist(m->x, m->y, G.p.x, G.p.y) < 1.8) hurt_player(5, false);
    if (peer_targetable() && ab_dist(m->x, m->y, net_peer.x, net_peer.y) < 1.8)
      net_send_hurt_to_peer(5, false, false);
    ab_burst(m->x, m->y, 16, 1, 0.4, 0.1, 6);
    G.shake = 0.35;
  }
  if (td && td->split) {
    for (int k = 0; k < 2; k++) {
      for (int j = 0; j < MAX_MONSTERS; j++) {
        if (G.mons[j].active) continue;
        const AbMonDef *jd = ab_mon_def('j');
        G.mons[j].active = true;
        G.mons[j].type = 'j';
        G.mons[j].x = G.mons[j].rx = m->x + (k ? 0.4 : -0.4);
        G.mons[j].y = G.mons[j].ry = m->y;
        G.mons[j].max_hp = G.mons[j].hp = ab_scaled_stat(jd->hp, G.depth, 0.16);
        G.mons[j].dmg = ab_scaled_stat(jd->dmg, G.depth, 0.11);
        G.mons[j].speed = jd->speed; G.mons[j].aggro = jd->aggro;
        G.mons[j].is_boss = false; G.mons[j].affix = 0;
        G.mons[j].atk_cd = 0; G.mons[j].fx_mode = 0; G.mons[j].dent_t = 0;
        G.mons[j].atk_anim = 0; G.mons[j].atk_dur = 0.3;
        G.mons[j].phase = ((j * 37) % 100) / 100.0;
        break;
      }
    }
  }
  if (m->is_boss) {
    G.boss_dead = true;
    G.boss_active = false;
    G.map.gates_closed = false;
    sfx_bossKill();
    ab_toast("BOSS SCONFITTO!");
    ab_add_log("Il boss cade. La tana si apre.");
    ab_burst(m->x, m->y, 40, 1, 0.9, 0.4, 6);
    G.shake = 0.5;
    if (G.depth > G.best_depth) { G.best_depth = G.depth; ab_save_record(); }
  } else {
    sfx_kill();
  }
  m->active = false;
}

void ab_player_attack(void) {
  if (G.p.dead || G.p.downed) return;
  if (G.p.atk_cd > 0) return;
  const AbClassDef *c = ab_class_def(G.p.cls);
  if (c->max_mp > 0 && c->mana_cost > 0 && G.p.mp < c->mana_cost) {
    ab_toast("Mana insufficiente");
    return;
  }
  G.p.atk_cd = c->atk_cooldown * (G.p.buffs[BUFF_FOCUS] > 0 ? 0.5 : 1.0);
  G.p.anim_t = 0.22;
  if (c->max_mp > 0 && c->mana_cost) { G.p.mp -= c->mana_cost; if (G.p.mp < 0) G.p.mp = 0; }
  if (!c->ranged) sfx_swing();
  else if (G.p.cls == CLS_MAGO || G.p.cls == CLS_NEGROMANTE) sfx_shootMage();
  else if (G.p.cls == CLS_PROF) sfx_shootPlasma();
  else sfx_shootRanger();
  double ar = c->ranged ? c->range : (c->range + 0.7 > 2.1 ? c->range + 0.7 : 2.1);
  AbMonster *nm = nearest_mon(ar, true);
  if (nm) {
    double dx = nm->rx - G.p.x, dy = nm->ry - G.p.y;
    double l = sqrt(dx*dx + dy*dy);
    if (l > 0.01) { G.p.fx = dx / l; G.p.fy = dy / l; }
  }
  double rage = G.p.buffs[BUFF_RAGE] > 0 ? 1.4 : 1.0;
  if (!c->ranged) {
    bool joined = (net_role == NET_JOIN && net_connected);
    for (int i = 0; i < MAX_MONSTERS; i++) {
      if (!G.mons[i].active) continue;
      double dx = G.mons[i].rx - G.p.x, dy = G.mons[i].ry - G.p.y;
      double dd = sqrt(dx*dx + dy*dy);
      if (dd > c->range) continue;
      double dot = 0;
      if (dd > 0.01) dot = (dx * G.p.fx + dy * G.p.fy) / dd;
      if (dot < cos(c->arc)) continue;
      bool cr = false;
      int amount = (int)(roll_amount(G.p.cls, &cr) * rage + 0.5);
      if (joined) net_send_hit(i, amount);
      else G.mons[i].hp -= amount;
      G.mons[i].facing_x = -G.p.fx; G.mons[i].facing_y = -G.p.fy;
      char b[16]; snprintf(b, sizeof b, cr ? "%d!" : "%d", amount);
      ab_float_text(G.mons[i].rx, G.mons[i].ry - 0.5, b, cr ? 1 : 0.95, cr ? 0.81 : 0.95, cr ? 0.36 : 0.95);
      if (cr) { sfx_crit(); G.shake = 0.32; ab_crit_flash(); }
      else sfx_hit();
      ab_burst(G.mons[i].rx, G.mons[i].ry, 6, 1, 0.8, 0.3, 3);
    }
    ab_burst(G.p.x + G.p.fx * 0.8, G.p.y + G.p.fy * 0.8, 4, 1, 1, 1, 2.5);
  } else {
    bool cr = false;
    int dmg = (int)(roll_amount(G.p.cls, &cr) * rage + 0.5);
    if (cr) { sfx_crit(); G.shake = 0.32; ab_crit_flash(); }
    double pr = 0.55, pg = 0.85, pb = 1.0;
    if (G.p.cls == CLS_NEGROMANTE) { pr = 0.56; pg = 0.88; pb = 0.48; }
    else if (G.p.cls == CLS_RANGER) { pr = 0.85; pg = 0.9; pb = 0.69; }
    else if (G.p.cls == CLS_PROF) { pr = 0.49; pg = 0.98; pb = 1.0; }
    spawn_proj(G.p.x, G.p.y, G.p.fx, G.p.fy, c->proj_speed, c->range, dmg, true, pr, pg, pb, 0.45, false, false, 1.6, false);
    ab_burst(G.p.x + G.p.fx * 0.5, G.p.y + G.p.fy * 0.5, 8, pr, pg, pb, 2.5);
  }
}

void ab_use_ability(void) {
  if (G.p.dead || G.p.downed) return;
  const AbClassDef *c = ab_class_def(G.p.cls);
  if (G.p.ability_cd > 0) {
    char b[64];
    snprintf(b, sizeof b, "Abilita in recupero (%.0fs)", G.p.ability_cd + 0.99);
    ab_toast(b);
    return;
  }
  if (c->ability_mana > 0 && G.p.mp < c->ability_mana) {
    ab_toast("Mana insufficiente");
    return;
  }
  G.p.ability_cd = c->ability_cd;
  if (c->ability_mana) { G.p.mp -= c->ability_mana; if (G.p.mp < 0) G.p.mp = 0; }
  sfx_ability();
  bool cr = false;
  double rage = G.p.buffs[BUFF_RAGE] > 0 ? 1.4 : 1.0;
  int dmg = (int)(roll_amount(G.p.cls, &cr) * rage + 0.5);
  switch (G.p.cls) {
    case CLS_GUERRIERO: { /* Carica: 4.5 in 18 passi, danno x1.3 */
      double ox = G.p.x, oy = G.p.y;
      int hits = 0;
      bool done[MAX_MONSTERS] = {false};
      for (int s = 1; s <= 18; s++) {
        double nx = ox + G.p.fx * 4.5 * s / 18.0;
        double ny = oy + G.p.fy * 4.5 * s / 18.0;
        if (!ab_is_walkable((int)nx, (int)ny)) break;
        G.p.x = nx; G.p.y = ny;
        if (s % 4 == 0) ab_burst(nx, ny + 0.3, 2, 0.54, 0.48, 0.36, 0.6);
        for (int i = 0; i < MAX_MONSTERS; i++) {
          if (!G.mons[i].active || done[i]) continue;
          if (ab_dist(nx, ny, G.mons[i].rx, G.mons[i].ry) < 0.7) {
            done[i] = true;
            bool c2 = false;
            int a = (int)(roll_amount(G.p.cls, &c2) * 1.3 * rage + 0.5);
            G.mons[i].hp -= a;
            char b[16]; snprintf(b, sizeof b, "%d", a);
            ab_float_text(G.mons[i].rx, G.mons[i].ry - 0.5, b, 1, 0.81, 0.36);
            ab_burst(G.mons[i].rx, G.mons[i].ry, 6, 0.86, 0.86, 0.88, 3);
            sfx_hit();
            hits++;
          }
        }
      }
      G.p.iframes = G.p.iframes > 0.35 ? G.p.iframes : 0.35;
      G.shake = 0.28;
      ab_burst(G.p.x, G.p.y, 10, 0.86, 0.86, 0.88, 3);
      ab_float_text(G.p.x, G.p.y - 0.8, "CARICA!", 0.86, 0.86, 0.88);
      {
        char l[64];
        snprintf(l, sizeof l, "Carica: travolti %d nemici.", hits);
        ab_add_log(l);
      }
      break;
    }
    case CLS_LADRO: { /* Passo Furtivo: raggio 6, danno medio x1.9 */
      AbMonster *tgt = NULL;
      double bd = 36.0;
      for (int i = 0; i < MAX_MONSTERS; i++) {
        if (!G.mons[i].active) continue;
        double dx = G.mons[i].rx - G.p.x, dy = G.mons[i].ry - G.p.y;
        double d2 = dx * dx + dy * dy;
        if (d2 < 36.0 && d2 < bd) { bd = d2; tgt = &G.mons[i]; }
      }
      double ox = G.p.x, oy = G.p.y;
      if (tgt) {
        double dx = tgt->rx - G.p.x, dy = tgt->ry - G.p.y;
        double d = sqrt(bd);
        if (d < 0.01) d = 1;
        double sx = tgt->rx - dx / d * 0.85, sy = tgt->ry - dy / d * 0.85;
        if (ab_is_walkable((int)sx, (int)sy)) { G.p.x = sx; G.p.y = sy; }
        G.p.fx = dx / d; G.p.fy = dy / d;
        int a = (int)(((c->dmg_min + c->dmg_max) / 2.0) * 1.9 + 0.5);
        tgt->hp -= a;
        char b[16]; snprintf(b, sizeof b, "%d!", a);
        ab_float_text(tgt->rx, tgt->ry - 0.5, b, 1, 0.81, 0.36);
        ab_burst(tgt->rx, tgt->ry, 8, 1, 0.88, 0.54, 4);
        sfx_crit();
        ab_crit_flash();
        G.shake = 0.24;
        ab_add_log("Passo furtivo: critico garantito!");
      } else {
        double nx = G.p.x + G.p.fx * 2, ny = G.p.y + G.p.fy * 2;
        if (ab_is_walkable((int)nx, (int)ny)) { G.p.x = nx; G.p.y = ny; }
        ab_add_log("Nessun bersaglio: solo uno scatto.");
      }
      G.p.iframes = 1.4;
      ab_burst(ox, oy, 10, 0.29, 0.23, 0.48, 2);
      ab_burst(G.p.x, G.p.y, 10, 0.54, 0.36, 1.0, 2);
      ab_float_text(G.p.x, G.p.y - 0.8, "PASSO FURTIVO", 0.85, 0.9, 1.0);
      break;
    }
    case CLS_MAGO: { /* Onda d'Urto: raggio 2.3, x1.2 */
      int hits = 0;
      for (int i = 0; i < MAX_MONSTERS; i++) {
        if (!G.mons[i].active) continue;
        if (ab_dist(G.p.x, G.p.y, G.mons[i].rx, G.mons[i].ry) < 2.3) {
          bool c2 = false;
          int a = (int)(roll_amount(G.p.cls, &c2) * 1.2 * rage + 0.5);
          G.mons[i].hp -= a;
          char b[16]; snprintf(b, sizeof b, "%d", a);
          ab_float_text(G.mons[i].rx, G.mons[i].ry - 0.5, b, 0.56, 0.82, 1.0);
          ab_burst(G.mons[i].rx, G.mons[i].ry, 6, 0.56, 0.82, 1.0, 3);
          sfx_hit();
          hits++;
        }
      }
      ab_burst(G.p.x, G.p.y, 22, 0.56, 0.82, 1.0, 4);
      ab_shock(G.p.x, G.p.y, 0.56, 0.82, 1.0);
      sfx_boom();
      G.shake = 0.55;
      ab_float_text(G.p.x, G.p.y - 0.8, "ONDA D'URTO", 0.56, 0.82, 1.0);
      {
        char l[64];
        snprintf(l, sizeof l, "Onda d'urto: colpiti %d nemici.", hits);
        ab_add_log(l);
      }
      break;
    }
    case CLS_NEGROMANTE: { /* Drenaggio: raggio 2.4, x1.2, cura meta drenato */
      int hits = 0, drained = 0;
      for (int i = 0; i < MAX_MONSTERS; i++) {
        if (!G.mons[i].active) continue;
        if (ab_dist(G.p.x, G.p.y, G.mons[i].rx, G.mons[i].ry) < 2.4) {
          bool c2 = false;
          int a = (int)(roll_amount(G.p.cls, &c2) * 1.2 * rage + 0.5);
          G.mons[i].hp -= a;
          char b[16]; snprintf(b, sizeof b, "%d", a);
          ab_float_text(G.mons[i].rx, G.mons[i].ry - 0.5, b, 0.56, 0.88, 0.48);
          ab_burst(G.mons[i].rx, G.mons[i].ry, 6, 0.56, 0.88, 0.48, 3);
          sfx_hit();
          hits++; drained += a;
        }
      }
      if (drained > 0) {
        int heal = drained / 2;
        if (heal < 1) heal = 1;
        G.p.hp += heal;
        if (G.p.hp > G.p.max_hp) G.p.hp = G.p.max_hp;
        char b[32]; snprintf(b, sizeof b, "+%d", heal);
        ab_float_text(G.p.x, G.p.y - 0.7, b, 0.56, 0.88, 0.48);
        ab_burst(G.p.x, G.p.y - 0.3, 10, 0.56, 0.88, 0.48, 2);
      }
      ab_burst(G.p.x, G.p.y, 18, 0.56, 1.0, 0.84, 4);
      ab_shock(G.p.x, G.p.y, 0.56, 0.88, 0.48);
      sfx_boom();
      G.shake = 0.5;
      ab_float_text(G.p.x, G.p.y - 0.8, "DRENAGGIO", 0.56, 0.88, 0.48);
      {
        char l[96];
        snprintf(l, sizeof l, "Drenaggio: %d nemici, %d danni.", hits, drained);
        ab_add_log(l);
      }
      break;
    }
    case CLS_RANGER: { /* Raffica: 5 frecce, danno x0.7 */
      double base = atan2(G.p.fy, G.p.fx);
      const double off[5] = {-0.42, -0.21, 0, 0.21, 0.42};
      for (int k = 0; k < 5; k++) {
        double a = base + off[k];
        int admg = (int)(dmg * 0.7 + 0.5);
        spawn_proj(G.p.x, G.p.y, cos(a), sin(a), 15, 9, admg, true, 0.85, 0.9, 0.69, 0.45, false, false, 1.6, false);
      }
      sfx_shootRanger();
      ab_burst(G.p.x, G.p.y, 8, 0.85, 0.9, 0.69, 3);
      ab_float_text(G.p.x, G.p.y - 0.8, "RAFFICA!", 0.85, 0.9, 0.69);
      break;
    }
    case CLS_PALADINO:
      G.p.buffs[BUFF_SHIELD] = 5.0;
      ab_burst(G.p.x, G.p.y, 10, 1, 0.85, 0.54, 3);
      ab_float_text(G.p.x, G.p.y - 0.8, "MURO SACRO", 1, 0.85, 0.54);
      ab_add_log("Muro Sacro: danno dimezzato per 5s.");
      break;
    case CLS_BARDO:
      G.p.buffs[BUFF_RAGE] = 8.0;
      ab_burst(G.p.x, G.p.y - 0.3, 10, 1, 0.81, 0.36, 2);
      ab_float_text(G.p.x, G.p.y - 0.8, "CANTO", 1, 0.81, 0.36);
      ab_add_log("Canto: +40% danno per 8s.");
      break;
    case CLS_MONACO: { /* Onda di Chi: un proiettile perforante x1.5 */
      int wdmg = (int)(dmg * 1.5 + 0.5);
      spawn_proj(G.p.x, G.p.y, G.p.fx, G.p.fy, 13, 12, wdmg, true, 1, 0.79, 0.48, 0.45, false, false, 0.9, true);
      sfx_shootPlasma();
      G.shake = 0.18;
      ab_burst(G.p.x, G.p.y, 8, 1, 0.79, 0.48, 3);
      ab_float_text(G.p.x, G.p.y - 0.8, "ONDA DI CHI", 1, 0.79, 0.48);
      break;
    }
    case CLS_PROF: /* Colpo Caricato: parte il conto alla rovescia, poi spara da solo */
      G.p.charge_t = 0.8;
      sfx_shootPlasma();
      ab_float_text(G.p.x, G.p.y - 0.9, "CARICA...", 0.49, 0.98, 1.0);
      ab_burst(G.p.x, G.p.y - 0.2, 6, 0.49, 1.0, 1.0, 1.5);
      ab_add_log("Carica plasma in corso...");
      break;
  }
}

/* ---------------- update ---------------- */
static void summon_minions(AbMonster *m) {
  const AbMonDef *td = ab_mon_def(m->type);
  if (!td || !td->summon || m->summon_left <= 0) return;
  int n = td->summon_n;
  if (n > m->summon_left) n = m->summon_left;
  char ks[2] = {td->summon, 0};
  const AbMonDef *jd = ab_mon_def(td->summon);
  (void)ks;
  if (!jd) return;
  for (int k = 0; k < n; k++) {
    for (int j = 0; j < MAX_MONSTERS; j++) {
      if (G.mons[j].active) continue;
      G.mons[j].active = true;
      G.mons[j].type = td->summon;
      G.mons[j].x = G.mons[j].rx = m->x + (k % 2 ? 1 : -1);
      G.mons[j].y = G.mons[j].ry = m->y + (k / 2 ? 1 : -1);
      G.mons[j].max_hp = G.mons[j].hp = ab_scaled_stat(jd->hp, G.depth, 0.16);
      G.mons[j].dmg = ab_scaled_stat(jd->dmg, G.depth, 0.11);
      G.mons[j].speed = jd->speed; G.mons[j].aggro = 99;
      G.mons[j].is_boss = false; G.mons[j].affix = 0; G.mons[j].atk_cd = 0;
      G.mons[j].fx_mode = 0; G.mons[j].dent_t = 0;
      G.mons[j].atk_anim = 0; G.mons[j].atk_dur = 0.3;
      G.mons[j].phase = ((j * 37) % 100) / 100.0;
      m->summon_left--;
      break;
    }
  }
  ab_add_log("Il boss evoca servitori!");
  ab_burst(m->x, m->y, 12, 1, 0.5, 0.2, 4);
}

void ab_update(double dt, unsigned keys) {
  if (G.state != ST_GAME) return;
  if (dt > 0.05) dt = 0.05;
  G.time += dt;
  G.torch_clk += dt;
  if (G.shake > 0) G.shake -= dt * 2.6;
  if (G.toast_t > 0) G.toast_t -= dt;
  if (G.loot_t > 0) G.loot_t -= dt;
  for (int i = 0; i < MAX_LOGLINES; i++) if (G.log_t[i] > 0) G.log_t[i] -= dt;

  static double respawn_t = 10, power_t = 20;

  int ehp, edp, esp, eap;
  ab_equip_bonus(&ehp, &edp, &esp, &eap);
  const AbClassDef *cc = ab_class_def(G.p.cls);
  double spd = cc->speed * (G.speed5 ? 5 : 1);
  if (G.p.buffs[BUFF_HASTE] > 0) spd *= 1.4;
  spd *= (1 + esp / 100.0);
  if (G.p.web_t > 0) { spd *= 0.5; G.p.web_t -= dt; }
  G.p.speed = spd;

  for (int i = 0; i < BUFF_COUNT; i++)
    if (G.p.buffs[i] > 0) G.p.buffs[i] -= dt;
  /* Colpo Caricato: conto alla rovescia poi spara da solo */
  if (G.p.charge_t > 0 && !G.p.dead && !G.p.downed) {
    G.p.charge_t -= dt;
    if (G.p.charge_t <= 0) {
      G.p.charge_t = 0;
      const AbClassDef *cc2 = ab_class_def(CLS_PROF);
      bool ccr = false;
      double rage2 = G.p.buffs[BUFF_RAGE] > 0 ? 1.4 : 1.0;
      int cdm = (int)(roll_amount(CLS_PROF, &ccr) * 2.0 * rage2 + 0.5);
      spawn_proj(G.p.x, G.p.y, G.p.fx, G.p.fy, cc2->proj_speed * 1.35, cc2->range,
        cdm, true, 0.74, 0.95, 1.0, 0.5, false, false, 2.4, false);
      sfx_shootPlasma();
      G.shake = 0.4;
      ab_burst(G.p.x + G.p.fx * 0.6, G.p.y + G.p.fy * 0.6, 14, 0.49, 1.0, 1.0, 3);
      ab_burst(G.p.x, G.p.y, 10, 0.5, 1, 1, 4);
    }
  }
  if (G.p.poison_t > 0) {
    G.p.poison_t -= dt;
    G.p.poison_acc += dt;
    if (G.p.poison_acc > 1) {
      G.p.poison_acc -= 1;
      G.p.hp -= 1;
      if (G.p.hp < 0) G.p.hp = 0;
      if (G.p.hp <= 0 && !G.p.dead && !G.p.downed) {
        G.p.downed = true; G.p.downed_t = DOWNED_BLEEDOUT; G.p.hp = 0;
        ab_toast("A TERRA!");
      }
    }
  }
  if (cc->max_mp > 0 && G.p.mp < G.p.max_mp) {
    G.p.mp += dt * 0.8;
    if (G.p.mp > G.p.max_mp) G.p.mp = G.p.max_mp;
  }

  if (!G.p.dead && !G.p.downed && !G.merchant_open) {
    double dx = 0, dy = 0;
    if (keys & K_UP) dy -= 1;
    if (keys & K_DOWN) dy += 1;
    if (keys & K_LEFT) dx -= 1;
    if (keys & K_RIGHT) dx += 1;
    if (dx != 0 || dy != 0) {
      double l = sqrt(dx*dx + dy*dy);
      dx /= l; dy /= l;
      G.p.fx = dx; G.p.fy = dy;
      try_move(&G.p.x, &G.p.y, dx * spd * dt, dy * spd * dt);
      if (((int)(G.time * 8)) != ((int)((G.time - dt) * 8)))
        ab_burst(G.p.x, G.p.y + 0.3, 1, 0.4, 0.35, 0.3, 0.8);
      {
        static double step_acc = 0;
        step_acc += dt;
        if (step_acc > 0.26) { step_acc = 0; sfx_step(); }
      }
    }
    if (keys & K_ATK) {
      ab_player_attack();
    }
  }
  if (G.p.atk_cd > 0) G.p.atk_cd -= dt;
  if (G.p.ability_cd > 0) G.p.ability_cd -= dt;
  if (G.p.iframes > 0) G.p.iframes -= dt;
  if (G.p.anim_t > 0) G.p.anim_t -= dt;
  if (G.p.downed && !G.p.dead) {
    G.p.downed_t -= dt;
    if (G.p.downed_t <= 0) {
      G.p.downed = false;
      G.p.dead = true;
      G.state = ST_DEAD;
      sfx_death();
      G.p.gold = 0; G.p.potions = 0; G.p.mana_potions = 0;
      memset(G.p.equip, 0, sizeof G.p.equip);
      if (G.depth > G.best_depth) G.best_depth = G.depth;
      ab_save_record();
      ab_add_log("L'abisso ti ha reclamato.");
    }
  }

  G.p.rx += (G.p.x - G.p.rx) * fmin(1, dt * 12);
  G.p.ry += (G.p.y - G.p.ry) * fmin(1, dt * 12);

  static int last_fov_tx = -999, last_fov_ty = -999;
  int ftx = (int)floor(G.p.x), fty = (int)floor(G.p.y);
  if (ftx != last_fov_tx || fty != last_fov_ty) {
    last_fov_tx = ftx; last_fov_ty = fty;
    ab_update_fov();
  }

  /* trigger boss: solo host/solo (il joiner lo riceve) */
  if (G.map.has_arena && !G.boss_active && !G.boss_dead &&
      (net_role != NET_JOIN || !net_connected)) {
    AbMonster *boss = NULL;
    for (int i = 0; i < MAX_MONSTERS; i++)
      if (G.mons[i].active && G.mons[i].is_boss) { boss = &G.mons[i]; break; }
    if (boss) {
      double dx = G.p.x - (G.map.arena_cx + 0.5), dy = G.p.y - (G.map.arena_cy + 0.5);
      if (fabs(dx) < G.map.arena_w / 2.0 && fabs(dy) < G.map.arena_h / 2.0) {
        G.boss_active = true;
        G.map.gates_closed = true;
        sfx_bossRoar();
        char b[96];
        snprintf(b, sizeof b, "%s si risveglia!", G.boss_name);
        ab_toast(b);
        ab_add_log(b);
        G.shake = 0.5;
      }
    }
  }
  G.boss_hp = 0; G.boss_max = 1;
  for (int i = 0; i < MAX_MONSTERS; i++) {
    if (G.mons[i].active && G.mons[i].is_boss) {
      G.boss_hp = G.mons[i].hp; G.boss_max = G.mons[i].max_hp;
      break;
    }
  }

  /* mostri: il joiner li riceve dall'host, non li simula */
  bool joined_sim = (net_role == NET_JOIN && net_connected);
  for (int i = 0; i < MAX_MONSTERS; i++) {
    AbMonster *m = &G.mons[i];
    if (!m->active) continue;
    if (joined_sim) {
      m->rx += (m->x - m->rx) * fmin(1, dt * 8);
      m->ry += (m->y - m->ry) * fmin(1, dt * 8);
      continue;
    }
    if (m->hp <= 0) { kill_monster(i); continue; }
    const AbMonDef *td = ab_mon_def(m->type);
    if (!td) { m->active = false; continue; }
    if (m->is_boss && G.map.has_arena && !G.boss_active && !G.boss_dead) {
      /* drago dormiente: resta nella tana */
      double bx0 = G.map.arena_cx - G.map.arena_w / 2.0 + 0.45;
      double bx1 = G.map.arena_cx + G.map.arena_w / 2.0 - 0.45;
      double by0 = G.map.arena_cy - G.map.arena_h / 2.0 + 0.45;
      double by1 = G.map.arena_cy + G.map.arena_h / 2.0 - 0.45;
      if (m->x < bx0) m->x = bx0;
      if (m->x > bx1) m->x = bx1;
      if (m->y < by0) m->y = by0;
      if (m->y > by1) m->y = by1;
      m->rx = m->x; m->ry = m->y;
      continue;
    }
    if (m->atk_cd > 0) m->atk_cd -= dt;
    if (m->atk_anim > 0) m->atk_anim -= dt;
    if (m->affix == 3) {
      m->regen_acc += m->max_hp * 0.045 * dt;
      if (m->regen_acc >= 1) { int h = (int)m->regen_acc; m->regen_acc -= h; m->hp += h; if (m->hp > m->max_hp) m->hp = m->max_hp; }
    }
    if (m->dot_t > 0) {
      m->dot_t -= dt; m->dot_acc += dt;
      if (m->dot_acc >= 0.5) { m->dot_acc = 0; m->hp -= 1; if (m->hp <= 0) { kill_monster(i); continue; } }
    }

    if (m->is_boss) {
      /* ---- boss AI per varianti ---- */
      if (!G.boss_active || G.p.dead) {
        m->rx += (m->x - m->rx) * fmin(1, dt * 10);
        m->ry += (m->y - m->ry) * fmin(1, dt * 10);
        continue;
      }
      double pdx = G.p.x - m->x, pdy = G.p.y - m->y;
      double pdd = sqrt(pdx*pdx + pdy*pdy);
      bool tgt_peer = false;
      if (peer_targetable()) {
        double qx = net_peer.x - m->x, qy = net_peer.y - m->y;
        if (qx*qx + qy*qy < pdx*pdx + pdy*pdy) {
          tgt_peer = true; pdx = qx; pdy = qy;
          pdd = sqrt(pdx*pdx + pdy*pdy);
        }
      }
      if (pdd > 0.05) { m->facing_x = pdx / pdd; m->facing_y = pdy / pdd; }
      /* volo drago */
      if (m->type == 'D') {
        if (m->fly_cd > 0) m->fly_cd -= dt;
        if (m->fly_t > 0) {
          m->fly_t -= dt;
          if (m->fly_t <= 0) m->flying = false;
        } else if (m->fly_cd <= 0) {
          m->flying = true; m->fly_t = 5.0;
          m->fly_cd = 8.5 + ((int)(G.time * 10 + i) % 25) / 10.0;
          ab_add_log("Il Drago spicca il volo!");
        }
      }
      double mspd = m->speed * (m->flying ? 1.5 : 1.0);
      if (pdd > 1.0) {
        double vx = pdx / (pdd + 0.001), vy = pdy / (pdd + 0.001);
        try_move(&m->x, &m->y, vx * mspd * dt, vy * mspd * dt);
      }
      /* picchiata drago */
      if (m->type == 'D') {
        if (m->dive_t > 0) {
          m->dive_t -= dt;
          if (m->dive_t <= 0) {
            double hx = tgt_peer ? net_peer.x : G.p.x;
            double hy = tgt_peer ? net_peer.y : G.p.y;
            if (ab_dist(hx, hy, m->dive_x, m->dive_y) < 2.0) damage_hero(tgt_peer, 17, false, false);
            ab_burst(m->dive_x, m->dive_y, 20, 1, 0.5, 0.2, 5);
            G.shake = 0.5;
            m->spec_cd = 2.0;
          }
        } else if (m->spec_cd <= 0) {
          m->dive_x = tgt_peer ? net_peer.x : G.p.x;
          m->dive_y = tgt_peer ? net_peer.y : G.p.y;
          m->dive_t = 1.3;
          m->spec_cd = 6.0;
          ab_add_log("Il Drago punta la preda!");
        }
        if (m->spec_cd > 0) m->spec_cd -= dt;
      }
      /* soffio (D base, K ridotto) */
      if ((m->type == 'D' || m->type == 'K')) {
        if (m->breath_cd > 0) m->breath_cd -= dt;
        double bdist = m->type == 'D' ? 3.2 : 2.6;
        if (m->breath_cd <= 0 && pdd < bdist + 0.5) {
          m->breath_cd = m->type == 'D' ? 4.2 : 5.2;
          double ang = atan2(pdy, pdx);
          for (int k = -2; k <= 2; k++) {
            double a = ang + k * 0.22;
            spawn_proj(m->x, m->y, cos(a), sin(a), 7, bdist, 10, false, 1, 0.5, 0.1, 0.5, true, false, 4.0, false);
          }
          G.shake = 0.4;
          ab_add_log(m->type == 'D' ? "Soffio di fuoco!" : "Soffio pestilenziale!");
        }
      }
      /* palle di fuoco / sputi / tele */
      if (m->fb_cd > 0) m->fb_cd -= dt;
      if (m->fb_cd <= 0 && pdd < 9 && pdd > 1.2) {
        double fspd = 3.6, fr = 1.4;
        int fdmg = 13;
        double fcr = 1, fcg = 0.5, fcb = 0.2;
        bool fpois = false, fweb = false;
        if (m->type == 'X') { fspd = 2.8; fr = 1.7; fdmg = 14; fcr = 0.79; fcg = 0.70; fcb = 0.54; }
        else if (m->type == 'L') { fspd = 4.2; fr = 1.7; fdmg = 15; fcr = 0.54; fcg = 0.42; fcb = 1.0; }
        else if (m->type == 'M') { fspd = 2.6; fr = 1.6; fdmg = 10; fcr = 0.49; fcg = 1.0; fcb = 0.60; }
        else if (m->type == 'R') { fspd = 3.2; fr = 2.0; fdmg = 11; fcr = 0.88; fcg = 0.85; fcb = 0.80; fpois = true; fweb = true; }
        else if (m->type == 'K') { fspd = 3.0; fr = 1.5; fdmg = 9; fcr = 0.79; fcg = 0.66; fcb = 0.39; }
        m->fb_cd = 6.4;
        m->atk_anim = 0.4;
        spawn_proj(m->x, m->y, pdx, pdy, fspd, 10, fdmg / 2 + 4, false, fcr, fcg, fcb, fr, fpois, fweb, 4.0, false);
        ab_burst(m->x, m->y, 8, fcr, fcg, fcb, 3);
      }
      /* evocazioni */
      if (m->summon_left > 0) {
        if (m->spec_cd <= 0 || m->type != 'D') {
          if (m->type != 'D') { summon_minions(m); m->spec_cd = 7.0; }
        }
      }
      /* pestone golem */
      if (m->type == 'X' && pdd < 2.2 && m->atk_cd <= 0) {
        m->atk_cd = 1.5;
        damage_hero(tgt_peer, m->dmg, false, false);
        ab_burst(tgt_peer ? net_peer.x : G.p.x, tgt_peer ? net_peer.y : G.p.y, 12, 0.8, 0.7, 0.5, 4);
        G.shake = 0.45;
        ab_add_log("Pestone del Golem!");
      }
      /* carica Re dei ratti */
      if (m->type == 'K' && pdd > 3 && pdd < 8 && m->atk_cd <= 0) {
        m->atk_cd = 4.0;
        double vx = pdx / (pdd + 0.001), vy = pdy / (pdd + 0.001);
        m->x += vx * 3; m->y += vy * 3;
        G.shake = 0.4;
        double hx = tgt_peer ? net_peer.x : G.p.x;
        double hy = tgt_peer ? net_peer.y : G.p.y;
        if (ab_dist(hx, hy, m->x, m->y) < 1.4) damage_hero(tgt_peer, m->dmg, false, false);
      }
      /* contatto */
      if (pdd < 0.9 && m->atk_cd <= 0) {
        m->atk_cd = 1.2;
        m->atk_anim = 0.4;
        damage_hero(tgt_peer, m->dmg, td->poison, false);
      }
      m->rx += (m->x - m->rx) * fmin(1, dt * 10);
      m->ry += (m->y - m->ry) * fmin(1, dt * 10);
      continue;
    }

    /* ---- IA comuni 1:1 ---- */
    double ddx = G.p.x - m->x, ddy = G.p.y - m->y;
    double dd = sqrt(ddx * ddx + ddy * ddy);
    bool tgt_peer = false;
    if (peer_targetable()) {
      double qx = net_peer.x - m->x, qy = net_peer.y - m->y;
      if (qx*qx + qy*qy < ddx*ddx + ddy*ddy) {
        tgt_peer = true; ddx = qx; ddy = qy;
        dd = sqrt(ddx * ddx + ddy * ddy);
      }
    }
    double hx = tgt_peer ? net_peer.x : G.p.x;
    double hy = tgt_peer ? net_peer.y : G.p.y;
    bool hdown = tgt_peer ? net_peer.downed : G.p.downed;
    bool hdead = tgt_peer ? net_peer.dead : G.p.dead;
    bool has_target = !hdead && !hdown && dd < m->aggro;
    /* stati speciali in corso */
    if (m->fx_mode == 2 || m->fx_mode == 3) { /* dash / swoop in volo */
      m->fx_t += dt;
      double vx = m->fx_tx - m->x, vy = m->fx_ty - m->y;
      double l = sqrt(vx*vx + vy*vy);
      if (l > 0.001) try_move(&m->x, &m->y, vx / l * m->fx_speed * dt, vy / l * m->fx_speed * dt);
      double lim = m->fx_mode == 2 ? 0.55 : 0.62;
      if (l < lim && !m->fx_hit) {
        m->fx_hit = true;
        damage_hero(tgt_peer, m->dmg, td->poison || m->fx_mode == 2, false);
      }
      if (m->fx_t >= m->fx_dur) { m->fx_mode = 0; m->atk_cd = 0.25; }
      m->rx += (m->x - m->rx) * fmin(1, dt * 10);
      m->ry += (m->y - m->ry) * fmin(1, dt * 10);
      continue;
    }
    if (m->fx_mode == 4) { /* bolt: il corpo vola */
      m->fx_t += dt;
      m->x += m->wx * dt; m->y += m->wy * dt;
      if (m->fx_t >= m->fx_dur) { m->fx_mode = 0; ab_burst(m->x, m->y, 8, 0.7, 0.5, 1, 3); }
      else if (ab_dist(hx, hy, m->x, m->y) <= 0.45) {
        damage_hero(tgt_peer, m->dmg, td->poison, false);
        ab_burst(hx, hy, 12, 0.7, 0.5, 1, 3);
        m->fx_mode = 0;
      }
      m->rx = m->x; m->ry = m->y;
      continue;
    }
    if (m->dent_t > 0) {
      m->dent_t -= dt;
      if (m->dent_t <= 0 && ab_dist(m->x, m->y, hx, hy) <= 1.5) {
        damage_hero(tgt_peer, m->dmg, false, false);
        m->atk_cd = 0.25;
      }
    }
    if (m->fx_mode == 1) { /* carica attacco dedicato: fermo */
      m->fx_t -= dt;
      if (m->fx_t <= 0) {
        m->fx_mode = 0;
        m->atk_cd = 0.3;
        m->atk_anim = 0.3;
        if (m->wind_kind == 1) { m->fx_mode = 2; m->fx_t = 0; m->fx_dur = 0.24; m->fx_speed = 7.2; m->fx_hit = false; m->atk_anim = 0.25; }
        else if (m->wind_kind == 2) { m->fx_mode = 3; m->fx_t = 0; m->fx_dur = 0.30; m->fx_speed = 6.4; m->fx_hit = false; m->atk_anim = 0.25; }
        else if (m->wind_kind == 3) {
          for (int k = 0; k < 1; k++) {
            if (ab_dist(m->x, m->y, hx, hy) <= 1.8) damage_hero(tgt_peer, m->dmg, false, false);
          }
          ab_burst(m->x, m->y, 14, 0.6, 0.6, 0.6, 4);
          G.shake = 0.35;
        } else if (m->wind_kind == 4) {
          AbRng rr; ab_rng_seed(&rr, (uint32_t)(G.time * 91 + i));
          double vx2 = hx - m->x, vy2 = hy - m->y;
          double dd2 = sqrt(vx2*vx2 + vy2*vy2);
          if (dd2 <= 1.6 && dd2 > 0.01) {
            double dot = (vx2 / dd2) * m->facing_x + (vy2 / dd2) * m->facing_y;
            if (dot >= 0.55) damage_hero(tgt_peer, m->dmg + ab_rng_range(&rr, 0, 1, false), false);
          }
          ab_burst(m->x + m->facing_x, m->y + m->facing_y, 8, 0.9, 0.9, 0.9, 3);
        }
      }
      m->rx += (m->x - m->rx) * fmin(1, dt * 10);
      m->ry += (m->y - m->ry) * fmin(1, dt * 10);
      continue;
    }

    if (has_target) {
      double d = dd > 0.001 ? dd : 0.001;
      double dx = ddx / d, dy = ddy / d;
      m->facing_x = dx; m->facing_y = dy;
      double reach = td->ranged_range > 0 ? td->ranged_range : 0.85;
      if (dd > reach) {
        double vx = dx, vy = dy;
        if (td->erratic) {
          AbRng rr; ab_rng_seed(&rr, (uint32_t)(G.time * 57 + i * 13));
          if (ab_rng_next(&rr) < 0.03 * 60 * dt + 0.001) {
            vx += ab_rng_frange(&rr, -0.7, 0.7);
            vy += ab_rng_frange(&rr, -0.7, 0.7);
          }
        }
        double mv = m->speed;
        if (!in_safe((int)floor(m->x + vx * mv * dt), (int)floor(m->y + vy * mv * dt)) ||
            in_safe((int)floor(m->x), (int)floor(m->y)))
          try_move(&m->x, &m->y, vx * mv * dt, vy * mv * dt);
      } else if (m->atk_cd <= 0 && !hdead && !hdown) {
        AbRng rr; ab_rng_seed(&rr, (uint32_t)(G.time * 131 + i * 17));
        if (td->dash) {
          m->fx_mode = 1; m->wind_kind = 1; m->fx_t = 0.35;
          m->fx_tx = hx; m->fx_ty = hy;
          m->atk_cd = 2.0 + ab_rng_next(&rr) * 0.8;
          ab_burst(m->x, m->y, 6, 0.5, 0.7, 0.4, 2);
        } else if (td->swoop) {
          m->fx_mode = 1; m->wind_kind = 2; m->fx_t = 0.35;
          m->fx_tx = hx; m->fx_ty = hy;
          m->atk_cd = 2.2 + ab_rng_next(&rr) * 0.8;
        } else if (td->ranged_range > 0) {
          double sp = td->beam ? 3.8 : 3.2;
          double l = dd > 0.01 ? dd : 1;
          m->fx_mode = 4; m->fx_t = 0;
          double life = l / sp + 0.35;
          if (life > 1.9) life = 1.9;
          m->fx_dur = life;
          m->wx = ddx / l * sp; m->wy = ddy / l * sp;
          m->atk_cd = 2.1 + ab_rng_next(&rr) * 0.6;
          ab_burst(m->x, m->y, 6, td->beam ? 0.5 : 0.7, td->beam ? 1.0 : 0.5, td->beam ? 0.5 : 0.8, 2);
        } else if (td->dbl) {
          m->atk_cd = 0.8 + ab_rng_next(&rr) * 0.2;
          m->atk_anim = 0.28;
          m->dent_t = 0.22;
          damage_hero(tgt_peer, m->dmg, false, false);
        } else if (td->stomp) {
          m->fx_mode = 1; m->wind_kind = 3; m->fx_t = 0.55;
          m->atk_cd = 1.3 + ab_rng_next(&rr) * 0.4;
        } else if (td->cone) {
          m->fx_mode = 1; m->wind_kind = 4; m->fx_t = 0.45;
          m->atk_cd = 1.15 + ab_rng_next(&rr) * 0.3;
        } else {
          m->atk_cd = 1.05 + ab_rng_next(&rr) * 0.3;
          m->atk_anim = 0.28;
          int extra = ab_rng_range(&rr, 0, 1);
          damage_hero(tgt_peer, m->dmg + extra, td->poison, false);
          if (td->lifesteal) { m->hp += 2; if (m->hp > m->max_hp) m->hp = m->max_hp; }
        }
      }
    } else {
      m->wander_t -= dt;
      if (m->wander_t <= 0) {
        AbRng rr; ab_rng_seed(&rr, (uint32_t)(G.time * 61) + (uint32_t)i * 131 + 5);
        double a = ab_rng_next(&rr) * 6.28318;
        m->wx = m->x + cos(a) * 3; m->wy = m->y + sin(a) * 3;
        m->wander_t = 2 + ab_rng_next(&rr) * 3;
      }
      double dx = m->wx - m->x, dy = m->wy - m->y;
      double d = sqrt(dx*dx + dy*dy);
      if (d > 0.35) {
        if (!in_safe((int)floor(m->x + dx / d * m->speed * 0.55 * dt), (int)floor(m->y + dy / d * m->speed * 0.55 * dt)) ||
            in_safe((int)floor(m->x), (int)floor(m->y)))
          try_move(&m->x, &m->y, dx / d * m->speed * 0.55 * dt, dy / d * m->speed * 0.55 * dt);
        m->facing_x = dx / d; m->facing_y = dy / d;
      }
    }
    m->rx += (m->x - m->rx) * fmin(1, dt * 10);
    m->ry += (m->y - m->ry) * fmin(1, dt * 10);
  }

  /* proiettili */
  for (int i = 0; i < MAX_PROJS; i++) {
    AbProj *p = &G.projs[i];
    if (!p->active) continue;
    p->life -= dt;
    double step = sqrt(p->vx * p->vx + p->vy * p->vy) * dt;
    p->range_left -= step;
    if (p->life <= 0 || p->range_left <= 0) { p->active = false; continue; }
    double nx = p->x + p->vx * dt, ny = p->y + p->vy * dt;
    if (!ab_is_walkable((int)nx, (int)ny)) {
      ab_burst(p->x, p->y, 4, p->r, p->g, p->b, 2);
      p->active = false; continue;
    }
    p->x = nx; p->y = ny;
    if (p->friendly) {
      bool joined = (net_role == NET_JOIN && net_connected);
      for (int j = 0; j < MAX_MONSTERS; j++) {
        if (!G.mons[j].active) continue;
        if (G.mons[j].hit_mark == p->id_mark) continue;
        if (ab_dist(p->x, p->y, G.mons[j].x, G.mons[j].y) < 0.45) {
          if (joined) net_send_hit(j, p->dmg);
          else G.mons[j].hp -= p->dmg;
          sfx_hit();
          char b[16]; snprintf(b, sizeof b, "%d", p->dmg);
          ab_float_text(G.mons[j].x, G.mons[j].y - 0.5, b, 1, 0.85, 0.4);
          ab_burst(p->x, p->y, 4, p->r, p->g, p->b, 2.5);
          if (p->pierce) G.mons[j].hit_mark = p->id_mark;
          else { p->active = false; }
          break;
        }
      }
    } else {
      bool hit_p = !G.p.dead && !G.p.downed && ab_dist(p->x, p->y, G.p.x, G.p.y) < p->hit_r;
      bool hit_q = peer_targetable() && !net_peer.downed &&
                   ab_dist(p->x, p->y, net_peer.x, net_peer.y) < p->hit_r;
      if (hit_p || hit_q) {
        bool to_peer = hit_q && (!hit_p ||
          ab_dist(p->x, p->y, net_peer.x, net_peer.y) < ab_dist(p->x, p->y, G.p.x, G.p.y));
        if (to_peer) net_send_hurt_to_peer(p->dmg, p->poison, p->web);
        else {
          hurt_player(p->dmg, p->poison);
          if (p->web) { G.p.web_t = 3.0; ab_add_log("Intrappolato nella tela!"); }
        }
        p->active = false;
      }
    }
  }

  /* oggetti a terra */
  if (!G.p.dead && !G.p.downed) {
    bool joined = (net_role == NET_JOIN && net_connected);
    static double take_cd = 0;
    if (take_cd > 0) take_cd -= dt;
    for (int i = 0; i < MAX_ITEMS; i++) {
      AbItem *it = &G.items[i];
      if (!it->active) continue;
      double dx = G.p.x - it->x, dy = G.p.y - it->y;
      if (dx * dx + dy * dy > 0.62 * 0.62) continue;
      if (joined) {
        if (take_cd <= 0) { take_cd = 0.3; net_send_take(i); }
        continue;
      }
      it->active = false;
      if (it->kind == 0) {
        G.p.gold += it->amount;
        sfx_pickup();
        char b[32]; snprintf(b, sizeof b, "+%d", it->amount);
        ab_float_text(G.p.x, G.p.y - 0.7, b, 0.83, 0.69, 0.22);
      } else if (it->kind == 1) {
        G.p.gold += it->amount;
        sfx_gem();
        char b[48]; snprintf(b, sizeof b, "+%d gemma", it->amount);
        ab_float_text(G.p.x, G.p.y - 0.7, b, 0.56, 0.82, 1.0);
        ab_add_log("Gemma preziosa: oro!");
      } else if (it->kind == 2) {
        G.p.buffs[it->buff] = BUFF_DURATION[it->buff];
        sfx_power();
        char b[48]; snprintf(b, sizeof b, "%s!", BUFF_NAMES[it->buff]);
        ab_float_text(G.p.x, G.p.y - 0.7, b, 0.6, 1, 0.6);
        ab_add_log("Potenziamento raccolto.");
      } else if (it->kind == 3) {
        G.p.potions += it->amount;
        sfx_potion();
        ab_float_text(G.p.x, G.p.y - 0.7, "+pozione", 0.76, 0.27, 0.23);
      } else if (it->kind == 4) {
        G.p.mana_potions += it->amount;
        sfx_mana();
        ab_float_text(G.p.x, G.p.y - 0.7, "+mana", 0.37, 0.63, 0.79);
      } else if (it->kind == 5) {
        sfx_gem();
        equip_or_keep(it->slot, it->rarity, it->st_hp, it->st_dmg, it->st_spd, it->st_arm);
      }
    }
  }

  /* respawn come hostRespawnTick (solo host/solo) */
  respawn_t -= dt;
  if (net_role != NET_JOIN || !net_connected) {
    int cap = 9 + G.depth * 2;
    if (cap > 30) cap = 30;
    int alive = 0;
    for (int i = 0; i < MAX_MONSTERS; i++) if (G.mons[i].active) alive++;
    if (respawn_t <= 0) {
      AbRng rr; ab_rng_seed(&rr, (uint32_t)(G.time * 77 + 1));
      respawn_t = 13 + ab_rng_next(&rr) * 11;
      if (alive < cap && mon_spot_n > 0) {
        int si = ab_rng_range(&rr, 0, mon_spot_n - 1);
        static const char pool[] = {'r','b','g','j','J','s','o','z','S','W','k','h','C','c','m','q','G'};
        int tot = 0;
        for (size_t k = 0; k < sizeof pool; k++) tot += ab_mon_weight(pool[k], G.depth);
        if (tot > 0) {
          int roll = 1 + (int)(ab_rng_next(&rr) * tot);
          char tk = 'g';
          for (size_t k = 0; k < sizeof pool; k++) {
            roll -= ab_mon_weight(pool[k], G.depth);
            if (roll <= 0) { tk = pool[k]; break; }
          }
          const AbMonDef *td = ab_mon_def(tk);
          for (int j = 0; j < MAX_MONSTERS; j++) {
            if (G.mons[j].active) continue;
            G.mons[j].active = true; G.mons[j].type = tk;
            G.mons[j].x = G.mons[j].rx = mon_spots[si][0] + 0.5;
            G.mons[j].y = G.mons[j].ry = mon_spots[si][1] + 0.5;
            G.mons[j].max_hp = G.mons[j].hp = ab_scaled_stat(td->hp, G.depth, 0.16);
            G.mons[j].dmg = ab_scaled_stat(td->dmg, G.depth, 0.11);
            G.mons[j].speed = td->speed; G.mons[j].aggro = td->aggro;
            G.mons[j].is_boss = false; G.mons[j].affix = 0; G.mons[j].atk_cd = 0;
            G.mons[j].fx_mode = 0; G.mons[j].dent_t = 0;
            G.mons[j].atk_anim = 0; G.mons[j].atk_dur = 0.3;
            G.mons[j].phase = ((j * 37) % 100) / 100.0;
            break;
          }
        }
      }
    }
  }
  }
  power_t -= dt;
  if (power_t <= 0 && (net_role != NET_JOIN || !net_connected)) {
    AbRng rr; ab_rng_seed(&rr, (uint32_t)(G.time * 55 + 2));
    power_t = 30 + ab_rng_next(&rr) * 20;
    bool has = false;
    for (int i = 0; i < MAX_ITEMS; i++)
      if (G.items[i].active && G.items[i].kind == 2) { has = true; break; }
    if (!has && pow_spot_n > 0) {
      for (int i = 0; i < MAX_ITEMS; i++) {
        if (G.items[i].active) continue;
        int si = ab_rng_range(&rr, 0, pow_spot_n - 1);
        G.items[i].active = true;
        G.items[i].x = pow_spots[si][0] + 0.5;
        G.items[i].y = pow_spots[si][1] + 0.5;
        G.items[i].kind = 2;
        G.items[i].buff = ab_rng_range(&rr, 0, BUFF_COUNT - 1);
        break;
      }
    }
  }

  /* particelle/float */
  for (int i = 0; i < MAX_PARTS; i++) {
    if (!G.parts[i].active) continue;
    G.parts[i].life -= dt;
    if (G.parts[i].life <= 0) { G.parts[i].active = false; continue; }
    G.parts[i].x += G.parts[i].vx * dt;
    G.parts[i].y += G.parts[i].vy * dt;
    G.parts[i].vx *= (1 - dt * 2); G.parts[i].vy *= (1 - dt * 2);
  }
  for (int i = 0; i < MAX_FLOATS; i++) {
    if (!G.floats[i].active) continue;
    G.floats[i].life -= dt;
    G.floats[i].y -= dt * 0.8;
    if (G.floats[i].life <= 0) G.floats[i].active = false;
  }
}

/* ---------------- multiplayer LAN ---------------- */
static bool peer_targetable(void) {
  return net_role == NET_HOST && net_connected && net_peer.active && !net_peer.dead;
}

/* ferisce l'eroe giusto: compagno (via rete) o locale */
static void damage_hero(bool peer, int dmg, bool poison, bool web) {
  if (peer) net_send_hurt_to_peer(dmg, poison, web);
  else hurt_player(dmg, poison);
}

void net_apply_hurt(int dmg, bool poison, bool web) {
  hurt_player(dmg, poison);
  if (web && !G.p.dead) { G.p.web_t = 3.0; ab_add_log("Intrappolato nella tela!"); }
}

void net_apply_giv(int kind, int amount, int buff, int rar, int slot,
                   int s0, int s1, int s2, int s3) {
  if (kind == 0 || kind == 1) {
    G.p.gold += amount;
    char b[32]; snprintf(b, sizeof b, "+%d", amount);
    ab_float_text(G.p.x, G.p.y - 0.7, b, 1, 0.85, 0.3);
    sfx_pickup();
  } else if (kind == 2 && buff >= 0 && buff < BUFF_COUNT) {
    G.p.buffs[buff] = BUFF_DURATION[buff];
    sfx_power();
    ab_float_text(G.p.x, G.p.y - 0.7, (char *)BUFF_NAMES[buff], 0.6, 1, 0.6);
  } else if (kind == 3) {
    G.p.potions += amount;
    sfx_potion();
  } else if (kind == 4) {
    G.p.mana_potions += amount;
    sfx_mana();
  } else if (kind == 5 && slot >= 0 && slot < 5) {
    sfx_gem();
    equip_or_keep(slot, rar, s0, s1, s2, s3);
  }
}

void net_apply_depth(int depth) {
  if (depth < 1) depth = 1;
  if (depth == G.depth) return;
  ab_gen_depth(depth);
  char b[64];
  snprintf(b, sizeof b, "Piano %d (host)", depth);
  ab_toast(b);
}

void net_apply_revive(void) {
  if (!G.p.downed || G.p.dead) return;
  G.p.downed = false;
  G.p.downed_t = 0;
  G.p.hp = G.p.max_hp / 2;
  if (G.p.hp < 1) G.p.hp = 1;
  sfx_revive();
  ab_toast("Rianimato dal compagno!");
  ab_add_log("Il compagno ti ha rianimato.");
}

void net_host_on_hit(int slot, int dmg) {
  if (slot < 0 || slot >= MAX_MONSTERS) return;
  if (!G.mons[slot].active) return;
  G.mons[slot].hp -= dmg;
  char b[16]; snprintf(b, sizeof b, "%d", dmg);
  ab_float_text(G.mons[slot].x, G.mons[slot].y - 0.5, b, 1, 0.85, 0.4);
}

void net_host_on_take(int slot) {
  if (slot < 0 || slot >= MAX_ITEMS) return;
  AbItem *it = &G.items[slot];
  if (!it->active) return;
  it->active = false;
  net_send_giv(it->kind, it->amount, it->buff, it->rarity, it->slot,
               it->st_hp, it->st_dmg, it->st_spd, it->st_arm);
  ab_burst(it->x, it->y, 6, 1, 0.9, 0.5, 2);
}

void net_host_on_open(int idx) {
  if (idx < 0 || idx >= G.chest_count) return;
  AbChest *c = &G.chests[idx];
  if (!c->active || c->open) return;
  if (c->boss_chest && !G.boss_dead) return;
  c->open = true;
  AbRng r; ab_rng_seed(&r, (uint32_t)(G.time * 131 + idx * 17 + G.depth));
  if (c->boss_chest) {
    int gold = 60 + ab_rng_range(&r, 0, 25 * G.depth - 1 > 0 ? 25 * G.depth - 1 : 0);
    net_send_giv(0, gold, 0, 0, 0, 0, 0, 0, 0);
    net_send_giv(3, 1, 0, 0, 0, 0, 0, 0, 0);
    net_send_giv(4, 1, 0, 0, 0, 0, 0, 0, 0);
    int rar = ab_rng_next(&r) < 0.45 ? R_LEGGENDARIO : R_EPICO;
    int s = ab_rng_range(&r, 0, 4);
    AbRng r2; ab_rng_seed(&r2, (uint32_t)(G.time * 977 + 9));
    int rr = rarity_roll(&r2, G.depth);
    (void)rr;
    AbEquip e; make_equip(&r2, G.depth, s, rar, &e);
    net_send_giv(5, 0, 0, e.rarity, s, e.hp, e.dmg_pct, e.spd_pct, e.arm_pct);
  } else {
    int gold = 6 + ab_rng_range(&r, 0, 9 * G.depth - 1 > 0 ? 9 * G.depth - 1 : 0);
    net_send_giv(0, gold, 0, 0, 0, 0, 0, 0, 0);
    if (ab_rng_next(&r) < 0.32) net_send_giv(3, 1, 0, 0, 0, 0, 0, 0, 0);
    if (ab_rng_next(&r) < 0.24) net_send_giv(4, 1, 0, 0, 0, 0, 0, 0, 0);
    if (ab_rng_next(&r) < 0.2) {
      int s = ab_rng_range(&r, 0, 4);
      int rar = rarity_roll(&r, G.depth);
      AbEquip e; make_equip(&r, G.depth, s, rar, &e);
      net_send_giv(5, 0, 0, e.rarity, s, e.hp, e.dmg_pct, e.spd_pct, e.arm_pct);
    }
  }
  ab_burst(c->tx + 0.5, c->ty + 0.5, 12, 1, 0.85, 0.3, 3.5);
  sfx_chest();
}

void net_host_on_stairs(void) {
  ab_descend();
  net_send_depth(G.depth);
}

void net_host_on_revive(void) {
  if (!G.p.downed || G.p.dead) return;
  G.p.downed = false;
  G.p.downed_t = 0;
  G.p.hp = G.p.max_hp / 2;
  if (G.p.hp < 1) G.p.hp = 1;
  sfx_revive();
  ab_toast("Rianimato dal compagno!");
}

/* adotta lo snapshot dell'host (joiner): sostituzione integrale */
void net_adopt_snapshot(const unsigned char *pl, int n) {
  int o = 0;
  if (n < 6) return;
  int depth = pl[o++];
  int bact = pl[o++];
  int bdead = pl[o++];
  int bhp = (pl[o] | (pl[o + 1] << 8));
  if (bhp >= 32768) bhp -= 65536;
  o += 2;
  if (depth != G.depth) {
    ab_gen_depth(depth);
  }
  if (o >= n) return;
  int nm = pl[o++];
  for (int i = 0; i < MAX_MONSTERS; i++) G.mons[i].active = false;
  for (int k = 0; k < nm && o + 11 <= n; k++) {
    int on = pl[o++];
    int ty = pl[o++];
    float fx, fy;
    memcpy(&fx, pl + o, 4); o += 4;
    memcpy(&fy, pl + o, 4); o += 4;
    int hp = pl[o] | (pl[o + 1] << 8);
    if (hp >= 32768) hp -= 65536;
    o += 2;
    int dmg = pl[o] | (pl[o + 1] << 8);
    if (dmg >= 32768) dmg -= 65536;
    o += 2;
    if (!on || k >= MAX_MONSTERS) continue;
    const AbMonDef *td = ab_mon_def((char)ty);
    if (!td) continue;
    AbMonster *m = &G.mons[k];
    m->active = true;
    m->type = (char)ty;
    m->x = fx; m->y = fy;
    m->rx += (m->x - m->rx) * 0.5;
    m->ry += (m->y - m->ry) * 0.5;
    if (m->rx == 0 && m->ry == 0) { m->rx = m->x; m->ry = m->y; }
    m->hp = hp; m->dmg = dmg;
    m->max_hp = ab_scaled_stat(td->hp, G.depth, td->boss ? 0.10 : 0.16);
    if (m->max_hp < 1) m->max_hp = 1;
    if (m->hp > m->max_hp) m->hp = m->max_hp;
    m->speed = td->speed; m->aggro = td->aggro;
    m->is_boss = td->boss;
    m->affix = 0;
    if (m->is_boss) {
      G.boss_hp = hp;
      G.boss_max = m->max_hp;
    }
  }
  if (o >= n) return;
  int ni = pl[o++];
  for (int i = 0; i < MAX_ITEMS; i++) G.items[i].active = false;
  G.item_count = 0;
  for (int k = 0; k < ni && o + 16 <= n; k++) {
    int on = pl[o++];
    int kind = pl[o++];
    float fx, fy;
    memcpy(&fx, pl + o, 4); o += 4;
    memcpy(&fy, pl + o, 4); o += 4;
    int amount = pl[o] | (pl[o + 1] << 8);
    if (amount >= 32768) amount -= 65536;
    o += 2;
    int buff = pl[o++], rar = pl[o++], slot = pl[o++];
    int s0 = pl[o] | (pl[o + 1] << 8); o += 2;
    int s1 = pl[o] | (pl[o + 1] << 8); o += 2;
    int s2 = pl[o] | (pl[o + 1] << 8); o += 2;
    int s3 = pl[o] | (pl[o + 1] << 8); o += 2;
    if (!on || k >= MAX_ITEMS) continue;
    AbItem *it = &G.items[k];
    it->active = true;
    it->kind = kind; it->x = fx; it->y = fy;
    it->amount = amount; it->buff = buff; it->rarity = rar; it->slot = slot;
    it->st_hp = s0; it->st_dmg = s1; it->st_spd = s2; it->st_arm = s3;
    G.item_count++;
  }
  if (o + 5 > n) return;
  uint32_t mask = (uint32_t)pl[o] | ((uint32_t)pl[o + 1] << 8) |
                  ((uint32_t)pl[o + 2] << 16) | ((uint32_t)pl[o + 3] << 24);
  o += 4;
  for (int i = 0; i < G.chest_count && i < 24; i++)
    G.chests[i].open = (mask & (1u << i)) ? true : false;
  G.boss_active = bact ? true : false;
  G.boss_dead = bdead ? true : false;
  G.map.gates_closed = (G.boss_active && !G.boss_dead);
  if (!G.boss_dead) {
    bool any = false;
    for (int i = 0; i < MAX_MONSTERS; i++)
      if (G.mons[i].active && G.mons[i].is_boss) any = true;
    if (!any) { G.boss_active = false; G.map.gates_closed = false; }
  }
}
