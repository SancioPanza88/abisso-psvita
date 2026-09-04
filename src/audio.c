/* ABISSO Vita - synth 1:1: tone sine/square/tri/saw + noise con filtri,
 * envelope esponenziale, delay, tabella sfx web (+C64), vento + battito. */
#include "audio.h"
#include "abisso.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

#define SR 22050
#define MAXV 16

enum { W_SINE = 0, W_SQUARE, W_TRI, W_SAW, W_NOISE };
enum { F_NONE = 0, F_LOW, F_HIGH };

typedef struct {
  bool on;
  int type, filt;
  double f0, f1, t, dur, vol, delay;
  double fcut0, fcut1;
  double phase, lp, hpx1, hpy1;
} Voice;

static Voice voices[MAXV];
static SDL_AudioDeviceID dev = 0;
static unsigned noise_s = 22222;
static double heart_t = 0.4;
static double lfo_t = 0;

static unsigned nrand(void) {
  noise_s = noise_s * 1664525u + 1013904223u;
  return noise_s >> 9;
}
static double frand(void) { return (nrand() % 2000) / 1000.0 - 1.0; }

static void play(int type, double f0, double f1, double dur, double vol,
                 double delay, int filt, double fc0, double fc1) {
  if (G.mute) return;
  for (int i = 0; i < MAXV; i++) {
    if (!voices[i].on) {
      voices[i].on = true;
      voices[i].type = type;
      voices[i].f0 = f0 < 20 ? 20 : f0;
      voices[i].f1 = f1 < 20 ? 20 : f1;
      voices[i].t = -(delay < 0 ? 0 : delay);
      voices[i].dur = dur;
      voices[i].vol = vol;
      voices[i].filt = filt;
      voices[i].fcut0 = fc0 < 20 ? 20 : fc0;
      voices[i].fcut1 = fc1 < 20 ? 20 : fc1;
      voices[i].phase = 0; voices[i].lp = 0;
      voices[i].hpx1 = 0; voices[i].hpy1 = 0;
      return;
    }
  }
}
static void tone(int type, double f0, double f1, double dur, double vol, double delay) {
  play(type, f0, f1, dur, vol, delay, F_NONE, 1000, 1000);
}
static void anoise(double dur, double vol, int filt, double f0, double f1, double delay) {
  play(W_NOISE, 440, 440, dur, vol, delay, filt, f0, f1);
}
static bool C64(void) { return G.c64; }

