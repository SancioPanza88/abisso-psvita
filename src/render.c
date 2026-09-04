/* ABISSO Vita - renderer SDL2. Sprite PNG originali, font 5x7 integrato,
 * viste topdown/isometrica, torce/fog, HUD, minimappa, menu, modalita C64. */
#include "abisso.h"
#include "render.h"
#include "input.h"
#include "audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

bool ab_merchant_buy(int idx);

static SDL_Window *win = NULL;
static SDL_Renderer *ren = NULL;

/* ---------- sprite ---------- */
#define MAXSPR 72
typedef struct { char name[48]; SDL_Texture *tex; int w, h; } Spr;
static Spr sprs[MAXSPR];
static int nspr = 0;

static void asset_try(const char *rel, char *out, size_t n) {
#ifdef ABISSO_VITA
  snprintf(out, n, "app0:%s", rel);
  FILE *f = fopen(out, "rb");
  if (f) { fclose(f); return; }
#endif
  snprintf(out, n, "%s", rel);
}

static SDL_Texture *spr_get(const char *name) {
  for (int i = 0; i < nspr; i++)
    if (strcmp(sprs[i].name, name) == 0) return sprs[i].tex;
  return NULL;
}

static void spr_load(const char *name, const char *rel) {
  if (nspr >= MAXSPR) return;
  char path[160];
  asset_try(rel, path, sizeof path);
  SDL_Surface *s = IMG_Load(path);
  if (!s) {
    /* prova con ./ prefisso */
    char p2[170];
    snprintf(p2, sizeof p2, "./%s", rel);
    s = IMG_Load(p2);
  }
  strncpy(sprs[nspr].name, name, 47);
  if (s) {
    sprs[nspr].tex = SDL_CreateTextureFromSurface(ren, s);
    sprs[nspr].w = s->w; sprs[nspr].h = s->h;
    SDL_FreeSurface(s);
  } else {
    sprs[nspr].tex = NULL;
    sprs[nspr].w = sprs[nspr].h = 32;
  }
  nspr++;
}

static void sprites_init(void) {
  const char *files[] = {
    "hero_guerriero","hero_ladro","hero_mago","hero_ranger","hero_paladino",
    "hero_negromante","hero_bardo","hero_monaco","prof",
    "mon_cultista","mon_arpia","mon_mantide","mon_golem","mon_cavaliere","mon_cavaliere_alt",
    "mon_sciamano","mon_drago","mon_gelatina","mon_goblin","mon_melma","mon_orco",
    "mon_pipistrello","mon_ragno","mon_ratto","mon_scheletro","mon_serpente","mon_spettro",
    "mon_zombie","boss_golem","boss_lich","boss_melme","boss_ragno","boss_ratti",
    "floor_stone","floor_dirt","wall_stone","wall_brick","stairs","torch",
    "chest_closed","chest_open","merchant","rianima","icon_gold","icon_potion_hp",
    "icon_potion_mana","icon_lightning","icon_eye","icon_shield_buff","icon_gem_blue",
    "equip_helm","equip_armor","equip_ring","equip_necklace","equip_greaves",
    "potenziamento_fretta","powerupfocus","furia",NULL
  };
  for (int i = 0; files[i]; i++) {
    char rel[96];
    if (strcmp(files[i], "prof") == 0) snprintf(rel, sizeof rel, "assets/speciali/prof.png");
    else snprintf(rel, sizeof rel, "assets/sprites/%s.png", files[i]);
    spr_load(files[i], rel);
  }
}

