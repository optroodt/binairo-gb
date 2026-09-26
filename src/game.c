#pragma bank 1
// Gameplay scene: board, cursor, HUD, hint panel, tutorial.
#include "game.h"
#include "assets.h"
#include "puzzles.h"

// ------------------------------------------------------------ tile map
#define T_PANEL 0     // 7x18 = 126 tiles (0x9000 block)
#define T_BLANK 126
#define T_CELL 128    // 48 tiles (0x8800 block)
#define T_RING 176    // 16 tiles
#define T_HUD 192     // HUD surfaces

#define PANEL_W 7
#define PANEL_PX 56
#define PANEL_X (160 - PANEL_PX)

// sprites
#define SP_CUR 0      // 0..3
#define SP_TAB 4      // 4..6
#define SP_CHK 8      // 8..15

static uint8_t *const panel_buf = gbuf;
static uint8_t *const hud1_buf = gbuf + 126 * 16;
static uint8_t *const hud2_buf = gbuf + 126 * 16 + 18 * 16;
static surf_t panel = {gbuf, PANEL_W, 18};
static surf_t hud1, hud2;

static uint8_t bx0, by0;           // board ring top-left (tiles)
static uint8_t cur_r, cur_c;
static uint8_t spr_x, spr_y;       // cursor sprite pos (screen px)
static uint8_t cursor_on;
static uint8_t dim_on;
static uint8_t qmark = 0xFF;
static uint8_t scx;                // board scroll (8x8 hints)
static uint8_t tab_x;
static uint8_t gmode;               // 0..3
static uint8_t tut;                // tutorial active
static uint8_t tut_step;
static uint8_t edit_line = 0xFF;   // tutorial: only this line editable
static uint8_t frames;
static uint16_t secs;
static uint8_t undo_i[48], undo_v[48], undo_n, undo_p;
static uint8_t puz_index;
static char tbuf[200];

static uint8_t *vram_of(uint8_t idx) {
    return (uint8_t *)(idx < 128 ? 0x9000 + ((uint16_t)idx << 4) : 0x8800 + ((uint16_t)(idx - 128) << 4));
}

// ------------------------------------------------------------ board drawing
static uint8_t cell_px(uint8_t c) { return ((bx0 + 1 + (c << 1)) << 3); }
static uint8_t cell_py(uint8_t r) { return ((by0 + 1 + (r << 1)) << 3); }

static void draw_cell(uint8_t i) {
    uint8_t v, d, base, t[4];
    uint8_t val = cell[i];
    if (i == qmark) v = 5;
    else if (!val) v = 0;
    else if (giv[i]) v = 2 + val;
    else v = val;
    d = (dim_on && !hl[i]) ? 1 : 0;
    base = T_CELL + (((v << 1) + d) << 2);
    t[0] = base; t[1] = base + 1; t[2] = base + 2; t[3] = base + 3;
    set_bkg_tiles(bx0 + 1 + ((i & 7) << 1), by0 + 1 + ((i >> 3) << 1), 2, 2, t);
}

static void draw_ring(void) {
    uint8_t row[18], i, d = dim_on ? 8 : 0, w = (N << 1) + 2;
    row[0] = T_RING + d + 0;
    for (i = 1; i < w - 1; i++) row[i] = T_RING + d + 1;
    row[w - 1] = T_RING + d + 2;
    set_bkg_tiles(bx0, by0, w, 1, row);
    row[0] = T_RING + d + 5;
    for (i = 1; i < w - 1; i++) row[i] = T_RING + d + 6;
    row[w - 1] = T_RING + d + 7;
    set_bkg_tiles(bx0, by0 + w - 1, w, 1, row);
    row[0] = T_RING + d + 3;
    for (i = 1; i < w - 1; i++) {
        set_bkg_tiles(bx0, by0 + i, 1, 1, row);
    }
    row[0] = T_RING + d + 4;
    for (i = 1; i < w - 1; i++) {
        set_bkg_tiles(bx0 + w - 1, by0 + i, 1, 1, row);
    }
}

