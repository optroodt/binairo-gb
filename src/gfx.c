// Software surfaces: tile-ordered 2bpp bitmaps + proportional text.
#include "common.h"
#include "assets.h"

uint8_t gbuf[FB_BYTES];
surf_t fb = {gbuf, 20, 18};

// ---------------------------------------------------------------- bank-safe copies
// These live at the start of the non-banked code (bank 0), so they may map
// other ROM banks into 0x4000-0x7FFF while they run.
void vcopy(uint8_t *dst, const uint8_t *src, uint16_t n) {
    set_data(dst, src, n);
}

void vcopy_b(uint8_t bank, uint8_t *dst, const uint8_t *src, uint16_t n) {
    uint8_t save = _current_bank;
    SWITCH_ROM(bank);
    set_data(dst, src, n);
    SWITCH_ROM(save);
}

void bcopy(uint8_t bank, uint8_t *dst, const uint8_t *src, uint16_t n) {
    uint8_t save = _current_bank;
    SWITCH_ROM(bank);
    memcpy(dst, src, n);
    SWITCH_ROM(save);
}

// copy tiles from the asset bank into a surface
void surf_tiles(surf_t *s, uint8_t tx, uint8_t ty, uint8_t tw, uint8_t th, const uint8_t *tiles) {
    uint8_t r, save = _current_bank;
    uint16_t n;
    uint8_t *d;
    SWITCH_ROM(2);
    n = (uint16_t)tw << 4;
    for (r = 0; r < th; r++) {
        d = s->buf + ((uint16_t)((ty + r) * s->w + tx) << 4);
        memcpy(d, tiles, n);
        tiles += n;
    }
    SWITCH_ROM(save);
}

static uint8_t *pptr(surf_t *s, uint8_t tx, uint8_t py) {
    uint16_t t = tx;
    uint8_t i = py >> 3, w = s->w;
    while (i--) t += w;
    return s->buf + (t << 4) + ((py & 7) << 1);
}

static void apply(uint8_t *p, uint8_t m, uint8_t c) {
    if (c & 1) p[0] |= m; else p[0] &= ~m;
    if (c & 2) p[1] |= m; else p[1] &= ~m;
}

void surf_clear(surf_t *s, uint8_t c) {
    uint16_t n = (uint16_t)s->w * s->h * 16;
    if (c == 0) memset(s->buf, 0, n);
    else if (c == 3) memset(s->buf, 0xFF, n);
    else {
        uint8_t *p = s->buf;
        uint8_t lo = (c & 1) ? 0xFF : 0, hi = (c & 2) ? 0xFF : 0;
        n >>= 1;
        while (n--) { *p++ = lo; *p++ = hi; }
    }
}

static void hspan(surf_t *s, uint8_t x0, uint8_t x1, uint8_t y, uint8_t c) {
    // fills [x0, x1), clipped to the surface width
    uint8_t tx0, tx1, ml, mr, maxx = s->w << 3;
    uint8_t *p;
    if (x1 > maxx) x1 = maxx;
    if (x1 <= x0 || y >= (s->h << 3)) return;
    tx0 = x0 >> 3;
    tx1 = (x1 - 1) >> 3;
    ml = 0xFF >> (x0 & 7);
    mr = (uint8_t)(0xFF << (7 - ((x1 - 1) & 7)));
    p = pptr(s, tx0, y);
    if (tx0 == tx1) {
        apply(p, ml & mr, c);
        return;
    }
    apply(p, ml, c);
    for (++tx0; tx0 < tx1; ++tx0) { p += 16; apply(p, 0xFF, c); }
    apply(p + 16, mr, c);
}

void surf_fill(surf_t *s, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t c) {
    uint8_t i;
    if ((x >> 3) == ((x + w - 1) >> 3)) {
        // narrow: single tile column, step the pointer
        uint8_t m = (0xFF >> (x & 7)) & (uint8_t)(0xFF << (7 - ((x + w - 1) & 7)));
        uint8_t *p = pptr(s, x >> 3, y), yy = y;
        uint16_t adj = ((uint16_t)s->w << 4) - 16;
        for (i = 0; i < h; i++) {
            apply(p, m, c);
            p += 2;
            if (!(++yy & 7)) p += adj;
        }
        return;
    }
    for (i = 0; i < h; i++) hspan(s, x, x + w, y + i, c);
}

