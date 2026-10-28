// ===========================================================================
//  PICODOOM  -  a Doom-style raycaster for the Pimoroni PicoSystem (RP2040)
//  100% original code (no id Software code or assets).
//
//  Controls
//    D-pad UP/DOWN  walk (hold UP to build up speed)   LEFT/RIGHT  turn
//    B + LEFT/RIGHT strafe        A  fire        X  switch weapon
//    Y  toggle minimap
//
//  Build flags (see CMakeLists.txt):  PD_SOUND=1 sound, PD_SAVE=1 saved high scores.
// ===========================================================================
#include "picosystem.hpp"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
using namespace picosystem;

#ifndef PD_SOUND
#define PD_SOUND 0
#endif

#define SW 120
#define SH 120
#define HORIZ 60
#define TS 32
#define MW 24
#define MH 24
#define NTEX 10
#define FOV 0.66f

#ifndef PD_SAVE
#define PD_SAVE 1
#endif
#if PD_SAVE
#include "hardware/flash.h"
#include "hardware/sync.h"
#endif

#include "levels.h"

namespace pd {

// ------------------------------------------------------------ framebuffer
typedef uint16_t col_t;                      // 0x0RGB, 4 bits per channel
static col_t fb[SW * SH];

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline col_t C(int r, int g, int b) {
  return (col_t)((clampi(r, 0, 15) << 8) | (clampi(g, 0, 15) << 4) | clampi(b, 0, 15));
}
static inline col_t shadeC(col_t c, int s) {          // s: 16 = unchanged
  int r = (((c >> 8) & 15) * s) >> 4, g = (((c >> 4) & 15) * s) >> 4, b = ((c & 15) * s) >> 4;
  if (r > 15) r = 15; if (g > 15) g = 15; if (b > 15) b = 15;
  return (col_t)((r << 8) | (g << 4) | b);
}
static void fillR(int x, int y, int w, int h, col_t c) {
  int x0 = clampi(x, 0, SW), x1 = clampi(x + w, 0, SW), y0 = clampi(y, 0, SH), y1 = clampi(y + h, 0, SH);
  for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) fb[yy * SW + xx] = c;
}
static inline void put(int x, int y, col_t c) { if ((unsigned)x < SW && (unsigned)y < SH) fb[y * SW + x] = c; }
// horizontal "cylinder" gradient rectangle - gives weapons a rounded look
static void gradR(int x, int y, int w, int h, int r, int g, int b) {
  for (int i = 0; i < w; i++) {
    float t = (i + 0.5f) / w;
    int s = 6 + (int)(10.0f * sinf(3.14159f * t));
    fillR(x + i, y, 1, h, shadeC(C(r, g, b), s));
  }
}
static void present() {                       // blit through the PicoSystem API
  for (int y = 0; y < SH; y++) {
    const col_t *row = &fb[y * SW];
    int x = 0;
    while (x < SW) {
      col_t c = row[x]; int e = x + 1;
      while (e < SW && row[e] == c) e++;
      pen((c >> 8) & 15, (c >> 4) & 15, c & 15);
      frect(x, y, e - x, 1);
      x = e;
    }
  }
}

// ------------------------------------------------------------------- rng
static uint32_t rs = 2463534242u;
static inline uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static inline float rf() { return (rnd() & 0xffff) / 65535.0f; }
static uint32_t hsh(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }

// ----------------------------------------------------------------- font
static const char *FONT[43] = {
 "111101101101111","010110010010111","111001111100111","111001111001111","101101111001001",
 "111100111001111","111100111101111","111001001001001","111101111101111","111101111001111",
 "010101111101101","110101110101110","011100100100011","110101101101110","111100110100111",
 "111100110100100","011100101101011","101101111101101","111010010010111","001001001101010",
 "101101110101101","100100100100111","101111111101101","110101101101101","010101101101010",
 "110101110100100","010101101111011","110101110101101","011100010001110","111010010010010",
 "101101101101111","101101101101010","101101111111101","101101010101101","101101010010010",
 "111001010100111","000000000000000","000010000010000","000010111010000","000000111000000",
 "010010010000010","000000000000010","001001010100100"};
static int fontIdx(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c >= 'A' && c <= 'Z') return 10 + c - 'A';
  switch (c) { case ':': return 37; case '+': return 38; case '-': return 39; case '!': return 40; case '.': return 41; case '/': return 42; }
  return 36;
}
static int txtW(const char *s, int sc) { return (int)strlen(s) * 4 * sc - sc; }
static void txt(const char *s, int x, int y, int sc, col_t c) {
  for (; *s; s++) {
    const char *g = FONT[fontIdx(*s)];
    for (int r = 0; r < 5; r++) for (int k = 0; k < 3; k++) if (g[r * 3 + k] == '1') fillR(x + k * sc, y + r * sc, sc, sc, c);
    x += 4 * sc;
  }
}
static void txtS(const char *s, int x, int y, int sc, col_t c) { txt(s, x + 1, y + 1, sc, C(0, 0, 0)); txt(s, x, y, sc, c); }
static void txtC(const char *s, int y, int sc, col_t c) { txtS(s, (SW - txtW(s, sc)) / 2, y, sc, c); }

// ------------------------------------------------------------- textures
static col_t tex[NTEX][TS * TS];   // 0 brick 1 steel 2 tech 3 exit 4 door 5 floor 6 ceiling
static void buildTextures() {
  for (int y = 0; y < TS; y++) for (int x = 0; x < TS; x++) {
    int i = y * TS + x;
    { // brick
      int row = y / 8, off = (row & 1) * 8, bx = (x + off) & 15, bid = row * 7 + ((x + off) >> 4);
      int n = hsh(bid) % 3;
      if ((y & 7) == 0 || bx == 0) tex[0][i] = C(5, 5, 4);
      else {
        int v = n + (((y & 7) == 1) ? 1 : 0) - (((y & 7) == 7) ? 1 : 0) - (((hsh(x * 31 + y * 17 + bid) & 7) == 0) ? 1 : 0);
        tex[0][i] = C(9 + v, 4 + v / 2, 3 + v / 2);
      }
    }
    { // steel
      int a = x & 15, b = y & 15; col_t c = C(7, 8, 10);
      if (a == 0 || b == 0) c = C(11, 12, 14);
      else if (a == 15 || b == 15) c = C(3, 4, 5);
      else if ((hsh(x + y * 61) & 7) == 0) c = C(6, 7, 9);
      if ((a == 3 || a == 12) && (b == 3 || b == 12)) c = C(13, 14, 15);
      tex[1][i] = c;
    }
    { // tech panels with blinking lights
      int cx = x >> 3, cy = y >> 3, a = x & 7, b = y & 7; col_t c = C(2, 3, 6);
      if (a == 0 || b == 0) c = C(1, 1, 3);
      else if (a >= 2 && a <= 5 && b >= 2 && b <= 5) {
        uint32_t h = hsh(cx * 5 + cy * 13);
        if (h % 5 == 0) c = C(3, 14, 15); else if (h % 3 == 0) c = C(6, 7, 11); else if (h % 7 == 0) c = C(14, 5, 3);
      }
      tex[2][i] = c;
    }
    { // exit
      col_t c = C(1, 8, 2);
      if (x < 3 || x > 28 || y < 3 || y > 28) c = C(13, 15, 13);
      else { int d = y - (x > 16 ? x - 16 : 16 - x); if (((d + 64) >> 2) & 1) c = C(13, 15, 11); }
      tex[3][i] = c;
    }
    { // door
      col_t c = C(8, 8, 9);
      if (x < 2 || x > 29) c = C(4, 4, 5);
      else if ((y & 7) == 0) c = C(5, 5, 6);
      if (y >= 12 && y < 20 && x >= 2 && x <= 29) c = (((x + y) >> 2) & 1) ? C(15, 13, 1) : C(2, 2, 2);
      if ((x == 5 || x == 26) && (y == 4 || y == 27)) c = C(13, 13, 14);
      tex[4][i] = c;
    }
    for (int k = 0; k < 3; k++) {   // locked doors: red, blue, yellow
      static const int kc[3][3] = {{14, 2, 2}, {3, 5, 15}, {15, 14, 2}};
      col_t c = C(kc[k][0] * 2 / 3, kc[k][1] * 2 / 3, kc[k][2] * 2 / 3);
      if (x < 2 || x > 29) c = C(4, 4, 5);
      else if ((y & 7) == 0) c = C(kc[k][0] / 3, kc[k][1] / 3, kc[k][2] / 3);
      if (y >= 12 && y < 20 && x >= 2 && x <= 29) c = (((x + y) >> 2) & 1) ? C(kc[k][0], kc[k][1], kc[k][2]) : C(2, 2, 2);
      if (y >= 3 && y <= 8 && x >= 13 && x <= 18) c = C(kc[k][0], kc[k][1], kc[k][2]);
      tex[7 + k][i] = c;
    }
    { // floor
      int cell = ((x >> 4) ^ (y >> 4)) & 1; col_t c = cell ? C(5, 5, 6) : C(4, 4, 5);
      if ((x & 15) == 0 || (y & 15) == 0) c = C(2, 2, 3);
      else if ((hsh(x * 7 + y * 131) & 7) == 0) c = C(3, 3, 4);
      tex[5][i] = c;
    }
    { // ceiling
      col_t c = C(3, 3, 5);
      if ((x & 15) == 0 || (y & 15) == 0) c = C(1, 1, 3);
      if ((x >> 4) == 1 && (y >> 4) == 1 && (x & 15) >= 3 && (x & 15) <= 12 && (y & 15) >= 3 && (y & 15) <= 12) c = C(14, 14, 11);
      tex[6][i] = c;
    }
  }
}