static void draw_board(void) {
    uint8_t r, c;
    draw_ring();
    for (r = 0; r < N; r++)
        for (c = 0; c < N; c++) draw_cell((r << 3) + c);
}

// ------------------------------------------------------------ HUD
static void hud_time(void) {
    char *p;
    surf_clear(&hud1, 0);
    if (N == 6) {
        surf_text_c(&hud1, 24, 1, "TIME", 3, 0);
        str_time(tbuf, secs);
        surf_text_c(&hud1, 24, 12, tbuf, 3, 0);
    } else {
        uint16_t s = secs > 5999 ? 5999 : secs;
        surf_text_c(&hud1, 8, 1, "\x08", 3, 0);
        p = tbuf;
        *p++ = '0' + (s / 60) / 10; *p++ = '0' + (s / 60) % 10; *p = 0;
        surf_text_c(&hud1, 8, 13, tbuf, 3, 0);
        p = tbuf;
        *p++ = '0' + (s % 60) / 10; *p++ = '0' + (s % 60) % 10; *p = 0;
        surf_text_c(&hud1, 8, 23, tbuf, 3, 0);
    }
    surf_upload(&hud1, vram_of(T_HUD));
}

static void hud_setup(void) {
    uint8_t x, y, t[20];
    // blank everything right of the board
    for (x = 0; x < 20; x++) t[x] = T_BLANK;
    for (y = 0; y < 32; y++) set_bkg_tiles(0, y, 20, 1, t);
    for (y = 0; y < 18; y++) set_bkg_tiles(20, y, 12, 1, t);
    if (N == 6) {
        hud1.buf = hud1_buf; hud1.w = 6; hud1.h = 3;
        hud2.buf = hud2_buf; hud2.w = 6; hud2.h = 4;
        for (y = 0; y < 3; y++) {
            for (x = 0; x < 6; x++) t[x] = T_HUD + y * 6 + x;
            set_bkg_tiles(14, 2 + y, 6, 1, t);
        }
        for (y = 0; y < 4; y++) {
            for (x = 0; x < 6; x++) t[x] = T_HUD + 18 + y * 6 + x;
            set_bkg_tiles(14, 12 + y, 6, 1, t);
        }
        surf_clear(&hud2, 0);
        surf_text(&hud2, 6, 1, "\x01 Set", 3, 0);
        surf_text(&hud2, 6, 11, "\x02 Help", 3, 0);
        surf_text(&hud2, 6, 21, "\x03 Move", 3, 0);
        surf_upload(&hud2, vram_of(T_HUD + 18));
    } else {
        hud1.buf = hud1_buf; hud1.w = 2; hud1.h = 4;
        for (y = 0; y < 4; y++) {
            t[0] = T_HUD + y * 2; t[1] = T_HUD + y * 2 + 1;
            set_bkg_tiles(18, 1 + y, 2, 1, t);
        }
    }
    hud_time();
}

// ------------------------------------------------------------ sprites
static void place_cursor(void) {
    uint8_t x = spr_x + 8, y = spr_y + 16;
    if (!cursor_on) {
        hide_sprite(SP_CUR); hide_sprite(SP_CUR + 1);
        hide_sprite(SP_CUR + 2); hide_sprite(SP_CUR + 3);
        return;
    }
    move_sprite(SP_CUR, x, y);
    move_sprite(SP_CUR + 1, x + 8, y);
    move_sprite(SP_CUR + 2, x, y + 8);
    move_sprite(SP_CUR + 3, x + 8, y + 8);
}

static void cursor_target(uint8_t *tx, uint8_t *ty) {
    *tx = cell_px(cur_c) - scx;
    *ty = cell_py(cur_r);
}

static void cursor_snap(void) {
    cursor_target(&spr_x, &spr_y);
    place_cursor();
}