void surf_rrect(surf_t *s, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t c) {
    uint8_t i;
    for (i = 0; i < h; i++) {
        uint8_t in = 0;
        if (i == 0 || i == h - 1) in = 2;
        else if (i == 1 || i == h - 2) in = 1;
        hspan(s, x + in, x + w - in, y + i, c);
    }
}

void surf_frame(surf_t *s, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t c) {
    uint8_t i;
    hspan(s, x, x + w, y, c);
    hspan(s, x, x + w, y + h - 1, c);
    for (i = 1; i < h - 1; i++) {
        hspan(s, x, x + 1, y + i, c);
        hspan(s, x + w - 1, x + w, y + i, c);
    }
}

static const uint8_t *glyph_bits(uint8_t ch, uint8_t *w) {
    if (ch == 5 || ch == 6) {
        uint8_t k = (cur_theme << 1) + (ch - 5);
        *w = theme_mini_widths[k];
        return theme_mini_bits + k * 9;
    }
    if (ch >= 128) ch = '?';
    *w = font_widths[ch];
    return font_bits + ((uint16_t)ch << 3) + ch;
}

uint8_t text_width(const char *t, uint8_t bold) {
    uint8_t w = 0, gw;
    if (!*t) return 0;
    while (*t) {
        if (*t == '\n') break;
        glyph_bits((uint8_t)*t, &gw);
        w += gw + 1 + bold;
        t++;
    }
    return w - 1;
}

// ---- fast glyph blitter (hand written, parameters via globals)
uint8_t *gl_p;
const uint8_t *gl_bits;
uint8_t gl_n1, gl_n2, gl_sh, gl_ml, gl_has2, gl_c;
uint16_t gl_adj;

static void glyph_blit(void) __naked {
    __asm
    ld hl,#_gl_bits
    ld a,(hl+)
    ld e,a
    ld d,(hl)
    ld hl,#_gl_p
    ld a,(hl+)
    ld h,(hl)
    ld l,a
    ld a,(_gl_n1)
    ld b,a
07100$:
    ld a,(de)
    inc de
    or a
    jp z,07150$
    push bc
    ld c,a
    ld a,(_gl_sh)
    or a
    jr z,07105$
    ld b,a
    ld a,c
07101$:
    rrca
    dec b
    jr nz,07101$
    ld c,a
07105$:
    ld a,(_gl_ml)
    and c
    ld b,a
    ld a,(_gl_c)
    cp #3
    jr nz,07200$
    ; ---- fast path: ink on both planes
    ld a,(hl)
    or b
    ld (hl+),a
    ld a,(hl)
    or b
    ld (hl-),a
    ld a,(_gl_ml)
    cpl
    and c
    jr z,07149$
    ld b,a
    ld a,(_gl_has2)
    or a
    jr z,07149$
    push hl
    ld a,l
    add a,#16
    ld l,a
    jr nc,07106$
    inc h
07106$:
    ld a,(hl)
    or b
    ld (hl+),a
    ld a,(hl)
    or b
    ld (hl),a
    pop hl
    jr 07149$
07200$:
    ; ---- generic colour
    ld a,(_gl_c)
    rrca
    jr nc,07110$
    ld a,(hl)
    or b
    jr 07111$
07110$:
    ld a,b
    cpl
    and (hl)
07111$:
    ld (hl+),a
    ld a,(_gl_c)
    and #2
    jr z,07112$
    ld a,(hl)
    or b
    jr 07113$
07112$:
    ld a,b
    cpl
    and (hl)
07113$:
    ld (hl-),a
    ld a,(_gl_ml)
    cpl
    and c
    jr z,07149$
    ld b,a
    ld a,(_gl_has2)
    or a
    jr z,07149$
    push hl
    ld a,l
    add a,#16
    ld l,a
    jr nc,07120$
    inc h
07120$:
    ld a,(_gl_c)
    rrca
    jr nc,07121$
    ld a,(hl)
    or b
    jr 07122$
07121$:
    ld a,b
    cpl
    and (hl)
07122$:
    ld (hl+),a
    ld a,(_gl_c)
    and #2
    jr z,07123$
    ld a,(hl)
    or b
    jr 07124$
07123$:
    ld a,b
    cpl
    and (hl)
07124$:
    ld (hl),a
    pop hl
07149$:
    pop bc
07150$:
    inc hl
    inc hl
    dec b
    jp nz,07100$
    ld a,(_gl_n2)
    or a
    ret z
    ld b,a
    xor a
    ld (_gl_n2),a
    push de
    ld a,(_gl_adj)
    ld e,a
    ld a,(_gl_adj+1)
    ld d,a
    add hl,de
    pop de
    jp 07100$
    __endasm;
}

