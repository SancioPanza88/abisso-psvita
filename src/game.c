/* ABISSO Vita - logica di gioco (RNG, dungeon, entita, combattimento).
 * Fedelta al JS originale: mulberry32/FNV-1a identici, scaling mostri
 * identico (scaledStat), corridoi a L, boss ogni 5 piani in ordine D,X,L,M,R,K.
 */
#include "abisso.h"
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
void ab_loot_banner(const char *s, int rarity) {
  if (!s) return;
  strncpy(G.loot, s, 95);
  G.loot[95] = '\0';
  G.loot_t = 2.6;
  G.loot_rarity = rarity;
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

bool ab_is_walkable(int tx, int ty) {
  if (tx < 0 || ty < 0 || tx >= G.map.w || ty >= G.map.h) return false;
  return G.map.tiles[ty][tx] != T_WALL;
}

/* movimento con slide + nudge come nel JS */
static bool try_move(double *px, double *py, double dx, double dy) {
  double nx = *px + dx, ny = *py + dy;
  int tx = (int)floor(nx), ty = (int)floor(ny);
  if (ab_is_walkable(tx, ty)) { *px = nx; *py = ny; return true; }
  /* prova singoli assi */
  bool mx = false, my = false;
  int tx2 = (int)floor(nx), ty0 = (int)floor(*py);
  if (ab_is_walkable(tx2, ty0)) { *px = nx; mx = true; }
  int tx0 = (int)floor(*px), ty2 = (int)floor(ny);
  if (ab_is_walkable(tx0, ty2)) { *py = ny; my = true; }
  if (mx || my) return true;
  /* nudge verso centro cella se diagonale bloccata */
  if (dx != 0 && dy != 0) {
    double cy = floor(*py) + 0.5;
    double diff = cy - *py;
    if (fabs(diff) > 0.02) {
      double nudge = (diff > 0 ? 1 : -1) * fmin(fabs(diff), fabs(dx) * 0.5);
      int txx = (int)floor(*px), tyy = (int)floor(*py + nudge);
      if (ab_is_walkable(txx, tyy)) { *py += nudge; return true; }
    }
  }
  return false;
}

/* ---------------- save/record ---------------- */
static void ab_save_dir(char *out, size_t n) {
#ifdef ABISSO_VITA
  snprintf(out, n, "ux0:data/ABISSO");
#else
  snprintf(out, n, ".");
#endif
}
static void ab_save_file(char *out, size_t n) {
#ifdef ABISSO_VITA
  snprintf(out, n, "ux0:data/ABISSO/save.txt");
#else
  snprintf(out, n, "abisso_save.txt");
#endif
}
void ab_save_record(void) {
  char dir[128], file[160];
  ab_save_dir(dir, sizeof dir);
  ab_save_file(file, sizeof file);
#ifdef ABISSO_VITA
#ifdef _WIN32
  _mkdir(dir);
#else
  mkdir(dir, 0777);
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
  G.world_seed = ab_hash_str(G.room);
  G.depth = 1;
  G.state = ST_GAME;
  G.minimap = false; G.help = false; G.merchant_open = false;
  G.boss_active = false;
  G.arena_locked = false;
  /* reset equip/stats base */
  memset(G.p.equip, 0, sizeof G.p.equip);
  G.p.gold = 0; G.p.potions = 1; G.p.mana_potions = 1;
  G.p.xp = 0; G.p.level = 1;
  G.p.buff_rage_t = G.p.buff_shield_t = G.p.buff_haste_t = 0;
  G.p.dead = false; G.p.downed = false;
  ab_gen_depth(1);
  const AbClassDef *c = ab_class_def(G.p.cls);
  char b[96];
  snprintf(b, sizeof b, "Benvenuto, %s %s! Stanza '%s'", c->name, G.p.name, G.room);
  ab_add_log(b);
  ab_toast(G.c64 ? "MODALITA' C64!" : "L'abisso ti attende...");
  if (G.c64) ab_add_log("Easter egg C64 attivo: sprite e suoni 8-bit.");
  ab_add_log("OFFLINE Vita: single-player. Stesso seed della web.");
}

/* ---------------- dungeon gen ---------------- */
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

void ab_gen_depth(int depth) {
  G.depth = depth;
  AbMap *m = &G.map;
  memset(m, 0, sizeof *m);
  AbRng rng;
  ab_rng_seed(&rng, G.world_seed + (uint32_t)depth * 7919u + 13u);

  m->w = 40 + depth * 2;
  if (m->w > MAP_MAX_W) m->w = MAP_MAX_W;
  m->h = 40 + depth * 2;
  if (m->h > MAP_MAX_H) m->h = MAP_MAX_H;

  Room rooms[14];
  int nr = 0;
  int tries = 0;
  int want = 8 + ab_rng_range(&rng, 0, 3);
  while (nr < want && tries < 200) {
    tries++;
    int rw = ab_rng_range(&rng, 5, 10);
    int rh = ab_rng_range(&rng, 5, 9);
    int rx = ab_rng_range(&rng, 2, m->w - rw - 3);
    int ry = ab_rng_range(&rng, 2, m->h - rh - 3);
    bool ov = false;
    for (int i = 0; i < nr; i++) {
      if (rx < rooms[i].x + rooms[i].w + 1 && rx + rw + 1 > rooms[i].x &&
          ry < rooms[i].y + rooms[i].h + 1 && ry + rh + 1 > rooms[i].y) { ov = true; break; }
    }
    if (ov) continue;
    rooms[nr].x = rx; rooms[nr].y = ry;
    rooms[nr].w = rw; rooms[nr].h = rh;
    rooms[nr].cx = rx + rw / 2; rooms[nr].cy = ry + rh / 2;
    nr++;
    for (int y = ry; y < ry + rh; y++)
      for (int x = rx; x < rx + rw; x++) m->tiles[y][x] = T_FLOOR;
  }
  if (nr < 2) { /* fallback: apri tutto centrale */
    for (int y = 4; y < m->h - 4; y++)
      for (int x = 4; x < m->w - 4; x++) m->tiles[y][x] = T_FLOOR;
    rooms[0].cx = 6; rooms[0].cy = 6;
    rooms[1].cx = m->w - 7; rooms[1].cy = m->h - 7;
    nr = 2;
  }
  for (int i = 1; i < nr; i++) {
    int ax = rooms[i-1].cx, ay = rooms[i-1].cy;
    int bx = rooms[i].cx, by = rooms[i].cy;
    if (ab_rng_next(&rng) < 0.5) { carve_h(m, ax, bx, ay); carve_v(m, ay, by, bx); }
    else { carve_v(m, ay, by, ax); carve_h(m, ax, bx, by); }
  }

  m->spawn_x = rooms[0].cx; m->spawn_y = rooms[0].cy;
  m->stairs_x = rooms[nr-1].cx; m->stairs_y = rooms[nr-1].cy;
  m->tiles[m->stairs_y][m->stairs_x] = T_STAIRS;
  m->merch_x = rooms[0].x + 1; m->merch_y = rooms[0].y + 1;

  /* arena boss in fondo su piani boss */
  m->has_arena = false;
  if (ab_is_boss_floor(depth)) {
    int aw = 16, ah = 11;
    int ax = (m->w - aw) / 2;
    int ay = m->h - ah - 3;
    if (ax < 2) ax = 2;
    if (ay < 2) ay = 2;
    for (int y = ay; y < ay + ah && y < m->h - 1; y++)
      for (int x = ax; x < ax + aw && x < m->w - 1; x++) m->tiles[y][x] = T_FLOOR;
    /* corridoio verso arena */
    carve_v(m, rooms[nr-1].cy, ay + ah / 2, ax + aw / 2);
    m->arena_cx = ax + aw / 2; m->arena_cy = ay + ah / 2;
    m->arena_w = aw; m->arena_h = ah;
    m->has_arena = true;
  }

  /* torce: lungo i muri */
  G.torch_count = 0;
  for (int y = 2; y < m->h - 2 && G.torch_count < MAX_TORCHES; y++) {
    for (int x = 2; x < m->w - 2 && G.torch_count < MAX_TORCHES; x++) {
      if (m->tiles[y][x] == T_FLOOR && ab_rng_next(&rng) < 0.02) {
        /* vicino a un muro? */
        if (m->tiles[y-1][x] == T_WALL || m->tiles[y+1][x] == T_WALL ||
            m->tiles[y][x-1] == T_WALL || m->tiles[y][x+1] == T_WALL) {
          G.torches[G.torch_count].tx = x;
          G.torches[G.torch_count].ty = y;
          G.torch_count++;
        }
      }
    }
  }

  /* forzieri */
  G.chest_count = 0;
  int nch = 3 + ab_rng_range(&rng, 0, 2);
  for (int i = 0; i < nch && G.chest_count < MAX_CHESTS; i++) {
    int ri = ab_rng_range(&rng, 1, nr - 1);
    int tx = rooms[ri].x + ab_rng_range(&rng, 0, rooms[ri].w - 1);
    int ty = rooms[ri].y + ab_rng_range(&rng, 0, rooms[ri].h - 1);
    if (m->tiles[ty][tx] != T_FLOOR) continue;
    if ((tx == m->stairs_x && ty == m->stairs_y)) continue;
    AbChest *c = &G.chests[G.chest_count++];
    c->active = true; c->tx = tx; c->ty = ty; c->open = false;
    double f = ab_rng_next(&rng);
    if (f < 0.45) { c->kind = 0; c->gold = ab_rng_range(&rng, 5 + depth * 2, 15 + depth * 4); }
    else if (f < 0.65) c->kind = 1;
    else if (f < 0.8) c->kind = 2;
    else c->kind = 3;
    double rf = ab_rng_next(&rng);
    c->rarity = rf < 0.6 ? 0 : rf < 0.85 ? 1 : rf < 0.96 ? 2 : 3;
    if (c->kind == 3 && c->rarity == 0 && ab_rng_next(&rng) < 0.5) c->rarity = 1;
  }

  /* reset visibilita */
  for (int y = 0; y < m->h; y++)
    for (int x = 0; x < m->w; x++) m->visited[y][x] = 0;

  /* giocatore */
  const AbClassDef *cd = ab_class_def(G.p.cls);
  int eq_hp = 0; double eq_spd = 0;
  for (int i = 0; i < 5; i++) { eq_hp += G.p.equip[i].hp; eq_spd += G.p.equip[i].speed; }
  /* a cambio piano tieni hp% */
  double keep = 1.0;
  if (G.p.max_hp > 0 && G.p.hp > 0) keep = (double)G.p.hp / (double)G.p.max_hp;
  if (depth == 1) keep = 1.0;
  G.p.max_hp = cd->hp + eq_hp;
  G.p.hp = (int)(G.p.max_hp * keep + 0.5);
  if (G.p.hp < 1) G.p.hp = 1;
  G.p.max_mp = cd->max_mp; G.p.mp = cd->max_mp;
  G.p.speed = cd->speed + eq_spd;
  G.p.x = G.p.rx = m->spawn_x + 0.5;
  G.p.y = G.p.ry = m->spawn_y + 0.5;
  G.p.fx = 0; G.p.fy = 1;
  G.p.atk_cd = 0; G.p.ability_cd = 0; G.p.iframes = 0; G.p.anim_t = 0; G.p.charge_t = 0;
  G.p.downed = false; G.p.dead = false;

  /* mostri */
  for (int i = 0; i < MAX_MONSTERS; i++) G.mons[i].active = false;
  for (int i = 0; i < MAX_PROJS; i++) G.projs[i].active = false;
  for (int i = 0; i < MAX_PARTS; i++) G.parts[i].active = false;
  for (int i = 0; i < MAX_FLOATS; i++) G.floats[i].active = false;

  /* pesi come nel JS */
  struct { char k; int w; } pool[24];
  int pn = 0;
  int d = depth;
#define ADDW(_k,_w) do { if ((_w) > 0 && pn < 24) { pool[pn].k = (_k); pool[pn].w = (_w); pn++; } } while(0)
  ADDW('r', d < 10 ? 10 - d : 0);
  ADDW('b', d < 9 ? 9 - d : 0);
  ADDW('g', 8);
  ADDW('j', d >= 2 ? 6 : 2);
  ADDW('J', d >= 2 ? 5 : 1);
  ADDW('s', d >= 2 ? 9 : 2);
  ADDW('o', d >= 3 ? 8 : 1);
  ADDW('z', d >= 3 ? 7 : 1);
  ADDW('S', d >= 4 ? 7 : 0);
  ADDW('W', d >= 5 ? 6 : 0);
  ADDW('k', 7);
  ADDW('c', d >= 2 ? 6 : 1);
  ADDW('h', d >= 3 ? 6 : 0);
  ADDW('m', d >= 4 ? 5 : 0);
  ADDW('G', d >= 4 ? 5 : 0);
  ADDW('v', d >= 5 ? 5 : 0);
  ADDW('w', d >= 5 ? 4 : 0);
#undef ADDW
  int totalw = 0;
  for (int i = 0; i < pn; i++) totalw += pool[i].w;
  int nm = 6 + depth;
  if (nm > 24) nm = 24;
  int placed = 0, guard = 0;
  while (placed < nm && guard < 600) {
    guard++;
    int ri = ab_rng_range(&rng, 1, nr - 1);
    int tx = rooms[ri].x + ab_rng_range(&rng, 0, rooms[ri].w - 1);
    int ty = rooms[ri].y + ab_rng_range(&rng, 0, rooms[ri].h - 1);
    if (m->tiles[ty][tx] != T_FLOOR) continue;
    if (abs(tx - m->spawn_x) + abs(ty - m->spawn_y) < 6) continue;
    if (tx == m->stairs_x && ty == m->stairs_y) continue;
    /* scegli tipo */
    char tk = 'g';
    if (totalw > 0) {
      int roll = ab_rng_range(&rng, 1, totalw);
      for (int i = 0; i < pn; i++) { roll -= pool[i].w; if (roll <= 0) { tk = pool[i].k; break; } }
    }
    char ks[2] = {tk, 0};
    const AbMonDef *td = ab_mon_def(ks);
    if (!td) continue;
    for (int i = 0; i < MAX_MONSTERS; i++) {
      if (G.mons[i].active) continue;
      AbMonster *mm = &G.mons[i];
      mm->active = true;
      mm->type = tk;
      mm->x = mm->rx = tx + 0.5; mm->y = mm->ry = ty + 0.5;
      mm->max_hp = mm->hp = ab_scaled_stat(td->hp, depth, 0.16);
      mm->dmg = ab_scaled_stat(td->dmg, depth, 0.11);
      mm->speed = td->speed;
      mm->aggro = td->aggro;
      mm->facing_x = 0; mm->facing_y = 1;
      mm->atk_cd = 0; mm->wander_t = 0;
      mm->dot_t = 0; mm->regen_acc = 0;
      mm->is_boss = false;
      mm->boss_t = 0; mm->spec_cd = ab_rng_frange(&rng, 2, 4); mm->boss_phase = 0;
      double af = ab_rng_next(&rng);
      mm->affix = 0;
      if (!td->boss && depth >= 3 && af < 0.14) mm->affix = 1 + ab_rng_range(&rng, 0, 2);
      if (mm->affix == 1) mm->speed *= 1.6;
      placed++;
      break;
    }
  }
  /* boss */
  G.boss_active = false;
  if (m->has_arena) {
    char bk = ab_boss_for_depth(depth);
    char ks[2] = {bk, 0};
    const AbMonDef *td = ab_mon_def(ks);
    if (td) {
      for (int i = 0; i < MAX_MONSTERS; i++) {
        if (G.mons[i].active) continue;
        AbMonster *mm = &G.mons[i];
        mm->active = true;
        mm->type = bk;
        mm->x = mm->rx = m->arena_cx + 0.5;
        mm->y = mm->ry = m->arena_cy + 0.5;
        mm->max_hp = mm->hp = ab_scaled_stat(td->hp, depth, 0.10);
        mm->dmg = ab_scaled_stat(td->dmg, depth, 0.08);
        mm->speed = td->speed;
        mm->aggro = 99;
        mm->is_boss = true;
        mm->affix = 0;
        mm->boss_t = 0; mm->spec_cd = 2.5; mm->boss_phase = 0;
        strncpy(G.boss_name, td->name, sizeof G.boss_name - 1);
        G.boss_hp = mm->hp; G.boss_max = mm->max_hp;
        break;
      }
    }
  }
  G.boss_active = false;
  G.arena_locked = false;
}

/* ---------------- combattimento ---------------- */
static int roll_dmg(int cls) {
  const AbClassDef *c = ab_class_def(cls);
  AbRng r;
  ab_rng_seed(&r, (uint32_t)(G.time * 977) + (uint32_t)(G.p.x * 57) + (uint32_t)(G.p.y * 131) + (uint32_t)rand());
  int base = c->dmg_min + (int)(ab_rng_next(&r) * (double)(c->dmg_max - c->dmg_min + 1));
  int eqd = 0;
  for (int i = 0; i < 5; i++) eqd += G.p.equip[i].dmg;
  base += eqd;
  if (G.p.buff_rage_t > 0) base = (int)(base * 1.4 + 0.5);
  /* crit */
  double cr = c->crit;
  if (ab_rng_next(&r) < cr) base = (int)(base * 1.8 + 0.5);
  return base < 1 ? 1 : base;
}

static AbMonster *nearest_mon(double range) {
  AbMonster *best = NULL;
  double bd = range;
  for (int i = 0; i < MAX_MONSTERS; i++) {
    if (!G.mons[i].active) continue;
    double dd = ab_dist(G.p.x, G.p.y, G.mons[i].x, G.mons[i].y);
    if (dd < bd) { bd = dd; best = &G.mons[i]; }
  }
  return best;
}

static void spawn_proj(double x, double y, double dx, double dy, double spd, double range, int dmg, bool friendly, double r, double g, double b) {
  for (int i = 0; i < MAX_PROJS; i++) {
    if (G.projs[i].active) continue;
    AbProj *p = &G.projs[i];
    p->active = true;
    p->x = x; p->y = y;
    double l = sqrt(dx*dx + dy*dy);
    if (l < 0.001) { dx = 0; dy = 1; l = 1; }
    p->vx = dx / l * spd; p->vy = dy / l * spd;
    p->life = 3.0; p->range_left = range;
    p->dmg = dmg;
    p->friendly = friendly;
    p->r = r; p->g = g; p->b = b;
    p->size = 0.09;
    return;
  }
}

void ab_player_attack(void) {
  if (G.p.dead || G.p.downed) return;
  if (G.p.atk_cd > 0) return;
  const AbClassDef *c = ab_class_def(G.p.cls);
  if (c->max_mp > 0 && c->mana_cost > 0 && G.p.mp < c->mana_cost) {
    ab_float_text(G.p.x, G.p.y - 0.6, "NO MANA", 0.4, 0.6, 1.0);
    G.p.atk_cd = 0.25;
    return;
  }
  /* auto-mira come nel JS */
  double ar = c->ranged ? c->range : (c->range + 0.7 > 2.1 ? c->range + 0.7 : 2.1);
  AbMonster *nm = nearest_mon(ar);
  if (nm) {
    double dx = nm->x - G.p.x, dy = nm->y - G.p.y;
    double l = sqrt(dx*dx + dy*dy);
    if (l > 0.01) { G.p.fx = dx / l; G.p.fy = dy / l; }
  }
  G.p.atk_cd = c->atk_cooldown * (G.p.buff_haste_t > 0 ? 0.7 : 1.0);
  G.p.anim_t = 0.22;
  if (c->max_mp > 0 && c->mana_cost) { G.p.mp -= c->mana_cost; if (G.p.mp < 0) G.p.mp = 0; }
  int dmg = roll_dmg(G.p.cls);
  /* prof colpo caricato */
  if (G.p.cls == CLS_PROF && G.p.charge_t > 0.8) { dmg *= 2; G.p.charge_t = 0; ab_burst(G.p.x, G.p.y, 10, 0.5, 1, 1, 4); }

  if (!c->ranged) {
    /* melee ad arco */
    for (int i = 0; i < MAX_MONSTERS; i++) {
      if (!G.mons[i].active) continue;
      double dx = G.mons[i].x - G.p.x, dy = G.mons[i].y - G.p.y;
      double dd = sqrt(dx*dx + dy*dy);
      if (dd > c->range + 0.35) continue;
      double dot = 0;
      if (dd > 0.01) dot = (dx * G.p.fx + dy * G.p.fy) / dd;
      double need = cos(c->arc);
      if (dot < need && dd > 0.6) continue;
      G.mons[i].hp -= dmg;
      G.mons[i].facing_x = -G.p.fx; G.mons[i].facing_y = -G.p.fy;
      char b[16]; snprintf(b, sizeof b, "%d", dmg);
      ab_float_text(G.mons[i].x, G.mons[i].y - 0.5, b, 1, 0.85, 0.4);
      ab_burst(G.mons[i].x, G.mons[i].y, 5, 1, 0.8, 0.3, 3);
    }
    ab_burst(G.p.x + G.p.fx * 0.8, G.p.y + G.p.fy * 0.8, 4, 1, 1, 1, 2.5);
  } else {
    double cr = 0.55, cg = 0.85, cb = 1.0;
    if (G.p.cls == CLS_NEGROMANTE) { cr = 0.56; cg = 0.88; cb = 0.48; }
    else if (G.p.cls == CLS_RANGER) { cr = 0.85; cg = 0.9; cb = 0.69; }
    else if (G.p.cls == CLS_PROF) { cr = 0.49; cg = 0.98; cb = 1.0; }
    spawn_proj(G.p.x, G.p.y, G.p.fx, G.p.fy, c->proj_speed, c->range, dmg, true, cr, cg, cb);
    ab_burst(G.p.x + G.p.fx * 0.5, G.p.y + G.p.fy * 0.5, 3, cr, cg, cb, 2);
  }
}

void ab_use_ability(void) {
  if (G.p.dead || G.p.downed) return;
  if (G.p.ability_cd > 0) return;
  const AbClassDef *c = ab_class_def(G.p.cls);
  if (c->ability_mana > 0 && G.p.mp < c->ability_mana) {
    ab_float_text(G.p.x, G.p.y - 0.6, "NO MANA", 0.4, 0.6, 1.0);
    return;
  }
  G.p.ability_cd = c->ability_cd;
  if (c->ability_mana) { G.p.mp -= c->ability_mana; if (G.p.mp < 0) G.p.mp = 0; }
  int dmg = roll_dmg(G.p.cls);
  switch (G.p.cls) {
    case CLS_GUERRIERO: { /* Carica: scatto + danno linea */
      double nx = G.p.x + G.p.fx * 3.0, ny = G.p.y + G.p.fy * 3.0;
      int tx = (int)nx, ty = (int)ny;
      if (ab_is_walkable(tx, ty)) { G.p.x = nx; G.p.y = ny; }
      for (int i = 0; i < MAX_MONSTERS; i++) {
        if (!G.mons[i].active) continue;
        if (ab_dist(G.p.x, G.p.y, G.mons[i].x, G.mons[i].y) < 2.2) {
          G.mons[i].hp -= dmg + 2;
          ab_float_text(G.mons[i].x, G.mons[i].y - 0.5, "CARICA!", 1, 0.8, 0.3);
        }
      }
      ab_burst(G.p.x, G.p.y, 12, 1, 0.8, 0.3, 4);
      G.shake = 0.25;
      break;
    }
    case CLS_LADRO: { /* Passo furtivo: blink sul nearest + crit */
      AbMonster *nm = nearest_mon(9);
      if (nm) {
        G.p.x = nm->x - G.p.fx * 0.8; G.p.y = nm->y - G.p.fy * 0.8;
        nm->hp -= (int)(dmg * 1.8 + 0.5);
        ab_float_text(nm->x, nm->y - 0.5, "FURTIVO!", 0.8, 0.9, 1);
      }
      G.p.iframes = 1.2;
      break;
    }
    case CLS_MAGO: case CLS_NEGROMANTE: { /* AoE */
      int hits = 0;
      for (int i = 0; i < MAX_MONSTERS; i++) {
        if (!G.mons[i].active) continue;
        if (ab_dist(G.p.x, G.p.y, G.mons[i].x, G.mons[i].y) < 2.6) {
          G.mons[i].hp -= dmg;
          hits++;
          if (G.p.cls == CLS_NEGROMANTE) { G.p.hp += dmg / 2; if (G.p.hp > G.p.max_hp) G.p.hp = G.p.max_hp; }
        }
      }
      ab_burst(G.p.x, G.p.y, 20, 0.6, 0.8, 1, 5);
      G.shake = 0.3;
      if (hits == 0) ab_float_text(G.p.x, G.p.y - 0.6, "VUOTO", 0.6, 0.6, 0.6);
      break;
    }
    case CLS_RANGER: { /* raffica ventaglio 5 frecce */
      for (int k = -2; k <= 2; k++) {
        double a = atan2(G.p.fy, G.p.fx) + k * 0.18;
        spawn_proj(G.p.x, G.p.y, cos(a), sin(a), 15, 9, dmg, true, 0.85, 0.9, 0.69);
      }
      break;
    }
    case CLS_PALADINO:
      G.p.buff_shield_t = 5.0;
      ab_add_log("Muro Sacro: -50% danno per 5s.");
      break;
    case CLS_BARDO:
      G.p.buff_rage_t = 8.0;
      ab_add_log("Canto: +40% danno per 8s.");
      break;
    case CLS_MONACO: { /* onda perforante */
      for (int k = 0; k < 3; k++)
        spawn_proj(G.p.x, G.p.y, G.p.fx, G.p.fy, 9 + k * 2, 7, dmg, true, 1, 0.8, 0.5);
      break;
    }
    case CLS_PROF:
      G.p.charge_t = 0.0001; /* inizia carica */
      ab_add_log("Carica plasma... attacca per sparare!");
      G.p.ability_cd = 4.0;
      break;
  }
}

void ab_drink_potion(void) {
  if (G.p.dead) return;
  if (G.p.potions <= 0) { ab_float_text(G.p.x, G.p.y - 0.6, "VUOTA", 1, 0.4, 0.4); return; }
  if (G.p.hp >= G.p.max_hp && !G.p.downed) return;
  G.p.potions--;
  int heal = G.p.max_hp / 2 + 4;
  G.p.hp += heal;
  if (G.p.hp > G.p.max_hp) G.p.hp = G.p.max_hp;
  if (G.p.downed) { G.p.downed = false; G.p.downed_t = 0; ab_toast("Rianimato!"); }
  ab_burst(G.p.x, G.p.y, 10, 0.5, 1, 0.5, 3);
  char b[32]; snprintf(b, sizeof b, "+%d HP", heal);
  ab_float_text(G.p.x, G.p.y - 0.6, b, 0.5, 1, 0.5);
}
void ab_drink_mana(void) {
  if (G.p.dead || G.p.max_mp <= 0) return;
  if (G.p.mana_potions <= 0) return;
  if (G.p.mp >= G.p.max_mp) return;
  G.p.mana_potions--;
  G.p.mp = G.p.max_mp;
  ab_float_text(G.p.x, G.p.y - 0.6, "+MANA", 0.4, 0.7, 1);
}

void ab_try_interact(void) {
  if (G.p.dead || G.p.downed) return;
  int px = (int)floor(G.p.x), py = (int)floor(G.p.y);
  /* scale */
  if (abs(px - G.map.stairs_x) + abs(py - G.map.stairs_y) <= 1) { ab_descend(); return; }
  /* mercante */
  if (abs(px - G.map.merch_x) + abs(py - G.map.merch_y) <= 2) {
    G.merchant_open = !G.merchant_open;
    return;
  }
  /* forzieri */
  for (int i = 0; i < G.chest_count; i++) {
    AbChest *c = &G.chests[i];
    if (!c->active || c->open) continue;
    if (abs(px - c->tx) + abs(py - c->ty) <= 1) {
      c->open = true;
      ab_burst(c->tx + 0.5, c->ty + 0.5, 12, 1, 0.85, 0.3, 3.5);
      if (c->kind == 0) {
        G.p.gold += c->gold;
        char b[48]; snprintf(b, sizeof b, "+%d ORO", c->gold);
        ab_loot_banner(b, c->rarity);
        ab_add_log(b);
      } else if (c->kind == 1) { G.p.potions++; ab_loot_banner("Pozione HP!", c->rarity); }
      else if (c->kind == 2) { G.p.mana_potions++; ab_loot_banner("Pozione Mana!", c->rarity); }
      else {
        /* equip random */
        static const char *slots[5] = {"Elmo","Collana","Armatura","Anello","Gambali"};
        int s = rand() % 5;
        AbEquip *e = &G.p.equip[s];
        e->filled = true;
        int mult = c->rarity + 1;
        e->rarity = c->rarity;
        e->hp = mult * (2 + rand() % 4);
        e->dmg = mult * (1 + rand() % 2);
        e->speed = (c->rarity >= 2) ? 0.2 : 0;
        snprintf(e->name, sizeof e->name, "%s %s", c->rarity == 3 ? "Leggendario" : c->rarity == 2 ? "Epico" : c->rarity == 1 ? "Raro" : "Comune", slots[s]);
        /* applica */
        const AbClassDef *cd = ab_class_def(G.p.cls);
        int eqh = 0; double eqs = 0;
        for (int k = 0; k < 5; k++) { eqh += G.p.equip[k].hp; eqs += G.p.equip[k].speed; }
        int oldmax = G.p.max_hp;
        G.p.max_hp = cd->hp + eqh;
        G.p.hp += (G.p.max_hp - oldmax);
        G.p.speed = cd->speed + eqs;
        ab_loot_banner(e->name, c->rarity);
      }
      return;
    }
  }
}

/* acquisti mercante: 0 poz HP 10, 1 poz mana 8, 2 +danno 30, 3 +vita 30, 4 gamble 50 */
bool ab_merchant_buy(int idx);
bool ab_merchant_buy(int idx) {
  int price[5] = {10, 8, 30, 30, 50};
  if (idx < 0 || idx > 4) return false;
  if (G.p.gold < price[idx]) { ab_float_text(G.p.x, G.p.y - 0.6, "POCHI SOLDI", 1, 0.8, 0.3); return false; }
  if (idx == 0) { G.p.gold -= price[idx]; G.p.potions++; }
  else if (idx == 1) { G.p.gold -= price[idx]; G.p.mana_potions++; }
  else if (idx == 2) {
    G.p.gold -= price[idx];
    G.p.equip[3].filled = true; G.p.equip[3].dmg += 1;
    snprintf(G.p.equip[3].name, sizeof G.p.equip[3].name, "Anello Potente");
    if (G.p.equip[3].rarity < 1) G.p.equip[3].rarity = 1;
  } else if (idx == 3) {
    G.p.gold -= price[idx];
    G.p.max_hp += 3; G.p.hp += 3;
  } else {
    G.p.gold -= price[idx];
    int s = rand() % 5;
    AbEquip *e = &G.p.equip[s];
    e->filled = true;
    int r = rand() % 100;
    e->rarity = r < 55 ? 0 : r < 80 ? 1 : r < 94 ? 2 : 3;
    e->hp = (e->rarity + 1) * (2 + rand() % 4);
    e->dmg = (e->rarity + 1);
    snprintf(e->name, sizeof e->name, "Bottino %d", s);
    const AbClassDef *cd = ab_class_def(G.p.cls);
    int eqh = 0;
    for (int k = 0; k < 5; k++) eqh += G.p.equip[k].hp;
    G.p.max_hp = cd->hp + eqh;
  }
  ab_burst(G.p.x, G.p.y, 8, 1, 0.85, 0.3, 3);
  return true;
}

void ab_descend(void) {
  int nd = G.depth + 1;
  ab_add_log("Scendi piu' a fondo...");
  ab_gen_depth(nd);
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

/* danno al giocatore (con scudo/iframe/god) */
static void hurt_player(int dmg) {
  if (G.p.dead || G.god) return;
  if (G.p.iframes > 0) return;
  if (G.p.buff_shield_t > 0) dmg = (dmg + 1) / 2;
  G.p.hp -= dmg;
  G.p.iframes = 0.5;
  G.shake = 0.3;
  char b[16]; snprintf(b, sizeof b, "-%d", dmg);
  ab_float_text(G.p.x, G.p.y - 0.6, b, 1, 0.3, 0.3);
  ab_burst(G.p.x, G.p.y, 6, 1, 0.2, 0.2, 3);
  if (G.p.hp <= 0) {
    if (!G.p.downed) {
      G.p.downed = true; G.p.downed_t = 10.0; G.p.hp = 0;
      ab_toast("A TERRA! Un compagno ti rianimi... (Q per pozione)");
      ab_add_log("Sei a terra! Bevi una pozione (Q) entro 10s.");
    } else {
      G.p.dead = true;
      G.state = ST_DEAD;
      ab_add_log("Sei morto. L'abisso ti ha reclamato.");
      /* permadeath: perdi oro/poz/equip */
      G.p.gold = 0; G.p.potions = 0; G.p.mana_potions = 0;
      memset(G.p.equip, 0, sizeof G.p.equip);
      if (G.depth > G.best_depth) G.best_depth = G.depth;
      if (G.p.gold > G.best_gold) G.best_gold = G.p.gold;
      ab_save_record();
    }
  }
}

/* uccisione mostro: oro/xp/split/esplosivo */
static void kill_monster(int i) {
  AbMonster *m = &G.mons[i];
  char ks[2] = {m->type, 0};
  const AbMonDef *td = ab_mon_def(ks);
  AbRng r;
  ab_rng_seed(&r, (uint32_t)(G.time * 131) + (uint32_t)i * 17 + 3);
  int gold = td ? td->gold_min + (int)(ab_rng_next(&r) * (double)(td->gold_max - td->gold_min + 1)) : 2;
  G.p.gold += gold;
  G.p.xp += td ? td->xp : 2;
  /* level up semplice */
  int need = 10 + G.p.level * 8;
  if (G.p.xp >= need) {
    G.p.xp -= need; G.p.level++;
    G.p.max_hp += 2; G.p.hp = G.p.max_hp;
    ab_toast("LIVELLO SU!");
    ab_add_log("Sali di livello: +2 HP max.");
  }
  ab_burst(m->x, m->y, m->is_boss ? 30 : 10, 1, 0.5, 0.2, 4);
  if (m->affix == 2) {
    /* esplosivo */
    for (int k = 0; k < MAX_MONSTERS; k++) {
      if (k != i && G.mons[k].active && ab_dist(m->x, m->y, G.mons[k].x, G.mons[k].y) < 1.8)
        G.mons[k].hp -= 6;
    }
    if (ab_dist(m->x, m->y, G.p.x, G.p.y) < 1.8) hurt_player(5);
    ab_burst(m->x, m->y, 16, 1, 0.4, 0.1, 6);
    G.shake = 0.35;
  }
  if (td && td->split) {
    /* gelatina -> 2 melme */
    for (int k = 0; k < 2; k++) {
      for (int j = 0; j < MAX_MONSTERS; j++) {
        if (G.mons[j].active) continue;
        char js[2] = {'j', 0};
        const AbMonDef *jd = ab_mon_def(js);
        G.mons[j].active = true;
        G.mons[j].type = 'j';
        G.mons[j].x = G.mons[j].rx = m->x + (k ? 0.4 : -0.4);
        G.mons[j].y = G.mons[j].ry = m->y;
        G.mons[j].max_hp = G.mons[j].hp = ab_scaled_stat(jd->hp, G.depth, 0.16);
        G.mons[j].dmg = ab_scaled_stat(jd->dmg, G.depth, 0.11);
        G.mons[j].speed = jd->speed; G.mons[j].aggro = jd->aggro;
        G.mons[j].is_boss = false; G.mons[j].affix = 0;
        G.mons[j].atk_cd = 0;
        break;
      }
    }
  }
  if (m->is_boss) {
    ab_toast("BOSS SCONFITTO!");
    ab_add_log("Il boss cade. Bottino leggendario!");
    G.p.gold += 40 + G.depth * 3;
    G.p.potions += 2;
    G.boss_active = false;
    ab_burst(m->x, m->y, 40, 1, 0.9, 0.4, 6);
    G.shake = 0.5;
  }
  m->active = false;
}

/* ---------------- update ---------------- */
void ab_update(double dt, unsigned keys) {
  if (G.state != ST_GAME) return;
  if (dt > 0.05) dt = 0.05;
  G.time += dt;
  G.torch_clk += dt;
  if (G.shake > 0) G.shake -= dt;
  if (G.toast_t > 0) G.toast_t -= dt;
  if (G.loot_t > 0) G.loot_t -= dt;
  for (int i = 0; i < MAX_LOGLINES; i++) if (G.log_t[i] > 0) G.log_t[i] -= dt;

  double spd = G.p.speed * (G.speed5 ? 5 : 1) * (G.p.buff_haste_t > 0 ? 1.3 : 1.0);
  /* carica prof */
  if (G.p.charge_t > 0 && G.p.charge_t < 0.8) {
    G.p.charge_t += dt;
    if (G.p.charge_t >= 0.8) ab_float_text(G.p.x, G.p.y - 0.6, "CARICO!", 0.5, 1, 1);
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
    }
    if (keys & K_ATK) {
      if (G.p.cls == CLS_PROF && G.p.charge_t > 0) {
        /* continua carica, spara al rilascio? semplifica: spara subito se carico */
        if (G.p.charge_t >= 0.8) ab_player_attack();
      } else ab_player_attack();
    } else {
      if (G.p.cls == CLS_PROF && G.p.charge_t >= 0.8) ab_player_attack();
      else if (G.p.cls == CLS_PROF && G.p.charge_t > 0 && G.p.charge_t < 0.8) { /* annulla */ }
    }
  }
  if (G.p.atk_cd > 0) G.p.atk_cd -= dt;
  if (G.p.ability_cd > 0) G.p.ability_cd -= dt;
  if (G.p.iframes > 0) G.p.iframes -= dt;
  if (G.p.anim_t > 0) G.p.anim_t -= dt;
  if (G.p.buff_rage_t > 0) G.p.buff_rage_t -= dt;
  if (G.p.buff_shield_t > 0) G.p.buff_shield_t -= dt;
  if (G.p.buff_haste_t > 0) G.p.buff_haste_t -= dt;
  /* mana regen */
  if (G.p.max_mp > 0 && G.p.mp < G.p.max_mp) {
    static double acc = 0;
    acc += dt;
    if (acc > 1.5) { acc = 0; G.p.mp++; }
  }
  /* downed */
  if (G.p.downed && !G.p.dead) {
    G.p.downed_t -= dt;
    if (G.p.downed_t <= 0) {
      G.p.downed = false;
      G.p.dead = true;
      G.state = ST_DEAD;
      if (G.depth > G.best_depth) G.best_depth = G.depth;
      ab_save_record();
    }
  }

  /* smoothing giocatore */
  G.p.rx += (G.p.x - G.p.rx) * fmin(1, dt * 12);
  G.p.ry += (G.p.y - G.p.ry) * fmin(1, dt * 12);

  /* fog */
  {
    int px = (int)G.p.x, py = (int)G.p.y;
    int R = 8;
    for (int y = py - R; y <= py + R; y++)
      for (int x = px - R; x <= px + R; x++) {
        if (x < 0 || y < 0 || x >= G.map.w || y >= G.map.h) continue;
        if (abs(x - px) + abs(y - py) <= R) G.map.visited[y][x] = 1;
      }
  }

  /* boss trigger: entra in arena */
  if (G.map.has_arena && !G.boss_active) {
    /* c'e ancora un boss vivo? */
    bool alive = false;
    for (int i = 0; i < MAX_MONSTERS; i++)
      if (G.mons[i].active && G.mons[i].is_boss) { alive = true; break; }
    if (alive) {
      if (fabs(G.p.x - (G.map.arena_cx + 0.5)) < G.map.arena_w / 2.0 &&
          fabs(G.p.y - (G.map.arena_cy + 0.5)) < G.map.arena_h / 2.0) {
        G.boss_active = true;
        char b[96];
        snprintf(b, sizeof b, "%s si risveglia!", G.boss_name);
        ab_toast(b);
        ab_add_log(b);
        G.shake = 0.5;
      }
    }
  }
  /* boss bar */
  G.boss_hp = 0; G.boss_max = 1;
  for (int i = 0; i < MAX_MONSTERS; i++) {
    if (G.mons[i].active && G.mons[i].is_boss) {
      G.boss_hp = G.mons[i].hp; G.boss_max = G.mons[i].max_hp;
      break;
    }
  }

  /* mostri */
  for (int i = 0; i < MAX_MONSTERS; i++) {
    AbMonster *m = &G.mons[i];
    if (!m->active) continue;
    if (m->hp <= 0) { kill_monster(i); continue; }
    char ks[2] = {m->type, 0};
    const AbMonDef *td = ab_mon_def(ks);
    /* regen affix */
    if (m->affix == 3 && m->hp < m->max_hp) {
      m->regen_acc += dt * m->max_hp * 0.045;
      if (m->regen_acc >= 1) { int h = (int)m->regen_acc; m->regen_acc -= h; m->hp += h; if (m->hp > m->max_hp) m->hp = m->max_hp; }
    }
    if (m->dot_t > 0) {
      m->dot_t -= dt;
      m->dot_acc += dt;
      if (m->dot_acc >= 0.5) {
        m->dot_acc = 0;
        m->hp -= 1;
        if (m->hp <= 0) { kill_monster(i); continue; }
      }
    }
    if (m->atk_cd > 0) m->atk_cd -= dt;
    double dd = ab_dist(G.p.x, G.p.y, m->x, m->y);
    bool aggro = dd < m->aggro || (m->is_boss && G.boss_active);
    double mvx = 0, mvy = 0;
    if (!G.p.dead && !G.p.downed && aggro) {
      double dx = G.p.x - m->x, dy = G.p.y - m->y;
      double l = sqrt(dx*dx + dy*dy);
      if (l > 0.05) {
        /* erratic: zigzag */
        if (td && td->erratic) {
          double w = sin(G.time * 7 + i) * 0.8;
          double nx = -dy / l, ny = dx / l;
          mvx = dx / l + nx * w; mvy = dy / l + ny * w;
          double ml = sqrt(mvx*mvx + mvy*mvy);
          if (ml > 0.01) { mvx /= ml; mvy /= ml; }
        } else {
          mvx = dx / l; mvy = dy / l;
        }
        /* dash serpente/mantide: scatti */
        double sp = m->speed;
        if (td && td->dash && dd > 2 && ((int)(G.time * 1.5 + i) % 3 == 0)) sp *= 2.2;
        if (dd > 0.7) try_move(&m->x, &m->y, mvx * sp * dt, mvy * sp * dt);
        m->facing_x = mvx; m->facing_y = mvy;
        /* attacco contatto / proiettili sciamano/boss */
        if (dd < 0.9 && m->atk_cd <= 0) {
          m->atk_cd = 1.0;
          hurt_player(m->dmg + ((td && td->poison) ? 2 : 0));
          if (td && td->lifesteal) { m->hp += 2; if (m->hp > m->max_hp) m->hp = m->max_hp; }
        }
        /* sciamano e boss ranged */
        if ((m->type == 'w' || m->is_boss) && dd < 8 && dd > 1.5 && m->atk_cd <= 0) {
          m->atk_cd = m->is_boss ? 1.2 : 1.8;
          double dx2 = G.p.x - m->x, dy2 = G.p.y - m->y;
          spawn_proj(m->x, m->y, dx2, dy2, 6, 9, m->dmg / 2 + 1, false, 1, 0.4, 0.3);
        }
      }
    } else {
      /* wander */
      m->wander_t -= dt;
      if (m->wander_t <= 0) {
        AbRng r;
        ab_rng_seed(&r, (uint32_t)(G.time * 61) + (uint32_t)i * 131 + 5);
        double a = ab_rng_next(&r) * 6.28318;
        m->wx = cos(a); m->wy = sin(a);
        m->wander_t = ab_rng_frange(&r, 1, 3);
      }
      try_move(&m->x, &m->y, m->wx * m->speed * 0.3 * dt, m->wy * m->speed * 0.3 * dt);
    }
    /* boss special */
    if (m->is_boss && G.boss_active && !G.p.dead) {
      m->spec_cd -= dt;
      if (m->spec_cd <= 0) {
        m->spec_cd = 3.0 + (m->hp < m->max_hp / 2 ? -0.8 : 0);
        m->boss_phase++;
        if (m->type == 'D') {
          /* soffio: ventaglio 5 */
          for (int k = -2; k <= 2; k++) {
            double a = atan2(G.p.y - m->y, G.p.x - m->x) + k * 0.2;
            spawn_proj(m->x, m->y, cos(a), sin(a), 7, 7, m->dmg / 2 + 2, false, 1, 0.5, 0.1);
          }
          ab_add_log("Il Drago sputa fuoco!");
          G.shake = 0.4;
        } else if (m->type == 'X') {
          /* carica */
          double dx = G.p.x - m->x, dy = G.p.y - m->y;
          double l = sqrt(dx*dx + dy*dy);
          if (l > 0.1) { m->x += dx / l * 3; m->y += dy / l * 3; }
          G.shake = 0.5;
          ab_add_log("Il Golem carica!");
          if (ab_dist(G.p.x, G.p.y, m->x, m->y) < 1.4) hurt_player(m->dmg);
        } else if (m->type == 'M') {
          /* evoca 2 melme */
          for (int k = 0; k < 2; k++) {
            for (int j = 0; j < MAX_MONSTERS; j++) {
              if (G.mons[j].active) continue;
              G.mons[j].active = true; G.mons[j].type = 'j';
              G.mons[j].x = G.mons[j].rx = m->x + (k ? 1 : -1);
              G.mons[j].y = G.mons[j].ry = m->y;
              char js[2] = {'j', 0};
              const AbMonDef *jd = ab_mon_def(js);
              G.mons[j].max_hp = G.mons[j].hp = ab_scaled_stat(jd->hp, G.depth, 0.16);
              G.mons[j].dmg = ab_scaled_stat(jd->dmg, G.depth, 0.11);
              G.mons[j].speed = jd->speed; G.mons[j].aggro = 99;
              G.mons[j].is_boss = false; G.mons[j].affix = 0; G.mons[j].atk_cd = 0;
              break;
            }
          }
          ab_add_log("La Regina evoca melme!");
        } else if (m->type == 'R') {
          /* telegrafo area: danno se resti vicino dopo 1s -> semplifica danno immediato + shake */
          G.shake = 0.45;
          ab_burst(G.p.x, G.p.y, 14, 0.6, 0.2, 0.5, 4);
          if (ab_dist(G.p.x, G.p.y, m->x, m->y) < 3.0) hurt_player(m->dmg / 2 + 2);
          ab_add_log("Il Re Ragno tesse la tela!");
        } else if (m->type == 'K') {
          for (int k = 0; k < 2; k++) {
            for (int j = 0; j < MAX_MONSTERS; j++) {
              if (G.mons[j].active) continue;
              G.mons[j].active = true; G.mons[j].type = 'r';
              G.mons[j].x = G.mons[j].rx = m->x + (k ? 1 : -1);
              G.mons[j].y = G.mons[j].ry = m->y;
              char js[2] = {'r', 0};
              const AbMonDef *jd = ab_mon_def(js);
              G.mons[j].max_hp = G.mons[j].hp = ab_scaled_stat(jd->hp, G.depth, 0.16);
              G.mons[j].dmg = ab_scaled_stat(jd->dmg, G.depth, 0.11);
              G.mons[j].speed = jd->speed * 1.3; G.mons[j].aggro = 99;
              G.mons[j].is_boss = false; G.mons[j].affix = 0; G.mons[j].atk_cd = 0;
              break;
            }
          }
          ab_add_log("Il Re dei Ratti fischia il branco!");
        } else if (m->type == 'L') {
          for (int k = -1; k <= 1; k++) {
            double a = atan2(G.p.y - m->y, G.p.x - m->x) + k * 0.35;
            spawn_proj(m->x, m->y, cos(a), sin(a), 6, 10, m->dmg / 2 + 1, false, 0.5, 1, 0.6);
          }
          ab_add_log("Il Lich evoca anime!");
        }
        ab_burst(m->x, m->y, 12, 1, 0.5, 0.2, 4);
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
      for (int j = 0; j < MAX_MONSTERS; j++) {
        if (!G.mons[j].active) continue;
        if (ab_dist(p->x, p->y, G.mons[j].x, G.mons[j].y) < 0.45) {
          G.mons[j].hp -= p->dmg;
          char b[16]; snprintf(b, sizeof b, "%d", p->dmg);
          ab_float_text(G.mons[j].x, G.mons[j].y - 0.5, b, 1, 0.85, 0.4);
          ab_burst(p->x, p->y, 4, p->r, p->g, p->b, 2.5);
          p->active = false;
          break;
        }
      }
    } else {
      if (!G.p.dead && ab_dist(p->x, p->y, G.p.x, G.p.y) < 0.45) {
        hurt_player(p->dmg);
        p->active = false;
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