static uint8_t ease(uint8_t cur, uint8_t tgt) {
    if (cur < tgt) return cur + ((tgt - cur + 1) >> 1);
    if (cur > tgt) return cur - ((cur - tgt + 1) >> 1);
    return cur;
}

static void cursor_update(void) {
    uint8_t tx, ty;
    cursor_target(&tx, &ty);
    spr_x = ease(spr_x, tx);
    spr_y = ease(spr_y, ty);
    place_cursor();
}

static void place_tab(void) {
    uint8_t y = (N == 6) ? 60 : 64;
    move_sprite(SP_TAB, tab_x + 8, y + 16);
    move_sprite(SP_TAB + 1, tab_x + 8, y + 24);
    move_sprite(SP_TAB + 2, tab_x + 8, y + 32);
}

static void checks_hide(void) {
    uint8_t i;
    for (i = 0; i < 8; i++) hide_sprite(SP_CHK + i);
}

// pop checkmarks over a line
static void checks_line(uint8_t line, uint8_t lift) {
    uint8_t k, i;
    for (k = 0; k < N; k++) {
        i = line_cell(line, k);
        move_sprite(SP_CHK + k, cell_px(i & 7) - scx + 12 + 8, cell_py(i >> 3) - lift + 16 - 4);
    }
}

static void setup_sprites(void) {
    uint8_t i;
    vcopy_b(ASSET_BANK, (uint8_t *)0x8000, sprite_tiles, SPRITE_TILE_COUNT * 16);
    for (i = 0; i < 40; i++) hide_sprite(i);
    for (i = 0; i < 4; i++) set_sprite_tile(SP_CUR + i, SPR_T_CURSOR + i);
    for (i = 0; i < 3; i++) set_sprite_tile(SP_TAB + i, SPR_T_TAB + i);
    for (i = 0; i < 8; i++) set_sprite_tile(SP_CHK + i, SPR_T_CHECK);
    for (i = 0; i < 40; i++) set_sprite_prop(i, 0);
}

// ------------------------------------------------------------ setup display
static void load_theme_tiles(void) {
    vcopy_b(ASSET_BANK, vram_of(T_CELL), cell_tiles + (uint16_t)cur_theme * (48 * 16), 48 * 16);
}

static void setup_display(void) {
    static const uint8_t blank[16] = {0};
    screen_off();
    display_game_mode();
    SCX_REG = scx; SCY_REG = 0;
    HIDE_WIN;
    WX_REG = 167; WY_REG = 0;
    vcopy(vram_of(T_BLANK), blank, 16);
    load_theme_tiles();
    vcopy_b(ASSET_BANK, vram_of(T_RING), ring_tiles, 16 * 16);
    setup_sprites();
    hud_setup();
    draw_board();
    tab_x = 152;
    place_tab();
    cursor_snap();
}

static void show_display(void) {
    SHOW_SPRITES;
    screen_on();
}

// ------------------------------------------------------------ puzzle load
static void load_puzzle(const uint8_t *src) {
    uint8_t r, c, i, p[16];
    bcopy(PUZZLE_BANK, p, src, 16);
    memset(cell, 0, 64); memset(giv, 0, 64); memset(sol, 0, 64);
    for (r = 0; r < N; r++) {
        for (c = 0; c < N; c++) {
            i = (r << 3) + c;
            sol[i] = (p[r] >> c) & 1 ? V_ONE : V_ZERO;
            if ((p[N + r] >> c) & 1) { giv[i] = 1; cell[i] = sol[i]; }
        }
    }
}

static const uint8_t *puzzle_ptr(uint8_t m, uint8_t idx) {
    switch (m) {
    case 0: return puz_6_easy[idx];
    case 1: return puz_6_hard[idx];
    case 2: return puz_8_easy[idx];
    default: return puz_8_hard[idx];
    }
}

