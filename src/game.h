#ifndef GAME_H
#define GAME_H
#include "common.h"

// cell values
#define V_EMPTY 0
#define V_ZERO 1
#define V_ONE 2

extern uint8_t N;          // 6 or 8
extern uint8_t cell[64];   // index r*8+c
extern uint8_t giv[64];
extern uint8_t sol[64];
extern uint8_t hl[64];     // highlight flags for hint display

enum {
    H_NONE,
    H_PAIR,
    H_TRIO,
    H_COUNT,
    H_COMBO,
    H_UNIQUE,
    H_MISTAKE,
    H_GUESS
};

// mistake subtypes
enum { M_TRIO, M_COUNT, M_DUP, M_WRONG };

typedef struct {
    uint8_t kind;
    uint8_t target;  // cell index
    uint8_t value;   // value to place (0 = remove)
    uint8_t line;    // line id (0..N-1 rows, N..2N-1 cols)
    uint8_t aux;     // symbol / other line / mistake subtype
    uint8_t aux2;
} hint_t;

extern hint_t hint;
void hint_init(void);
uint8_t hint_find(void);
uint8_t line_cell(uint8_t line, uint8_t k);
uint8_t board_full_correct(void);

#endif
