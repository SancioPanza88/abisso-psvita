/* ABISSO Vita - audio 1:1 dalla web: stessi synth (tone/noise) e stessa
 * tabella sfx (+varianti C64), vento ambientale e battito a HP bassi. */
#ifndef AB_AUDIO_H
#define AB_AUDIO_H

void au_init(void);
void au_quit(void);
void au_update(double dt);

void sfx_swing(void);
void sfx_shootMage(void);
void sfx_shootRanger(void);
void sfx_shootPlasma(void);
void sfx_hit(void);
void sfx_crit(void);
void sfx_kill(void);
void sfx_bossKill(void);
void sfx_bossRoar(void);
void sfx_hurt(void);
void sfx_death(void);
void sfx_pickup(void);
void sfx_gem(void);
void sfx_power(void);
void sfx_potion(void);
void sfx_mana(void);
void sfx_chest(void);
void sfx_ability(void);
void sfx_boom(void);
void sfx_stair(void);
void sfx_step(void);
void sfx_revive(void);
void sfx_click(void);
/* compat: attacco generico */
void sfx_attack(void);

#endif
