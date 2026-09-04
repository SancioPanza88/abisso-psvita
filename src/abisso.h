/* ABISSO - Port PSVita (SDL2) - header condiviso logica di gioco.
 * Port 1:1 della versione web (index.html): stessi semi RNG, stesse classi,
 * stessi mostri/boss, stessa generazione dungeon, stessi controlli rimappati.
 * Single-player offline completo. Multiplayer P2P WebRTC non disponibile su
 * PSVita (manca lo stack di sistema): il room-code resta seed-compatibile
 * con la versione web (stesso codice = stesso dungeon).
 */
#ifndef ABISSO_H
#define ABISSO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ABISSO_VERSION "1.0.0"
#define ABISSO_TITLEID "ABSS00001"

#define SCR_W 960
#define SCR_H 544

#define MAP_MAX_W 64
#define MAP_MAX_H 64

#define T_WALL 0
#define T_FLOOR 1
#define T_STAIRS 2

#define MAX_MONSTERS 80
#define MAX_PROJS 96
#define MAX_PARTS 320
#define MAX_FLOATS 32
#define MAX_CHESTS 12
#define MAX_TORCHES 64
#define MAX_LOGLINES 8
#define MAX_NAME 24
#define MAX_ROOM 24

#define TILE_BASE 32

/* ---------- RNG identico al JS: FNV-1a + mulberry32 ---------- */
uint32_t ab_hash_str(const char *s);
typedef struct { uint32_t a; } AbRng;
void ab_rng_seed(AbRng *r, uint32_t seed);
double ab_rng_next(AbRng *r);              /* [0,1) */
int ab_rng_range(AbRng *r, int lo, int hi); /* [lo,hi] */
double ab_rng_frange(AbRng *r, double lo, double hi);
int ab_scaled_stat(int base, int depth, double factor);

/* ---------- Classi (9, valori copiati da index.html) ---------- */
typedef enum {
  CLS_GUERRIERO = 0, CLS_LADRO, CLS_MAGO, CLS_RANGER,
  CLS_PALADINO, CLS_NEGROMANTE, CLS_BARDO, CLS_MONACO, CLS_PROF,
  CLS_COUNT
} AbClassId;

typedef struct {
  const char *key;
  const char *name;
  const char *ability_name;
  const char *desc;
  int hp;
  int max_mp;
  int mana_cost;
  double speed;        /* tile/sec */
  double atk_cooldown; /* sec */
  double range;        /* tile */
  double arc;          /* radianti semi-ampiezza melee */
  int dmg_min, dmg_max;
  bool ranged;
  double crit;         /* 0..1 */
  double proj_speed;
  double ability_cd;
  int ability_mana;
  const char *sprite;  /* path asset */
} AbClassDef;

const AbClassDef *ab_class_def(int id);
int ab_class_from_key(const char *key);
extern const char *AB_CLASS_KEYS[CLS_COUNT];

/* ---------- Mostri ---------- */
typedef struct {
  const char *key;      /* 'r','b',... 'D','X','L','M','R','K' boss */
  const char *name;
  int hp, dmg;
  double speed, aggro;
  int gold_min, gold_max, xp;
  bool boss;
  bool erratic, poison, lifesteal, split, dash;
  const char *sprite;
} AbMonDef;

const AbMonDef *ab_mon_def(const char *key);
const AbMonDef *ab_mon_def_by_idx(int i);
int ab_mon_def_count(void);
bool ab_is_boss_floor(int depth);
char ab_boss_for_depth(int depth); /* 'D','X','L','M','R','K' */
const char *ab_boss_name(char b);

/* ---------- Entita ---------- */
typedef struct {
  bool active;
  char type; /* chiave AbMonDef */
  double x, y;       /* pos logica tile (float) */
  double rx, ry;     /* pos render (smoothing) */
  int hp, max_hp, dmg;
  double speed, aggro;
  double facing_x, facing_y;
  double atk_cd, wander_t, wx, wy;
  double dot_t, dot_acc;   /* veleno */
  double regen_acc;
  int affix; /* 0 nessuno,1 veloce,2 esplosivo,3 rigenerante */
  bool is_boss;
  /* boss state */
  double boss_t, spec_cd;
  int boss_phase;
  double tx, ty; /* telegraph target */
} AbMonster;

typedef struct {
  bool active;
  double x, y, vx, vy;
  double life, range_left;
  int dmg;
  bool friendly;
  double r, g, b;
  double size;
} AbProj;

typedef struct {
  bool active;
  double x, y, vx, vy;
  double life, max_life;
  double r, g, b, size;
} AbPart;