/* ---- tabella 1:1 ---- */
void sfx_swing(void) {
  if (C64()) { tone(W_SQUARE, 220, 110, 0.08, 0.12, 0); return; }
  anoise(0.09, 0.1, F_HIGH, 2400, 900, 0);
  tone(W_SINE, 320, 190, 0.08, 0.05, 0);
}
void sfx_shootMage(void) {
  if (C64()) {
    tone(W_SQUARE, 880, 440, 0.12, 0.14, 0);
    tone(W_TRI, 660, 220, 0.1, 0.1, 0); return;
  }
  tone(W_SINE, 950, 330, 0.14, 0.14, 0);
  anoise(0.08, 0.06, F_HIGH, 1800, 1800, 0);
}
void sfx_shootRanger(void) {
  if (C64()) { tone(W_SQUARE, 1200, 600, 0.1, 0.12, 0); return; }
  anoise(0.09, 0.1, F_HIGH, 3200, 1600, 0);
}
void sfx_shootPlasma(void) {
  if (C64()) {
    tone(W_SQUARE, 160, 480, 0.1, 0.14, 0);
    tone(W_SQUARE, 320, 160, 0.08, 0.08, 0); return;
  }
  tone(W_SQUARE, 180, 420, 0.1, 0.1, 0);
  anoise(0.14, 0.12, F_HIGH, 2600, 5200, 0);
}
void sfx_hit(void) {
  if (C64()) { tone(W_SQUARE, 280, 80, 0.07, 0.18, 0); return; }
  tone(W_SQUARE, 300, 95, 0.08, 0.16, 0);
  anoise(0.06, 0.12, F_HIGH, 700, 700, 0);
}
void sfx_crit(void) {
  if (C64()) {
    tone(W_SQUARE, 520, 160, 0.1, 0.22, 0);
    tone(W_SQUARE, 780, 320, 0.08, 0.14, 0); return;
  }
  tone(W_SQUARE, 520, 150, 0.12, 0.2, 0);
  tone(W_SINE, 760, 320, 0.1, 0.12, 0);
  anoise(0.1, 0.14, F_HIGH, 1200, 1200, 0);
}
void sfx_kill(void) {
  if (C64()) {
    tone(W_SQUARE, 440, 880, 0.15, 0.2, 0);
    tone(W_SQUARE, 880, 440, 0.2, 0.16, 0.12); return;
  }
  tone(W_SINE, 150, 42, 0.32, 0.3, 0);
  anoise(0.26, 0.22, F_LOW, 520, 520, 0);
  tone(W_TRI, 90, 60, 0.2, 0.15, 0.05);
}
void sfx_bossKill(void) {
  if (C64()) {
    sfx_kill();
    tone(W_SQUARE, 220, 110, 0.5, 0.3, 0); return;
  }
  sfx_kill();
  tone(W_SINE, 70, 30, 0.7, 0.35, 0);
  anoise(0.5, 0.3, F_LOW, 350, 350, 0);
}
void sfx_bossRoar(void) {
  if (C64()) {
    tone(W_SAW, 80, 40, 0.8, 0.28, 0);
    tone(W_SQUARE, 120, 60, 0.6, 0.2, 0.1); return;
  }
  tone(W_SAW, 80, 42, 0.9, 0.3, 0);
  tone(W_SINE, 55, 30, 1.1, 0.28, 0.05);
  anoise(0.8, 0.25, F_LOW, 300, 300, 0);
}
void sfx_hurt(void) {
  if (C64()) { tone(W_SQUARE, 200, 60, 0.12, 0.2, 0); return; }
  tone(W_SAW, 190, 70, 0.18, 0.22, 0);
  anoise(0.12, 0.14, F_LOW, 500, 500, 0);
}
void sfx_death(void) {
  if (C64()) {
    tone(W_SQUARE, 300, 40, 0.5, 0.28, 0);
    tone(W_TRI, 150, 30, 0.6, 0.2, 0.1); return;
  }
  tone(W_SAW, 140, 35, 0.7, 0.3, 0);
  tone(W_SINE, 80, 30, 0.9, 0.3, 0.05);
  anoise(0.6, 0.2, F_LOW, 300, 300, 0);
}
void sfx_pickup(void) {
  if (C64()) {
    tone(W_SQUARE, 880, 880, 0.06, 0.14, 0);
    tone(W_SQUARE, 1320, 1320, 0.08, 0.14, 0.06); return;
  }
  tone(W_SINE, 880, 880, 0.07, 0.12, 0);
  tone(W_SINE, 1318, 1318, 0.1, 0.12, 0.07);
}
void sfx_gem(void) {
  if (C64()) {
    tone(W_SQUARE, 660, 660, 0.06, 0.14, 0);
    tone(W_SQUARE, 990, 990, 0.06, 0.14, 0.05);
    tone(W_SQUARE, 1320, 1320, 0.1, 0.14, 0.1); return;
  }
  tone(W_SINE, 660, 660, 0.07, 0.12, 0);
  tone(W_SINE, 880, 880, 0.07, 0.12, 0.06);
  tone(W_SINE, 1320, 1320, 0.12, 0.12, 0.12);
}
void sfx_power(void) {
  if (C64()) {
    tone(W_SQUARE, 440, 440, 0.08, 0.16, 0);
    tone(W_SQUARE, 660, 660, 0.08, 0.16, 0.07);
    tone(W_SQUARE, 880, 880, 0.12, 0.16, 0.14); return;
  }
  tone(W_SINE, 440, 440, 0.1, 0.15, 0);
  tone(W_SINE, 660, 660, 0.1, 0.15, 0.09);
  tone(W_SINE, 990, 990, 0.16, 0.15, 0.18);
}
void sfx_potion(void) {
  if (C64()) {
    tone(W_TRI, 330, 660, 0.16, 0.16, 0);
    tone(W_SQUARE, 440, 440, 0.12, 0.1, 0.1); return;
  }
  tone(W_SINE, 280, 560, 0.2, 0.16, 0);
  anoise(0.22, 0.08, F_HIGH, 900, 1600, 0);
}
void sfx_mana(void) {
  if (C64()) {
    tone(W_TRI, 550, 1100, 0.14, 0.14, 0);
    tone(W_SQUARE, 770, 1540, 0.1, 0.1, 0.07); return;
  }
  tone(W_SINE, 500, 1000, 0.18, 0.14, 0);
  tone(W_SINE, 750, 1500, 0.14, 0.1, 0.08);
}
void sfx_chest(void) {
  if (C64()) {
    tone(W_SQUARE, 440, 440, 0.1, 0.16, 0);
    tone(W_SQUARE, 660, 660, 0.1, 0.16, 0.1);
    tone(W_SQUARE, 880, 880, 0.15, 0.16, 0.2); return;
  }
  anoise(0.4, 0.16, F_HIGH, 420, 90, 0);
  tone(W_SINE, 880, 880, 0.12, 0.14, 0.28);
  tone(W_SINE, 1174, 1174, 0.16, 0.14, 0.4);
}
void sfx_ability(void) {
  if (C64()) {
    tone(W_SQUARE, 220, 880, 0.25, 0.18, 0);
    tone(W_TRI, 440, 1320, 0.2, 0.12, 0.08); return;
  }
  tone(W_SAW, 180, 900, 0.32, 0.18, 0);
  anoise(0.3, 0.1, F_HIGH, 800, 2400, 0);
}
void sfx_boom(void) {
  if (C64()) {
    tone(W_SQUARE, 110, 35, 0.4, 0.32, 0);
    tone(W_SAW, 80, 25, 0.5, 0.24, 0); return;
  }
  tone(W_SINE, 120, 38, 0.45, 0.34, 0);
  anoise(0.4, 0.3, F_LOW, 420, 420, 0);
}
void sfx_stair(void) {
  if (C64()) {
    tone(W_SQUARE, 262, 262, 0.12, 0.16, 0);
    tone(W_SQUARE, 330, 330, 0.12, 0.16, 0.1);
    tone(W_SQUARE, 392, 392, 0.12, 0.16, 0.2);
    tone(W_SQUARE, 523, 523, 0.2, 0.16, 0.3); return;
  }
  tone(W_SINE, 130, 60, 0.6, 0.22, 0);
  tone(W_SINE, 196, 196, 0.5, 0.12, 0.18);
  tone(W_SINE, 261, 261, 0.7, 0.1, 0.36);
}
void sfx_step(void) {
  if (C64()) { tone(W_SQUARE, 60 + (nrand() % 40), 60, 0.03, 0.04, 0); return; }
  anoise(0.05, 0.045, F_HIGH, 480, 480, 0);
}
void sfx_revive(void) {
  if (C64()) {
    tone(W_SQUARE, 392, 392, 0.1, 0.16, 0);
    tone(W_SQUARE, 523, 523, 0.1, 0.16, 0.08);
    tone(W_SQUARE, 784, 784, 0.18, 0.16, 0.16); return;
  }
  tone(W_SINE, 392, 392, 0.14, 0.14, 0);
  tone(W_SINE, 523, 523, 0.14, 0.14, 0.12);
  tone(W_SINE, 784, 784, 0.24, 0.14, 0.24);
}
void sfx_click(void) {
  if (C64()) { tone(W_SQUARE, 800, 800, 0.02, 0.08, 0); return; }
  tone(W_SINE, 620, 620, 0.03, 0.06, 0);
}
void sfx_attack(void) { sfx_swing(); }

