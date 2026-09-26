// Tiny sound effects + jingle sequencer.
#include "common.h"

// note frequency table (GB register values), C4..B6
static const uint16_t notes[] = {
    1046, 1102, 1155, 1205, 1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517,  // C4..B4
    1546, 1575, 1602, 1627, 1650, 1673, 1694, 1714, 1732, 1750, 1767, 1783,  // C5..B5
    1798, 1812, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915,  // C6..B6
};

static const uint8_t *seq;
static uint8_t seq_t;

void snd_init(void) {
    NR52_REG = 0x80;
    NR50_REG = 0x77;
    NR51_REG = 0xFF;
}

static void ch1(uint8_t note, uint8_t env, uint8_t duty, uint8_t sweep) {
    uint16_t f;
    if (!sv.sfx) return;
    f = notes[note];
    NR10_REG = sweep;
    NR11_REG = duty;
    NR12_REG = env;
    NR13_REG = (uint8_t)f;
    NR14_REG = 0x80 | (f >> 8);
}

static void ch2(uint8_t note, uint8_t env, uint8_t duty) {
    uint16_t f;
    if (!sv.sfx) return;
    f = notes[note];
    NR21_REG = duty;
    NR22_REG = env;
    NR23_REG = (uint8_t)f;
    NR24_REG = 0x80 | (f >> 8);
}

static void noise(uint8_t poly, uint8_t env, uint8_t len) {
    if (!sv.sfx) return;
    NR41_REG = len;
    NR42_REG = env;
    NR43_REG = poly;
    NR44_REG = 0xC0;
}

void sfx_move(void) { ch2(24 + 7, 0x31, 0x40); }
void sfx_set(uint8_t v) {
    if (v == 0) ch1(12 + 7, 0x71, 0x80, 0x00);
    else if (v == 1) ch1(12 + 12, 0x71, 0x80, 0x00);
    else ch1(12 + 16, 0x71, 0x80, 0x00);
}
void sfx_hint(void) { ch1(12, 0x82, 0x80, 0x15); }
void sfx_page(void) { ch2(24 + 12, 0x52, 0x80); }
void sfx_back(void) { ch1(24, 0x62, 0x80, 0x1D); }
void sfx_error(void) { noise(0x55, 0x62, 0x30); }
void sfx_check(uint8_t n) { ch2(24 + ((n * 2) % 12), 0x62, 0x80); }

// jingle: pairs of (note, frames); 0xFF end
static const uint8_t win_seq[] = {
    12 + 0, 6, 12 + 4, 6, 12 + 7, 6, 24 + 0, 10, 12 + 7, 5, 24 + 0, 5, 24 + 4, 20, 0xFF
};

void sfx_win(void) {
    seq = win_seq;
    seq_t = 0;
}

void snd_update(void) {
    if (!seq) return;
    if (seq_t) { seq_t--; return; }
    if (*seq == 0xFF) { seq = 0; return; }
    ch1(seq[0], 0xA3, 0x80, 0x00);
    ch2(seq[0] + 12 > 35 ? seq[0] : seq[0] + 12, 0x53, 0x40);
    seq_t = seq[1];
    seq += 2;
}
