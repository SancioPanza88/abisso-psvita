/* ABISSO Vita - tabelle dati 1:1 da index.html (MONSTER_TYPES, CLASSES,
 * RARITY_TIERS, POWERUP_TYPES, SPRITE_CROP, ENTITY_SPRITES). */
#include "abisso.h"
#include <string.h>

const char *AB_CLASS_KEYS[CLS_COUNT] = {
  "guerriero","ladro","mago","ranger",
  "paladino","negromante","bardo","monaco","prof"
};

/* sprite: chiave ENTITY_SPRITES della web (sheet o file singolo) */
static const AbClassDef CLASSES[CLS_COUNT] = {
  {"guerriero","Guerriero","Carica","Tanto in salute, corpo a corpo.",
    16,0,0, 3.6,0.50,1.55,1.45, 3,6,false, 0.0,0.0, 11,0, "guerriero"},
  {"ladro","Ladro","Passo Furtivo","Veloce, critici frequenti.",
    10,0,0, 4.6,0.32,1.35,1.15, 2,4,false, 0.28,0.0, 9,0, "ladro"},
  {"mago","Mago","Onda d'Urto","Energia a distanza, usa mana.",
    9,14,2, 3.2,0.75,8.0,0.0, 4,8,true, 0.0,11.0, 13,6, "mago"},
  {"ranger","Ranger","Raffica","Frecce veloci, mobile.",
    11,0,0, 3.9,0.42,9.0,0.0, 2,5,true, 0.0,15.0, 10,0, "ranger"},
  {"paladino","Paladino","Muro Sacro","Tank sacro, mazza dorata.",
    18,0,0, 3.3,0.60,1.50,1.30, 3,5,false, 0.0,0.0, 14,0, "paladino"},
  {"negromante","Negromante","Drenaggio","Magia verde, ruba vita.",
    10,14,2, 3.2,0.70,8.0,0.0, 4,7,true, 0.0,11.0, 13,6, "negromante"},
  {"bardo","Bardo","Canto","Rapido, ispira +danno.",
    12,0,0, 4.0,0.35,1.40,1.10, 2,4,false, 0.15,0.0, 12,0, "bardo"},
  {"monaco","Monaco","Onda di Chi","Pugni fulminei, velocissimo.",
    14,0,0, 4.4,0.30,1.30,1.25, 3,5,false, 0.20,0.0, 8,0, "monaco"},
  {"prof","Prof","Colpo Caricato","Fucile al plasma, robusto.",
    22,0,0, 3.6,0.55,8.5,0.0, 5,9,true, 0.0,14.0, 10,0, "prof"},
};

const AbClassDef *ab_class_def(int id) {
  if (id < 0 || id >= CLS_COUNT) id = 0;
  return &CLASSES[id];
}

int ab_class_from_key(const char *key) {
  if (!key) return 0;
  for (int i = 0; i < CLS_COUNT; i++)
    if (strcmp(key, AB_CLASS_KEYS[i]) == 0) return i;
  return 0;
}

/* key,name,hp,dmg,speed,aggro,goldmin,goldmax,boss,erratic,poison,lifesteal,
   split,dash,swoop,cone,dbl,beam,stomp,ranged_range,summon,summon_n,sprite */
