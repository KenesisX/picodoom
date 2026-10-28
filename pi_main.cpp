#include <SDL2/SDL.h>
#include <stdint.h>
SDL_Renderer *pd_ren; uint32_t pd_held = 0, pd_prev = 0;
void pd_init(); void pd_update(uint32_t); void pd_draw(uint32_t);
int main() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *w = SDL_CreateWindow("Try Pico Doom", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 720, 720, SDL_WINDOW_RESIZABLE);
  pd_ren = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED); SDL_RenderSetLogicalSize(pd_ren, 720, 720);
  pd_init(); bool run = true; uint32_t tick = 0;
  while (run) {
    SDL_Event e; while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) run = false;
    const Uint8 *k = SDL_GetKeyboardState(nullptr);
    if (k[SDL_SCANCODE_ESCAPE]) run = false;
    pd_prev = pd_held; pd_held = 0;
    if (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]) pd_held |= 1;
    if (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) pd_held |= 2;
    if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]) pd_held |= 4;
    if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) pd_held |= 8;
    if (k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_Z]) pd_held |= 16;
    if (k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_B]) pd_held |= 32;
    if (k[SDL_SCANCODE_X]) pd_held |= 64;
    if (k[SDL_SCANCODE_Y] || k[SDL_SCANCODE_M]) pd_held |= 128;
    pd_update(tick); pd_draw(tick++); SDL_RenderPresent(pd_ren); SDL_Delay(25);
  }
  SDL_Quit(); return 0;
}
