// Hint engine: finds the easiest next logical step, or a mistake.
#include "game.h"

uint8_t N = 6;
uint8_t cell[64];
uint8_t giv[64];
uint8_t sol[64];
uint8_t hl[64];
hint_t hint;

static uint8_t pats[34];
static uint8_t npats;
static uint8_t full;
static uint8_t comp_ok[16];
static uint8_t comp_pat[16];

void hint_init(void) {
    uint16_t p;
    uint8_t half = N >> 1, cnt, b, inv;
    full = (uint8_t)((1u << N) - 1);
    npats = 0;
    for (p = 0; p <= full; p++) {
        b = (uint8_t)p;
        cnt = 0;
        for (inv = 0; inv < N; inv++) if (b & (1 << inv)) cnt++;
        if (cnt != half) continue;
        if (b & (b >> 1) & (b >> 2)) continue;
        inv = ~b & full;
        if (inv & (inv >> 1) & (inv >> 2)) continue;
        pats[npats++] = b;
    }
}

uint8_t line_cell(uint8_t line, uint8_t k) {
    return line < N ? (line << 3) + k : (k << 3) + (line - N);
}

#define LV(line, k) cell[line_cell(line, k)]

static void line_bits(uint8_t line, uint8_t *known, uint8_t *vals) {
    uint8_t k, v, kn = 0, va = 0;
    for (k = 0; k < N; k++) {
        v = LV(line, k);
        if (v) {
            kn |= 1 << k;
            if (v == V_ONE) va |= 1 << k;
        }
    }
    *known = kn;
    *vals = va;
}

static void hl_clear(void) { memset(hl, 0, 64); }
static void hl_line(uint8_t line) {
    uint8_t k;
    for (k = 0; k < N; k++) hl[line_cell(line, k)] = 1;
}

static void calc_complete(void) {
    uint8_t L, kn, va;
    for (L = 0; L < 2 * N; L++) {
        line_bits(L, &kn, &va);
        comp_ok[L] = (kn == full);
        comp_pat[L] = va;
    }
}

// is pattern p equal to another complete line of the same orientation?
static uint8_t excluded_by(uint8_t line, uint8_t p) {
    uint8_t L, L0 = line < N ? 0 : N;
    for (L = L0; L < L0 + N; L++)
        if (L != line && comp_ok[L] && comp_pat[L] == p) return L + 1;
    return 0;
}

static uint8_t enum_line(uint8_t line, uint8_t uniq, uint8_t *f1, uint8_t *f0) {
    uint8_t kn, va, i, p, a = 0xFF, o = 0, cnt = 0;
    line_bits(line, &kn, &va);
    for (i = 0; i < npats; i++) {
        p = pats[i];
        if ((p ^ va) & kn) continue;
        if (uniq && excluded_by(line, p)) continue;
        a &= p;
        o |= p;
        cnt++;
    }
    *f1 = a & ~kn & full;
    *f0 = ~o & ~kn & full;
    return cnt;
}

static uint8_t first_bit(uint8_t m) {
    uint8_t k = 0;
    while (!(m & 1)) { m >>= 1; k++; }
    return k;
}

static uint8_t find_mistake(void) {
    uint8_t i, r, c, L, k, v, cnt, o, kn, va;
    for (i = 0; i < 64; i++) {
        if (giv[i] || !cell[i] || cell[i] == sol[i]) continue;
        r = i >> 3; c = i & 7;
        if (r >= N || c >= N) continue;
        hint.kind = H_MISTAKE;
        hint.target = i;
        hint.value = 0;
        hl_clear();
        v = cell[i];
        // trio through this cell?
        for (o = 0; o < 2; o++) {
            L = o ? N + c : r;
            for (k = 0; k + 2 < N; k++) {
                if (LV(L, k) == v && LV(L, k + 1) == v && LV(L, k + 2) == v) {
                    uint8_t t0 = line_cell(L, k), t1 = line_cell(L, k + 1), t2 = line_cell(L, k + 2);
                    if (t0 == i || t1 == i || t2 == i) {
                        hl[t0] = hl[t1] = hl[t2] = 1;
                        hint.aux = M_TRIO; hint.line = L;
                        return 1;
                    }
                }
            }
        }
        // too many of this symbol?
        for (o = 0; o < 2; o++) {
            L = o ? N + c : r;
            cnt = 0;
            for (k = 0; k < N; k++) if (LV(L, k) == v) cnt++;
            if (cnt > (N >> 1)) {
                hl_line(L);
                hint.aux = M_COUNT; hint.aux2 = v; hint.line = L;
                return 1;
            }
        }
        // duplicate line?
        calc_complete();
        for (o = 0; o < 2; o++) {
            L = o ? N + c : r;
            line_bits(L, &kn, &va);
            if (kn == full) {
                k = excluded_by(L, va);
                if (k) {
                    hl_line(L);
                    hl_line(k - 1);
                    hint.aux = M_DUP; hint.line = L; hint.aux2 = k - 1;
                    return 1;
                }
            }
        }
        hl[i] = 1;
        hint.aux = M_WRONG;
        hint.line = r;
        return 1;
    }
    return 0;
}