static const AbMonDef MONS[] = {
  {"r","ratto",7,2,2.6,5.5,1,3,false,false,false,false,false,false,false,false,false,false,false,0.0,0,0,"ratto"},
  {"b","pipistrello",8,2,2.9,6.5,1,3,false,true,false,false,false,false,false,false,false,false,false,0.0,0,0,"pipistrello"},
  {"g","goblin",14,3,2.3,6.0,2,5,false,false,false,false,false,false,false,false,false,false,false,0.0,0,0,"goblin"},
  {"j","melma",9,2,1.4,4.5,1,2,false,false,false,false,false,false,false,false,false,false,false,0.0,0,0,"melma"},
  {"J","gelatina",22,3,1.2,4.5,2,4,false,false,false,false,true,false,false,false,false,false,false,0.0,0,0,"gelatina"},
  {"s","scheletro",20,4,1.9,6.0,3,6,false,false,false,false,false,false,false,false,false,false,false,0.0,0,0,"scheletro"},
  {"o","orco",32,6,2.0,6.5,4,9,false,false,false,false,false,false,false,false,false,false,false,0.0,0,0,"orco"},
  {"z","zombie",26,5,1.3,5.0,3,6,false,false,false,false,false,false,false,false,false,false,false,0.0,0,0,"zombie"},
  {"S","ragno gigante",24,5,3.0,7.5,4,8,false,false,true,false,false,false,false,false,false,false,false,0.0,0,0,"ragno"},
  {"W","spettro",28,6,2.7,8.0,5,10,false,false,false,true,false,false,false,false,false,false,false,0.0,0,0,"spettro"},
  {"k","serpente",10,2,2.5,5.5,1,3,false,false,true,false,false,true,false,false,false,false,false,0.0,0,0,"serpente"},
  {"h","arpia",9,2,2.9,6.5,1,3,false,true,false,false,false,false,true,false,false,false,false,0.0,0,0,"arpia"},
  {"C","cavaliere caduto",26,5,1.8,6.0,3,6,false,false,false,false,false,false,false,true,false,false,false,0.0,0,0,"cavaliere"},
  {"c","cultista",18,4,2.0,6.5,3,6,false,false,false,false,false,false,false,false,false,false,false,3.0,0,0,"cultista"},
  {"m","mantide abissale",22,4,3.2,7.5,4,8,false,false,false,false,false,false,false,false,true,false,false,0.0,0,0,"mantide"},
  {"q","sciamano goblin",20,4,1.9,7.0,3,7,false,false,false,false,false,false,false,false,false,true,false,2.6,0,0,"sciamano"},
  {"G","golem di roccia",40,7,1.4,5.0,6,12,false,false,false,false,false,false,false,false,false,false,true,0.0,0,0,"golem"},
  {"D","drago minore",120,11,2.3,11.0,40,70,true,false,false,false,false,false,false,false,false,false,false,0.0,0,0,"drago"},
  {"X","Golem di Pietra",150,12,1.6,12.0,220,300,true,false,false,false,false,false,false,false,false,false,true,0.0,0,0,"boss_golem"},
  {"L","Lich Signore dei Nonmorti",135,11,2.1,12.0,200,280,true,false,false,false,false,false,false,false,false,false,false,0.0,'s',2,"boss_lich"},
  {"M","Regina delle Melme",130,10,1.5,10.0,190,260,true,false,false,false,false,false,false,false,false,false,false,0.0,'j',2,"boss_melme"},
  {"R","Re Ragno",140,12,2.6,12.0,210,290,true,false,true,false,false,false,false,false,false,false,false,0.0,'b',2,"boss_ragno"},
  {"K","Re dei ratti",120,11,2.9,12.0,200,280,true,false,false,false,false,false,false,false,false,false,false,0.0,'r',3,"boss_ratti"},
};

const AbMonDef *ab_mon_def(char key) {
  for (size_t i = 0; i < sizeof(MONS)/sizeof(MONS[0]); i++)
    if (MONS[i].key[0] == key) return &MONS[i];
  return NULL;
}

/* pesi di spawn 1:1 (weight(depth) della web) */
int ab_mon_weight(char key, int depth) {
  int d = depth;
  switch (key) {
    case 'r': { int w = 10 - d; return w > 0 ? w : 0; }
    case 'b': { int w = 9 - d; return w > 0 ? w : 0; }
    case 'g': return 8;
    case 'j': return d >= 2 ? 6 : 2;
    case 'J': return d >= 2 ? 5 : 1;
    case 's': return d >= 2 ? 9 : 2;
    case 'o': return d >= 3 ? 8 : 1;
    case 'z': return d >= 3 ? 7 : 1;
    case 'S': return d >= 4 ? 7 : 0;
    case 'W': return d >= 5 ? 6 : 0;
    case 'k': return 7;
    case 'h': { int w = 11 - d; return w > 0 ? w : 0; }
    case 'C': return d >= 2 ? 8 : 1;
    case 'c': return d >= 3 ? 6 : 1;
    case 'm': return d >= 4 ? 6 : 0;
    case 'q': return d >= 4 ? 5 : 0;
    case 'G': return d >= 4 ? 5 : 0;
    default: return 0;
  }
}

