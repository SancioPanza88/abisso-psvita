/* ABISSO Vita - input tastiera + controller SDL (Vita) + touch/mouse. */
#ifndef AB_INPUT_H
#define AB_INPUT_H
#include <SDL2/SDL.h>
#include <stdbool.h>

void in_init(void);
void in_quit(void);
/* Ritorna bitmask K_* (vedi abisso.h). Aggiorna anche mouse/touch per UI. */
unsigned in_poll(double dt);
/* mouse/touch condivisi con render per i menu */
extern int in_mouse_x, in_mouse_y;
extern bool in_mouse_pressed;   /* click avvenuto questo frame */
extern bool in_mouse_down;
extern char in_text[64];        /* testo digitato (PC) */
extern bool in_text_ready;
extern bool in_backspace;
extern bool in_enter;
extern bool in_quit_requested;

void in_consume_click(void);
void in_consume_text(void);

#endif