uint8_t board_full_correct(void) {
    uint8_t r, c;
    for (r = 0; r < N; r++)
        for (c = 0; c < N; c++)
            if (cell[(r << 3) + c] != sol[(r << 3) + c]) return 0;
    return 1;
}

uint8_t hint_find(void) {
    uint8_t L, k, a, t, f1, f0, cnt0, cnt1, v;
    hint.kind = H_NONE;
    hint.aux2 = 0;
    if (find_mistake()) return hint.kind;
    if (board_full_correct()) return H_NONE;

    hl_clear();
    // 1. pairs
    for (L = 0; L < 2 * N; L++) {
        for (k = 0; k + 1 < N; k++) {
            a = LV(L, k);
            if (!a || a != LV(L, k + 1)) continue;
            t = 0xFF;
            if (k > 0 && !LV(L, k - 1)) t = k - 1;
            else if (k + 2 < N && !LV(L, k + 2)) t = k + 2;
            if (t == 0xFF) continue;
            hl[line_cell(L, k)] = hl[line_cell(L, k + 1)] = 1;
            hint.target = line_cell(L, t);
            hl[hint.target] = 1;
            hint.value = 3 - a;
            hint.kind = H_PAIR;
            hint.line = L;
            hint.aux = a;
            return H_PAIR;
        }
    }
    // 2. gaps (avoid trios)
    for (L = 0; L < 2 * N; L++) {
        for (k = 0; k + 2 < N; k++) {
            a = LV(L, k);
            if (!a || a != LV(L, k + 2) || LV(L, k + 1)) continue;
            hl[line_cell(L, k)] = hl[line_cell(L, k + 1)] = hl[line_cell(L, k + 2)] = 1;
            hint.target = line_cell(L, k + 1);
            hint.value = 3 - a;
            hint.kind = H_TRIO;
            hint.line = L;
            hint.aux = a;
            return H_TRIO;
        }
    }
    // 3. counting
    for (L = 0; L < 2 * N; L++) {
        cnt0 = cnt1 = 0; t = 0xFF;
        for (k = 0; k < N; k++) {
            v = LV(L, k);
            if (v == V_ZERO) cnt0++;
            else if (v == V_ONE) cnt1++;
            else if (t == 0xFF) t = k;
        }
        if (t == 0xFF) continue;
        if (cnt0 == (N >> 1) || cnt1 == (N >> 1)) {
            hl_line(L);
            hint.target = line_cell(L, t);
            hint.value = cnt0 == (N >> 1) ? V_ONE : V_ZERO;
            hint.aux = 3 - hint.value;
            hint.kind = H_COUNT;
            hint.line = L;
            return H_COUNT;
        }
    }
    // 4. impossible combinations (line enumeration)
    calc_complete();
    for (L = 0; L < 2 * N; L++) {
        if (!enum_line(L, 0, &f1, &f0)) continue;
        if (f1 | f0) {
            hl_line(L);
            if (f1) { k = first_bit(f1); hint.value = V_ONE; }
            else { k = first_bit(f0); hint.value = V_ZERO; }
            hint.target = line_cell(L, k);
            hint.aux = 3 - hint.value;
            hint.kind = H_COMBO;
            hint.line = L;
            return H_COMBO;
        }
    }
    // 5. unique rows / columns
    for (L = 0; L < 2 * N; L++) {
        if (!enum_line(L, 1, &f1, &f0)) continue;
        if (f1 | f0) {
            uint8_t kn, va, i, p, L0 = L < N ? 0 : N;
            hl_line(L);
            if (f1) { k = first_bit(f1); hint.value = V_ONE; }
            else { k = first_bit(f0); hint.value = V_ZERO; }
            hint.target = line_cell(L, k);
            hint.kind = H_UNIQUE;
            hint.line = L;
            hint.aux = 3 - hint.value;
            // find the complete line that rules out the other symbol
            line_bits(L, &kn, &va);
            hint.aux2 = L0 == 0 ? (L == 0 ? 1 : 0) : (L == N ? N + 1 : N);
            for (i = L0; i < L0 + N; i++) {
                if (i == L || !comp_ok[i]) continue;
                p = comp_pat[i];
                if ((p ^ va) & kn) continue;
                if (((p >> k) & 1) != (hint.value == V_ONE)) { hint.aux2 = i; break; }
            }
            hl_line(hint.aux2);
            return H_UNIQUE;
        }
    }
    // 6. fallback: reveal a cell
    for (k = 0; k < 64; k++) {
        if ((k >> 3) < N && (k & 7) < N && !cell[k]) {
            hl[k] = 1;
            hint.target = k;
            hint.value = sol[k];
            hint.kind = H_GUESS;
            hint.line = k >> 3;
            return H_GUESS;
        }
    }
    return H_NONE;
}