static void cb(void *ud, Uint8 *stream, int len) {
  (void)ud;
  Sint16 *o = (Sint16 *)stream;
  int n = len / 2;
  for (int i = 0; i < n; i++) {
    double s = 0;
    for (int v = 0; v < MAXV; v++) {
      if (!voices[v].on) continue;
      voices[v].t += 1.0 / SR;
      if (voices[v].t < 0) continue;
      double k = voices[v].t / voices[v].dur;
      if (k >= 1) { voices[v].on = false; continue; }
      double f = voices[v].f0 * pow(voices[v].f1 / voices[v].f0, k);
      voices[v].phase += f / SR;
      double ph = voices[v].phase;
      double w = 0;
      if (voices[v].type == W_SQUARE) w = (fmod(ph, 1.0) < 0.5 ? 1 : -1);
      else if (voices[v].type == W_TRI) w = 2.0 * fabs(2.0 * fmod(ph, 1.0) - 1.0) - 1.0;
      else if (voices[v].type == W_SAW) w = 2.0 * fmod(ph, 1.0) - 1.0;
      else if (voices[v].type == W_SINE) w = sin(2.0 * 3.14159265 * ph);
      else {
        w = frand();
        double fc = voices[v].fcut0 * pow(voices[v].fcut1 / voices[v].fcut0, k);
        double a = 1.0 - exp(-2.0 * 3.14159265 * fc / SR);
        if (a < 0.001) a = 0.001;
        if (a > 1) a = 1;
        if (voices[v].filt == F_LOW) {
          voices[v].lp += a * (w - voices[v].lp);
          w = voices[v].lp * 2.2;
        } else {
          double y = a * (voices[v].hpy1 + w - voices[v].hpx1);
          voices[v].hpx1 = w; voices[v].hpy1 = y;
          w = y * 1.4;
        }
      }
      /* envelope esponenziale come la web */
      double env = exp(log(0.0001 / (voices[v].vol > 0.0002 ? voices[v].vol : 0.0002)) * k);
      if (k < 0.04) env *= k / 0.04;
      s += w * voices[v].vol * env;
    }
    /* vento ambientale del dungeon (loop originale) */
    if (!G.mute && G.state == 2) {
      lfo_t += 1.0 / SR;
      double lfo = 0.045 + 0.02 * sin(2.0 * 3.14159265 * 0.07 * lfo_t);
      static double alp = 0;
      double a = 1.0 - exp(-2.0 * 3.14159265 * 110.0 / SR);
      double x = frand();
      alp += a * (x - alp);
      s += alp * lfo * 2.0;
    }
    if (s > 1) s = 1;
    if (s < -1) s = -1;
    o[i] = (Sint16)(s * 9000);
  }
}

void au_init(void) {
  memset(voices, 0, sizeof voices);
  SDL_AudioSpec want;
  memset(&want, 0, sizeof want);
  want.freq = SR;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 1024;
  want.callback = cb;
  dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
  if (dev) SDL_PauseAudioDevice(dev, 0);
}

void au_quit(void) {
  if (dev) { SDL_CloseAudioDevice(dev); dev = 0; }
}

void au_update(double dt) {
  (void)dt;
  /* battito a HP bassi come la web */
  if (G.mute || G.p.dead || G.p.hp <= 0 || G.p.hp >= G.p.max_hp * 0.35 || G.state != 2) {
    heart_t = 0.4;
    return;
  }
  heart_t -= dt;
  if (heart_t <= 0) {
    heart_t = 0.85;
    tone(W_SINE, 58, 42, 0.12, 0.18, 0);
    tone(W_SINE, 52, 38, 0.1, 0.12, 0.16);
  }
}
