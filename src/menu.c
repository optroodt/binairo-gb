#pragma bank 1
// Title, home/statistics and pause (side menu) screens - bitmap mode.
#include "common.h"
#include "assets.h"

uint8_t last_result_time_valid;
uint16_t last_result_time;
uint8_t last_result_best;

static char sbuf[40];
static uint8_t rnd_s = 0x5A;

static uint8_t rnd(void) {
    rnd_s ^= rnd_s << 3;
    rnd_s ^= rnd_s >> 5;
    rnd_s ^= DIV_REG;
    return rnd_s;
}

// copy a 16x16 board cell (plain variant) into the framebuffer
static void fb_cell(uint8_t tx, uint8_t ty, uint8_t v, uint8_t given) {
    uint8_t var = v == 0 ? 0 : (given ? 2 + v : v);
    const uint8_t *p = cell_tiles + ((uint16_t)cur_theme * 48 + ((var << 1) << 2)) * 16;
    surf_tiles(&fb, tx, ty, 2, 1, p);
    surf_tiles(&fb, tx, ty + 1, 2, 1, p + 32);
}

// ================================================================ title
static const char top_digits[] = "0101101011";
static const char bot_digits[] = "1001010100";
static uint8_t tvals[20];

static void title_start_text(uint8_t on) {
    surf_fill(&fb, 0, 120, 160, 12, 0);
    if (on) surf_text_c(&fb, 80, 122, "\x01 Start", 3, 1);
    fb_upload(15, 17);
}

uint8_t scene_title(void) {
    uint8_t i, t = 0, blink = 1;
    screen_off();
    display_bitmap_mode();
    surf_clear(&fb, 0);
    for (i = 0; i < 10; i++) {
        tvals[i] = top_digits[i] - '0' + 1;
        tvals[10 + i] = bot_digits[i] - '0' + 1;
    }
    for (i = 0; i < 10; i++) {
        fb_cell(i << 1, 2, tvals[i], (i & 3) == 1);
        fb_cell(i << 1, 11, tvals[10 + i], (i & 3) == 2);
    }
    surf_fill(&fb, 0, 15, 160, 1, 3);
    surf_fill(&fb, 0, 87, 160, 1, 3);
    surf_tiles(&fb, (20 - LOGO_W) >> 1, 5, LOGO_W, LOGO_H, logo_tiles);
    surf_text_c(&fb, 80, 136, "After Binairo for Playdate", 2, 0);
    fb_upload(0, 18);
    title_start_text(1);
    screen_on();
    fade_in();
    while (1) {
        frame();
        t++;
        if ((t & 31) == 0) { blink ^= 1; title_start_text(blink); }
        if ((t % 20) == 10) {
            // flip a random digit cell
            uint8_t k = rnd() % 20, tx, ty;
            tvals[k] = tvals[k] == 1 ? 2 : 1;
            tx = (k % 10) << 1;
            ty = k < 10 ? 2 : 11;
            fb_cell(tx, ty, tvals[k], 0);
            fb_upload_tile(tx, ty); fb_upload_tile(tx + 1, ty);
            fb_upload_tile(tx, ty + 1); fb_upload_tile(tx + 1, ty + 1);
        }
        if (keys_new & (J_A | J_START)) {
            sfx_page();
            fade_out();
            return sv.tut_done ? SC_HOME : SC_TUTORIAL;
        }
    }
}

// ================================================================ home / stats
static void home_draw(uint8_t results) {
    uint8_t m = (sv.sel_size << 1) | sv.sel_diff, i, cx, x;
    uint16_t started = sv.started[m], won = sv.won[m];
    surf_clear(&fb, 0);
    surf_tiles(&fb, (20 - LOGO_W) >> 1, 0, LOGO_W, LOGO_H, logo_tiles);

    // ---- statistics (left)
    for (i = 0; i < 3; i++) surf_tiles(&fb, 1, 5 + i * 3, 2, 1, icon_tiles + i * 64),
                            surf_tiles(&fb, 1, 6 + i * 3, 2, 1, icon_tiles + i * 64 + 32);
    str_num(sbuf, started > 999 ? 999 : started);
    surf_text(&fb, 44 - text_width(sbuf, 1), 45, sbuf, 3, 1);
    surf_text(&fb, 48, 45, "Started", 3, 0);
    str_num(sbuf, won > 999 ? 999 : won);
    surf_text(&fb, 44 - text_width(sbuf, 1), 69, sbuf, 3, 1);
    surf_text(&fb, 48, 69, "Won", 3, 0);
    {
        char *p = str_num(sbuf, started ? (uint16_t)((uint32_t)won * 100 / started) : 0);
        *p++ = '%'; *p = 0;
    }
    surf_text(&fb, 44 - text_width(sbuf, 1), 93, sbuf, 3, 1);
    surf_text(&fb, 48, 93, "Rate", 3, 0);

    // ---- size selector (right)
    surf_rrect(&fb, 86, 40, 68, 15, 3);
    surf_text(&fb, 91, 44, "<", 0, 1);
    surf_text(&fb, 146, 44, ">", 0, 1);
    surf_text_c(&fb, 120, 44, sv.sel_size ? "8x8" : "6x6", 0, 1);
    for (i = 0; i < 2; i++) {
        uint16_t b = sv.best[(sv.sel_size << 1) | i];
        cx = i ? 138 : 102;
        if (sv.sel_diff == i) {
            surf_rrect(&fb, cx - 16, 60, 32, 13, 3);
            surf_text_c(&fb, cx, 63, i ? "Hard" : "Easy", 0, 0);
        } else {
            surf_text_c(&fb, cx, 63, i ? "Hard" : "Easy", 3, 0);
        }
        if (b) {
            sbuf[0] = 7; sbuf[1] = ' ';
            str_time(sbuf + 2, b);
        } else {
            str_cpy(sbuf, "--:--");
        }
        surf_text_c(&fb, cx, 79, sbuf, b ? 3 : 2, 0);
    }

    // ---- message line
    if (results && last_result_time_valid) {
        char *p = str_cpy(sbuf, "Solved in ");
        str_time(p, last_result_time);
        if (last_result_best) {
            p = str_cpy(p + 5, " - new best!");
        }
        surf_text_c(&fb, 80, 110, sbuf, 3, text_width(sbuf, 1) <= 152);
    } else if (sv.has_resume[m]) {
        surf_text_c(&fb, 120, 97, "Game in progress", 2, 0);
    }

    // ---- bottom legend
    surf_fill(&fb, 4, 124, 152, 1, 3);
    surf_text(&fb, 6, 131, results ? "\x02 Home" : "\x02 Rules", 3, 0);
    if (results) str_cpy(sbuf, "\x01 Play Again");
    else if (sv.has_resume[m]) str_cpy(sbuf, "\x01 Resume");
    else str_cpy(sbuf, "\x01 Play");
    x = 154 - text_width(sbuf, 0);
    surf_text(&fb, x, 131, sbuf, 3, 0);
    fb_upload(0, 18);
}

