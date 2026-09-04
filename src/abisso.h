/* ABISSO - Port PSVita (SDL2) - header condiviso logica di gioco.
 * Port 1:1 della versione web (index.html): stessi semi RNG, stesse classi,
 * stessi mostri/boss, stessa generazione dungeon, stessi controlli rimappati.
 * Single-player offline. Il multiplayer P2P WebRTC non esiste su PSVita:
 * ogni run crea un mondo fresco con seed casuale come fa la web
 * (bootstrapFreshWorld), e il room-code resta solo etichetta della run.
 */
#ifndef ABISSO_H
#define ABISSO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ABISSO_VERSION "1.1.0"
#define ABISSO_TITLEID "ABSS00001"

#define SCR_W 960
#define SCR_H 544

#define MAP_MAX_W 132
#define MAP_MAX_H 76

#define T_WALL 0
#define T_FLOOR 1
#define T_STAIRS 2

#define MAX_MONSTERS 96
#define MAX_PROJS 96
#define MAX_PARTS 320
#define MAX_FLOATS 32
#define MAX_CHESTS 24
#define MAX_TORCHES 96
#define MAX_LOGLINES 8
#define MAX_ITEMS 64
#define MAX_GATES 12
#define MAX_NAME 24
#define MAX_ROOM 24

#define TILE_BASE 27
#define FOV_RADIUS 7.4
#define DOWNED_BLEEDOUT 22.0

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
  const char *sprite;  /* chiave ENTITY_SPRITES (sheet o file) */
} AbClassDef;

const AbClassDef *ab_class_def(int id);
int ab_class_from_key(const char *key);
double ab_class_atk_dur(int id);
extern const char *AB_CLASS_KEYS[CLS_COUNT];

/* ---------- Mostri ---------- */
typedef struct {
  const char *key;      /* 'r','b',... 'D','X','L','M','R','K' boss */
  const char *name;
  int hp, dmg;
  double speed, aggro;
  int gold_min, gold_max;
  bool boss;
  bool erratic, poison, lifesteal, split, dash;
  bool swoop, cone, dbl, beam, stomp;
  double ranged_range;  /* >0: attacca a distanza */
  char summon;          /* boss: tipo evocato, 0 nessuno */
  int summon_n;
  const char *sprite;   /* chiave ENTITY_SPRITES */
} AbMonDef;

const AbMonDef *ab_mon_def(char key);
bool ab_is_boss_floor(int depth);
char ab_boss_for_depth(int depth); /* 'D','X','L','M','R','K' */
const char *ab_boss_name(char b);
int ab_mon_weight(char key, int depth);

/* ---------- Rarita / equip / powerup (da index.html) ---------- */
typedef enum { R_COMUNE = 0, R_RARO, R_EPICO, R_LEGGENDARIO } AbRarity;
const char *ab_rarity_name(int r);
void ab_rarity_color(int r, double *pr, double *pg, double *pb);
bool ab_sprite_crop(const char *key, int *x, int *y, int *w, int *h);
bool ab_entity_sheet(const char *key, const char **sheet, int *cell, int *cells);
const char *ab_entity_file(const char *key);
typedef enum { BUFF_RAGE = 0, BUFF_SHIELD, BUFF_HASTE, BUFF_FOCUS, BUFF_COUNT } AbBuff;
extern const double BUFF_DURATION[BUFF_COUNT];
extern const char *BUFF_NAMES[BUFF_COUNT];

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
  double dot_t;      /* veleno subito */
  double dot_acc;
  double regen_acc;
  int affix; /* 0 nessuno,1 veloce,2 esplosivo,3 rigenerante */
  bool is_boss;
  /* boss state */
  double spec_cd, breath_cd, fb_cd, fly_cd, fly_t, dive_t, dive_x, dive_y;
  int summon_left;
  bool flying;
  /* attacchi dedicati comuni: 0 nessuno,1 carica,2 dash,3 swoop,4 bolt */
  int fx_mode, wind_kind;
  double fx_t, fx_dur, fx_tx, fx_ty, fx_speed;
  bool fx_hit;
  double dent_t;
  unsigned hit_mark; /* ultimo proiettile perforante che l'ha colpito */
  double atk_anim, atk_dur; /* timer posa d'attacco (render) */
  double phase; /* 0..1 fase animazione */
} AbMonster;