static void save_progress(void) {
    if (tut) return;
    sv.has_resume[gmode] = 1;
    sv.res_puz[gmode] = puz_index;
    sv.res_time[gmode] = secs;
    memcpy(sv.res_cells[gmode], cell, 64);
    save_write();
}

// ------------------------------------------------------------ panel
static void panel_draw(const char *title, const char *body, const char *legend) {
    uint8_t y, b;
    surf_clear(&panel, 0);
    // rounded frame, open to the right
    surf_fill(&panel, 2, 2, PANEL_PX - 2, 2, 3);
    surf_fill(&panel, 2, 140, PANEL_PX - 2, 2, 3);
    surf_fill(&panel, 0, 4, 2, 136, 3);
    surf_fill(&panel, 1, 3, 2, 1, 3);
    surf_fill(&panel, 1, 140, 2, 1, 3);
    b = text_width(title, 1) <= 52 ? 1 : 0;
    surf_text_c(&panel, 29, 8, title, 3, b);
    y = surf_wrap(&panel, 6, 22, 48, body, 3);
    (void)y;
    if (legend) {
        const char *l2 = legend;
        while (*l2 && *l2 != '\n') l2++;
        if (*l2) {
            // two-line legend
            memcpy(tbuf, legend, l2 - legend);
            tbuf[l2 - legend] = 0;
            surf_text(&panel, 6, 118, tbuf, 3, 0);
            surf_text(&panel, 6, 128, l2 + 1, 3, 0);
        } else {
            surf_text_c(&panel, 29, 128, legend, 3, 0);
        }
    }
    surf_upload(&panel, vram_of(T_PANEL));
}

static void panel_map(void) {
    uint8_t y, x, t[PANEL_W];
    for (y = 0; y < 18; y++) {
        for (x = 0; x < PANEL_W; x++) t[x] = T_PANEL + y * PANEL_W + x;
        set_win_tiles(0, y, PANEL_W, 1, t);
    }
}

static uint8_t want_scx;

static void anim_frame(void);

static void panel_slide(uint8_t open) {
    uint8_t target = open ? PANEL_X : 160;
    uint8_t x = open ? 160 : PANEL_X;
    SHOW_WIN;
    while (1) {
        x = ease(x, target);
        WX_REG = x + 7;
        tab_x = x - 7;
        if (tab_x > 152) tab_x = 152;
        if (x < 150) tab_x = 200;  // tuck the tab away while the panel is open
        place_tab();
        anim_frame();
        frame();
        if (x == target) break;
    }
    if (!open) HIDE_WIN;
}

// timer & smooth scrolling while other UI runs
static void tick_timer(void) {
    if (++frames >= 60) {
        frames = 0;
        if (secs < 5999) secs++;
        if (!tut || tut_step >= 5) hud_time();
    }
}

static void anim_frame(void) {
    if (scx != want_scx) {
        scx = ease(scx, want_scx);
        SCX_REG = scx;
    }
    cursor_update();
    tick_timer();
}

// ------------------------------------------------------------ text helpers
static char *app_line(char *p, uint8_t line) {
    if (line < N) { p = str_cpy(p, "row "); p = str_num(p, line + 1); }
    else { p = str_cpy(p, "column "); p = str_num(p, line - N + 1); }
    return p;
}
static char *app_sym(char *p, uint8_t v) {
    *p++ = v == V_ZERO ? 5 : 6;
    *p = 0;
    return p;
}