// -------------------------------------------------------------- sprites
#define NSPR 15
static const char *SPR[NSPR][16] = {
 { "....kk....kk....","....kBk..kBk....","....kBBkkBBk....","...kBBBBBBBBk...","...kBrrBBrrBk...","...kBBBBBBBBk...","....kBwBBwBk....","..kkBBBBBBBBkk..",
   ".kBBBBbBBbBBBBk.",".kBBkBbBBbBkBBk.","..kk.kBBBBk.kk..","......kBBBBk....","......kBkkBk....",".....kBBk.kBBk..",".....kBBk.kBBk..","....kkkk...kkkk." },
 { "....kk....kk....","....kBk..kBk....","....kBBkkBBk....","...kBBBBBBBBk...","...kBrrBBrrBk...","...kBBBBBBBBk...","....kBwBBwBk....","..kkBBBBBBBBkk..",
   ".kBBBBbBBbBBBBk.",".kBBkBbBBbBkBBk.","..kk.kBBBBk.kk..","......kBBBBk....","......kBkkBk....","....kBBk...kBBk.","....kBBk....kBBk","...kkkk.....kkkk" },
 { "....kk....kk....","....kBk..kBk....","....kBBkkBBk....","...kBBBBBBBBk...","...kByyBByyBk...","...kBBBBBBBBk...","....krrrrrrk....","..kkBwwwwwwBkk..",
   ".kBBBBbBBbBBBBk.",".kBBkBbBBbBkBBk.","..kk.kBBBBk.kk..","......kBBBBk....","......kBkkBk....",".....kBBk.kBBk..",".....kBBk.kBBk..","....kkkk...kkkk." },
 { "................","................","................","................","................","................","................","................",
   "......kkkk......","....kkBBBBkk....","...kBBrBBrBBk...","..kBBBBBBBBBBk..","..kBBBkBBkBBBk..",".kBBBk....kBBBk.",".kkkk......kkkk.","................" },
 { "................","................","................","................","................","................","................","................",
   "................","................","................","................","....kkkkkkkk....","..kkBBBBBBBBkk..",".kBBBrBBBBrBBBk.","kkkkkkkkkkkkkkkk" },
 { "................","................","................","................","................","................","................","................",
   "..kkkkkkkkkkkk..","..kwwwwwwwwwwk..","..kwwwwrrwwwwk..","..kwwwrrrrwwwk..","..kwwwrrrrwwwk..","..kwwwwrrwwwwk..","..kwwwwwwwwwwk..","..kkkkkkkkkkkk.." },
 { "................","................","................","................","................","................","................","................",
   "..kkkkkkkkkkkk..","..kyyyyyyyyyyk..","..kykykykykyyk..","..kyyyyyyyyyyk..","..kGGGGGGGGGGk..","..kGGGGGGGGGGk..","..kGGGGGGGGGGk..","..kkkkkkkkkkkk.." },
 { "................","................","................","................","................","................","................","................",
   "................","................","................",".kkkkkkkkkkkkkk.","kddddddddddBBBBk",".kkkkkkkkkkkkkk.","................","................" },
 { "................","................","................","................","......kkkk......","....kkOOOOkk....","...kOOyyyyOOk...","..kOOyyyyyyOOk..",
   "..kOOyyyyyyOOk..","...kOOyyyyOOk...","....kkOOOOkk....","......kkkk......","................","................","................","................" },
 // 9 floater idle
 { "................",".....kkkkkk.....","...kkBBBBBBkk...","..kBBBBBBBBBBk..",".kBBBBkkkkBBBBk.",".kBBBkwwwwkBBBk.",".kBBBkwkkwkBBBk.",".kBBBBkwwkBBBBk.",
   ".kBBBBBkkBBBBBk.","..kBBbBBBBbBBk..","..kBBBBBBBBBBk..","...kBrrrrrrBk...","...kBrwrwrwBk...","....kkBBBBkk....","......kkkk......","................" },
 // 10 floater attack
 { "................",".....kkkkkk.....","...kkBBBBBBkk...","..kBBBBBBBBBBk..",".kBBBBkkkkBBBBk.",".kBBBkyyyykBBBk.",".kBBBkykkykBBBk.",".kBBBBkyykBBBBk.",
   ".kBBBBBkkBBBBBk.","..kBBbBBBBbBBk..","..kBBBBBBBBBBk..","...krrrrrrrrk...","...krwrwrwrwk...","....krrrrrrk....","......kkkk......","................" },
 // 11 keycard
 { "................","................","................","................","................","................","................","................",
   "..kkkkkkkkkkkk..","..kBBBBBBBBBBk..","..kBBBBBBBBBBk..","..kkkkkkkkBBBk..","..kBBBBBBBBBBk..","..kbbbbbbbbbbk..","..kkkkkkkkkkkk..","................" },
 // 12 chaingun pickup
 { "................","................","................","................","................","................","................","................",
   "................","................","................",".kkkkkkkkkkkkkk.","kddddddddddddddk","kBBBBddddddddddk",".kkkkkkkkkkkkkk.","................" },
 // 13 armor vest
 { "................","................","................","................","................","................","................","................",
   "...kkk....kkk...","..kGGGkkkkGGGk..","..kGGGGGGGGGGk..","..kGGGGGGGGGGk..","..kGGGddddGGGk..","...kGGGGGGGGk...","....kkkkkkkk....","................" },
 // 14 crawler
 { "................","................","................","................","................","................","................","................",
   "................","......kkkk......","....kkBBBBkk....","..kkBBrBBrBBkk..",".kBBBBBBBBBBBBk.","kBBkkBBBBBBkkBBk","kkk.kkBBBBkk.kkk","..kk..kkkk..kk.." } };
static uint8_t spr[NSPR][256];
static col_t pal[7][11];
static void buildSprites() {
  const char *key = "kBborywOGd";
  for (int s = 0; s < NSPR; s++) for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
    char c = SPR[s][y][x]; const char *p = strchr(key, c);
    spr[s][y * 16 + x] = (c == '.' || !p) ? 0 : (uint8_t)(p - key + 1);
  }
  // 0:k 1:B 2:b 3:o 4:r 5:y 6:w 7:O 8:G 9:d   (index+1 in spr)
  col_t common[11] = {0, C(1, 1, 2), 0, 0, 0, C(15, 2, 2), C(15, 14, 3), C(14, 14, 14), C(15, 8, 1), C(3, 9, 3), C(6, 6, 7)};
  for (int p = 0; p < 7; p++) for (int i = 0; i < 11; i++) pal[p][i] = common[i];
  pal[0][2] = C(11, 5, 3); pal[0][3] = C(8, 3, 2); pal[0][4] = C(13, 9, 6);   // imp
  pal[1][2] = C(13, 6, 7); pal[1][3] = C(9, 3, 5); pal[1][4] = C(14, 9, 9);   // demon
  pal[2][2] = C(4, 9, 3);  pal[2][3] = C(2, 6, 2); pal[2][4] = C(11, 13, 6);  // baron
  pal[3][2] = C(13, 4, 3); pal[3][3] = C(9, 1, 1); pal[3][4] = C(15, 9, 6);   // floater
  pal[4][2] = C(15, 2, 2); pal[4][3] = C(9, 0, 0);                            // red key
  pal[5][2] = C(4, 6, 15); pal[5][3] = C(1, 2, 9);                            // blue key
  pal[6][2] = C(15, 14, 2); pal[6][3] = C(10, 8, 0);                          // yellow key
}