// draw len chars (or until NUL if len == 255); returns the new x
static uint8_t draw_str(surf_t *s, uint8_t x, uint8_t y, const char *t, uint8_t len, uint8_t c, uint8_t bold) {
    uint8_t maxy = s->h << 3, gw, n, n1, n2, ch, tx, px, k, w = s->w;
    uint8_t *rowbase;
    const uint8_t *bits;
    if (y >= maxy) return x;
    n = maxy - y;
    if (n > 9) n = 9;
    n1 = 8 - (y & 7);
    if (n1 > n) n1 = n;
    n2 = n - n1;
    rowbase = pptr(s, 0, y);
    gl_adj = ((uint16_t)s->w << 4) - 16;
    gl_c = c;
    while (len-- && *t) {
        ch = (uint8_t)*t++;
        bits = glyph_bits(ch, &gw);
        if (ch != ' ') {
            for (k = 0; k <= (bold && ch >= 16); k++) {
                px = x + k;
                tx = px >> 3;
                if (tx >= w) break;
                gl_p = rowbase + ((uint16_t)tx << 4);
                gl_bits = bits;
                gl_n1 = n1;
                gl_n2 = n2;
                gl_sh = px & 7;
                gl_ml = 0xFF >> gl_sh;
                gl_has2 = (uint8_t)(tx + 1) < w;
                glyph_blit();
            }
        }
        x += gw + 1 + bold;
    }
    return x;
}

uint8_t surf_text(surf_t *s, uint8_t x, uint8_t y, const char *t, uint8_t c, uint8_t bold) {
    return draw_str(s, x, y, t, 255, c, bold);
}

void surf_text_c(surf_t *s, uint8_t cx, uint8_t y, const char *t, uint8_t c, uint8_t bold) {
    draw_str(s, cx - (text_width(t, bold) >> 1), y, t, 255, c, bold);
}

// word wrap; '\n' forces a break. Draws word by word.
uint8_t surf_wrap(surf_t *s, uint8_t x, uint8_t y, uint8_t maxw, const char *t, uint8_t c) {
    uint8_t cx = x, ww, ch, first = 1;
    const char *w;
    while (*t) {
        if (*t == '\n') { cx = x; y += 10; first = 1; t++; continue; }
        if (*t == ' ') { t++; continue; }
        // measure word
        ww = 0;
        for (w = t; *w && *w != ' ' && *w != '\n'; w++) {
            ch = (uint8_t)*w;
            ww += (ch == 5 || ch == 6) ? theme_mini_widths[(cur_theme << 1) + ch - 5] + 1 : font_widths[ch] + 1;
        }
        if (!first) {
            if ((uint8_t)(cx - x) + 3 + ww - 1 > maxw) { cx = x; y += 10; }
            else cx += 3;
        }
        first = 0;
        cx = draw_str(s, cx, y, t, (uint8_t)(w - t), c, 0);
        t = w;
    }
    return y + 10;
}

void fb_upload(uint8_t ty0, uint8_t ty1) {
    uint16_t off = (uint16_t)ty0 * 320;
    vcopy((uint8_t *)(0x8000 + off), gbuf + off, (uint16_t)(ty1 - ty0) * 320);
}

void fb_upload_tile(uint8_t tx, uint8_t ty) {
    uint16_t off = ((uint16_t)ty * 20 + tx) << 4;
    vcopy((uint8_t *)(0x8000 + off), gbuf + off, 16);
}

void surf_upload(surf_t *s, uint8_t *vram) {
    vcopy(vram, s->buf, (uint16_t)s->w * s->h * 16);
}

// ----------------------------------------------------------------- strings
char *str_cpy(char *d, const char *s) {
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

char *str_num(char *d, uint16_t v) {
    char tmp[6];
    uint8_t n = 0;
    do { tmp[n++] = '0' + (v % 10); v /= 10; } while (v);
    while (n) *d++ = tmp[--n];
    *d = 0;
    return d;
}

char *str_time(char *d, uint16_t secs) {
    uint8_t m, s;
    if (secs > 5999) secs = 5999;
    m = secs / 60; s = secs % 60;
    *d++ = '0' + m / 10; *d++ = '0' + m % 10; *d++ = ':';
    *d++ = '0' + s / 10; *d++ = '0' + s % 10; *d = 0;
    return d;
}
