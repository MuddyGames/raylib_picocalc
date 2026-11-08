# The game on the desktop, with the real raylib:   make -C game run
# (raylib somewhere else?  make RAYLIB_SRC_PATH=/path/to/raylib/src run)

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

NAME := $(notdir $(CURDIR))
OUT  := build/$(NAME)$(EXE)
# Everything but shim_assets.c: that's only for the PicoCalc
SRC  := $(filter-out shim_assets.c,$(wildcard *.c))

.PHONY: all run clean
all: $(OUT)

$(OUT): $(SRC) $(wildcard *.h)
	@mkdir -p build
	$(CC) -std=c99 -O2 -g -Wall -Wextra $(SRC) -o $@ -I$(RAYLIB_SRC_PATH) -L$(RAYLIB_SRC_PATH) $(LDLIBS)

run: $(OUT)
	./$(OUT)

clean:
	rm -rf build