static void hint_texts(uint8_t page, const char **title) {
    char *p = tbuf + 60;  // body buffer (tbuf[0..59] used for legend temp)
    const char *line_word = hint.line < N ? "row" : "column";
    *title = "Hint";
    switch (hint.kind) {
    case H_PAIR:
        *title = "Find Pairs";
        if (!page) str_cpy(p, "No more than two identical symbols may be placed next to each other.");
        break;
    case H_TRIO:
        *title = "Avoid Trios";
        if (!page) str_cpy(p, "An empty cell between two identical symbols needs the opposite symbol.");
        break;
    case H_COUNT:
        *title = "Same Sum";
        if (!page) {
            p = str_cpy(p, "This ");
            p = str_cpy(p, line_word);
            p = str_cpy(p, " already has ");
            p = str_num(p, N >> 1);
            p = str_cpy(p, "x ");
            p = app_sym(p, hint.aux);
            p = str_cpy(p, ". Every line needs as many ");
            p = app_sym(p, V_ZERO);
            p = str_cpy(p, " as ");
            p = app_sym(p, V_ONE);
            str_cpy(p, ".");
        } else {
            p = str_cpy(p, "So all empty cells in it get a ");
            p = app_sym(p, hint.value);
            str_cpy(p, ". Start at the ?");
            return;
        }
        break;
    case H_COMBO:
        *title = "Look Ahead";
        if (!page) {
            p = str_cpy(p, "Try a ");
            p = app_sym(p, hint.aux);
            p = str_cpy(p, " at the ?: the rest of this ");
            p = str_cpy(p, line_word);
            str_cpy(p, " can't be filled without breaking a rule.");
        }
        break;
    case H_UNIQUE:
        *title = "Be Unique";
        if (!page) {
            p = str_cpy(p, "No two ");
            p = str_cpy(p, line_word);
            p = str_cpy(p, "s may match. A ");
            p = app_sym(p, hint.aux);
            p = str_cpy(p, " at the ? makes this ");
            p = str_cpy(p, line_word);
            p = str_cpy(p, " like ");
            p = app_line(p, hint.aux2);
            str_cpy(p, ".");
        }
        break;
    case H_MISTAKE:
        *title = "Oops!";
        if (!page) {
            switch (hint.aux) {
            case M_TRIO: str_cpy(p, "Three identical symbols are next to each other."); break;
            case M_COUNT:
                p = str_cpy(p, "This ");
                p = str_cpy(p, line_word);
                p = str_cpy(p, " has too many ");
                p = app_sym(p, hint.aux2);
                str_cpy(p, ".");
                break;
            case M_DUP:
                p = app_line(p, hint.line);
                p = str_cpy(p, " and ");
                p = app_line(p, hint.aux2);
                str_cpy(p, " are the same.");
                tbuf[60] -= 32;  // capitalise
                break;
            default: str_cpy(p, "This symbol does not fit the solution."); break;
            }
        } else {
            str_cpy(p, "Let's remove it and try again.");
        }
        return;
    default:
        *title = "Tough One";
        if (!page) str_cpy(p, "No single rule helps here. Thinking a few steps ahead shows the answer.");
        break;
    }
    if (page) {
        p = tbuf + 60;
        if (hint.kind == H_PAIR || hint.kind == H_TRIO) {
            p = str_cpy(p, "So the opposite symbol, a ");
            p = app_sym(p, hint.value);
            str_cpy(p, ", goes where the ? is.");
        } else {
            p = str_cpy(p, "So the ? must be a ");
            p = app_sym(p, hint.value);
            str_cpy(p, ".");
        }
    }
}

// choose board scroll so the hint focus stays visible (8x8 only)
static void hint_scroll(uint8_t on) {
    uint8_t i, x, minx = 255, maxx = 0, tx;
    want_scx = 0;
    if (!on || N != 8) return;
    for (i = 0; i < 64; i++) {
        if (!hl[i] && i != hint.target) continue;
        x = cell_px(i & 7);
        if (x < minx) minx = x;
        if (x + 16 > maxx) maxx = x + 16;
    }
    tx = cell_px(hint.target & 7) + 16;
    // visible area left of the tab: 0 .. PANEL_X-8
    if (maxx + 2 > PANEL_X - 8) want_scx = maxx + 2 - (PANEL_X - 8);
    if (minx >= 3 && want_scx > minx - 3) want_scx = minx - 3;
    if (tx + 2 > PANEL_X - 8 + want_scx) want_scx = tx + 2 - (PANEL_X - 8);
    if (want_scx > 48) want_scx = 48;
}

