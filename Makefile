CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
INC = -Isrc/core -Isrc/util -Isrc/ui -Isrc/player -Isrc/battle -Isrc/gacha -Isrc/inventory -Isrc/shop -Isrc/storage -Isrc/dungeon -Isrc/platform
CFLAGS += $(INC)
MODUL = main.c \
        src/util/util.c src/ui/ui.c src/ui/term.c src/ui/caps.c src/ui/style.c \
        src/ui/render.c src/ui/widgets.c src/player/player.c src/battle/battle.c \
        src/gacha/gacha.c src/inventory/inventory.c src/shop/shop.c \
        src/storage/storage.c src/dungeon/dungeon.c \
        src/platform/rng.c src/platform/atomicfile.c src/platform/lock.c \
        src/platform/sig.c src/platform/crash.c src/platform/autosave.c \
        src/platform/platform.c
BIN = bin
UIPREVIEW_SRC = tools/ui_preview.c \
        src/ui/ui.c src/ui/term.c src/ui/caps.c src/ui/style.c \
        src/ui/render.c src/ui/widgets.c src/util/util.c \
        src/platform/rng.c src/platform/atomicfile.c src/platform/lock.c \
        src/platform/sig.c src/platform/crash.c src/platform/autosave.c \
        src/platform/platform.c

ifeq ($(OS),Windows_NT)
LDLIBS =
else
LDLIBS = -lpthread
endif

ifeq ($(OS),Windows_NT)
CC_SAN = C:/msys64/clang64/bin/clang.exe
else
CC_SAN = $(CC)
endif

.PHONY: all clean test debug preview
.DEFAULT_GOAL := all

preview: $(BIN)/ui_preview
	$(BIN)/ui_preview

all: $(BIN)/game

$(BIN)/game: $(MODUL)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -o $(BIN)/game $(MODUL) $(LDLIBS)

$(BIN)/ui_preview: $(UIPREVIEW_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -o $(BIN)/ui_preview $(UIPREVIEW_SRC)

debug:
	@mkdir -p $(BIN)
	$(CC_SAN) $(CFLAGS) -fsanitize=address,undefined -g -o $(BIN)/game $(MODUL) $(LDLIBS)
	CC_BIN="$(CC_SAN)" sh tests/run_tests.sh "-fsanitize=address,undefined -g"

test: all
	sh tests/run_tests.sh

clean:
	rm -rf $(BIN)
