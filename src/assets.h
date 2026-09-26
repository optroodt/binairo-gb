#ifndef ASSETS_H
#define ASSETS_H
#include <stdint.h>
#define ASSET_BANK 2
#define PUZZLE_BANK 3
extern const uint8_t font_widths[128];
extern const uint8_t font_bits[128*9];
extern const uint8_t theme_mini_bits[8*9];
extern const uint8_t theme_mini_widths[8];
#define CELL_TILE_COUNT 48
extern const uint8_t cell_tiles[4*48*16];
#define THEME_COUNT 4
extern const uint8_t ring_tiles[16*16];
#define SPR_T_CURSOR 0
#define SPR_T_CHECK 4
#define SPR_T_TAB 5
#define SPR_T_ARROW 8
#define SPRITE_TILE_COUNT 9
extern const uint8_t sprite_tiles[144];
#define LOGO_W 16
#define LOGO_H 4
extern const uint8_t logo_tiles[1024];
extern const uint8_t icon_tiles[192];
#define CARD_W 10
#define CARD_H 18
extern const uint8_t rules_card_tiles[2880];
#endif