typedef struct {
  bool active;
  double x, y, vx, vy;
  double life, range_left;
  int dmg;
  bool friendly;
  bool poison;   /* avvelena */
  bool web;      /* rallenta */
  bool pierce;   /* perfora (onda di chi) */
  double hit_r;  /* raggio impatto */
  double r, g, b;
  double size;
  unsigned id_mark;
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
  bool boss_chest;
  char id[32];
} AbChest;

typedef struct {
  int tx, ty;
} AbTorch;

/* oggetto a terra: tesori, pozioni, powerup, equip */
typedef struct {
  bool active;
  double x, y;
  int kind; /* 0 gold,1 gem,2 power,3 potion,4 manapotion,5 equip */
  int amount;
  int buff; /* per kind power */
  int rarity;
  int slot; /* per kind equip */
  int st_hp, st_dmg, st_spd, st_arm;
} AbItem;

/* ---------- Equip (slot come la web, stat in %) ---------- */
typedef struct {
  bool filled;
  int rarity;
  int hp, dmg_pct, spd_pct, arm_pct;
  char name[48];
} AbEquip; /* 5 slot: helm, necklace, armor, ring, greaves */
extern const char *EQUIP_SLOT_NAMES[5];

/* ---------- Giocatore ---------- */
typedef struct {
  double x, y, rx, ry;
  double fx, fy; /* facing */
  int cls;
  int hp, max_hp;
  double mp, max_mp;
  double speed;
  int gold, potions, mana_potions;
  double atk_cd, ability_cd;
  double buffs[BUFF_COUNT];
  double anim_t; /* attacco */
  double iframes;
  double poison_t, poison_acc;
  double web_t;  /* rallentato dalla tela */
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
  uint8_t visible[MAP_MAX_H][MAP_MAX_W];
  int spawn_x, spawn_y;
  int stairs_x, stairs_y;
  int merch_x, merch_y;
  int safe_x, safe_y, safe_w, safe_h;
  int arena_cx, arena_cy, arena_w, arena_h;
  bool has_arena;
  char arena_boss;
  int gates[MAX_GATES][2];
  int gate_count;
  bool gates_closed;
} AbMap;

/* ---------- Stato gioco ---------- */
typedef enum {
  ST_LOGIN = 0,
  ST_CLASS,
  ST_NET,
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
  AbItem items[MAX_ITEMS];
  int torch_count, chest_count, item_count;
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
  double loot_r, loot_g, loot_b;
  char boss_name[48];
  int boss_hp, boss_max;
  bool boss_active, boss_dead;
  bool minimap, help, merchant_open;
  bool mute;
  double zoom;
  /* record */
  int best_depth, best_gold;
  /* debug */
  bool god, speed5;
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
void ab_loot_banner(const char *s, double r, double g, double b);
void ab_float_text(double x, double y, const char *s, double r, double g, double b);
void ab_burst(double x, double y, int n, double r, double g, double b, double spd);
void ab_shock(double x, double y, double r, double g, double b);
void ab_crit_flash(void);
bool ab_is_walkable(int tx, int ty);
void ab_save_record(void);
void ab_load_record(void);
bool ab_merchant_buy(int idx);
int ab_merchant_price(int idx);
void ab_equip_bonus(int *hp, int *dmg_pct, int *spd_pct, int *arm_pct);
void ab_update_fov(void);

/* tasti logici (bitmask) condivisa con input/render */
enum {
  K_UP = 1u << 0, K_DOWN = 1u << 1, K_LEFT = 1u << 2, K_RIGHT = 1u << 3,
  K_ATK = 1u << 4, K_INTER = 1u << 5, K_POT = 1u << 6, K_MANA = 1u << 7,
  K_ABIL = 1u << 8, K_MAP = 1u << 9, K_VIEW = 1u << 10, K_HELP = 1u << 11,
  K_MUTE = 1u << 12, K_PAUSE = 1u << 13,
  K_ZIN = 1u << 14, K_ZOUT = 1u << 15
};

/* util */
double ab_clamp(double v, double lo, double hi);
double ab_dist(double x0, double y0, double x1, double y1);

#ifdef __cplusplus
}
#endif
#endif