static void run_to_scroll(void) {
    uint8_t n = 0;
    while (scx != want_scx && n++ < 30) { anim_frame(); frame(); }
}

// returns 1 if the hint was applied
static uint8_t run_hint(void) {
    const char *title;
    uint8_t page = 0, save_r = cur_r, save_c = cur_c, applied = 0;
    if (hint_find() == H_NONE) return 0;
    sfx_hint();
    dim_on = 1;
    draw_board();
    hint_scroll(1);
    hint_texts(0, &title);
    panel_draw(title, tbuf + 60, "\x01 Continue");
    panel_map();
    // point the cursor at the target (mistakes) or hide it
    cur_r = hint.target >> 3; cur_c = hint.target & 7;
    cursor_on = hint.kind == H_MISTAKE;
    place_cursor();
    run_to_scroll();
    panel_slide(1);
    while (1) {
        anim_frame();
        frame();
        if (keys_new & J_B) { sfx_back(); break; }
        if (keys_new & (J_A | J_START)) {
            if (page == 0) {
                sfx_page();
                page = 1;
                if (hint.kind != H_MISTAKE) {
                    qmark = hint.target;
                    draw_cell(qmark);
                    cursor_on = 1;
                    cursor_snap();
                }
                hint_texts(1, &title);
                panel_draw(title, tbuf + 60, "\x01 Continue");
            } else {
                applied = 1;
                break;
            }
        }
    }
    panel_slide(0);
    qmark = 0xFF;
    dim_on = 0;
    if (applied) {
        uint8_t i = hint.target;
        undo_i[undo_p] = i; undo_v[undo_p] = cell[i];
        undo_p = (undo_p + 1) % 48; if (undo_n < 48) undo_n++;
        cell[i] = hint.value;
        sfx_set(hint.value);
        save_progress();
    } else {
        cur_r = save_r; cur_c = save_c;
    }
    draw_board();
    want_scx = 0;
    cursor_on = 1;
    run_to_scroll();
    return applied;
}

// ------------------------------------------------------------ win
static uint8_t win_sequence(void) {
    uint8_t r, f;
    cursor_on = 0;
    place_cursor();
    for (r = 0; r < N; r++) {
        checks_line(r, 0);
        sfx_check(r);
        for (f = 0; f < 4; f++) { checks_line(r, f); tick_timer(); frame(); }
        for (f = 0; f < 3; f++) frame();
    }
    checks_hide();
    sfx_win();
    for (f = 0; f < 8; f++) {
        set_shades(f & 1 ? 0xE4 : 0x1B);
        frame(); frame(); frame(); frame();
    }
    set_shades(0xE4);
    for (f = 0; f < 50; f++) frame();
    return 1;
}

// ------------------------------------------------------------ tutorial
typedef struct {
    const char *title;
    const char *text;
    uint8_t line;   // editable line, 0xFF = none
    uint8_t line2;  // extra highlighted line
} tstep_t;

static const tstep_t tsteps[] = {
    {"Binairo", "A puzzle with only 0's and 1's. Some cells are filled at the start. Use three rules to solve it.", 0xFF, 0xFF},
    {"1. Rule", "Every row and every column must contain as many 0's as 1's. Try it out!", 3, 0xFF},
    {"2. Rule", "No more than two 0's or two 1's may sit next to each other. Try it!", 6 + 4, 0xFF},
    {"3. Rule", "All rows and all columns must be unique. Column 6 may not match column 3.", 6 + 5, 6 + 2},
    {"Well done!", "Now you know all the rules. Solve the rest of the puzzle.\n\nStuck? Press \x02 for help.", 0xFF, 0xFF},
};

