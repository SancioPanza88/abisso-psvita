/* ABISSO Vita - audio sintetico (come WebAudio originale: tutto generato). */
#ifndef AB_AUDIO_H
#define AB_AUDIO_H

void au_init(void);
void au_quit(void);
void au_update(double dt);

/* SFX */
void sfx_attack(void);
void sfx_hit(void);
void sfx_hurt(void);
void sfx_pickup(void);
void sfx_potion(void);
void sfx_stairs(void);
void sfx_boss(void);
void sfx_death(void);
void sfx_click(void);
void sfx_ability(void);

#endif
