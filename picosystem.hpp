// PicoSystem-API shim for desktop / Raspberry Pi using SDL2
#pragma once
#include <SDL2/SDL.h>
#include <stdint.h>
extern SDL_Renderer *pd_ren; extern uint32_t pd_held, pd_prev;
#define PD_SCALE 6
namespace picosystem {
  static const uint32_t UP=1,DOWN=2,LEFT=4,RIGHT=8,A=16,B=32,X=64,Y=128;
  inline void pen(int r,int g,int b,int a=15){(void)a;SDL_SetRenderDrawColor(pd_ren,r*17,g*17,b*17,255);}
  inline void frect(int x,int y,int w,int h){SDL_Rect q={x*PD_SCALE,y*PD_SCALE,w*PD_SCALE,h*PD_SCALE};SDL_RenderFillRect(pd_ren,&q);}
  inline bool button(uint32_t b){return pd_held&b;}
  inline bool pressed(uint32_t b){return (pd_held&b)&&!(pd_prev&b);}
  inline uint32_t time(){return SDL_GetTicks();}
  inline void backlight(int){}
  struct voice_t{int f;};
  inline voice_t voice(uint32_t,uint32_t,uint32_t,uint32_t,int32_t,uint32_t,uint32_t,uint32_t){return {0};}
  inline void play(voice_t,uint32_t,uint32_t,uint32_t){}
}