// ---------------------------------------------------------------- game
enum State { TITLE, PLAY, DEAD, DONE, WIN };
struct EType { int hp; float speed; float rad; float h; int pal; };
static const EType ET[5] = { {30, 0.9f, 0.30f, 0.85f, 0}, {50, 2.1f, 0.35f, 0.90f, 1}, {220, 0.6f, 0.50f, 1.45f, 2}, {60, 1.3f, 0.38f, 0.85f, 3}, {35, 1.3f, 0.32f, 0.55f, 1} };
struct Ent { float x, y, cd, flash, walk, atk, deadT; int hp, type; bool alive, alert; };
struct Item { float x, y; char type; bool on; };
struct Proj { float x, y, vx, vy, life; int dmg; bool on, pl; };
#define MAXE 28
#define MAXI 32
#define MAXP 24

static char map[MH][MW];
static float doorO[MH][MW];
static int doorX[32], doorY[32], nDoor; static char doorC[32];
static Ent ents[MAXE]; static int nEnt;
static Item items[MAXI]; static int nItem;
static Proj projs[MAXP];
static float zbuf[SW];
static float px, py, pa, exitX, exitY;
static int hp, ammo, armor, kills, totalKills, enemiesInLevel, level, weapon, score, levelScore, lastTime;
static bool hasW[4], keys[3], showMap, newBest, newHi;
static float levelTime;
static const char *WNAME[4] = {"PISTOL", "SHOTGUN", "CHAINGUN", "ROCKET"};
static const char *TAUNT[34] = {"TRY HARDER NEXT TIME LOSER","YOU SUCK","WOW TERRIBLE","YOURE LAME","BETTER LUCK NEXT TIME","SO SORRY","DISRESPECTFUL","IN YOUR FACE WITH A CAN OF MACE","FATALITY","GO TO HELL","CYA!","GOBLIN GOT YOUR TONGUE","DONNA DONNA","GAME OVER MAN","ITS OVER LOSER","LOST AGAIN HOW SAD","PATHETIC","DEAD AND LAME","THE DEMONS SEND THEIR REGARDS","UNINSTALL THE GAME","WAS THAT YOUR BEST","SKILL ISSUE","GIT GUD","YOUR CORPSE IS MORE USEFUL","HELL HAS A SPOT FOR YOU","REST IN PIECES","YOU PLAY LIKE A ZOMBIE","TRY USING THE FIRE BUTTON","THAT WAS EMBARRASSING","TOTAL LOSER MOVE","THE END LOSER","YOURE FOOD NOW","NOBODY WILL MISS YOU","YOUR FAMILY WILL BE DISAPPOINTED"};
static const char *taunt = "FATALITY";
static const int SUIT[6][3] = {{12,2,2},{3,5,13},{2,10,3},{3,3,3},{9,3,12},{14,7,0}};
static int chGen, chSuit;
static float pwR, pwD, pwE;
struct Fx { float x, y, t; };
static Fx fxs[8];
static const char *KNAME[3] = {"NEED RED KEY", "NEED BLUE KEY", "NEED YELLOW KEY"};

// ----- saved high scores (last flash sector)
struct SaveData { uint32_t magic; uint32_t hi; uint16_t best[8]; uint32_t check; };
static SaveData sv;
__attribute__((unused)) static uint32_t svCheck() { uint32_t c = sv.magic ^ (sv.hi * 2654435761u); for (int i = 0; i < 8; i++) c = c * 31 + sv.best[i]; return c; }
#if PD_SAVE
static const uint32_t SAVE_OFF = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
#endif
static void loadSave() {
  memset(&sv, 0, sizeof sv);
#if PD_SAVE
  SaveData tmp; memcpy(&tmp, (const void *)(XIP_BASE + SAVE_OFF), sizeof tmp);
  SaveData keep = sv; sv = tmp;
  if (sv.magic != 0x50444F4Du || sv.check != svCheck()) { sv = keep; sv.magic = 0x50444F4Du; }
#endif
}
static void storeSave() {
#if PD_SAVE
  sv.magic = 0x50444F4Du; sv.check = svCheck();
  uint8_t page[FLASH_PAGE_SIZE]; memset(page, 0xFF, sizeof page); memcpy(page, &sv, sizeof sv);
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(SAVE_OFF, FLASH_SECTOR_SIZE);
  flash_range_program(SAVE_OFF, page, FLASH_PAGE_SIZE);
  restore_interrupts(ints);
#endif
}
static State state = TITLE;
static float fireT, hurtT, pickT, flashT, bobT, runT, msgT, painT, stateT, faceLook;
static const char *msg = "";
static int frameCount;

// ----- sound (optional)
#if PD_SOUND
static voice_t vShot, vShot2, vHurt, vPick, vDoor, vDie, vChain, vBall, vLock, vWin;
#endif
static void sfx(int id) {
#if PD_SOUND
  switch (id) {
    case 0: play(vShot, 220, 90, 100); break;
    case 1: play(vShot2, 120, 180, 100); break;
    case 2: play(vHurt, 150, 120, 100); break;
    case 3: play(vPick, 880, 120, 80); break;
    case 4: play(vDoor, 110, 250, 70); break;
    case 5: play(vDie, 200, 250, 90); break;
    case 6: play(vChain, 180, 60, 90); break;
    case 7: play(vBall, 330, 160, 70); break;
    case 8: play(vLock, 90, 150, 80); break;
    case 9: play(vWin, 660, 500, 90); break;
  }
#else
  (void)id;
#endif
}
static void say(const char *m) { msg = m; msgT = 2.0f; }

static inline bool isWallCh(char c) { return c == '#' || c == '%' || c == 'W' || c == 'E'; }
static inline bool isDoor(char c) { return c == 'D' || c == 'R' || c == 'U' || c == 'Y'; }
static inline bool solid(int x, int y) {
  if ((unsigned)x >= MW || (unsigned)y >= MH) return true;
  char c = map[y][x];
  if (isDoor(c)) return doorO[y][x] < 0.8f;
  return isWallCh(c);
}
static bool canMoveR(float x, float y, float r) {
  return !solid((int)(x - r), (int)(y - r)) && !solid((int)(x + r), (int)(y - r)) &&
         !solid((int)(x - r), (int)(y + r)) && !solid((int)(x + r), (int)(y + r));
}
static bool los(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0, d = sqrtf(dx * dx + dy * dy);
  int n = (int)(d / 0.2f) + 1;
  for (int i = 1; i < n; i++) { float t = (float)i / n; if (solid((int)(x0 + dx * t), (int)(y0 + dy * t))) return false; }
  return true;
}

static void loadLevel(int l) {
  nEnt = nItem = nDoor = 0; kills = 0; levelTime = 0; keys[0] = keys[1] = keys[2] = false;
  memset(projs, 0, sizeof projs); memset(fxs, 0, sizeof fxs); memset(doorO, 0, sizeof doorO);
  for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
    char c = LEVELS[l][y][x]; float cx = x + 0.5f, cy = y + 0.5f;
    if (c == 'P') { px = cx; py = cy; pa = LEVEL_ANG[l]; c = '.'; }
    else if ((c == 'i' || c == 'd' || c == 'B' || c == 'f' || c == 'c') && nEnt < MAXE) {
      int t = c == 'i' ? 0 : (c == 'd' ? 1 : (c == 'f' ? 3 : (c == 'c' ? 4 : 2)));
      ents[nEnt++] = {cx, cy, 1.5f + rf(), 0, rf() * 6, 0, 0, ET[t].hp, t, true, false}; c = '.';
    } else if (strchr("hasgvruyxzeb", c) && nItem < MAXI) { items[nItem++] = {cx, cy, c, true}; c = '.'; }
    else if (c == 'E') { exitX = cx; exitY = cy; }
    else if (isDoor(c) && nDoor < 32) { doorX[nDoor] = x; doorY[nDoor] = y; doorC[nDoor] = c; nDoor++; }
    map[y][x] = c;
  }
  enemiesInLevel = nEnt;
  fireT = hurtT = pickT = flashT = painT = 0;
}
static void newGame() {
  hp = 100; ammo = 24; armor = 0; level = 0; totalKills = 0; score = 0; weapon = 0;
  hasW[0] = true; hasW[1] = hasW[2] = hasW[3] = false; pwR = pwD = pwE = 0; showMap = false;
  loadLevel(0); state = PLAY; stateT = 0; say(LEVEL_NAMES[0]);
}
static void hurtPlayer(int d) {
  if (state != PLAY) return;
  if (armor > 0) { int ab = d / 2; if (ab > armor) ab = armor; armor -= ab; d -= ab; }
  hp -= d; hurtT = 0.35f; painT = 0.5f; sfx(2);
  if (hp <= 0) { hp = 0; state = DEAD; stateT = 0; taunt = TAUNT[rnd() % 34]; }
}