static uint8_t line_done(uint8_t line) {
    uint8_t k, i;
    for (k = 0; k < N; k++) {
        i = line_cell(line, k);
        if (cell[i] != sol[i]) return 0;
    }
    return 1;
}

static void tut_show(void) {
    const tstep_t *s = &tsteps[tut_step];
    uint8_t k;
    memset(hl, 0, 64);
    edit_line = s->line;
    dim_on = s->line != 0xFF;
    if (s->line != 0xFF) {
        for (k = 0; k < N; k++) hl[line_cell(s->line, k)] = 1;
        if (s->line2 != 0xFF)
            for (k = 0; k < N; k++) hl[line_cell(s->line2, k)] = 1;
        // cursor to first empty cell in the line
        for (k = 0; k < N; k++) {
            uint8_t i = line_cell(s->line, k);
            if (!cell[i]) { cur_r = i >> 3; cur_c = i & 7; break; }
        }
    }
    cursor_on = s->line != 0xFF;
    draw_board();
    panel_draw(s->title, s->text, s->line != 0xFF ? "\x01 Set 0/1\n\x03 Move" : "\x01 Continue");
    panel_map();
}

static void tut_move(int8_t d) {
    // move along the editable line
    uint8_t k, i;
    for (k = 0; k < N; k++) {
        i = line_cell(edit_line, k);
        if (i == (cur_r << 3) + cur_c) break;
    }
    if (d < 0 && k > 0) k--;
    else if (d > 0 && k + 1 < N) k++;
    else return;
    i = line_cell(edit_line, k);
    cur_r = i >> 3; cur_c = i & 7;
    sfx_move();
}

