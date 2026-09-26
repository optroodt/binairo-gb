# Binairo for Game Boy - build with GBDK-2020
# Usage: make GBDK=/path/to/gbdk
GBDK ?= gbdk
LCC  = $(GBDK)/bin/lcc
# MBC1 + RAM + battery, 64 KB ROM, 8 KB SRAM, CGB compatible
LCCFLAGS = -Wl-yt0x03 -Wl-yo4 -Wl-ya1 -Wm-yc -Wm-yn"BINAIRO" -Wl-m -Wl-j

SRC = src/gfx.c src/main.c src/font.c src/sound.c src/save.c src/hint.c src/game.c src/menu.c src/assets.c src/puzzles.c
ROM = binairo.gb

all: $(ROM)

$(ROM): $(SRC) src/*.h
	$(LCC) $(LCCFLAGS) -o $@ $(SRC)

assets:
	python3 tools/gen_assets.py
	python3 tools/gen_puzzles.py 120 src/puzzles.c

clean:
	rm -f $(ROM) *.o *.map *.sym *.noi *.ihx src/*.o src/*.asm src/*.lst src/*.sym

.PHONY: all assets clean