const char *ab_mon_sprite_key(char key) {
  const AbMonDef *t = ab_mon_def(key);
  return t ? t->sprite : NULL;
}

static const char BOSS_ORDER[6] = {'D','X','L','M','R','K'};

bool ab_is_boss_floor(int depth) { return depth >= 5 && (depth % 5) == 0; }

char ab_boss_for_depth(int depth) {
  int k = (depth / 5) - 1;
  k %= 6; if (k < 0) k += 6;
  return BOSS_ORDER[k];
}

const char *ab_boss_name(char b) {
  const AbMonDef *t = ab_mon_def(b);
  return t ? t->name : "essere oscuro";
}

/* ---------- rarita / equip / powerup ---------- */
static const char *RAR_NAMES[4] = {"Comune","Raro","Epico","Leggendario"};
const char *ab_rarity_name(int r) {
  if (r < 0 || r > 3) r = 0;
  return RAR_NAMES[r];
}
void ab_rarity_color(int r, double *pr, double *pg, double *pb) {
  /* #b8b8b8 #5fa0c9 #b06bf2 #ff9d2e */
  static const double c[4][3] = {
    {0.72,0.72,0.72},{0.37,0.63,0.79},{0.69,0.42,0.95},{1.0,0.62,0.18}
  };
  if (r < 0 || r > 3) r = 0;
  *pr = c[r][0]; *pg = c[r][1]; *pb = c[r][2];
}

const char *EQUIP_SLOT_NAMES[5] = {"Elmo","Collana","Armatura","Anello","Gambali"};

const double BUFF_DURATION[BUFF_COUNT] = {12.0, 10.0, 12.0, 10.0};
const char *BUFF_NAMES[BUFF_COUNT] = {"Furia","Scudo","Fretta","Concentrazione"};

/* ---------- SPRITE_CROP 1:1 dalla web ---------- */
typedef struct { const char *key; int x, y, w, h; } CropDef;
static const CropDef CROPS[] = {
  {"torch",516,132,401,480},
  {"stairs",486,111,437,542},
  {"icon_lightning",538,116,345,548},
  {"icon_eye",444,133,519,502},
  {"wall_stone",186,0,1037,768},
  {"wall_brick",368,45,643,678},
  {"floor_stone",350,67,709,638},
  {"floor_dirt",420,92,555,601},
  {NULL,0,0,0,0}
};
bool ab_sprite_crop(const char *key, int *x, int *y, int *w, int *h) {
  for (int i = 0; CROPS[i].key; i++) {
    if (strcmp(key, CROPS[i].key) == 0) {
      *x = CROPS[i].x; *y = CROPS[i].y; *w = CROPS[i].w; *h = CROPS[i].h;
      return true;
    }
  }
  return false;
}

/* ---------- ENTITY_SPRITES 1:1: sheet+cell o file ---------- */
typedef struct { const char *key; const char *sheet; int cell; int cells; } EntSpr;
static const EntSpr ENTSPR[] = {
  {"guerriero","heroes_sheet",0,4},{"ladro","heroes_sheet",1,4},
  {"mago","heroes_sheet",2,4},{"ranger","heroes_sheet",3,4},
  {"ratto","monsters_sheet1",0,4},{"pipistrello","monsters_sheet1",1,4},
  {"goblin","monsters_sheet1",2,4},{"scheletro","monsters_sheet1",3,4},
  {"melma","monsters_sheet2",0,4},{"gelatina","monsters_sheet2",1,4},
  {"zombie","monsters_sheet2",2,4},{"ragno","monsters_sheet2",3,4},
  {"chest_closed","chest_sheet",0,2},{"chest_open","chest_sheet",1,2},
  {NULL,NULL,0,0}
};
/* ritorna sheet+cell se l'entita usa uno sheet, altrimenti NULL (file singolo) */
bool ab_entity_sheet(const char *key, const char **sheet, int *cell, int *cells) {
  for (int i = 0; ENTSPR[i].key; i++) {
    if (strcmp(key, ENTSPR[i].key) == 0) {
      *sheet = ENTSPR[i].sheet; *cell = ENTSPR[i].cell; *cells = ENTSPR[i].cells;
      return true;
    }
  }
  return false;
}
