/* ABISSO Vita - tabelle dati 1:1 dalla versione web (index.html) */
#include "abisso.h"
#include <string.h>

const char *AB_CLASS_KEYS[CLS_COUNT] = {
  "guerriero","ladro","mago","ranger",
  "paladino","negromante","bardo","monaco","prof"
};

static const AbClassDef CLASSES[CLS_COUNT] = {
  /* guerriero */ {"guerriero","Guerriero","Carica","Tanto in salute, corpo a corpo.",
    16,0,0, 3.6,0.50,1.55,1.45, 3,6,false, 0.0,0.0, 11,0, "assets/sprites/hero_guerriero.png"},
  /* ladro */ {"ladro","Ladro","Passo Furtivo","Veloce, critici frequenti.",
    10,0,0, 4.6,0.32,1.35,1.15, 2,4,false, 0.28,0.0, 9,0, "assets/sprites/hero_ladro.png"},
  /* mago */ {"mago","Mago","Onda d'Urto","Energia a distanza, usa mana.",
    9,14,2, 3.2,0.75,8.0,0.0, 4,8,true, 0.0,11.0, 13,6, "assets/sprites/hero_mago.png"},
  /* ranger */ {"ranger","Ranger","Raffica","Frecce veloci, mobile.",
    11,0,0, 3.9,0.42,9.0,0.0, 2,5,true, 0.0,15.0, 10,0, "assets/sprites/hero_ranger.png"},
  /* paladino */ {"paladino","Paladino","Muro Sacro","Tank sacro, mazza dorata.",
    18,0,0, 3.3,0.60,1.50,1.30, 3,5,false, 0.0,0.0, 14,0, "assets/sprites/hero_paladino.png"},
  /* negromante */ {"negromante","Negromante","Drenaggio","Magia verde, ruba vita.",
    10,14,2, 3.2,0.70,8.0,0.0, 4,7,true, 0.0,11.0, 13,6, "assets/sprites/hero_negromante.png"},
  /* bardo */ {"bardo","Bardo","Canto","Rapido, ispira +danno.",
    12,0,0, 4.0,0.35,1.40,1.10, 2,4,false, 0.15,0.0, 12,0, "assets/sprites/hero_bardo.png"},
  /* monaco */ {"monaco","Monaco","Onda di Chi","Pugni fulminei, velocissimo.",
    14,0,0, 4.4,0.30,1.30,1.25, 3,5,false, 0.20,0.0, 8,0, "assets/sprites/hero_monaco.png"},
  /* prof */ {"prof","Prof","Colpo Caricato","Fucile al plasma, robusto.",
    22,0,0, 3.6,0.55,8.5,0.0, 5,9,true, 0.0,14.0, 10,0, "assets/speciali/prof.png"},
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

/* Mostri comuni + nuovi + boss. Valori base da index.html (MONSTER_TYPES),
 * scaling applicato in game.c via ab_scaled_stat come nel JS. */
static const AbMonDef MONS[] = {
  {"r","Ratto",7,2,2.6,5.5,1,3,2,false,false,false,false,false,false,"assets/sprites/mon_ratto.png"},
  {"b","Pipistrello",8,2,2.9,6.5,1,3,2,false,true,false,false,false,false,"assets/sprites/mon_pipistrello.png"},
  {"g","Goblin",14,3,2.3,6.0,2,5,4,false,false,false,false,false,false,"assets/sprites/mon_goblin.png"},
  {"j","Melma",9,2,1.4,4.5,1,2,2,false,false,false,false,false,false,"assets/sprites/mon_melma.png"},
  {"J","Gelatina",22,3,1.2,4.5,2,4,5,false,false,false,false,true,false,"assets/sprites/mon_gelatina.png"},
  {"s","Scheletro",20,4,1.9,6.0,3,6,6,false,false,false,false,false,false,"assets/sprites/mon_scheletro.png"},
  {"o","Orco",32,6,2.0,6.5,4,9,9,false,false,false,false,false,false,"assets/sprites/mon_orco.png"},
  {"z","Zombie",26,5,1.3,5.0,3,6,7,false,false,false,false,false,false,"assets/sprites/mon_zombie.png"},
  {"S","Ragno",24,5,3.0,7.5,4,8,8,false,false,true,false,false,false,"assets/sprites/mon_ragno.png"},
  {"W","Spettro",28,6,2.7,8.0,5,10,10,false,false,false,true,false,false,"assets/sprites/mon_spettro.png"},
  {"k","Serpente",10,2,2.5,5.5,1,3,3,false,false,true,false,false,true,"assets/sprites/mon_serpente.png"},
  {"c","Cultista",16,4,1.8,6.5,3,6,6,false,false,false,false,false,false,"assets/sprites/mon_cultista.png"},
  {"h","Arpia",18,5,3.2,7.5,4,7,8,false,false,false,false,false,false,"assets/sprites/mon_arpia.png"},
  {"m","Mantide",26,6,3.4,7.0,5,9,10,false,false,false,false,false,true,"assets/sprites/mon_mantide.png"},
  {"G","Golem Roccia",40,7,1.6,5.5,6,12,12,false,false,false,false,false,false,"assets/sprites/mon_golem.png"},
  {"v","Cavaliere",30,6,2.1,6.0,5,10,10,false,false,false,false,false,false,"assets/sprites/mon_cavaliere.png"},
  {"w","Sciamano",22,5,1.7,8.5,5,10,10,false,false,false,false,false,false,"assets/sprites/mon_sciamano.png"},
  /* boss */
  {"D","Drago Minore",120,11,2.3,11.0,40,70,60,true,false,false,false,false,false,"assets/sprites/mon_drago.png"},
  {"X","Golem di Pietra",160,12,1.8,11.0,50,80,70,true,false,false,false,false,false,"assets/sprites/boss_golem.png"},
  {"L","Lich dei Nonmorti",140,12,2.0,12.0,55,90,80,true,false,false,true,false,false,"assets/sprites/boss_lich.png"},
  {"M","Regina Melme",170,10,1.4,10.0,45,75,70,true,false,false,false,true,false,"assets/sprites/boss_melme.png"},
  {"R","Re Ragno",130,11,2.8,11.0,45,75,70,true,false,true,false,false,false,"assets/sprites/boss_ragno.png"},
  {"K","Re dei Ratti",110,9,3.0,11.0,40,70,65,true,false,false,false,false,false,"assets/sprites/boss_ratti.png"},
};

const AbMonDef *ab_mon_def(const char *key) {
  if (!key || !key[0]) return NULL;
  for (size_t i = 0; i < sizeof(MONS)/sizeof(MONS[0]); i++)
    if (MONS[i].key[0] == key[0] && MONS[i].key[1] == '\0') return &MONS[i];
  return NULL;
}
const AbMonDef *ab_mon_def_by_idx(int i) {
  int n = (int)(sizeof(MONS)/sizeof(MONS[0]));
  if (i < 0 || i >= n) return NULL;
  return &MONS[i];
}
int ab_mon_def_count(void) { return (int)(sizeof(MONS)/sizeof(MONS[0])); }

static const char BOSS_ORDER[6] = {'D','X','L','M','R','K'};

bool ab_is_boss_floor(int depth) { return depth >= 5 && (depth % 5) == 0; }

char ab_boss_for_depth(int depth) {
  int k = (depth / 5) - 1;
  k %= 6; if (k < 0) k += 6;
  return BOSS_ORDER[k];
}

const char *ab_boss_name(char b) {
  for (size_t i = 0; i < sizeof(MONS)/sizeof(MONS[0]); i++)
    if (MONS[i].key[0] == b) return MONS[i].name;
  return "Boss";
}