// ----- weapons / combat
static void spawnProj(float x, float y, float ang, float spd, int dmg, bool pl = false) {
  for (int i = 0; i < MAXP; i++) if (!projs[i].on) {
    projs[i] = {x, y, cosf(ang) * spd, sinf(ang) * spd, 4.0f, dmg, true, pl}; return;
  }
}
static void dmgEnt(Ent &e, int d) {
  e.hp -= d; e.flash = 0.12f; e.alert = true;
  if (e.hp <= 0 && e.alive) { e.alive = false; e.deadT = 0; kills++; totalKills++; sfx(5); }
}
static void boom(float x, float y, float r, int d) {
  for (int i = 0; i < nEnt; i++) if (ents[i].alive) {
    float ex = ents[i].x - x, ey = ents[i].y - y, q = sqrtf(ex * ex + ey * ey);
    if (q < r) dmgEnt(ents[i], (int)(d * (1.0f - q / r * 0.6f)));
  }
  for (int i = 0; i < 8; i++) if (fxs[i].t <= 0) { fxs[i] = {x, y, 0.45f}; break; }
  flashT = 0.25f; sfx(1);
}
static void hitscan(float ang, int dmg) {
  float dx = cosf(ang), dy = sinf(ang); int best = -1; float bd = 99;
  for (int i = 0; i < nEnt; i++) {
    if (!ents[i].alive) continue;
    float ex = ents[i].x - px, ey = ents[i].y - py, fwd = ex * dx + ey * dy;
    if (fwd < 0.1f || fwd > 14.0f) continue;
    float side = fabsf(-ex * dy + ey * dx);
    if (side < ET[ents[i].type].rad && fwd < bd && los(px, py, ents[i].x, ents[i].y)) { bd = fwd; best = i; }
  }
  if (best < 0) return;
  dmgEnt(ents[best], dmg * (pwD > 0 ? 2 : 1));
  if (pwE > 0) boom(ents[best].x, ents[best].y, 1.8f, 25);
}
static void fire() {
  if (fireT > 0) return;
  static const int COST[4] = {1, 2, 1, 3};
  bool rp = pwR > 0; int cost = rp ? 0 : COST[weapon];
  if (ammo < cost) { say("NO AMMO!"); fireT = 0.3f; return; }
  ammo -= cost; flashT = 0.12f; float cdm = rp ? 0.3f : 1.0f;
  if (weapon == 0) { fireT = 0.32f * cdm; hitscan(pa + (rf() - 0.5f) * 0.03f, 12 + rnd() % 8); sfx(0); }
  else if (weapon == 2) { fireT = 0.1f * cdm; hitscan(pa + (rf() - 0.5f) * 0.07f, 8 + rnd() % 6); sfx(6); }
  else if (weapon == 3) { fireT = 0.9f * cdm; spawnProj(px, py, pa, 9.0f, 80 * (pwD > 0 ? 2 : 1), true); flashT = 0.2f; sfx(1); }
  else { fireT = 0.85f * cdm; for (int i = 0; i < 7; i++) hitscan(pa + (rf() - 0.5f) * 0.26f, 8 + rnd() % 7); sfx(1); }
}

// ----- per-frame update
static void updateEnemies(float dt) {
  for (int i = 0; i < nEnt; i++) {
    Ent &e = ents[i];
    if (!e.alive) { e.deadT += dt; continue; }
    const EType &T = ET[e.type];
    e.walk += dt; if (e.flash > 0) e.flash -= dt; if (e.atk > 0) e.atk -= dt;
    float dx = px - e.x, dy = py - e.y, d = sqrtf(dx * dx + dy * dy);
    bool see = d < 12.0f && los(e.x, e.y, px, py);
    if (see) e.alert = true;
    if (!e.alert || d > 20.0f) continue;
    float ux = dx / (d + 0.001f), uy = dy / (d + 0.001f);
    bool approach = (e.type == 1 || e.type == 4) ? d > 0.9f : d > (e.type == 2 ? 4.0f : (e.type == 3 ? 3.0f : 2.2f));
    if (approach) {
      float nx = e.x + ux * T.speed * dt, ny = e.y + uy * T.speed * dt;
      if (canMoveR(nx, e.y, 0.25f)) e.x = nx;
      if (canMoveR(e.x, ny, 0.25f)) e.y = ny;
    }
    e.cd -= dt;
    if (e.cd <= 0 && see) {
      if (e.type == 1 || e.type == 4) {
        if (d < 1.3f) { e.cd = 0.8f; e.atk = 0.3f; hurtPlayer(8 + rnd() % 7); }
      } else if (e.type == 0 && d < 11.0f) {
        e.cd = 1.5f + rf() * 1.2f; e.atk = 0.45f; spawnProj(e.x, e.y, atan2f(dy, dx), 4.2f, 6 + rnd() % 7); sfx(7);
      } else if (e.type == 3 && d < 12.0f) {
        e.cd = 2.0f + rf(); e.atk = 0.5f; spawnProj(e.x, e.y, atan2f(dy, dx), 4.0f, 10 + rnd() % 9); sfx(7);
      } else if (e.type == 2 && d < 14.0f) {
        e.cd = 1.7f; e.atk = 0.6f; float a = atan2f(dy, dx);
        for (int k = -1; k <= 1; k++) spawnProj(e.x, e.y, a + k * 0.2f, 4.6f, 10 + rnd() % 7);
        sfx(7);
      }
    }
  }
}
static void updateProjs(float dt) {
  for (int i = 0; i < MAXP; i++) {
    Proj &p = projs[i]; if (!p.on) continue;
    p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt;
    if (p.pl) {
      bool hit = p.life <= 0 || solid((int)p.x, (int)p.y);
      for (int k = 0; k < nEnt && !hit; k++) if (ents[k].alive) {
        float ex = ents[k].x - p.x, ey = ents[k].y - p.y; if (ex * ex + ey * ey < 0.25f) hit = true;
      }
      if (hit) { p.on = false; boom(p.x, p.y, 2.2f, p.dmg); }
      continue;
    }
    if (p.life <= 0 || solid((int)p.x, (int)p.y)) { p.on = false; continue; }
    float dx = p.x - px, dy = p.y - py;
    if (dx * dx + dy * dy < 0.16f) { p.on = false; hurtPlayer(p.dmg); }
  }
}
static void updateDoors(float dt) {
  for (int i = 0; i < nDoor; i++) {
    int x = doorX[i], y = doorY[i]; float &o = doorO[y][x];
    char t = doorC[i]; int need = t == 'R' ? 0 : (t == 'U' ? 1 : (t == 'Y' ? 2 : -1));
    float dx = x + 0.5f - px, dy = y + 0.5f - py, d2 = dx * dx + dy * dy;
    bool near = d2 < 2.4f;
    if (need >= 0 && near && !keys[need]) { near = false; if (msgT <= 0 && d2 < 2.0f) { say(KNAME[need]); sfx(8); } }
    for (int k = 0; k < nEnt && !near && need < 0; k++) if (ents[k].alive && ents[k].alert) {
      float ex = x + 0.5f - ents[k].x, ey = y + 0.5f - ents[k].y; near = ex * ex + ey * ey < 1.0f;
    }
    if (near) { if (o == 0.0f) sfx(4); o += dt * 2.2f; if (o > 1.0f) o = 1.0f; }
    else if (o > 0 && d2 > 9.0f) { o -= dt * 1.2f; if (o < 0) o = 0; }
  }
}
static void finishLevel() {
  int t = (int)levelTime; lastTime = t;
  levelScore = kills * 100 + (kills == enemiesInLevel ? 200 : 0) + (LEVEL_PAR[level] > t ? (LEVEL_PAR[level] - t) * 5 : 0);
  score += levelScore;
  bool dirty = false; newBest = newHi = false;
  if (sv.best[level] == 0 || t < sv.best[level]) { sv.best[level] = (uint16_t)(t > 65535 ? 65535 : t); newBest = true; dirty = true; }
  bool last = level + 1 >= NLEVELS;
  if (last && score > (int)sv.hi) { sv.hi = (uint32_t)score; newHi = true; dirty = true; }
  if (dirty) storeSave();
  state = last ? WIN : DONE; stateT = 0; sfx(9);
}
static void updatePlay(float dt) {
  float dx = cosf(pa), dy = sinf(pa), mx = 0, my = 0, turn = 0;
  bool strafe = button(B);
  if (button(UP)) { runT += dt; if (runT > 1.5f) runT = 1.5f; } else runT = 0;
  float spd = (2.0f + (runT > 0.5f ? 1.4f * (runT - 0.5f) / 1.0f : 0.0f)) * dt;
  if (button(UP))   { mx += dx * spd; my += dy * spd; }
  if (button(DOWN)) { mx -= dx * spd * 0.8f; my -= dy * spd * 0.8f; }
  if (strafe) {
    if (button(LEFT))  { mx += dy * spd * 0.8f;  my -= dx * spd * 0.8f; }
    if (button(RIGHT)) { mx -= dy * spd * 0.8f;  my += dx * spd * 0.8f; }
  } else {
    if (button(LEFT))  turn -= 2.5f * dt;
    if (button(RIGHT)) turn += 2.5f * dt;
  }
  pa += turn; faceLook = turn * 30.0f;
  if (canMoveR(px + mx, py, 0.22f)) px += mx;
  if (canMoveR(px, py + my, 0.22f)) py += my;
  if (mx != 0 || my != 0) bobT += dt * 9;
  levelTime += dt; if (pwR > 0) pwR -= dt; if (pwD > 0) pwD -= dt; if (pwE > 0) pwE -= dt;
  for (int i = 0; i < 8; i++) if (fxs[i].t > 0) fxs[i].t -= dt;
  if (button(A)) fire();
  if (pressed(X)) { int w = weapon; do { w = (w + 1) % 4; } while (!hasW[w]); if (w != weapon) { weapon = w; fireT = 0.2f; say(WNAME[w]); } }
  if (pressed(Y)) showMap = !showMap;
  if (fireT > 0) fireT -= dt; if (hurtT > 0) hurtT -= dt; if (pickT > 0) pickT -= dt;
  if (flashT > 0) flashT -= dt; if (msgT > 0) msgT -= dt; if (painT > 0) painT -= dt;
  for (int i = 0; i < nItem; i++) {
    Item &it = items[i]; if (!it.on) continue;
    float ex = it.x - px, ey = it.y - py; if (ex * ex + ey * ey > 0.32f) continue;
    if (it.type == 'h') { if (hp >= 100) continue; hp = hp + 25 > 100 ? 100 : hp + 25; say("HEALTH +25"); }
    else if (it.type == 'a') { ammo = ammo + 12 > 99 ? 99 : ammo + 12; say("AMMO +12"); }
    else if (it.type == 's') { hasW[1] = true; weapon = 1; ammo = ammo + 8 > 99 ? 99 : ammo + 8; say("SHOTGUN!"); }
    else if (it.type == 'g') { hasW[2] = true; weapon = 2; ammo = ammo + 20 > 99 ? 99 : ammo + 20; say("CHAINGUN!"); }
    else if (it.type == 'v') { if (armor >= 100) continue; armor = armor + 50 > 100 ? 100 : armor + 50; say("ARMOR +50"); }
    else if (it.type == 'x') { pwR = 15; say("RAPID FIRE!"); }
    else if (it.type == 'z') { pwD = 15; say("DOUBLE DAMAGE!"); }
    else if (it.type == 'e') { pwE = 15; say("EXPLOSIVE ROUNDS!"); }
    else if (it.type == 'b') { hasW[3] = true; weapon = 3; ammo = ammo + 9 > 99 ? 99 : ammo + 9; say("ROCKET LAUNCHER!"); }
    else if (it.type == 'r') { keys[0] = true; say("RED KEY"); }
    else if (it.type == 'u') { keys[1] = true; say("BLUE KEY"); }
    else { keys[2] = true; say("YELLOW KEY"); }
    it.on = false; pickT = 0.2f; sfx(3);
  }
  updateDoors(dt); updateEnemies(dt); updateProjs(dt);
  float ex = exitX - px, ey = exitY - py;
  if (state == PLAY && ex * ex + ey * ey < 0.85f) {
    finishLevel();
  }
}

