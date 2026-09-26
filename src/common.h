#ifndef COMMON_H
#define COMMON_H

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------- input
extern uint8_t keys, keys_new, keys_rep;
void input_update(void);
void frame(void);  // wait vblank + input + sound

// ---------------------------------------------------------------- scenes
enum {
    SC_TITLE,
    SC_HOME,
    SC_GAME,
    SC_PAUSE,
    SC_RESULT,
    SC_TUTORIAL
};

// ---------------------------------------------------------------- display
void display_bitmap_mode(void);  // full screen 160x144 framebuffer
void display_game_mode(void);    // tile based board + window
void screen_off(void);
void screen_on(void);
void fade_out(void);
void fade_in(void);
void set_shades(uint8_t bgp);    // DMG BGP value, also mapped to CGB

// ---------------------------------------------------------------- surfaces
typedef struct {
    uint8_t *buf;
    uint8_t w;  // tiles
    uint8_t h;  // tiles
} surf_t;

#define FB_BYTES (20 * 18 * 16)
extern uint8_t gbuf[FB_BYTES];  // shared scratch (framebuffer / panels)
extern surf_t fb;               // full screen surface (bitmap mode)

extern uint8_t cur_theme;
extern volatile uint8_t cur_scene;

void surf_clear(surf_t *s, uint8_t c);
void surf_fill(surf_t *s, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t c);
void surf_rrect(surf_t *s, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t c);  // filled, r=2
void surf_frame(surf_t *s, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t c);  // 1px outline
uint8_t text_width(const char *t, uint8_t bold);
uint8_t surf_text(surf_t *s, uint8_t x, uint8_t y, const char *t, uint8_t c, uint8_t bold);
void surf_text_c(surf_t *s, uint8_t cx, uint8_t y, const char *t, uint8_t c, uint8_t bold);  // centred on cx
uint8_t surf_wrap(surf_t *s, uint8_t x, uint8_t y, uint8_t maxw, const char *t, uint8_t c);   // returns next y
void surf_tiles(surf_t *s, uint8_t tx, uint8_t ty, uint8_t tw, uint8_t th, const uint8_t *tiles);
void fb_upload(uint8_t ty0, uint8_t ty1);  // upload tile rows [ty0, ty1)
void fb_upload_tile(uint8_t tx, uint8_t ty);
void surf_upload(surf_t *s, uint8_t *vram);
void vcopy(uint8_t *dst, const uint8_t *src, uint16_t n);
void vcopy_b(uint8_t bank, uint8_t *dst, const uint8_t *src, uint16_t n);
void bcopy(uint8_t bank, uint8_t *dst, const uint8_t *src, uint16_t n);

// ---------------------------------------------------------------- strings
char *str_cpy(char *d, const char *s);
char *str_num(char *d, uint16_t v);
char *str_time(char *d, uint16_t secs);

// ---------------------------------------------------------------- sound
void snd_init(void);
void snd_update(void);
void sfx_move(void);
void sfx_set(uint8_t v);
void sfx_hint(void);
void sfx_page(void);
void sfx_check(uint8_t n);
void sfx_back(void);
void sfx_win(void);
void sfx_error(void);

// ---------------------------------------------------------------- save
#define MODE_COUNT 4  // (size 6/8) x (easy/hard)
typedef struct {
    uint8_t magic[4];
    uint8_t theme;
    uint8_t sfx;
    uint8_t tut_done;
    uint8_t sel_size;  // 0 = 6x6, 1 = 8x8
    uint8_t sel_diff;  // 0 easy, 1 hard
    uint16_t started[MODE_COUNT];
    uint16_t won[MODE_COUNT];
    uint16_t best[MODE_COUNT];  // seconds, 0 = none
    uint8_t next[MODE_COUNT];   // next puzzle index
    uint8_t has_resume[MODE_COUNT];
    uint8_t res_puz[MODE_COUNT];
    uint16_t res_time[MODE_COUNT];
    uint8_t res_cells[MODE_COUNT][64];
    uint8_t check;
} save_t;
extern save_t sv;
void save_load(void);
void save_write(void);

// ---------------------------------------------------------------- game
extern uint8_t last_result_time_valid;
extern uint16_t last_result_time;
extern uint8_t last_result_best;
uint8_t scene_title(void);
uint8_t scene_home(uint8_t results);
uint8_t scene_game(uint8_t tutorial, uint8_t resume);
uint8_t scene_pause(void);

#endif