// ------------------------------------------------------------ main scene
uint8_t scene_game(uint8_t tutorial, uint8_t resume) {
    uint8_t i, saved_theme = cur_theme, act;
    tut = tutorial;
    gmode = (sv.sel_size << 1) | sv.sel_diff;
    N = tut ? 6 : (sv.sel_size ? 8 : 6);
    bx0 = 0;
    by0 = N == 6 ? 2 : 0;
    scx = want_scx = 0;
    dim_on = 0; qmark = 0xFF;
    undo_n = undo_p = 0;
    frames = 0; secs = 0;
    edit_line = 0xFF;
    hint_init();
    if (tut) {
        cur_theme = 0;
        load_puzzle(puz_tutorial);
        tut_step = 0;
    } else if (resume && sv.has_resume[gmode]) {
        puz_index = sv.res_puz[gmode];
        load_puzzle(puzzle_ptr(gmode, puz_index));
        memcpy(cell, sv.res_cells[gmode], 64);
        secs = sv.res_time[gmode];
    } else {
        puz_index = sv.next[gmode] % PUZZLE_COUNT;
        sv.next[gmode] = (puz_index + 1) % PUZZLE_COUNT;
        load_puzzle(puzzle_ptr(gmode, puz_index));
        sv.started[gmode]++;
        save_progress();
    }
    cur_r = cur_c = 0;
    // start the cursor on the first free cell
    for (i = 0; i < 64; i++)
        if ((i >> 3) < N && (i & 7) < N && !giv[i]) { cur_r = i >> 3; cur_c = i & 7; break; }
    cursor_on = 1;
    setup_display();
    if (tut) {
        tut_show();
        SHOW_WIN;
        WX_REG = 167;
    }
    show_display();
    fade_in();
    if (tut) panel_slide(1);

    while (1) {
        frame();
        anim_frame();

        if (tut && tut_step < 4) {
            // tutorial flow
            if (edit_line == 0xFF) {
                if (keys_new & (J_A | J_START)) {
                    sfx_page();
                    tut_step++;
                    tut_show();
                    if (tut_step == 4) continue;
                }
                continue;
            }
            if (edit_line < N) {
                if (keys_rep & J_LEFT) tut_move(-1);
                if (keys_rep & J_RIGHT) tut_move(1);
            } else {
                if (keys_rep & J_UP) tut_move(-1);
                if (keys_rep & J_DOWN) tut_move(1);
            }
            if (keys_new & J_A) {
                i = (cur_r << 3) + cur_c;
                if (!giv[i]) {
                    cell[i] = (cell[i] + 1) % 3;
                    sfx_set(cell[i]);
                    draw_cell(i);
                    if (line_done(edit_line)) {
                        uint8_t f;
                        cursor_on = 0; place_cursor();
                        for (f = 0; f < 50; f++) {
                            if (f < N * 3 && f % 3 == 0) sfx_check(f / 3);
                            checks_line(edit_line, f < 8 ? f : 8);
                            frame();
                        }
                        checks_hide();
                        tut_step++;
                        tut_show();
                    }
                } else sfx_error();
            }
            continue;
        }
        if (tut && tut_step == 4) {
            // "well done" panel: continue into free play
            if (keys_new & (J_A | J_START)) {
                sfx_page();
                panel_slide(0);
                dim_on = 0;
                memset(hl, 0, 64);
                cursor_on = 1;
                draw_board();
                tut_step = 5;
            }
            continue;
        }

        // ---------------- normal play
        if (keys_rep & J_LEFT) { cur_c = cur_c ? cur_c - 1 : N - 1; sfx_move(); }
        if (keys_rep & J_RIGHT) { cur_c = cur_c + 1 < N ? cur_c + 1 : 0; sfx_move(); }
        if (keys_rep & J_UP) { cur_r = cur_r ? cur_r - 1 : N - 1; sfx_move(); }
        if (keys_rep & J_DOWN) { cur_r = cur_r + 1 < N ? cur_r + 1 : 0; sfx_move(); }
        if (keys_new & J_A) {
            i = (cur_r << 3) + cur_c;
            if (giv[i]) sfx_error();
            else {
                undo_i[undo_p] = i; undo_v[undo_p] = cell[i];
                undo_p = (undo_p + 1) % 48; if (undo_n < 48) undo_n++;
                cell[i] = (cell[i] + 1) % 3;
                sfx_set(cell[i]);
                draw_cell(i);
                save_progress();
            }
        }
        if (keys_new & J_SELECT) {
            if (undo_n) {
                undo_n--;
                undo_p = (undo_p + 47) % 48;
                i = undo_i[undo_p];
                cell[i] = undo_v[undo_p];
                cur_r = i >> 3; cur_c = i & 7;
                draw_cell(i);
                sfx_back();
                save_progress();
            } else sfx_error();
        }
        if (keys_new & J_B) {
            run_hint();
        }
        if (board_full_correct()) {
            win_sequence();
            if (tut) {
                sv.tut_done = 1;
                save_write();
                cur_theme = saved_theme;
                fade_out();
                return SC_HOME;
            }
            sv.has_resume[gmode] = 0;
            sv.won[gmode]++;
            last_result_time = secs;
            last_result_best = 0;
            if (!sv.best[gmode] || secs < sv.best[gmode]) {
                if (sv.best[gmode]) last_result_best = 1;
                sv.best[gmode] = secs;
            }
            last_result_time_valid = 1;
            save_write();
            fade_out();
            return SC_RESULT;
        }
        if (keys_new & J_START) {
            fade_out();
            HIDE_SPRITES;
            act = scene_pause();
            cur_scene = tut ? SC_TUTORIAL : SC_GAME;
            if (act == 2) { if (tut) cur_theme = saved_theme; return SC_HOME; }
            if (act == 1) {
                if (tut) { cur_theme = saved_theme; return SC_TUTORIAL; }
                sv.has_resume[gmode] = 0;
                save_write();
                return SC_GAME;
            }
            if (tut) { saved_theme = cur_theme; cur_theme = 0; }
            setup_display();
            show_display();
            fade_in();
        }
    }
}