// --------------------------------------------------------- title fire
#define FW 60
#define FH 30
static uint8_t fireBuf[FW * FH];
static col_t firePal[16];
static void stepFire() {
  for (int x = 0; x < FW; x++) fireBuf[(FH - 1) * FW + x] = 15;
  for (int y = 0; y < FH - 1; y++) for (int x = 0; x < FW; x++) {
    int src = fireBuf[(y + 1) * FW + x]; int r = rnd() & 3;
    int dx = x - r + 1; if (dx < 0) dx = 0; if (dx >= FW) dx = FW - 1;
    fireBuf[y * FW + dx] = src > 0 ? src - (r & 1) : 0;
  }
}
static void drawFire(int baseY) {
  for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) {
    col_t c = firePal[fireBuf[y * FW + x]]; fillR(x * 2, baseY + y * 2, 2, 2, c);
  }
}
static const char *GLYPH(char c) {
  switch (c) {
    case 'P': return "####.#...#####.#....#....";
    case 'I': return "#####..#....#....#..#####";
    case 'C': return ".#####....#....#.....####";
    case 'O': return ".###.#...##...##...#.###.";
    case 'D': return "####.#...##...##...#####.";
    case 'M': return "#...###.###.#.##...##...#";
  }
  return ".........................";
}
static void bigText(const char *s, int x, int y, int sc, col_t c) {
  for (; *s; s++) {
    const char *g = GLYPH(*s);
    for (int r = 0; r < 5; r++) for (int k = 0; k < 5; k++) if (g[r * 5 + k] == '#') fillR(x + k * sc, y + r * sc, sc, sc, c);
    x += 6 * sc;
  }
}
static void drawLogo() {
  const char *w1 = "PICO", *w2 = "DOOM";
  int x1 = (SW - (4 * 6 - 1) * 3) / 2, x2 = (SW - (4 * 6 - 1) * 4) / 2;
  bigText(w1, x1 + 2, 14, 3, C(2, 0, 0)); bigText(w1, x1, 12, 3, C(15, 12, 3));
  bigText(w2, x2 + 2, 33, 4, C(3, 0, 0)); bigText(w2, x2 + 1, 32, 4, C(9, 0, 0)); bigText(w2, x2, 31, 4, C(15, 4, 1));
}

// ------------------------------------------------------------ rendering
static inline int fogS(float d) { int s = 19 - (int)(d * 1.8f); return s < 2 ? 2 : s; }
static inline int liteS(int s, int boost) { s += boost; return s > 22 ? 22 : s; }
static inline int texFor(char c) { switch (c) { case '%': return 1; case 'W': return 2; case 'E': return 3; case 'D': return 4; case 'R': return 7; case 'U': return 8; case 'Y': return 9; } return 0; }

