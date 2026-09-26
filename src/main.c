// Binairo for Game Boy - main loop, display modes, input.
#include "common.h"
#include "assets.h"

uint8_t keys, keys_new, keys_rep;
static uint8_t rep_timer;
static volatile uint8_t bm_mode;
static uint8_t cur_bgp = 0xE4;
uint8_t cur_theme;
volatile uint8_t cur_scene;  // for debugging / tests

#define J_DIRS (J_UP | J_DOWN | J_LEFT | J_RIGHT)

static volatile uint8_t isr_keys, isr_prev, isr_new;

void input_update(void) {
    __critical {
        keys = isr_keys;
        keys_new = isr_new;
        isr_new = 0;
    }
    keys_rep = keys_new;
    if (keys & J_DIRS) {
        if (keys_new & J_DIRS) rep_timer = 14;
        else if (--rep_timer == 0) { rep_timer = 5; keys_rep |= keys & J_DIRS; }
    }
}

void frame(void) {
    wait_vbl_done();
    input_update();
    snd_update();
}

// ------------------------------------------------------------- palettes
static const palette_color_t base_cols[4] = {
    RGB(22, 22, 21),  // paper  (Playdate-like silver)
    RGB(16, 16, 15),
    RGB(10, 10, 9),
    RGB(5, 5, 4),     // ink
};

void set_shades(uint8_t bgp) {
    cur_bgp = bgp;
    BGP_REG = bgp;
    // sprites: color1 = paper, 2 = dark, 3 = ink (follows fade)
    OBP0_REG = (bgp & 0xF0) | ((bgp & 0x03) << 2);
    if (_cpu == CGB_TYPE) {
        palette_color_t p[4], q[4];
        uint8_t i;
        for (i = 0; i < 4; i++) p[i] = base_cols[(bgp >> (i << 1)) & 3];
        set_bkg_palette(0, 1, p);
        q[0] = p[0]; q[1] = p[0]; q[2] = p[2]; q[3] = p[3];
        set_sprite_palette(0, 1, q);
    }
}

static const uint8_t fade_tab[4] = {0xE4, 0x90, 0x40, 0x00};

void fade_out(void) {
    uint8_t i, j;
    for (i = 1; i < 4; i++) {
        set_shades(fade_tab[i]);
        for (j = 0; j < 3; j++) frame();
    }
}

void fade_in(void) {
    int8_t i;
    uint8_t j;
    for (i = 2; i >= 0; i--) {
        for (j = 0; j < 3; j++) frame();
        set_shades(fade_tab[i]);
    }
}

// ------------------------------------------------------------- interrupts
static void lcd_isr(void) {
    if (bm_mode) {
        while (STAT_REG & 3) ;
        LCDC_REG &= ~0x10;
    }
}

static void vbl_isr(void) {
    uint8_t k;
    if (bm_mode) LCDC_REG |= 0x10;
    // poll the pad here so short taps are not lost during long redraws
    k = joypad();
    isr_new |= k & ~isr_prev;
    isr_prev = k;
    isr_keys = k;
}

void screen_off(void) {
    if (LCDC_REG & 0x80) DISPLAY_OFF;
}

void screen_on(void) {
    DISPLAY_ON;
}

void display_bitmap_mode(void) {
    uint8_t row[20];
    uint8_t y, x;
    uint16_t t = 0;
    HIDE_SPRITES;
    HIDE_WIN;
    SCX_REG = 0; SCY_REG = 0;
    for (y = 0; y < 18; y++) {
        for (x = 0; x < 20; x++) row[x] = (uint8_t)(t++);
        set_bkg_tiles(0, y, 20, 1, row);
    }
    __critical {
        LYC_REG = 71;
        STAT_REG = STATF_LYC;
        bm_mode = 1;
        LCDC_REG = (LCDC_REG & 0x80) | 0x11;
        set_interrupts(VBL_IFLAG | LCD_IFLAG);
    }
}

void display_game_mode(void) {
    __critical {
        bm_mode = 0;
        set_interrupts(VBL_IFLAG);
        STAT_REG = 0;
        LCDC_REG = (LCDC_REG & 0x80) | 0x43;  // win map 9C00, BG tiles 8800, sprites, BG
    }
}

void main(void) {
    uint8_t scene = SC_TITLE;
    uint8_t results = 0;

    SWITCH_ROM(1);
    DISPLAY_OFF;
    set_shades(0x00);
    if (_cpu == CGB_TYPE) {
        VBK_REG = VBK_ATTRIBUTES;
        init_bkg(0);
        init_win(0);
        VBK_REG = VBK_TILES;
    }
    CRITICAL {
        add_LCD(lcd_isr);
        add_VBL(vbl_isr);
    }
    LCDC_REG = 0x91;
    DISPLAY_ON;

    snd_init();
    save_load();
    cur_theme = sv.theme;

    while (1) {
        cur_scene = scene;
        switch (scene) {
        case SC_TITLE:
            scene = scene_title();
            break;
        case SC_HOME:
            scene = scene_home(0);
            break;
        case SC_RESULT:
            scene = scene_home(1);
            break;
        case SC_GAME:
            scene = scene_game(0, 1);
            break;
        case SC_TUTORIAL:
            scene = scene_game(1, 0);
            break;
        default:
            scene = SC_TITLE;
        }
        (void)results;
    }
}