typedef struct {
  bool active;
  double x, y;
  double life;
  char text[48];
  double r, g, b;
} AbFloat;

typedef struct {
  bool active;
  int tx, ty;
  bool open;
  int gold;
  int kind; /* 0 oro,1 pozione,2 mana,3 equip */
  int rarity; /* 0 comune,1 raro,2 epico,3 leggendario */
} AbChest;

typedef struct {
  int tx, ty;
} AbTorch;

/* ---------- Equip ---------- */
typedef struct {
  bool filled;
  char name[32];
  int rarity;
  int hp, dmg;
  double speed;
} AbEquip; /* 5 slot: elmo, collana, armatura, anello, gambali */

/* ---------- Giocatore ---------- */
typedef struct {
  double x, y, rx, ry;
  double fx, fy; /* facing */
  int cls;
  int hp, max_hp, mp, max_mp;
  double speed;
  int gold, potions, mana_potions, xp, level;
  double atk_cd, ability_cd;
  double buff_rage_t, buff_shield_t, buff_haste_t;
  double anim_t; /* attacco */
  double iframes;
  double charge_t; /* prof colpo caricato */
  bool dead, downed;
  double downed_t;
  AbEquip equip[5];
  char name[MAX_NAME];
} AbPlayer;

/* ---------- Mappa ---------- */
typedef struct {
  int w, h;
  uint8_t tiles[MAP_MAX_H][MAP_MAX_W];
  uint8_t visited[MAP_MAX_H][MAP_MAX_W];
  int spawn_x, spawn_y;
  int stairs_x, stairs_y;
  int merch_x, merch_y;
  int arena_cx, arena_cy, arena_w, arena_h;
  bool has_arena;
} AbMap;

/* ---------- Stato gioco ---------- */
typedef enum {
  ST_LOGIN = 0,
  ST_CLASS,
  ST_GAME,
  ST_DEAD
} AbAppState;

typedef struct {
  AbAppState state;
  AbPlayer p;
  AbMap map;
  AbMonster mons[MAX_MONSTERS];
  AbProj projs[MAX_PROJS];
  AbPart parts[MAX_PARTS];
  AbFloat floats[MAX_FLOATS];
  AbChest chests[MAX_CHESTS];
  AbTorch torches[MAX_TORCHES];
  int torch_count, chest_count;
  int depth;
  uint32_t world_seed;
  char room[MAX_ROOM];
  bool c64;
  int view; /* 0 topdown, 1 isometric */
  double time;
  double shake;
  double torch_clk;
  char toast[128];
  double toast_t;
  char loglines[MAX_LOGLINES][96];
  double log_t[MAX_LOGLINES];
  int log_head;
  char loot[96];
  double loot_t;
  int loot_rarity;
  char boss_name[48];
  int boss_hp, boss_max;
  bool boss_active;
  bool minimap, help, merchant_open;
  bool mute;
  double zoom;
  /* record */
  int best_depth, best_gold;
  /* debug */
  bool god, speed5;
  /* boss arena lock */
  bool arena_locked;
} AbGame;

extern AbGame G;

/* ---------- API gioco ---------- */
void ab_game_init(void);
void ab_new_run(const char *name, int cls, const char *room);
void ab_gen_depth(int depth);
void ab_update(double dt, unsigned keys);
void ab_try_interact(void);
void ab_drink_potion(void);
void ab_drink_mana(void);
void ab_use_ability(void);
void ab_player_attack(void);
void ab_descend(void);
void ab_add_log(const char *s);
void ab_toast(const char *s);
void ab_loot_banner(const char *s, int rarity);
void ab_float_text(double x, double y, const char *s, double r, double g, double b);
void ab_burst(double x, double y, int n, double r, double g, double b, double spd);
bool ab_is_walkable(int tx, int ty);
void ab_save_record(void);
void ab_load_record(void);
bool ab_merchant_buy(int idx);

/* tasti logici (bitmask) condivisa con input/render */
enum {
  K_UP = 1u << 0, K_DOWN = 1u << 1, K_LEFT = 1u << 2, K_RIGHT = 1u << 3,
  K_ATK = 1u << 4, K_INTER = 1u << 5, K_POT = 1u << 6, K_MANA = 1u << 7,
  K_ABIL = 1u << 8, K_MAP = 1u << 9, K_VIEW = 1u << 10, K_HELP = 1u << 11,
  K_MUTE = 1u << 12, K_PAUSE = 1u << 13
};

/* util */
double ab_clamp(double v, double lo, double hi);
double ab_dist(double x0, double y0, double x1, double y1);

#ifdef __cplusplus
}
#endif
#endif
