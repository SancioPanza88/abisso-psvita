/* ABISSO Vita - main loop SDL2. 60fps, stati login/classe/gioco/morte. */
#include "abisso.h"
#include "render.h"
#include "input.h"
#include "audio.h"
#include "net.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  srand((unsigned)time(NULL));
  ab_game_init();

  if (!ren_init()) {
    fprintf(stderr, "ren_init fallito: %s\n", SDL_GetError());
    return 1;
  }
  in_init();
  au_init();
  net_init();

  Uint32 last = SDL_GetTicks();
  bool running = true;
  while (running) {
    Uint32 now = SDL_GetTicks();
    double dt = (now - last) / 1000.0;
    last = now;
    if (dt > 0.1) dt = 0.1;
    if (dt < 0.001) SDL_Delay(1);

    unsigned keys = in_poll(dt);
    if (in_quit_requested) break;
    /* zoom (stick destro / +/-): solo scala mondo->schermo */
    static bool was_zoom = false;
    if (keys & K_ZIN) { G.zoom += dt * 0.9; if (G.zoom > 2.0) G.zoom = 2.0; was_zoom = true; }
    if (keys & K_ZOUT) { G.zoom -= dt * 0.9; if (G.zoom < 0.6) G.zoom = 0.6; was_zoom = true; }
    if (was_zoom && !(keys & (K_ZIN | K_ZOUT))) { was_zoom = false; ab_save_record(); }
    if (keys & K_PAUSE) {
      /* ESC: esci da help/mercante, oppure chiudi app dal login */
      if (G.state == ST_GAME) {
        if (G.merchant_open) G.merchant_open = false;
        else if (G.help) G.help = false;
        else {
          /* doppio ESC esce: per sicurezza solo dal login */
        }
      }
    }

    /* update logica solo in game (render gestisce i toggle) */
    ab_update(dt, keys);
    au_update(dt);
    net_tick(dt);
    ren_frame(keys, dt);

    /* uscita PC: finestra chiusa gestita come PAUSE dal login */
  }

  au_quit();
  in_quit();
  net_quit();
  ren_quit();
  SDL_Quit();
  return 0;
}
