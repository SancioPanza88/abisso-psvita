/* ABISSO Vita - input */
#include "input.h"
#include "abisso.h"
#include <string.h>

int in_mouse_x = 0, in_mouse_y = 0;
bool in_mouse_pressed = false;
bool in_mouse_down = false;
char in_text[64] = {0};
bool in_text_ready = false;
bool in_backspace = false;
bool in_enter = false;
bool in_quit_requested = false;

static SDL_GameController *pad = NULL;
static unsigned held = 0; /* per edge (map/view/help/mute) gestiti dal main via eventi? qui level */

void in_init(void) {
  SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    if (SDL_IsGameController(i)) {
      pad = SDL_GameControllerOpen(i);
      if (pad) break;
    }
  }
  SDL_StartTextInput();
}

void in_quit(void) {
  if (pad) { SDL_GameControllerClose(pad); pad = NULL; }
  SDL_StopTextInput();
}

void in_consume_click(void) { in_mouse_pressed = false; }
void in_consume_text(void) { in_text[0] = 0; in_text_ready = false; in_backspace = false; in_enter = false; }

unsigned in_poll(double dt) {
  (void)dt;
  unsigned k = 0;
  in_mouse_pressed = false;
  in_text_ready = false;
  in_backspace = false;
  in_enter = false;

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) { k |= K_PAUSE; in_quit_requested = true; }
    else if (e.type == SDL_MOUSEMOTION) { in_mouse_x = e.motion.x; in_mouse_y = e.motion.y; }
    else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      in_mouse_x = e.button.x; in_mouse_y = e.button.y;
      in_mouse_down = true; in_mouse_pressed = true;
    } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
      in_mouse_down = false;
    } else if (e.type == SDL_FINGERMOTION) {
      in_mouse_x = (int)(e.tfinger.x * SCR_W);
      in_mouse_y = (int)(e.tfinger.y * SCR_H);
    } else if (e.type == SDL_FINGERDOWN) {
      in_mouse_x = (int)(e.tfinger.x * SCR_W);
      in_mouse_y = (int)(e.tfinger.y * SCR_H);
      in_mouse_down = true; in_mouse_pressed = true;
    } else if (e.type == SDL_FINGERUP) {
      in_mouse_down = false;
    } else if (e.type == SDL_TEXTINPUT) {
      strncpy(in_text, e.text.text, sizeof in_text - 1);
      in_text_ready = true;
    } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
      if (e.key.keysym.sym == SDLK_BACKSPACE) in_backspace = true;
      if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) in_enter = true;
    }
  }

  const Uint8 *kb = SDL_GetKeyboardState(NULL);
  if (kb[SDL_SCANCODE_W] || kb[SDL_SCANCODE_UP]) k |= K_UP;
  if (kb[SDL_SCANCODE_S] || kb[SDL_SCANCODE_DOWN]) k |= K_DOWN;
  if (kb[SDL_SCANCODE_A] || kb[SDL_SCANCODE_LEFT]) k |= K_LEFT;
  if (kb[SDL_SCANCODE_D] || kb[SDL_SCANCODE_RIGHT]) k |= K_RIGHT;
  if (kb[SDL_SCANCODE_SPACE]) k |= K_ATK;
  if (kb[SDL_SCANCODE_E]) k |= K_INTER;
  if (kb[SDL_SCANCODE_Q]) k |= K_POT;
  if (kb[SDL_SCANCODE_R]) k |= K_MANA;
  if (kb[SDL_SCANCODE_F]) k |= K_ABIL;
  /* M/V/H/N edge: il main li gestisce con debounce, qui level va bene */
  if (kb[SDL_SCANCODE_M]) k |= K_MAP;
  if (kb[SDL_SCANCODE_H] || kb[SDL_SCANCODE_F1]) k |= K_HELP;
  if (kb[SDL_SCANCODE_N]) k |= K_MUTE;
  if (kb[SDL_SCANCODE_ESCAPE]) k |= K_PAUSE;
  /* PC: zoom anche con + / - */
  if (kb[SDL_SCANCODE_KP_PLUS] || kb[SDL_SCANCODE_EQUALS]) k |= K_ZIN;
  if (kb[SDL_SCANCODE_KP_MINUS] || kb[SDL_SCANCODE_MINUS]) k |= K_ZOUT;

  /* controller SDL (su Vita: stick + tasti mappati da SDL) */
  if (pad) {
    Sint16 ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    Sint16 ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    if (ax < -6000) k |= K_LEFT;
    if (ax > 6000) k |= K_RIGHT;
    if (ay < -6000) k |= K_UP;
    if (ay > 6000) k |= K_DOWN;
    /* D-pad */
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) k |= K_UP;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) k |= K_DOWN;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) k |= K_LEFT;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) k |= K_RIGHT;
    /* X(croce)=A attacco, O=B interag, quadrato=X pozione, triangolo=Y mana */
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)) k |= K_ATK;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)) k |= K_INTER;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X)) k |= K_POT;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y)) k |= K_MANA;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) k |= K_ABIL;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) k |= K_MAP;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK)) k |= K_HELP;
    /* stick destro su/giu = zoom (non muove) */
    Sint16 rz = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
    Sint16 rx2 = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
    if (rz < -8000 || rx2 > 8000) k |= K_ZIN;
    if (rz > 8000 || rx2 < -8000) k |= K_ZOUT;
  } else {
    /* joystick grezzo (fallback Vita se controller db manca) */
    if (SDL_NumJoysticks() > 0) {
      SDL_Joystick *j = SDL_JoystickOpen(0);
      if (j) {
        Sint16 ax = SDL_JoystickGetAxis(j, 0);
        Sint16 ay = SDL_JoystickGetAxis(j, 1);
        if (ax < -6000) k |= K_LEFT;
        if (ax > 6000) k |= K_RIGHT;
        if (ay < -6000) k |= K_UP;
        if (ay > 6000) k |= K_DOWN;
        /* bottoni Vita: 0 croce,1 cerchio,2 quadrato,3 triangolo,4 L,5 R,8 select,9 start */
        if (SDL_JoystickGetButton(j, 0)) k |= K_ATK;
        if (SDL_JoystickGetButton(j, 1)) k |= K_INTER;
        if (SDL_JoystickGetButton(j, 2)) k |= K_POT;
        if (SDL_JoystickGetButton(j, 3)) k |= K_MANA;
        if (SDL_JoystickGetButton(j, 5)) k |= K_ABIL;
        if (SDL_JoystickGetButton(j, 9)) k |= K_MAP;
        if (SDL_JoystickGetButton(j, 8)) k |= K_HELP;
        /* stick destro su/giu = zoom */
        if (SDL_JoystickGetAxis(j, 3) < -8000 || SDL_JoystickGetAxis(j, 2) > 8000) k |= K_ZIN;
        if (SDL_JoystickGetAxis(j, 3) > 8000 || SDL_JoystickGetAxis(j, 2) < -8000) k |= K_ZOUT;
        SDL_JoystickClose(j);
      }
    }
  }

  held = k;
  return k;
}
