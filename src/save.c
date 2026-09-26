// Battery-backed save (MBC1 + RAM + battery).
#include "common.h"

save_t sv;
static const uint8_t MAGIC[4] = {'B', 'N', 'R', '2'};

static uint8_t checksum(void) {
    uint8_t *p = (uint8_t *)&sv;
    uint16_t n = sizeof(save_t) - 1;
    uint8_t c = 0x5A;
    while (n--) c = (c << 1 | c >> 7) ^ *p++;
    return c;
}

void save_load(void) {
    ENABLE_RAM;
    SWITCH_RAM(0);
    memcpy(&sv, (uint8_t *)0xA000, sizeof(save_t));
    DISABLE_RAM;
    if (memcmp(sv.magic, MAGIC, 4) != 0 || sv.check != checksum()) {
        memset(&sv, 0, sizeof(save_t));
        memcpy(sv.magic, MAGIC, 4);
        sv.sfx = 1;
        save_write();
    }
    if (sv.theme >= 4) sv.theme = 0;
}

void save_write(void) {
    sv.check = checksum();
    ENABLE_RAM;
    SWITCH_RAM(0);
    memcpy((uint8_t *)0xA000, &sv, sizeof(save_t));
    DISABLE_RAM;
}