uint8_t scene_home(uint8_t results) {
    uint8_t dirty = 0;
    screen_off();
    display_bitmap_mode();
    home_draw(results);
    screen_on();
    fade_in();
    while (1) {
        frame();
        if (keys_rep & (J_LEFT | J_RIGHT)) {
            sv.sel_size ^= 1; dirty = 1; sfx_move();
        }
        if (keys_rep & (J_UP | J_DOWN)) {
            sv.sel_diff ^= 1; dirty = 1; sfx_move();
        }
        if (dirty) {
            dirty = 0;
            if (results) { results = 0; last_result_time_valid = 0; }
            home_draw(results);
        }
        if (keys_new & (J_A | J_START)) {
            sfx_page();
            save_write();
            fade_out();
            return SC_GAME;
        }
        if (keys_new & J_B) {
            sfx_back();
            if (results) {
                results = 0;
                last_result_time_valid = 0;
                home_draw(0);
            } else {
                save_write();
                fade_out();
                return SC_TUTORIAL;
            }
        }
        if (keys_new & J_SELECT) {
            save_write();
            fade_out();
            return SC_TITLE;
        }
    }
}

// ================================================================ pause
static const char *const pause_items[] = {"Resume", "New game", "Theme", "Sound", "Home"};

static void pause_draw(uint8_t sel) {
    uint8_t i, y, c;
    surf_clear(&fb, 0);
    surf_tiles(&fb, 0, 0, CARD_W, CARD_H, rules_card_tiles);
    for (i = 0; i < 5; i++) {
        y = 14 + i * 22;
        c = 3;
        if (i == sel) {
            surf_rrect(&fb, 84, y - 4, 72, 17, 3);
            c = 0;
        }
        surf_text(&fb, 90, y, pause_items[i], c, 1);
        if (i == 2) {
            sbuf[0] = 5; sbuf[1] = ' '; sbuf[2] = 6; sbuf[3] = 0;
            surf_text(&fb, 150 - text_width(sbuf, 0), y, sbuf, c, 0);
        }
        if (i == 3) {
            const char *s = sv.sfx ? "On" : "Off";
            surf_text(&fb, 150 - text_width(s, 0), y, s, c, 0);
        }
    }
    surf_text_c(&fb, 120, 131, "\x01 Select  \x02 Back", 2, 0);
    fb_upload(0, 18);
}

// returns 0 resume, 1 new game, 2 home
uint8_t scene_pause(void) {
    uint8_t sel = 0, r = 0xFF;
    cur_scene = SC_PAUSE;
    screen_off();
    display_bitmap_mode();
    pause_draw(sel);
    screen_on();
    fade_in();
    while (r == 0xFF) {
        frame();
        if (keys_rep & J_UP) { sel = sel ? sel - 1 : 4; sfx_move(); pause_draw(sel); }
        if (keys_rep & J_DOWN) { sel = sel < 4 ? sel + 1 : 0; sfx_move(); pause_draw(sel); }
        if (keys_new & (J_B | J_START)) { sfx_back(); r = 0; break; }
        if ((keys_new & J_A) || ((keys_rep & (J_LEFT | J_RIGHT)) && (sel == 2 || sel == 3))) {
            switch (sel) {
            case 0: r = 0; break;
            case 1: r = 1; break;
            case 2:
                if (keys_rep & J_LEFT) sv.theme = (sv.theme + 3) & 3;
                else sv.theme = (sv.theme + 1) & 3;
                cur_theme = sv.theme;
                save_write();
                sfx_set(1);
                pause_draw(sel);
                break;
            case 3:
                sv.sfx ^= 1;
                save_write();
                sfx_set(1);
                pause_draw(sel);
                break;
            default: r = 2; break;
            }
            if (r != 0xFF) sfx_page();
        }
    }
    fade_out();
    return r;
}
