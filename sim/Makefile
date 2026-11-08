# The PicoCalc on your desktop: a game built with the shim, exactly as for the
# PicoCalc (same 312 x 234 picture, same sounds), shown in a raylib window.
# Uses the same raylib as the desktop build, so nothing else to install.
#
#     make -C sim run                     (the game in game/)
#     make -C sim GAME_DIR=otherfolder run   (a game somewhere else)
#
# raylib somewhere else?  make -C sim RAYLIB_SRC_PATH=/path/to/raylib/src run

GAME_DIR ?= game
ROOT     := ..

ifeq ($(wildcard $(ROOT)/$(GAME_DIR)/*.c),)
$(error No .c files in $(GAME_DIR))
endif
ifeq ($(wildcard $(ROOT)/$(GAME_DIR)/shim_assets.c),)
$(error No shim_assets.c in $(GAME_DIR): run  python3 tools/convert_assets.py $(GAME_DIR))
endif

# Where raylib lives
ifeq ($(OS),Windows_NT)
    RAYLIB_SRC_PATH ?= C:/raylib/raylib/src
    LDLIBS += -lraylib -lopengl32 -lgdi32 -lwinmm
    EXE := .exe
else ifeq ($(shell uname),Darwin)
    RAYLIB_SRC_PATH ?= $(HOME)/raylib/src
    LDLIBS += -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio
else
    RAYLIB_SRC_PATH ?= $(HOME)/raylib/src
    LDLIBS += -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif
LDFLAGS += -L$(RAYLIB_SRC_PATH)

ifeq ($(wildcard $(RAYLIB_SRC_PATH)/raylib.h),)
$(error No raylib.h in $(RAYLIB_SRC_PATH): build raylib first, or set RAYLIB_SRC_PATH)
endif

WARNINGS := -std=c99 -O2 -g -Wall -Wextra -Wno-unused-parameter

# The game and the shim: the shim's raylib.h comes first, and SHIM_RENAME keeps
# their names (ShimInitWindow ...) apart from the real raylib's. raymath goes
# inline here (the real raylib has its own Vector2Add and friends), so the
# shim's rl_math.c is left out.
GAME_SRC := $(wildcard $(ROOT)/$(GAME_DIR)/*.c) \
            $(filter-out %/rl_math.c,$(wildcard $(ROOT)/shim/src/*.c))
GAME_CFLAGS := $(WARNINGS) -DSHIM_RENAME -DRAYMATH_STATIC_INLINE \
               -I$(ROOT)/shim/include -I$(ROOT)/shim/src -I$(ROOT)/picocalc -I$(ROOT)/$(GAME_DIR)

# The simulated PicoCalc: the real raylib.h comes first, then the shim's platform.h
SIM_CFLAGS := $(WARNINGS) -I$(RAYLIB_SRC_PATH) -I$(ROOT)/shim/include -I$(ROOT)/picocalc

NAME  := $(notdir $(GAME_DIR))
BUILD := build/$(NAME)
OUT   := $(BUILD)/$(NAME)$(EXE)
OBJ   := $(patsubst $(ROOT)/%.c,$(BUILD)/obj/%.o,$(GAME_SRC)) $(BUILD)/obj/platform_sim.o
HDR   := $(wildcard $(ROOT)/$(GAME_DIR)/*.h $(ROOT)/shim/include/*.h $(ROOT)/shim/src/*.h $(ROOT)/picocalc/*.h)

.PHONY: all run clean
all: $(OUT)

$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/obj/%.o: $(ROOT)/%.c $(HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GAME_CFLAGS) -c $< -o $@

$(BUILD)/obj/platform_sim.o: platform_sim.c $(HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SIM_CFLAGS) -c $< -o $@

# Runs from the game's folder, like the desktop build (so resources/ is found)
run: $(OUT)
	cd $(ROOT)/$(GAME_DIR) && $(CURDIR)/$(OUT)

clean:
	rm -rf build