static void drawFloorCeil(float dirX, float dirY, float plX, float plY, int boost) {
  float rx0 = dirX - plX, ry0 = dirY - plY, rx1 = dirX + plX, ry1 = dirY + plY;
  for (int y = HORIZ; y < SH; y++) {
    float rd = (float)HORIZ / (float)(y - HORIZ + 0.5f);
    int s = liteS(fogS(rd), boost);
    int fx = (int)((px + rd * rx0) * 65536.0f), fy = (int)((py + rd * ry0) * 65536.0f);
    int sx = (int)(rd * (rx1 - rx0) / SW * 65536.0f), sy = (int)(rd * (ry1 - ry0) / SW * 65536.0f);
    col_t *fr = &fb[y * SW], *cr = &fb[(SH - 1 - y) * SW];
    for (int x = 0; x < SW; x++) {
      int ti = (((fy >> 11) & 31) << 5) | ((fx >> 11) & 31);
      fr[x] = shadeC(tex[5][ti], s); cr[x] = shadeC(tex[6][ti], s);
      fx += sx; fy += sy;
    }
  }
}
static void drawColumn(int x, int tidx, float dist, int side, float wx, bool flip, int boost, float open) {
  int lh = (int)(SH / dist); if (lh < 1) lh = 1;
  int top = HORIZ - lh / 2;
  int tx = (int)(wx * TS) & (TS - 1); if (flip) tx = TS - 1 - tx;
  int step = (TS << 16) / lh;
  int s = liteS(fogS(dist), boost); if (side) s = s * 3 / 4;
  int yEnd = top + (int)((1.0f - open) * lh), y0 = top;
  int pos = (int)(open * (float)(TS << 16));
  if (y0 < 0) { pos += (-y0) * step; y0 = 0; }
  if (yEnd > SH) yEnd = SH;
  const col_t *tp = tex[tidx];
  for (int y = y0; y < yEnd; y++) { fb[y * SW + x] = shadeC(tp[(((pos >> 16) & 31) << 5) | tx], s); pos += step; }
}
static void drawWalls(float dirX, float dirY, float plX, float plY, int boost) {
  for (int x = 0; x < SW; x++) {
    float cam = 2.0f * x / SW - 1.0f, rdx = dirX + plX * cam, rdy = dirY + plY * cam;
    int mx = (int)px, my = (int)py;
    float ddx = rdx == 0 ? 1e9f : fabsf(1.0f / rdx), ddy = rdy == 0 ? 1e9f : fabsf(1.0f / rdy);
    int sx, sy; float sdx, sdy;
    if (rdx < 0) { sx = -1; sdx = (px - mx) * ddx; } else { sx = 1; sdx = (mx + 1.0f - px) * ddx; }
    if (rdy < 0) { sy = -1; sdy = (py - my) * ddy; } else { sy = 1; sdy = (my + 1.0f - py) * ddy; }
    int side = 0, guard = 0; bool hit = false;
    bool dHit = false; float dDist = 0, dWx = 0, dOpen = 0; int dSide = 0, dTex = 4;
    while (!hit && guard++ < 40) {
      if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; } else { sdy += ddy; my += sy; side = 1; }
      if ((unsigned)mx >= MW || (unsigned)my >= MH) { hit = true; break; }
      char c = map[my][mx];
      if (isDoor(c)) {
        float o = doorO[my][mx];
        if (o >= 0.999f) continue;
        if (o > 0.001f) {
          if (!dHit) {
            dHit = true; dSide = side; dOpen = o; dTex = texFor(c);
            dDist = side == 0 ? sdx - ddx : sdy - ddy;
            dWx = side == 0 ? py + dDist * rdy : px + dDist * rdx; dWx -= floorf(dWx);
          }
          continue;
        }
        hit = true;
      } else if (isWallCh(c)) hit = true;
    }
    float dist = side == 0 ? sdx - ddx : sdy - ddy;
    if (dist < 0.05f) dist = 0.05f;
    float wx = side == 0 ? py + dist * rdy : px + dist * rdx; wx -= floorf(wx);
    bool flip = (side == 0 && rdx > 0) || (side == 1 && rdy < 0);
    char c = ((unsigned)mx < MW && (unsigned)my < MH) ? map[my][mx] : '#';
    drawColumn(x, texFor(c), dist, side, wx, flip, boost, 0.0f);
    zbuf[x] = dist;
    if (dHit) {
      if (dDist < 0.05f) dDist = 0.05f;
      bool f2 = (dSide == 0 && rdx > 0) || (dSide == 1 && rdy < 0);
      drawColumn(x, dTex, dDist, dSide, dWx, f2, boost, dOpen);
      zbuf[x] = dDist;
    }
  }
}

struct SprDraw { float d; int kind; int idx; };
static void drawSprites(float dirX, float dirY, float plX, float plY, int boost) {
  SprDraw list[MAXE + MAXI + MAXP + 8]; int n = 0;
  for (int i = 0; i < nEnt; i++) { float a = ents[i].x - px, b = ents[i].y - py; list[n++] = {a * a + b * b, 0, i}; }
  for (int i = 0; i < nItem; i++) if (items[i].on) { float a = items[i].x - px, b = items[i].y - py; list[n++] = {a * a + b * b, 1, i}; }
  for (int i = 0; i < MAXP; i++) if (projs[i].on) { float a = projs[i].x - px, b = projs[i].y - py; list[n++] = {a * a + b * b, 2, i}; }
  for (int i = 0; i < 8; i++) if (fxs[i].t > 0) { float a = fxs[i].x - px, b = fxs[i].y - py; list[n++] = {a * a + b * b, 3, i}; }
  for (int i = 1; i < n; i++) { SprDraw k = list[i]; int j = i - 1; while (j >= 0 && list[j].d < k.d) { list[j + 1] = list[j]; j--; } list[j + 1] = k; }
  float inv = 1.0f / (plX * dirY - dirX * plY);
  for (int si = 0; si < n; si++) {
    int kind = list[si].kind, idx = list[si].idx;
    float wx, wy; int frame, ps = 0; float hs, bobOff = 0; bool floating = false, flash = false;
    if (kind == 0) {
      Ent &e = ents[idx]; wx = e.x; wy = e.y; ps = ET[e.type].pal; hs = ET[e.type].h; flash = e.flash > 0;
      if (!e.alive) frame = e.deadT < 0.25f ? 3 : 4;
      else if (e.type == 4) frame = 14;
      else if (e.type == 3) { frame = e.atk > 0 ? 10 : 9; bobOff = -0.18f + sinf(e.walk * 2.0f) * 0.04f; }
      else if (e.atk > 0) frame = 2;
      else frame = ((int)(e.walk * 3.0f)) & 1;
    } else if (kind == 1) {
      char it = items[idx].type; wx = items[idx].x; wy = items[idx].y; hs = 0.7f;
      switch (it) {
        case 'h': frame = 5; break; case 'a': frame = 6; break; case 's': frame = 7; break;
        case 'g': case 'b': frame = 12; break; case 'v': frame = 13; break;
        case 'x': frame = 11; ps = 6; break; case 'z': frame = 11; ps = 4; break; case 'e': frame = 11; ps = 5; break;
        default: frame = 11; ps = it == 'r' ? 4 : (it == 'u' ? 5 : 6); break;
      }
    } else if (kind == 3) { wx = fxs[idx].x; wy = fxs[idx].y; hs = 0.6f + (0.45f - fxs[idx].t) * 3.2f; frame = 8; floating = true;
    } else { wx = projs[idx].x; wy = projs[idx].y; hs = 0.35f; frame = 8; floating = true; }
    float sx = wx - px, sy = wy - py;
    float tx = inv * (dirY * sx - dirX * sy), ty = inv * (-plY * sx + plX * sy);
    if (ty < 0.2f) continue;
    float unit = SH / ty;
    int h = (int)(unit * hs); if (h < 2) continue;
    int w = h, scr = (int)((SW / 2) * (1.0f + tx / ty)), left = scr - w / 2;
    int top = floating ? (int)(HORIZ - h / 2 + unit * 0.05f) : (int)(HORIZ + unit / 2 - h + unit * bobOff);
    int s = liteS(fogS(ty), boost); if (kind >= 2) s = 20;
    int stepY = (16 << 16) / h;
    int x0 = left < 0 ? 0 : left, x1 = left + w > SW ? SW : left + w;
    int y0 = top < 0 ? 0 : top, y1 = top + h > SH ? SH : top + h;
    for (int x = x0; x < x1; x++) {
      if (zbuf[x] < ty) continue;
      int sxp = ((x - left) * 16) / w; int pos = (y0 - top) * stepY;
      for (int y = y0; y < y1; y++, pos += stepY) {
        uint8_t pi = spr[frame][((pos >> 16) << 4) + sxp];
        if (!pi) continue;
        col_t c = flash ? C(15, 12, 11) : pal[ps][pi];
        fb[y * SW + x] = shadeC(c, s);
      }
    }
  }
}

