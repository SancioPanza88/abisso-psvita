/* ABISSO Vita - synth audio SDL (square/triangle/noise), musica generativa. */
#include "audio.h"
#include "abisso.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

#define SR 22050
#define MAXV 12

typedef struct {
  bool on;
  int type; /* 0 square,1 tri,2 noise */
  double f0, f1, t, dur, vol;
} Voice;

static Voice voices[MAXV];
static SDL_AudioDeviceID dev = 0;
static unsigned noise_s = 22222;
static double mus_t = 0;
static int mus_step = 0;

static unsigned nrand(void) {
  noise_s = noise_s * 1664525u + 1013904223u;
  return noise_s >> 9;
}

static void play(int type, double f0, double f1, double dur, double vol) {
  if (G.mute) return;
  for (int i = 0; i < MAXV; i++) {
    if (!voices[i].on) {
      voices[i].on = true;
      voices[i].type = type;
      voices[i].f0 = f0; voices[i].f1 = f1;
      voices[i].t = 0; voices[i].dur = dur; voices[i].vol = vol;
      return;
    }
  }
}

void sfx_attack(void) { play(2, 900, 300, 0.12, 0.35); play(0, 220, 110, 0.08, 0.25); }
void sfx_hit(void) { play(2, 500, 150, 0.15, 0.4); }
void sfx_pickup(void) { play(0, 660, 990, 0.12, 0.3); play(0, 990, 1320, 0.1, 0.25); }
void sfx_potion(void) { play(1, 300, 600, 0.25, 0.35); }
void sfx_stairs(void) { play(0, 220, 440, 0.2, 0.3); play(0, 330, 660, 0.2, 0.3); }
void sfx_boss(void) { play(0, 110, 55, 0.6, 0.5); play(2, 200, 60, 0.5, 0.4); }
void sfx_death(void) { play(0, 330, 60, 0.8, 0.45); }
void sfx_click(void) { play(0, 800, 800, 0.05, 0.25); }
void sfx_ability(void) { play(0, 440, 880, 0.25, 0.35); play(1, 220, 440, 0.3, 0.3); }

static void cb(void *ud, Uint8 *stream, int len) {
  (void)ud;
  Sint16 *o = (Sint16 *)stream;
  int n = len / 2;
  for (int i = 0; i < n; i++) {
    double s = 0;
    for (int v = 0; v < MAXV; v++) {
      if (!voices[v].on) continue;
      voices[v].t += 1.0 / SR;
      double k = voices[v].t / voices[v].dur;
      if (k >= 1) { voices[v].on = false; continue; }
      double f = voices[v].f0 + (voices[v].f1 - voices[v].f0) * k;
      double ph = voices[v].t * f;
      double w = 0;
      if (voices[v].type == 0) w = (fmod(ph, 1.0) < 0.5 ? 1 : -1);
      else if (voices[v].type == 1) w = 2.0 * fabs(2.0 * fmod(ph, 1.0) - 1.0) - 1.0;
      else w = ((double)(nrand() % 2000) / 1000.0 - 1.0);
      double env = (1 - k) * (k < 0.05 ? k / 0.05 : 1);
      s += w * voices[v].vol * env * 0.4;
    }
    /* musica: basso + arp leggeri */
    {
      extern AbGame G;
      if (!G.mute && G.state == 2) {
        static const double scale[8] = {110, 130.8, 146.8, 164.8, 196, 220, 246.9, 261.6};
        double mt = mus_t;
        int step = mus_step;
        double f = scale[step % 8] / 2.0;
        double ph = mt * f;
        double w = (fmod(ph, 1.0) < 0.5 ? 1 : -1) * 0.05;
        double f2 = scale[(step * 3 + 2) % 8] * 2.0;
        double ph2 = mt * f2;
        double w2 = (fmod(ph2, 1.0) < 0.25 ? 1 : -1) * 0.02;
        s += w + w2;
      }
    }
    if (s > 1) s = 1;
    if (s < -1) s = -1;
    o[i] = (Sint16)(s * 3000);
  }
}

void au_init(void) {
  memset(voices, 0, sizeof voices);
  SDL_AudioSpec want, got;
  memset(&want, 0, sizeof want);
  want.freq = SR;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 1024;
  want.callback = cb;
  dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
  if (dev) SDL_PauseAudioDevice(dev, 0);
}

void au_quit(void) {
  if (dev) { SDL_CloseAudioDevice(dev); dev = 0; }
}

void au_update(double dt) {
  mus_t += dt;
  static double acc = 0;
  acc += dt;
  double step_dur = G.c64 ? 0.16 : 0.24;
  if (acc >= step_dur) { acc = 0; mus_step++; }
}
