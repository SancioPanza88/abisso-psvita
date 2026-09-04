/* ABISSO Vita - rendering SDL2 (sprite, tile, HUD, menu). */
#ifndef AB_RENDER_H
#define AB_RENDER_H
#include <stdbool.h>

bool ren_init(void);
void ren_quit(void);
void ren_frame(unsigned keys, double dt);

#endif