static void drawWeapon() {
  int rec = (fireT > 0 && weapon != 2) ? (int)(fireT * (weapon ? 14 : 20)) : 0; if (rec > 10) rec = 10;
  int bob = (int)(sinf(bobT) * 2.0f), bx = SW / 2 + (int)(cosf(bobT * 0.5f) * 3.0f), by = SH - 50 + rec + (bob < 0 ? -bob : bob);
  bool flash = flashT > 0.04f;
  if (weapon == 0) {
    gradR(bx - 7, by + 22, 14, 30, 13, 10, 8);       // hand
    gradR(bx - 4, by + 12, 8, 16, 6, 4, 2);          // grip
    gradR(bx - 4, by - 2, 8, 16, 9, 9, 12);          // slide
    fillR(bx - 1, by - 5, 2, 3, C(3, 3, 4));         // sight
    fillR(bx - 3, by + 1, 6, 1, C(4, 4, 6));
    if (flash) { fillR(bx - 6, by - 14, 12, 10, C(15, 11, 2)); fillR(bx - 3, by - 11, 6, 7, C(15, 15, 11)); fillR(bx - 1, by - 18, 2, 5, C(15, 13, 4)); }
  } else if (weapon == 3) {
    gradR(bx - 9, by - 14, 18, 52, 5, 7, 4); fillR(bx - 10, by - 16, 20, 5, C(2, 2, 2));
    gradR(bx - 12, by + 26, 24, 26, 13, 10, 8); fillR(bx + 4, by - 6, 4, 10, C(8, 6, 2));
    if (flash) { fillR(bx - 8, by - 30, 16, 14, C(15, 9, 1)); fillR(bx - 4, by - 34, 8, 18, C(15, 14, 6)); }
  } else if (weapon == 2) {
    int j = fireT > 0 ? (frameCount & 1) : 0; by += j * 2;
    gradR(bx - 12, by + 26, 24, 30, 13, 10, 8);       // hands
    gradR(bx - 9, by + 10, 18, 22, 5, 5, 7);          // body
    for (int k = 0; k < 3; k++) gradR(bx - 8 + k * 6, by - 10 + ((j && k == 1) ? 1 : 0), 5, 22, 8, 8, 11);
    fillR(bx - 9, by + 8, 18, 3, C(2, 2, 3));
    if (flash) { fillR(bx - 5, by - 17, 10, 8, C(15, 11, 2)); fillR(bx - 2, by - 20, 4, 6, C(15, 15, 11)); }
  } else {
    gradR(bx - 6, by - 6, 6, 44, 6, 6, 8);           // twin barrels
    gradR(bx, by - 6, 6, 44, 7, 7, 9);
    fillR(bx - 6, by - 7, 12, 2, C(2, 2, 3));
    gradR(bx - 9, by + 12, 18, 10, 9, 4, 2);          // pump
    for (int k = 0; k < 3; k++) fillR(bx - 9, by + 14 + k * 3, 18, 1, C(4, 2, 1));
    gradR(bx - 10, by + 24, 20, 28, 13, 10, 8);       // hand
    if (flash) {
      fillR(bx - 10, by - 20, 20, 14, C(15, 9, 1)); fillR(bx - 6, by - 24, 12, 18, C(15, 13, 3));
      fillR(bx - 3, by - 26, 6, 14, C(15, 15, 12));
    }
  }
}

static void drawFace(int fx, int fy) {
  fillR(fx, fy, 14, 14, C(2, 2, 3)); fillR(fx + 1, fy + 1, 12, 12, C(13, 10, 8));
  col_t hair = chGen ? C(8, 4, 1) : C(4, 2, 1); fillR(fx + 1, fy + 1, 12, 3, hair);
  if (chGen) { fillR(fx + 1, fy + 4, 2, 6, hair); fillR(fx + 11, fy + 4, 2, 6, hair); }
  fillR(fx + 1, fy + 12, 12, 1, C(SUIT[chSuit][0], SUIT[chSuit][1], SUIT[chSuit][2]));
  int look = clampi((int)faceLook, -1, 1);
  if (state == DEAD) {
    for (int k = 0; k < 3; k++) { put(fx + 3 + k, fy + 5 + k, C(0, 0, 0)); put(fx + 5 - k, fy + 5 + k, C(0, 0, 0)); put(fx + 8 + k, fy + 5 + k, C(0, 0, 0)); put(fx + 10 - k, fy + 5 + k, C(0, 0, 0)); }
  } else {
    fillR(fx + 3, fy + 5, 3, 2, C(15, 15, 15)); fillR(fx + 8, fy + 5, 3, 2, C(15, 15, 15));
    put(fx + 4 + look, fy + 5, C(0, 0, 0)); put(fx + 9 + look, fy + 5, C(0, 0, 0)); put(fx + 4 + look, fy + 6, C(0, 0, 0)); put(fx + 9 + look, fy + 6, C(0, 0, 0));
    fillR(fx + 2, fy + 4, 4, 1, C(4, 2, 1)); fillR(fx + 8, fy + 4, 4, 1, C(4, 2, 1));
  }
  if (painT > 0 || state == DEAD) fillR(fx + 4, fy + 9, 6, 3, C(5, 0, 0));
  else if (hp > 50) fillR(fx + 4, fy + 10, 6, 1, C(6, 2, 2));
  else { fillR(fx + 4, fy + 10, 2, 1, C(6, 2, 2)); fillR(fx + 6, fy + 11, 2, 1, C(6, 2, 2)); fillR(fx + 8, fy + 10, 2, 1, C(6, 2, 2)); }
  col_t blood = C(12, 0, 0);
  if (hp < 70) { put(fx + 2, fy + 8, blood); put(fx + 11, fy + 9, blood); }
  if (hp < 45) { fillR(fx + 3, fy + 6, 1, 4, blood); put(fx + 10, fy + 4, blood); put(fx + 10, fy + 7, blood); }
  if (hp < 20) { fillR(fx + 1, fy + 3, 12, 1, blood); fillR(fx + 7, fy + 7, 1, 6, blood); fillR(fx + 5, fy + 5, 1, 4, blood); }
}

static void drawHud() {
  char b[24];
  fillR(0, SH - 18, SW, 18, C(3, 3, 4)); fillR(0, SH - 18, SW, 1, C(9, 9, 10)); fillR(0, SH - 17, SW, 1, C(1, 1, 2));
  fillR(48, SH - 17, 1, 17, C(1, 1, 2)); fillR(71, SH - 17, 1, 17, C(1, 1, 2));
  txt("HEALTH", 3, SH - 16, 1, C(9, 9, 11));
  snprintf(b, sizeof b, "%d", hp);
  txt(b, 3, SH - 10, 2, hp > 30 ? C(4, 15, 4) : C(15, 3, 3));
  txt(WNAME[weapon], 76, SH - 16, 1, C(9, 9, 11));
  snprintf(b, sizeof b, "%d", ammo);
  txt(b, 76, SH - 10, 2, ammo > 5 ? C(15, 13, 3) : C(15, 3, 3));
  if (armor > 0) { snprintf(b, sizeof b, "AR%d", armor); txt(b, 29, SH - 16, 1, C(6, 12, 15)); }
  drawFace(52, SH - 16);
  static const int KC[3][3] = {{15, 2, 2}, {4, 6, 15}, {15, 14, 2}};
  for (int k = 0; k < 3; k++) if (keys[k]) { fillR(2 + k * 7, 9, 6, 6, C(1, 1, 2)); fillR(3 + k * 7, 10, 4, 4, C(KC[k][0], KC[k][1], KC[k][2])); }
  // crosshair
  put(SW / 2, HORIZ - 3, C(15, 15, 15)); put(SW / 2, HORIZ + 3, C(15, 15, 15)); put(SW / 2 - 3, HORIZ, C(15, 15, 15)); put(SW / 2 + 3, HORIZ, C(15, 15, 15));
  snprintf(b, sizeof b, "KILLS %d/%d", kills, enemiesInLevel);
  txtS(b, 2, 2, 1, C(15, 15, 15));
  if (msgT > 0) txtC(msg, 12, 1, C(15, 15, 6));
  if (pwR > 0) txtS("RAPID", 2, 18, 1, C(15, 14, 2)); if (pwD > 0) txtS("2X DMG", 2, 25, 1, C(15, 3, 3)); if (pwE > 0) txtS("EXPLODE", 2, 32, 1, C(6, 8, 15));
}
static void drawMinimap() {
  int ox = SW - 2 * MW - 2, oy = 2;
  for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
    char c = map[y][x]; col_t k;
    if (c == 'D') k = C(12, 10, 1); else if (c == 'R') k = C(15, 2, 2); else if (c == 'U') k = C(4, 6, 15); else if (c == 'Y') k = C(15, 14, 2); else if (c == 'E') k = C(2, 14, 2); else if (isWallCh(c)) k = C(5, 5, 7); else k = C(1, 1, 2);
    fillR(ox + x * 2, oy + y * 2, 2, 2, k);
  }
  for (int i = 0; i < nEnt; i++) if (ents[i].alive) fillR(ox + (int)(ents[i].x * 2), oy + (int)(ents[i].y * 2), 2, 2, C(15, 0, 0));
  fillR(ox + (int)(px * 2), oy + (int)(py * 2), 2, 2, C(0, 15, 0));
  put(ox + (int)((px + cosf(pa) * 1.5f) * 2), oy + (int)((py + sinf(pa) * 1.5f) * 2), C(15, 15, 0));
}
static void tint(int dr, int dg, int db, int fade) {   // add colour, scale others
  for (int i = 0; i < SW * SH; i++) {
    col_t c = fb[i]; int r = (c >> 8) & 15, g = (c >> 4) & 15, b = c & 15;
    r = r + dr; g = g * (16 - fade) / 16 + dg; b = b * (16 - fade) / 16 + db; fb[i] = C(r, g, b);
  }
}