/* ---------- font 5x7 ---------- */
static const unsigned char F_A[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
static void font_rows(char c, unsigned char o[7]) {
  if (c >= 'a' && c <= 'z') c -= 32;
  switch (c) {
    case 'A': memcpy(o,F_A,7); break;
    case 'B': { static const unsigned char v[7]={0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; memcpy(o,v,7); break; }
    case 'C': { static const unsigned char v[7]={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; memcpy(o,v,7); break; }
    case 'D': { static const unsigned char v[7]={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}; memcpy(o,v,7); break; }
    case 'E': { static const unsigned char v[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; memcpy(o,v,7); break; }
    case 'F': { static const unsigned char v[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; memcpy(o,v,7); break; }
    case 'G': { static const unsigned char v[7]={0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}; memcpy(o,v,7); break; }
    case 'H': { static const unsigned char v[7]={0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; memcpy(o,v,7); break; }
    case 'I': { static const unsigned char v[7]={0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}; memcpy(o,v,7); break; }
    case 'J': { static const unsigned char v[7]={0x07,0x02,0x02,0x02,0x02,0x12,0x0C}; memcpy(o,v,7); break; }
    case 'K': { static const unsigned char v[7]={0x11,0x12,0x14,0x18,0x14,0x12,0x11}; memcpy(o,v,7); break; }
    case 'L': { static const unsigned char v[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; memcpy(o,v,7); break; }
    case 'M': { static const unsigned char v[7]={0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; memcpy(o,v,7); break; }
    case 'N': { static const unsigned char v[7]={0x11,0x19,0x19,0x15,0x13,0x13,0x11}; memcpy(o,v,7); break; }
    case 'O': { static const unsigned char v[7]={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(o,v,7); break; }
    case 'P': { static const unsigned char v[7]={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; memcpy(o,v,7); break; }
    case 'Q': { static const unsigned char v[7]={0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; memcpy(o,v,7); break; }
    case 'R': { static const unsigned char v[7]={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; memcpy(o,v,7); break; }
    case 'S': { static const unsigned char v[7]={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; memcpy(o,v,7); break; }
    case 'T': { static const unsigned char v[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; memcpy(o,v,7); break; }
    case 'U': { static const unsigned char v[7]={0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(o,v,7); break; }
    case 'V': { static const unsigned char v[7]={0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; memcpy(o,v,7); break; }
    case 'W': { static const unsigned char v[7]={0x11,0x11,0x11,0x15,0x15,0x1B,0x11}; memcpy(o,v,7); break; }
    case 'X': { static const unsigned char v[7]={0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; memcpy(o,v,7); break; }
    case 'Y': { static const unsigned char v[7]={0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; memcpy(o,v,7); break; }
    case 'Z': { static const unsigned char v[7]={0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; memcpy(o,v,7); break; }
    case '0': { static const unsigned char v[7]={0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; memcpy(o,v,7); break; }
    case '1': { static const unsigned char v[7]={0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; memcpy(o,v,7); break; }
    case '2': { static const unsigned char v[7]={0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}; memcpy(o,v,7); break; }
    case '3': { static const unsigned char v[7]={0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}; memcpy(o,v,7); break; }
    case '4': { static const unsigned char v[7]={0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; memcpy(o,v,7); break; }
    case '5': { static const unsigned char v[7]={0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}; memcpy(o,v,7); break; }
    case '6': { static const unsigned char v[7]={0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}; memcpy(o,v,7); break; }
    case '7': { static const unsigned char v[7]={0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; memcpy(o,v,7); break; }
    case '8': { static const unsigned char v[7]={0x0E,0x11,0x11,0x0A,0x11,0x11,0x0E}; memcpy(o,v,7); break; }
    case '9': { static const unsigned char v[7]={0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}; memcpy(o,v,7); break; }
    case ' ': memset(o,0,7); break;
    case '.': { static const unsigned char v[7]={0,0,0,0,0,0x0C,0x0C}; memcpy(o,v,7); break; }
    case ',': { static const unsigned char v[7]={0,0,0,0,0x0C,0x04,0x08}; memcpy(o,v,7); break; }
    case ':': { static const unsigned char v[7]={0,0x0C,0x0C,0,0,0x0C,0x0C}; memcpy(o,v,7); break; }
    case '!': { static const unsigned char v[7]={0x04,0x04,0x04,0x04,0x04,0,0x04}; memcpy(o,v,7); break; }
    case '?': { static const unsigned char v[7]={0x0E,0x11,0x01,0x02,0x04,0,0x04}; memcpy(o,v,7); break; }
    case '-': { static const unsigned char v[7]={0,0,0,0x1F,0,0,0}; memcpy(o,v,7); break; }
    case '+': { static const unsigned char v[7]={0,0x04,0x04,0x1F,0x04,0x04,0}; memcpy(o,v,7); break; }
    case '/': { static const unsigned char v[7]={0x01,0x02,0x04,0x08,0x10,0x10,0x10}; memcpy(o,v,7); break; }
    case '\'': { static const unsigned char v[7]={0x04,0x04,0,0,0,0,0}; memcpy(o,v,7); break; }
    case '(': { static const unsigned char v[7]={0x02,0x04,0x08,0x08,0x08,0x04,0x02}; memcpy(o,v,7); break; }
    case ')': { static const unsigned char v[7]={0x08,0x04,0x02,0x02,0x02,0x04,0x08}; memcpy(o,v,7); break; }
    case '%': { static const unsigned char v[7]={0x19,0x1A,0x02,0x04,0x08,0x14,0x13}; memcpy(o,v,7); break; }
    case '_': { static const unsigned char v[7]={0,0,0,0,0,0,0x1F}; memcpy(o,v,7); break; }
    case '>': { static const unsigned char v[7]={0x08,0x04,0x02,0x01,0x02,0x04,0x08}; memcpy(o,v,7); break; }
    case '<': { static const unsigned char v[7]={0x02,0x04,0x08,0x10,0x08,0x04,0x02}; memcpy(o,v,7); break; }
    case '=': { static const unsigned char v[7]={0,0,0x1F,0,0x1F,0,0}; memcpy(o,v,7); break; }
    case '*': { static const unsigned char v[7]={0,0x04,0x15,0x0E,0x15,0x04,0}; memcpy(o,v,7); break; }
    default: { static const unsigned char v[7]={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(o,v,7); break; }
  }
}

static void draw_ch(int x, int y, char c, int sc, Uint8 r, Uint8 g, Uint8 b) {
  unsigned char rows[7];
  font_rows(c, rows);
  SDL_Rect rc = {0,0,(int)sc,(int)sc};
  SDL_SetRenderDrawColor(ren, r, g, b, 255);
  for (int j = 0; j < 7; j++) {
    for (int i = 0; i < 5; i++) {
      if (rows[j] & (0x10 >> i)) {
        rc.x = x + i * sc; rc.y = y + j * sc;
        SDL_RenderFillRect(ren, &rc);
      }
    }
  }
}
static void draw_text(int x, int y, const char *s, int sc, Uint8 r, Uint8 g, Uint8 b) {
  if (!s) return;
  int cx = x;
  while (*s) {
    if (*s == '\n') { y += 9 * sc; cx = x; s++; continue; }
    draw_ch(cx, y, *s, sc, r, g, b);
    cx += 6 * sc;
    s++;
  }
}
static int text_w(const char *s, int sc) { return (int)strlen(s ? s : "") * 6 * sc; }
static void draw_box(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, Uint8 a, bool fill) {
  SDL_Rect rc = {x, y, w, h};
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ren, r, g, b, a);
  if (fill) SDL_RenderFillRect(ren, &rc);
  else SDL_RenderDrawRect(ren, &rc);
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}
static bool btn(int x, int y, int w, int h, const char *label, bool hi) {
  draw_box(x, y, w, h, hi ? 90 : 42, hi ? 66 : 33, hi ? 30 : 18, 255, true);
  draw_box(x, y, w, h, 232, 161, 61, 255, false);
  int tw = text_w(label, 2);
  draw_text(x + (w - tw) / 2, y + (h - 18) / 2, label, 2, 255, 217, 138);
  if (in_mouse_pressed && in_mouse_x >= x && in_mouse_x < x + w && in_mouse_y >= y && in_mouse_y < y + h) {
    in_consume_click();
    sfx_click();
    return true;
  }
  return false;
}

/* ---------- camera ---------- */
static double tile_px(void) { return TILE_BASE * G.zoom; }

static void w2s(double wx, double wy, double *sx, double *sy) {
  double t = tile_px();
  if (G.view == 1) {
    double px = (G.p.rx - G.p.ry) * t * 0.5;
    double py = (G.p.rx + G.p.ry) * t * 0.25;
    double qx = (wx - wy) * t * 0.5;
    double qy = (wx + wy) * t * 0.25;
    *sx = SCR_W / 2 + (qx - px);
    *sy = SCR_H / 2 + (qy - py) - 40;
  } else {
    *sx = SCR_W / 2 + (wx - G.p.rx) * t;
    *sy = SCR_H / 2 + (wy - G.p.ry) * t;
  }
}

static void draw_spr(const char *name, double sx, double sy, double scale, double alpha) {
  SDL_Texture *t = spr_get(name);
  int dw = (int)(tile_px() * scale);
  int dh = (int)(tile_px() * scale);
  SDL_Rect d = {(int)(sx - dw / 2), (int)(sy - dh / 2), dw, dh};
  if (t) {
    SDL_SetTextureAlphaMod(t, (Uint8)(alpha * 255));
    SDL_RenderCopy(ren, t, NULL, &d);
    SDL_SetTextureAlphaMod(t, 255);
  } else {
    draw_box(d.x, d.y, d.w, d.h, 120, 90, 60, (Uint8)(alpha * 255), true);
    draw_text(d.x + d.w / 2 - 6, d.y + d.h / 2 - 8, "?", 2, 255, 255, 255);
  }
}

/* ---------- stati UI ---------- */
static int login_field = 0;
static int class_sel = 0;
static int merch_sel = 0;
static double edge_cd = 0;
static char osk_buf_name[MAX_NAME] = "Viandante";
static char osk_buf_room[MAX_ROOM] = "abisso";
static bool osk_inited = false;

static const char *rar_name(int r) { return r == 3 ? "LEG" : r == 2 ? "EPI" : r == 1 ? "RAR" : "COM"; }

static void draw_world(void) {
  double t = tile_px();
  int x0 = (int)(G.p.rx - SCR_W / t), x1 = (int)(G.p.rx + SCR_W / t) + 2;
  int y0 = (int)(G.p.ry - SCR_H / t), y1 = (int)(G.p.ry + SCR_H / t) + 2;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > G.map.w) x1 = G.map.w;
  if (y1 > G.map.h) y1 = G.map.h;

  /* shake */
  double shx = 0, shy = 0;
  if (G.shake > 0) {
    shx = sin(G.time * 70) * G.shake * 14;
    shy = cos(G.time * 55) * G.shake * 14;
  }

  for (int y = y0; y < y1; y++) {
    for (int x = x0; x < x1; x++) {
      if (!G.map.visited[y][x]) continue;
      uint8_t tl = G.map.tiles[y][x];
      double sx, sy;
      w2s(x + 0.5, y + 0.5, &sx, &sy);
      sx += shx; sy += shy;
      if (sx < -t * 2 || sy < -t * 2 || sx > SCR_W + t * 2 || sy > SCR_H + t * 2) continue;
      const char *sn = "floor_stone";
      if (tl == T_WALL) sn = ((x + y) % 2) ? "wall_stone" : "wall_brick";
      else if (tl == T_STAIRS) sn = "stairs";
      else sn = ((x + y) % 2) ? "floor_stone" : "floor_dirt";
      draw_spr(sn, sx, sy, G.view == 1 ? 1.0 : 1.05, 1.0);
      /* luce torce */
      double light = 0;
      {
        double dp = ab_dist(G.p.x, G.p.y, x + 0.5, y + 0.5);
        if (dp < 7) light = 1;
        for (int i = 0; i < G.torch_count; i++) {
          double dt2 = ab_dist(G.torches[i].tx + 0.5, G.torches[i].ty + 0.5, x + 0.5, y + 0.5);
          double fl = 4.5 + sin(G.torch_clk * 9 + i * 1.7) * 0.6;
          if (dt2 < fl) { light = 1; break; }
        }
      }
      if (!light) {
        int sxx = (int)(sx - t / 2), syy = (int)(sy - t / 2);
        draw_box(sxx, syy, (int)t + 1, (int)t + 1, 0, 0, 0, 170, true);
      }
    }
  }
  /* torce */
  for (int i = 0; i < G.torch_count; i++) {
    int x = G.torches[i].tx, y = G.torches[i].ty;
    if (!G.map.visited[y][x]) continue;
    double sx, sy;
    w2s(x + 0.5, y + 0.5, &sx, &sy);
    double fl = 1 + sin(G.torch_clk * 9 + i * 1.7) * 0.08;
    draw_spr("torch", sx, sy - 4, 0.8 * fl, 1.0);
  }
  /* mercante */
  {
    double sx, sy;
    w2s(G.map.merch_x + 0.5, G.map.merch_y + 0.5, &sx, &sy);
    draw_spr("merchant", sx + shx, sy + shy + sin(G.time * 2) * 2, 1.1, 1.0);
  }
  /* forzieri */
  for (int i = 0; i < G.chest_count; i++) {
    AbChest *c = &G.chests[i];
    if (!c->active || !G.map.visited[c->ty][c->tx]) continue;
    double sx, sy;
    w2s(c->tx + 0.5, c->ty + 0.5, &sx, &sy);
    draw_spr(c->open ? "chest_open" : "chest_closed", sx + shx, sy + shy, 0.95, 1.0);
  }
  /* entita ordinate per y */
  typedef struct { int kind; int idx; double depth; } DR;
  static DR order[200];
  int n = 0;
  for (int i = 0; i < MAX_MONSTERS && n < 200; i++)
    if (G.mons[i].active) {
      int tx = (int)G.mons[i].x, ty = (int)G.mons[i].y;
      if (tx < 0 || ty < 0 || tx >= G.map.w || ty >= G.map.h) continue;
      if (!G.map.visited[ty][tx]) continue;
      order[n].kind = 0; order[n].idx = i;
      order[n].depth = G.view == 1 ? G.mons[i].rx + G.mons[i].ry : G.mons[i].ry;
      n++;
    }
  /* insertion sort */
  for (int i = 1; i < n; i++) {
    DR k2 = order[i];
    int j = i - 1;
    while (j >= 0 && order[j].depth > k2.depth) { order[j+1] = order[j]; j--; }
    order[j+1] = k2;
  }
  /* ombre + mostri */
  for (int i = 0; i < n; i++) {
    AbMonster *m = &G.mons[order[i].idx];
    double sx, sy;
    w2s(m->rx, m->ry, &sx, &sy);
    sx += shx; sy += shy;
    double bob = sin(G.time * 3 + order[i].idx) * 2;
    /* ombra */
    draw_box((int)(sx - t * 0.3), (int)(sy + t * 0.32), (int)(t * 0.6), (int)(t * 0.14), 0, 0, 0, 120, true);
    char ks[2] = {m->type, 0};
    const AbMonDef *td = ab_mon_def(ks);
    const char *sn = td ? td->sprite : "mon_zombie";
    /* strip assets/sprites/ + .png -> nome */
    const char *base = strrchr(sn, '/');
    base = base ? base + 1 : sn;
    static char nb[48];
    strncpy(nb, base, 47);
    char *dot = strrchr(nb, '.');
    if (dot) *dot = 0;
    double sc = m->is_boss ? 1.7 : 1.0;
    if (m->affix == 1) bob += sin(G.time * 10) * 1.5;
    draw_spr(nb, sx, sy + bob - (m->is_boss ? 6 : 0), sc, 1.0);
    if (m->affix > 0) {
      const char *a = m->affix == 1 ? ">" : m->affix == 2 ? "*" : "+";
      draw_text((int)sx - 4, (int)sy - (int)t - 8, a, 2, 255, m->affix == 2 ? 100 : 220, 80);
    }
    /* hp mini */
    if (m->hp < m->max_hp) {
      int bw = (int)(t * 0.9);
      double f = (double)m->hp / (double)m->max_hp;
      draw_box((int)sx - bw / 2, (int)sy - (int)(t * 0.75), bw, 5, 0, 0, 0, 200, true);
      draw_box((int)sx - bw / 2, (int)sy - (int)(t * 0.75), (int)(bw * f), 5,
        m->is_boss ? 200 : 180, m->is_boss ? 40 : 60, 40, 255, true);
    }
  }
  /* giocatore */
  {
    double sx, sy;
    w2s(G.p.rx, G.p.ry, &sx, &sy);
    sx += shx; sy += shy;
    if (!G.p.dead) {
      double bob = fabs(sin(G.time * 6)) * 2;
      if (G.p.downed) bob = 0;
      draw_box((int)(sx - t * 0.3), (int)(sy + t * 0.32), (int)(t * 0.6), (int)(t * 0.14), 0, 0, 0, 130, true);
      const AbClassDef *c = ab_class_def(G.p.cls);
      const char *sn = c->sprite;
      const char *base = strrchr(sn, '/');
      base = base ? base + 1 : sn;
      static char nb[48];
      strncpy(nb, base, 47);
      char *dot = strrchr(nb, '.');
      if (dot) *dot = 0;
      double sc = 1.15;
      if (G.p.cls == CLS_PROF) sc = 1.25;
      Uint8 alpha = 255;
      if (G.p.iframes > 0 && ((int)(G.time * 12) % 2 == 0)) alpha = 120;
      SDL_Texture *tt = spr_get(nb);
      int dw = (int)(t * sc), dh = (int)(t * sc);
      SDL_Rect d = {(int)(sx - dw / 2), (int)(sy - dh / 2 + bob) - 4, dw, dh};
      if (tt) {
        SDL_SetTextureAlphaMod(tt, alpha);
        /* flip verso facing */
        SDL_RenderCopyEx(ren, tt, NULL, &d, G.p.anim_t > 0 ? 12 * G.p.fx : 0, NULL,
          G.p.fx < -0.05 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(tt, 255);
      } else {
        draw_box(d.x, d.y, d.w, d.h, 200, 170, 90, alpha, true);
      }
      /* fendente attacco */
      if (G.p.anim_t > 0) {
        double ax = sx + G.p.fx * t * 0.7, ay = sy + G.p.fy * t * 0.7;
        draw_box((int)ax - 8, (int)ay - 3, 16, 6, 255, 240, 200, 200, true);
      }
      if (G.p.downed) draw_text((int)sx - 30, (int)sy - 40, "AIUTO!", 2, 255, 80, 80);
    }
  }
  /* proiettili */
  for (int i = 0; i < MAX_PROJS; i++) {
    if (!G.projs[i].active) continue;
    double sx, sy;
    w2s(G.projs[i].x, G.projs[i].y, &sx, &sy);
    draw_box((int)(sx + shx) - 4, (int)(sy + shy) - 4, 8, 8,
      (Uint8)(G.projs[i].r * 255), (Uint8)(G.projs[i].g * 255), (Uint8)(G.projs[i].b * 255), 255, true);
  }
  /* particelle */
  for (int i = 0; i < MAX_PARTS; i++) {
    if (!G.parts[i].active) continue;
    double sx, sy;
    w2s(G.parts[i].x, G.parts[i].y, &sx, &sy);
    double f = G.parts[i].life / G.parts[i].max_life;
    int s = (int)(2 + 4 * f);
    draw_box((int)(sx + shx) - s / 2, (int)(sy + shy) - s / 2, s, s,
      (Uint8)(G.parts[i].r * 255), (Uint8)(G.parts[i].g * 255), (Uint8)(G.parts[i].b * 255), 255, true);
  }
  /* float */
  for (int i = 0; i < MAX_FLOATS; i++) {
    if (!G.floats[i].active) continue;
    double sx, sy;
    w2s(G.floats[i].x, G.floats[i].y, &sx, &sy);
    draw_text((int)(sx + shx) - 10, (int)(sy + shy), G.floats[i].text, 2,
      (Uint8)(G.floats[i].r * 255), (Uint8)(G.floats[i].g * 255), (Uint8)(G.floats[i].b * 255));
  }
  /* vignetta low-hp */
  if (!G.p.dead && G.p.max_hp > 0 && (double)G.p.hp / G.p.max_hp < 0.3) {
    double p = 0.5 + 0.5 * sin(G.torch_clk * 6);
    draw_box(0, 0, SCR_W, 26, 150, 10, 10, (Uint8)(60 + 60 * p), true);
    draw_box(0, SCR_H - 26, SCR_W, 26, 150, 10, 10, (Uint8)(60 + 60 * p), true);
  }
}

static void draw_hud(void) {
  Uint8 tr = G.c64 ? 123 : 232, tg = G.c64 ? 113 : 220, tb = G.c64 ? 213 : 197;
  /* pannello eroe */
  draw_box(10, 10, 250, 86, 12, 10, 7, 220, true);
  draw_box(10, 10, 250, 86, 232, 161, 61, 255, false);
  const AbClassDef *c = ab_class_def(G.p.cls);
  draw_text(20, 16, c->name, 2, tr, tg, tb);
  char b[64];
  snprintf(b, sizeof b, "LV%d PIANO %d", G.p.level, G.depth);
  draw_text(20, 36, b, 2, 212, 175, 55);
  /* hp/mp bar */
  draw_box(20, 56, 150, 10, 0, 0, 0, 255, true);
  double hf = G.p.max_hp ? (double)G.p.hp / G.p.max_hp : 0;
  draw_box(20, 56, (int)(150 * hf), 10, 193, 68, 58, 255, true);
  snprintf(b, sizeof b, "HP %d/%d", G.p.hp, G.p.max_hp);
  draw_text(176, 54, b, 2, 255, 255, 255);
  if (G.p.max_mp > 0) {
    draw_box(20, 70, 150, 8, 0, 0, 0, 255, true);
    double mf = (double)G.p.mp / G.p.max_mp;
    draw_box(20, 70, (int)(150 * mf), 8, 95, 160, 201, 255, true);
    snprintf(b, sizeof b, "MP %d/%d", G.p.mp, G.p.max_mp);
    draw_text(176, 68, b, 2, 160, 200, 255);
  }
  /* oro/pozioni */
  snprintf(b, sizeof b, "ORO %d  HP+%d MANA+%d", G.p.gold, G.p.potions, G.p.mana_potions);
  draw_text(20, 100, b, 2, 212, 175, 55);
  /* offline badge */
  draw_text(SCR_W - 130, 14, "OFFLINE", 2, 150, 150, 150);
  snprintf(b, sizeof b, "STANZA %s", G.room);
  draw_text(SCR_W - 250, 34, b, 2, 170, 255, 238);
  if (G.best_depth > 0) {
    snprintf(b, sizeof b, "RECORD P%d", G.best_depth);
    draw_text(SCR_W - 250, 54, b, 2, 232, 161, 61);
  }
  /* buff */
  int by = 122;
  if (G.p.buff_rage_t > 0) { draw_text(20, by, "FURIA", 2, 255, 150, 60); by += 18; }
  if (G.p.buff_shield_t > 0) { draw_text(20, by, "SCUDO", 2, 255, 220, 140); by += 18; }
  if (G.p.ability_cd > 0) {
    snprintf(b, sizeof b, "ABIL %.0f", G.p.ability_cd);
    draw_text(20, by, b, 2, 127, 174, 99);
  }
  /* boss bar */
  bool boss_alive = false;
  for (int i = 0; i < MAX_MONSTERS; i++)
    if (G.mons[i].active && G.mons[i].is_boss) boss_alive = true;
  if (boss_alive || G.boss_active) {
    draw_box(SCR_W / 2 - 170, 12, 340, 30, 12, 10, 7, 220, true);
    draw_box(SCR_W / 2 - 170, 12, 340, 30, 232, 120, 40, 255, false);
    draw_text(SCR_W / 2 - 160, 15, G.boss_name, 2, 255, 210, 122);
    double f = G.boss_max ? (double)G.boss_hp / G.boss_max : 0;
    draw_box(SCR_W / 2 - 160, 32, 320, 8, 0, 0, 0, 255, true);
    draw_box(SCR_W / 2 - 160, 32, (int)(320 * f), 8, 200, 60, 40, 255, true);
  }
  /* log */
  int ly = SCR_H - 110;
  for (int k = 0; k < 4; k++) {
    int idx = (G.log_head - 1 - k + MAX_LOGLINES * 4) % MAX_LOGLINES;
    if (G.log_t[idx] <= 0) continue;
    const char *s = G.loglines[idx];
    int tw = text_w(s, 1);
    if (tw > SCR_W - 40) tw = SCR_W - 40;
    draw_text(SCR_W / 2 - tw / 2, ly - k * 16, s, 1, 232, 220, 197);
  }
  /* toast */
  if (G.toast_t > 0) {
    int tw = text_w(G.toast, 3);
    draw_text(SCR_W / 2 - tw / 2, SCR_H / 2 - 120, G.toast, 3, 255, 200, 110);
  }
  if (G.loot_t > 0) {
    int tw = text_w(G.loot, 3);
    draw_text(SCR_W / 2 - tw / 2, SCR_H / 2 - 80, G.loot, 3, 255, 230, 150);
  }
  /* zoom/mute/view hint */
  char h[96];
  snprintf(h, sizeof h, "Z %.1f %s %s", G.zoom, G.view ? "ISO" : "TOP", G.mute ? "MUTE" : "SND");
  draw_text(SCR_W - 250, SCR_H - 26, h, 2, 160, 150, 130);
  /* controlli Vita */
  draw_text(12, SCR_H - 26, "X ATK O USA Q XYZ R MANA R1 ABIL", 1, 140, 130, 115);
}

static void draw_minimap(void) {
  if (!G.minimap) return;
  int mw = 200, mh = 150;
  int x0 = SCR_W - mw - 12, y0 = 80;
  draw_box(x0 - 4, y0 - 4, mw + 8, mh + 8, 8, 6, 4, 230, true);
  draw_box(x0 - 4, y0 - 4, mw + 8, mh + 8, 232, 161, 61, 255, false);
  double sx = (double)mw / G.map.w, sy = (double)mh / G.map.h;
  double s = sx < sy ? sx : sy;
  for (int y = 0; y < G.map.h; y++) {
    for (int x = 0; x < G.map.w; x++) {
      if (!G.map.visited[y][x]) continue;
      int px = x0 + (int)(x * s), py = y0 + (int)(y * s);
      Uint8 r = 60, g = 48, b = 36;
      if (G.map.tiles[y][x] == T_WALL) { r = 30; g = 24; b = 18; }
      else if (G.map.tiles[y][x] == T_STAIRS) { r = 255; g = 210; b = 100; }
      SDL_Rect rc = {px, py, (int)s + 1, (int)s + 1};
      SDL_SetRenderDrawColor(ren, r, g, b, 255);
      SDL_RenderFillRect(ren, &rc);
    }
  }
  /* player */
  int px = x0 + (int)(G.p.x * s) - 2, py = y0 + (int)(G.p.y * s) - 2;
  draw_box(px, py, 5, 5, 255, 255, 255, 255, true);
  /* boss */
  for (int i = 0; i < MAX_MONSTERS; i++) {
    if (!G.mons[i].active || !G.mons[i].is_boss) continue;
    int bx = x0 + (int)(G.mons[i].x * s) - 2, by = y0 + (int)(G.mons[i].y * s) - 2;
    draw_box(bx, by, 5, 5, 255, 60, 40, 255, true);
  }
  /* scale */
  int stx = x0 + (int)(G.map.stairs_x * s) - 2, sty = y0 + (int)(G.map.stairs_y * s) - 2;
  draw_box(stx, sty, 5, 5, 255, 220, 120, 255, true);
}

static void draw_help(void) {
  if (!G.help) return;
  draw_box(90, 60, SCR_W - 180, SCR_H - 120, 10, 8, 5, 245, true);
  draw_box(90, 60, SCR_W - 180, SCR_H - 120, 232, 161, 61, 255, false);
  draw_text(120, 80, "ABISSO - AIUTO (VITA)", 2, 232, 161, 61);
  const char *lines[] = {
    "STICK/D-PAD MUOVI   X ATTACCO",
    "O INTERAGISCI  QUAD POZIONE HP",
    "TRIANG POZIONE MANA  R1 ABILITA",
    "L1 VISTA  START MAPPA  SELECT AIUTO",
    "TOUCH/MOUSE NEI MENU",
    "SCENDI LE SCALE, APRI FORZIERI,",
    "COMPRA DAL MERCANTE, UCCIDI I 6 BOSS",
    "OGNI 5 PIANI. STANZA 64 = MOD C64.",
    "MULTI P2P NON DISPONIBILE SU VITA:",
    "STESSO CODICE = STESSO DUNGEON WEB.",
    NULL
  };
  int y = 120;
  for (int i = 0; lines[i]; i++) { draw_text(120, y, lines[i], 2, 232, 220, 197); y += 22; }
  if (btn(SCR_W / 2 - 90, SCR_H - 120, 180, 40, "CHIUDI", true)) G.help = false;
}

static void draw_merchant(void) {
  if (!G.merchant_open) return;
  draw_box(SCR_W / 2 - 170, SCR_H / 2 - 150, 340, 300, 14, 10, 6, 245, true);
  draw_box(SCR_W / 2 - 170, SCR_H / 2 - 150, 340, 300, 232, 161, 61, 255, false);
  draw_text(SCR_W / 2 - 80, SCR_H / 2 - 136, "MERCANTE", 3, 232, 161, 61);
  char b[64];
  snprintf(b, sizeof b, "ORO: %d", G.p.gold);
  draw_text(SCR_W / 2 - 60, SCR_H / 2 - 104, b, 2, 212, 175, 55);
  const char *items[5] = {"POZIONE HP 10G","POZIONE MANA 8G","ANELLO DANNO 30G","VITA +3 30G","BOTTINO 50G"};
  for (int i = 0; i < 5; i++) {
    int y = SCR_H / 2 - 76 + i * 34;
    bool hi = (i == merch_sel);
    draw_box(SCR_W / 2 - 150, y, 300, 28, hi ? 90 : 40, hi ? 60 : 30, hi ? 25 : 15, 255, true);
    draw_text(SCR_W / 2 - 140, y + 6, items[i], 2, hi ? 255 : 220, hi ? 220 : 200, hi ? 150 : 170);
  }
  if (btn(SCR_W / 2 - 90, SCR_H / 2 + 104, 180, 34, "ESCI [O]", true)) G.merchant_open = false;
}

static void draw_downed(void) {
  if (!G.p.downed) return;
  char b[64];
  snprintf(b, sizeof b, "A TERRA! %.0F - BEVI Q!", G.p.downed_t);
  int tw = text_w(b, 3);
  draw_text(SCR_W / 2 - tw / 2, SCR_H - 160, b, 3, 255, 90, 90);
}

static void login_draw(unsigned keys) {
  (void)keys;
  if (!osk_inited) {
    strncpy(osk_buf_name, G.p.name, MAX_NAME - 1);
    strncpy(osk_buf_room, G.room, MAX_ROOM - 1);
    osk_inited = true;
  }
  /* PC: digitazione diretta */
  if (in_text_ready && in_text[0]) {
    char *dst = login_field == 0 ? osk_buf_name : osk_buf_room;
    size_t mx = login_field == 0 ? MAX_NAME - 1 : MAX_ROOM - 1;
    size_t l = strlen(dst);
    size_t a = strlen(in_text);
    if (l + a < mx) strcat(dst, in_text);
    in_consume_text();
  }
  if (in_backspace) {
    char *dst = login_field == 0 ? osk_buf_name : osk_buf_room;
    size_t l = strlen(dst);
    if (l) dst[l-1] = 0;
    in_consume_text();
  }
  draw_box(0, 0, SCR_W, SCR_H, G.c64 ? 64 : 12, G.c64 ? 49 : 10, G.c64 ? 141 : 7, 255, true);
  draw_text(SCR_W / 2 - 110, 60, "ABISSO", 6, 232, 161, 61);
  draw_text(SCR_W / 2 - 150, 120, "ROGUELIKE - PORT PSVITA 1:1", 2, 156, 142, 119);
  draw_text(SCR_W / 2 - 130, 150, "SINGLE-PLAYER OFFLINE", 2, 170, 255, 238);

  /* campi */
  draw_text(230, 200, "NOME:", 2, 200, 190, 170);
  draw_box(330, 194, 300, 30, login_field == 0 ? 70 : 25, login_field == 0 ? 50 : 20, 20, 255, true);
  draw_box(330, 194, 300, 30, 232, 161, 61, 255, false);
  draw_text(340, 200, osk_buf_name, 2, 255, 255, 255);
  draw_text(230, 240, "STANZA:", 2, 200, 190, 170);
  draw_box(330, 234, 300, 30, login_field == 1 ? 70 : 25, login_field == 1 ? 50 : 20, 20, 255, true);
  draw_box(330, 234, 300, 30, 232, 161, 61, 255, false);
  draw_text(340, 240, osk_buf_room, 2, 170, 255, 238);
  draw_text(230, 270, "USA 64/C64 PER MOD C64. STESSO", 1, 150, 140, 125);
  draw_text(230, 284, "CODICE = STESSO DUNGEON DELLA WEB.", 1, 150, 140, 125);

  if (btn(230, 310, 180, 40, "NOME", login_field == 0)) login_field = 0;
  if (btn(430, 310, 200, 40, "STANZA", login_field == 1)) login_field = 1;

  /* OSK lettere */
  const char *rows[] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789_+-", NULL};
  int oy = 370;
  for (int r = 0; rows[r]; r++) {
    int ox = 230;
    for (const char *p = rows[r]; *p; p++) {
      char label[2] = {*p, 0};
      SDL_Rect rc = {ox, oy, 34, 30};
      bool hov = in_mouse_x >= rc.x && in_mouse_x < rc.x + rc.w && in_mouse_y >= rc.y && in_mouse_y < rc.y + rc.h;
      draw_box(rc.x, rc.y, rc.w, rc.h, hov ? 90 : 35, hov ? 60 : 25, 20, 255, true);
      draw_text(rc.x + 9, rc.y + 6, label, 2, 255, 230, 180);
      if (in_mouse_pressed && hov) {
        in_consume_click();
        char *dst = login_field == 0 ? osk_buf_name : osk_buf_room;
        size_t mx = login_field == 0 ? MAX_NAME - 1 : MAX_ROOM - 1;
        if (strlen(dst) + 1 < mx) {
          size_t l = strlen(dst);
          dst[l] = *p; dst[l+1] = 0;
        }
        sfx_click();
      }
      ox += 38;
    }
    oy += 34;
  }
  if (btn(650, 370, 80, 64, "DEL", false)) {
    char *dst = login_field == 0 ? osk_buf_name : osk_buf_room;
    size_t l = strlen(dst);
    if (l) dst[l-1] = 0;
  }
  if (in_enter) {
    in_consume_text();
    G.state = ST_CLASS;
    sfx_click();
  }
  if (btn(330, 508, 300, 36, "AVANTI > SCEGLI EROE", true)) {
    G.state = ST_CLASS;
  }
  draw_text(230, 150 + 398, "X/INVIO AVANTI - TOUCH PER SCRIVERE", 1, 130, 120, 110);
}

static void class_draw(unsigned keys) {
  (void)keys;
  draw_box(0, 0, SCR_W, SCR_H, 12, 10, 7, 255, true);
  draw_text(SCR_W / 2 - 170, 20, "SCEGLI IL TUO EROE (9)", 3, 232, 161, 61);
  int cols = 3;
  int cw = 250, chh = 120;
  int gx = SCR_W / 2 - (cols * (cw + 16)) / 2;
  int gy = 70;
  for (int i = 0; i < CLS_COUNT; i++) {
    int cx = gx + (i % cols) * (cw + 16);
    int cy = gy + (i / cols) * (chh + 12);
    const AbClassDef *c = ab_class_def(i);
    bool hi = (i == class_sel);
    draw_box(cx, cy, cw, chh, hi ? 70 : 25, hi ? 50 : 20, hi ? 25 : 15, 255, true);
    draw_box(cx, cy, cw, chh, hi ? 255 : 150, hi ? 200 : 120, 60, 255, false);
    /* sprite */
    const char *base = strrchr(c->sprite, '/');
    base = base ? base + 1 : c->sprite;
    static char nb[48];
    strncpy(nb, base, 47);
    char *dot = strrchr(nb, '.');
    if (dot) *dot = 0;
    SDL_Texture *t = spr_get(nb);
    if (t) {
      SDL_Rect d = {cx + 10, cy + 14, 64, 64};
      SDL_RenderCopy(ren, t, NULL, &d);
    }
    draw_text(cx + 84, cy + 10, c->name, 2, 255, 230, 180);
    char b[64];
    snprintf(b, sizeof b, "HP%d %s", c->hp, c->ranged ? "RANGED" : "MELEE");
    draw_text(cx + 84, cy + 32, b, 1, 200, 190, 170);
    snprintf(b, sizeof b, "DMG%d-%d VEL%.1F", c->dmg_min, c->dmg_max, c->speed);
    draw_text(cx + 84, cy + 48, b, 1, 200, 190, 170);
    draw_text(cx + 10, cy + 84, c->ability_name, 1, 127, 200, 120);
    if (in_mouse_pressed && in_mouse_x >= cx && in_mouse_x < cx + cw && in_mouse_y >= cy && in_mouse_y < cy + chh) {
      in_consume_click();
      class_sel = i;
      sfx_click();
    }
  }
  const AbClassDef *c = ab_class_def(class_sel);
  draw_text(60, 470, c->desc, 2, 180, 170, 155);
  if (btn(SCR_W / 2 - 150, 496, 300, 36, "SCENDI NELL'ABISSO", true) || in_enter) {
    in_consume_text();
    ab_new_run(osk_buf_name, class_sel, osk_buf_room);
    sfx_stairs();
  }
  draw_text(60, 500, "D-PAD SCEGLI - X CONFERMA", 1, 130, 120, 110);
}

static void dead_draw(void) {
  draw_box(0, 0, SCR_W, SCR_H, 20, 5, 5, 255, true);
  draw_text(SCR_W / 2 - 140, 150, "SEI MORTO", 6, 193, 68, 58);
  char b[96];
  snprintf(b, sizeof b, "%s - PIANO %d - ORO PERSO", G.p.name, G.depth);
  draw_text(SCR_W / 2 - text_w(b, 2) / 2, 240, b, 2, 200, 180, 160);
  if (G.best_depth > 0) {
    snprintf(b, sizeof b, "RECORD: PIANO %d", G.best_depth);
    draw_text(SCR_W / 2 - text_w(b, 2) / 2, 270, b, 2, 232, 161, 61);
  }
  draw_text(SCR_W / 2 - 200, 310, "PERMADEATH COME NELLA WEB.", 2, 150, 140, 130);
  if (btn(SCR_W / 2 - 150, 360, 300, 44, "TORNA AL TITOLO", true) || in_enter) {
    in_consume_text();
    G.state = ST_LOGIN;
    osk_inited = false;
  }
  if (btn(SCR_W / 2 - 150, 412, 300, 44, "RIPROVA STESSA STANZA", false)) {
    ab_new_run(osk_buf_name, class_sel, osk_buf_room);
  }
}

/* edge detection per toggle */
static unsigned prev_keys = 0;

bool ren_init(void) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) return false;
  win = SDL_CreateWindow("ABISSO", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCR_W, SCR_H, SDL_WINDOW_SHOWN);
  if (!win) return false;
  ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  if (!ren) return false;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  int imgf = IMG_INIT_PNG;
  if ((IMG_Init(imgf) & imgf) != imgf) {
    /* continua comunque con fallback */
  }
  sprites_init();
  return true;
}

void ren_quit(void) {
  for (int i = 0; i < nspr; i++) if (sprs[i].tex) SDL_DestroyTexture(sprs[i].tex);
  IMG_Quit();
  if (ren) SDL_DestroyRenderer(ren);
  if (win) SDL_DestroyWindow(win);
  ren = NULL; win = NULL;
}

static void scanlines_c64(void) {
  if (!G.c64) return;
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 40);
  for (int y = 0; y < SCR_H; y += 3) {
    SDL_RenderDrawLine(ren, 0, y, SCR_W, y);
  }
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

void ren_frame(unsigned keys) {
  unsigned pressed = keys & ~prev_keys;
  prev_keys = keys;

  /* toggle globali */
  if (pressed & K_MUTE) { G.mute = !G.mute; ab_save_record(); }
  if (G.state == ST_GAME) {
    if (pressed & K_MAP) { G.minimap = !G.minimap; sfx_click(); }
    if (pressed & K_VIEW) { G.view = (G.view + 1) % 2; ab_toast(G.view ? "VISTA ISOMETRICA" : "VISTA DALL'ALTO"); sfx_click(); }
    if (pressed & K_HELP) { G.help = !G.help; sfx_click(); }
    if (pressed & K_POT) { ab_drink_potion(); sfx_potion(); }
    if (pressed & K_MANA) { ab_drink_mana(); sfx_potion(); }
    if (pressed & K_ABIL) { ab_use_ability(); sfx_ability(); }
    if (pressed & K_INTER) {
      /* debounce: gestito dal main? qui diretto */
      static double last_inter = -1;
      if (G.time - last_inter > 0.25) {
        last_inter = G.time;
        /* se mercante aperto, E lo chiude */
        if (G.merchant_open) G.merchant_open = false;
        else ab_try_interact();
        sfx_click();
      }
    }
    /* zoom con select+su/giu? usa pausa+...: semplifica tasti 1/2 PC */
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    if (kb[SDL_SCANCODE_1]) { G.zoom -= 0.02; if (G.zoom < 0.6) G.zoom = 0.6; }
    if (kb[SDL_SCANCODE_2]) { G.zoom += 0.02; if (G.zoom > 2.0) G.zoom = 2.0; }
    /* merchant nav */
    if (G.merchant_open) {
      /* su/giu per selezionare, atk per comprare */
      static double last_nav = 0;
      if (G.time - last_nav > 0.18) {
        if (keys & K_UP) { merch_sel = (merch_sel + 4) % 5; last_nav = G.time; sfx_click(); }
        else if (keys & K_DOWN) { merch_sel = (merch_sel + 1) % 5; last_nav = G.time; sfx_click(); }
        else if (pressed & K_ATK) {
          if (ab_merchant_buy(merch_sel)) sfx_pickup();
          last_nav = G.time;
        }
      }
      /* click diretto */
      if (in_mouse_pressed) {
        for (int i = 0; i < 5; i++) {
          int y = SCR_H / 2 - 76 + i * 34;
          if (in_mouse_x >= SCR_W / 2 - 150 && in_mouse_x < SCR_W / 2 + 150 && in_mouse_y >= y && in_mouse_y < y + 28) {
            in_consume_click();
            merch_sel = i;
            if (ab_merchant_buy(i)) sfx_pickup();
            break;
          }
        }
      }
    }
    /* login rapido: L1+R1? no */
  }
  if (G.state == ST_LOGIN) {
    /* nav tastiera/controller */
    if (pressed & (K_UP | K_DOWN)) { login_field = 1 - login_field; sfx_click(); }
    if (pressed & K_ATK) { G.state = ST_CLASS; sfx_click(); }
  } else if (G.state == ST_CLASS) {
    if (edge_cd > 0) edge_cd -= 0.016;
    if (pressed & K_LEFT) { class_sel = (class_sel + CLS_COUNT - 1) % CLS_COUNT; sfx_click(); }
    if (pressed & K_RIGHT) { class_sel = (class_sel + 1) % CLS_COUNT; sfx_click(); }
    if (pressed & K_UP) { class_sel = (class_sel + CLS_COUNT - 3) % CLS_COUNT; sfx_click(); }
    if (pressed & K_DOWN) { class_sel = (class_sel + 3) % CLS_COUNT; sfx_click(); }
    if (pressed & K_ATK) {
      ab_new_run(osk_buf_name, class_sel, osk_buf_room);
      sfx_stairs();
    }
    if (pressed & K_PAUSE) { G.state = ST_LOGIN; }
  } else if (G.state == ST_DEAD) {
    if (pressed & K_ATK) { G.state = ST_LOGIN; osk_inited = false; }
  }

  /* ---- draw ---- */
  if (G.c64) SDL_SetRenderDrawColor(ren, 64, 49, 141, 255);
  else SDL_SetRenderDrawColor(ren, 10, 9, 6, 255);
  SDL_RenderClear(ren);

  if (G.state == ST_LOGIN) login_draw(keys);
  else if (G.state == ST_CLASS) class_draw(keys);
  else if (G.state == ST_DEAD) dead_draw();
  else {
    draw_world();
    draw_hud();
    draw_minimap();
    draw_merchant();
    draw_help();
    draw_downed();
    /* touch buttons Vita (solo hint visivo, input via controller) */
    if (G.merchant_open) { /* sopra */ }
  }
  scanlines_c64();
  SDL_RenderPresent(ren);
  edge_cd = 0;
}
