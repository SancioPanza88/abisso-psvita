/* ABISSO Vita - main loop SDL2. 60fps, stati login/classe/gioco/morte. */
#include "abisso.h"
#include "render.h"
#include "input.h"
#include "audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <time.h>

#ifdef ABISSO_VITA
#include <psp2/kernel/processmgr.h>
#endif

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
    ren_frame(keys);

    /* uscita PC: finestra chiusa gestita come PAUSE dal login */
  }

  au_quit();
  in_quit();
  ren_quit();
  SDL_Quit();
#ifdef ABISSO_VITA
  sceKernelExitProcess(0);
#endif
  return 0;
}