static void drawPlay() {
  float dirX = cosf(pa), dirY = sinf(pa), plX = -dirY * FOV, plY = dirX * FOV;
  int boost = flashT > 0 ? (weapon == 1 ? 5 : 3) : 0;
  drawFloorCeil(dirX, dirY, plX, plY, boost);
  drawWalls(dirX, dirY, plX, plY, boost);
  drawSprites(dirX, dirY, plX, plY, boost);
  drawWeapon();
  if (hurtT > 0) tint(hurtT > 0.15f ? 5 : 3, 0, 0, 5);
  if (pickT > 0) tint(2, 2, 0, 0);
  drawHud();
  if (showMap) drawMinimap();
}

static void txtWrapC(const char *s, int y, int sc, col_t c) {
  int n = (int)strlen(s);
  if (txtW(s, sc) <= SW - 4) { txtC(s, y, sc, c); return; }
  char a[48], b[48]; int cut = n / 2;
  for (int d = 0; d < n; d++) { if (cut + d < n && s[cut + d] == ' ') { cut += d; break; } if (cut - d > 0 && s[cut - d] == ' ') { cut -= d; break; } }
  memcpy(a, s, cut); a[cut] = 0; snprintf(b, sizeof b, "%s", s + cut + 1);
  txtC(a, y, sc, c); txtC(b, y + 6 * sc + 2, sc, c);
}
static void drawDeadOverlay() {
  // blood drips + text
  for (int x = 0; x < SW; x++) {
    int h = 6 + (int)(hsh(x * 13) % 40);
    if (stateT < 1.0f) h = (int)(h * stateT);
    fillR(x, 0, 1, h, C(9, 0, 0)); fillR(x, h - 1, 1, 1, C(5, 0, 0));
  }
  txtC("YOU DIED", 30, 2, C(15, 15, 15)); txtWrapC(taunt, 54, 1, C(15, 13, 6)); txtC("PRESS A", 80, 1, C(15, 12, 3));
}

static void drawTitle() {
  for (int i = 0; i < SW * SH; i++) fb[i] = C(0, 0, 1);
  drawFire(SH - FH * 2);
  drawLogo();
  txtC("RP2040 EDITION", 58, 1, C(12, 12, 14));
  if (sv.hi > 0) { char hb[24]; snprintf(hb, sizeof hb, "HI SCORE %d", (int)sv.hi); txtC(hb, 68, 1, C(15, 13, 3)); }
  if ((frameCount / 20) & 1) txtC("PRESS A", 88, 1, C(15, 15, 6));
  txtC(chGen ? "< GIRL >" : "< GUY >", 72, 1, C(15, 15, 15)); fillR(SW / 2 - 12, 80, 24, 5, C(SUIT[chSuit][0], SUIT[chSuit][1], SUIT[chSuit][2]));
  txtC("A FIRE  X WEAPON", 98, 1, C(15, 11, 6)); txtC("ARROWS PICK HERO", 106, 1, C(15, 11, 6));
}
static void fmtTime(char *b, size_t n, int t) { snprintf(b, n, "%d:%02d", t / 60, t % 60); }
static void drawDone() {
  for (int i = 0; i < SW * SH; i++) fb[i] = C(0, 1, 0);
  drawFire(SH - FH * 2);
  char b[48], t1[12], t2[12];
  if (state == WIN) { txtC("YOU WIN!", 6, 3, C(15, 14, 3)); txtC("THE KEEP FALLS", 26, 1, C(14, 14, 14)); }
  else { txtC(LEVEL_NAMES[level], 8, 1, C(10, 15, 10)); txtC("CLEARED", 18, 2, C(15, 14, 3)); }
  fmtTime(t1, sizeof t1, lastTime); fmtTime(t2, sizeof t2, LEVEL_PAR[level]);
  snprintf(b, sizeof b, "TIME %s  PAR %s", t1, t2); txtC(b, 40, 1, C(15, 15, 15));
  if (newBest) txtC("NEW BEST TIME!", 50, 1, C(15, 15, 4));
  else { fmtTime(t1, sizeof t1, sv.best[level]); snprintf(b, sizeof b, "BEST %s", t1); txtC(b, 50, 1, C(10, 12, 15)); }
  snprintf(b, sizeof b, "KILLS %d/%d", kills, enemiesInLevel); txtC(b, 60, 1, C(15, 15, 15));
  snprintf(b, sizeof b, "SCORE +%d", levelScore); txtC(b, 70, 1, C(15, 15, 15));
  snprintf(b, sizeof b, "TOTAL %d", score); txtC(b, 80, 1, C(15, 13, 3));
  if (state == WIN) { if (newHi) txtC("NEW HIGH SCORE!", 90, 1, C(15, 15, 4)); else { snprintf(b, sizeof b, "HI SCORE %d", (int)sv.hi); txtC(b, 90, 1, C(10, 12, 15)); } }
  if ((frameCount / 20) & 1) txtC(state == WIN ? "A: PLAY AGAIN" : "A: NEXT LEVEL", 104, 1, C(15, 15, 6));
}

}  // namespace pd

// ------------------------------------------------------------------ entry
void pd_init() {
  using namespace pd;
  backlight(80);
  buildTextures(); buildSprites(); loadSave();
  const int fp[16][3] = {{0,0,0},{2,0,0},{4,0,0},{6,1,0},{8,1,0},{10,2,0},{12,3,0},{13,5,0},{14,7,0},{15,9,0},{15,11,1},{15,12,2},{15,13,4},{15,14,7},{15,15,10},{15,15,15}};
  for (int i = 0; i < 16; i++) firePal[i] = C(fp[i][0], fp[i][1], fp[i][2]);
  memset(fireBuf, 0, sizeof fireBuf);
  rs ^= (uint32_t)picosystem::time() * 2654435761u; if (!rs) rs = 1;
#if PD_SOUND
  vShot  = voice(0, 70, 0, 60, 0, 0, 40, 0);
  vShot2 = voice(0, 150, 0, 120, 0, 20, 70, 20);
  vHurt  = voice(0, 100, 0, 80, 0, 0, 30, 0);
  vPick  = voice(0, 60, 40, 60, 0, 0, 0, 0);
  vDoor  = voice(10, 150, 30, 100, 0, 0, 20, 0);
  vDie   = voice(0, 200, 0, 150, 0, 10, 50, 0);
  vChain = voice(0, 40, 0, 40, 0, 0, 50, 10);
  vBall  = voice(0, 120, 20, 100, 0, 10, 10, 0);
  vLock  = voice(0, 80, 0, 100, 0, 0, 0, 40);
  vWin   = voice(20, 200, 60, 300, 0, 30, 0, 0);
#endif
}

void pd_update(uint32_t tick) {
  using namespace pd;
  (void)tick;
  const float dt = 1.0f / 40.0f;
  frameCount++; stateT += dt;
  switch (state) {
    case TITLE: stepFire(); if (pressed(LEFT) || pressed(RIGHT)) chGen ^= 1; if (pressed(UP)) chSuit = (chSuit + 1) % 6; if (pressed(DOWN)) chSuit = (chSuit + 5) % 6; if (pressed(A)) newGame(); break;
    case PLAY: updatePlay(dt); break;
    case DEAD:
      if (stateT > 1.0f && pressed(A)) { hp = 100; if (ammo < 20) ammo = 20; loadLevel(level); state = PLAY; say("TRY AGAIN"); }
      break;
    case DONE: stepFire(); if (stateT > 0.5f && pressed(A)) { level++; loadLevel(level); state = PLAY; say(LEVEL_NAMES[level]); } break;
    case WIN:  stepFire(); if (stateT > 0.5f && pressed(A)) state = TITLE; break;
  }
}

void pd_draw(uint32_t tick) {
  using namespace pd;
  (void)tick;
  switch (state) {
    case TITLE: drawTitle(); break;
    case PLAY: drawPlay(); break;
    case DEAD: drawPlay(); tint(4, 0, 0, 8); drawDeadOverlay(); break;
    case DONE: case WIN: drawDone(); break;
  }
  present();
}

#ifndef PD_PORT   // PicoSystem build: the SDK calls these directly
void init() { pd_init(); }
void update(uint32_t tick) { pd_update(tick); }
void draw(uint32_t tick) { pd_draw(tick); }
#endif
