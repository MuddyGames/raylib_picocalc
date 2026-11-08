# raylib_picocalc

**Your raylib game, on a calculator.**

The [ClockworkPi PicoCalc](https://www.clockworkpi.com/product-page/picocalc) is a little handheld with a keyboard, a 320 x 320 screen, two speakers and a Raspberry Pi Pico for a brain: a 133 MHz chip with 264 KB of RAM. raylib needs OpenGL and a few hundred megabytes. They shouldn't get on.

This project gets them talking. Write an ordinary raylib game on your laptop, try it in a simulator, then flash it to the PicoCalc, **without changing a line of the game**.

> A stretch project, not a module requirement. If you've written a raylib game and fancy seeing how the trick works underneath, this is for you.

---

## How it works, in one picture

```
your game (main.c)           #include <raylib.h>   InitWindow, IsKeyDown, DrawRectangle ...
        │
        ├── on the desktop:   the real raylib       OpenGL, a window, your keyboard
        │
        └── on the PicoCalc:  shim/ (a pretend raylib.h)
                                  │  draws into a 312 x 234 framebuffer, one byte a pixel
                                  │  scales your 800 x 600 world by 25/64 as it goes
                                  ▼
                              picocalc/platform_picocalc.c
                                  the ten jobs only a real board can do:
                                  send pixels to the LCD, read the keyboard,
                                  play a sound, tell the time, print a log line
```

The **shim** is the clever bit: a cut-down `raylib.h` with the same names as the real one, so your game can't tell the difference. It only has what a small game needs (the list is `shim/include/raylib.h`); use anything else and the compiler says *implicit declaration of function*.

---

## What's here

```
raylib_picocalc/
├── game/               the game: yours to change (it starts as a square that chases coins)
├── shim/               the pretend raylib (include/raylib.h is the list of what works)
├── picocalc/           the PicoCalc: board_config.h (pins, scale), platform_picocalc.c, CMakeLists.txt
├── sim/                the PicoCalc in a desktop window, for trying things without the hardware
└── tools/              convert_assets.py: a game's PNGs and WAVs into C, for the Pico's flash
                        picocalc_log.sh: the game's TraceLog lines, live
```

---

## You'll need

Commands for both families of Linux: **Debian-based** (Debian, Ubuntu, Pop!_OS, Mint) and **Arch-based** (Arch, Manjaro, EndeavourOS). Pick your line.

**1. The tools.**

```bash
# Debian, Ubuntu, Pop!_OS, Mint
sudo apt install build-essential git cmake pkg-config gcc-arm-none-eabi libnewlib-arm-none-eabi \
                 libusb-1.0-0-dev python3-pil screen

# Arch, Manjaro, EndeavourOS
sudo pacman -S --needed base-devel git cmake pkgconf arm-none-eabi-gcc arm-none-eabi-newlib \
                        libusb python-pillow screen
```

| Package | For |
|---|---|
| `gcc-arm-none-eabi` + newlib | The compiler for the Pico's ARM chip, and its C library |
| `cmake` | The Pico SDK's build system |
| `libusb` | picotool talks to the Pico over USB |
| Pillow (`python3-pil` / `python-pillow`) | `convert_assets.py`, for pictures and sounds. (Not `pip install`: newer distros block that) |
| `screen` | One way to read the log (see below) |

**2. raylib**, built in `~/raylib/src`, as for the module. Already have it? Skip this.

```bash
# Debian-based: what raylib needs to build
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev \
                 libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev
# Arch-based
sudo pacman -S --needed alsa-lib mesa libx11 libxrandr libxi libxcursor libxinerama wayland libxkbcommon

# Both
git clone --depth 1 https://github.com/raysan5/raylib ~/raylib
make -C ~/raylib/src PLATFORM=PLATFORM_DESKTOP -j
```

(Arch also has a `raylib` package, but it installs somewhere else: the Makefiles here look in `~/raylib/src`, or wherever `RAYLIB_SRC_PATH=` says.)

**3. The Pico SDK and picotool** (the same on both):

```bash
git clone --depth 1 https://github.com/raspberrypi/pico-sdk ~/pico-sdk
git -C ~/pico-sdk submodule update --init --depth 1 lib/tinyusb
echo 'export PICO_SDK_PATH=~/pico-sdk' >> ~/.bashrc && source ~/.bashrc

git clone --depth 1 https://github.com/raspberrypi/picotool ~/picotool
cmake -S ~/picotool -B ~/picotool/build -DPICO_SDK_PATH=~/pico-sdk
cmake --build ~/picotool/build -j && sudo cp ~/picotool/build/picotool /usr/local/bin/
```

(Shortcuts if you'd rather: Debian 13 has `sudo apt install picotool`, and Arch has `picotool` in the AUR. Ubuntu 22.04 and Pop!_OS 22.04 have neither, hence building it.) Zsh or fish rather than bash? Put the `export` in your own shell's startup file.

**4. Permission to read the PicoCalc's log** without `sudo`, once, then log out and back in:

```bash
sudo usermod -aG dialout $USER     # Debian-based
sudo usermod -aG uucp $USER        # Arch-based
```

---

## The route: desktop → simulator → PicoCalc

### 1. On the desktop

```bash
make -C game run
```

Plain raylib. WASD walks, touch the gold square, score goes up.

### 2. In the simulator

```bash
make -C sim run
```

The same game, built with the shim, in a window the size of a (doubled) PicoCalc screen. What you see here is what the PicoCalc shows, down to the last chunky pixel.

### 3. On the PicoCalc

**Build:**

```bash
cmake -S picocalc -B build
cmake --build build -j
```

That makes `build/game.uf2`. (Pico 2 in yours? Add `-DPICO_BOARD=pico2` to the first line.)

**Flash.** The PicoCalc has two USB sockets that do different jobs:

| Socket | For |
|---|---|
| The **Pico's micro-USB** (through a gap in the case) | Flashing, and the game's log |
| The **PicoCalc's USB-C** (on the side) | Power: the screen and keyboard need it |

1. PicoCalc **off**, nothing plugged in.
2. Look through the grille on the back for the tiny **"reset"** label. The button under it is the Pico's **BOOTSEL** button. Hold it down with a toothpick.
3. Still holding it, plug the **micro-USB** into your computer. Let go.
4. Flash it:
   ```bash
   sudo picotool load -x build/game.uf2
   ```
   (or drag `game.uf2` onto the **RPI-RP2** drive that appeared)
5. Plug in the **USB-C** and press **power** on the top. Leave the micro-USB in for the log (below).

Nervous? Don't be: BOOTSEL mode lives in the chip's ROM, which nothing can overwrite. Whatever you flash, the toothpick trick gets you back. To keep what was on it first: `sudo picotool save -a picocalc_original.uf2` while it's in BOOTSEL mode.

---

## Seeing what the game is doing: the log

Your best friend on the PicoCalc. There's no debugger and no console window, but every `TraceLog()` your game makes comes out of the Pico's **micro-USB**:

```c
TraceLog(LOG_INFO, "[Player] x %.1f, y %.1f", player.x, player.y);
```

Plug the micro-USB into the computer (keep the USB-C in for power), then:

```bash
tools/picocalc_log.sh
```

It finds the Pico, prints everything it says, and when the Pico disappears (you flash it again, or unplug it) it waits for it to come back, so you can leave it running in a terminal while you work. Ctrl+C to stop.

The shim logs too: the game starting, pictures and sounds it can't find, and anything you've asked for that it can't do.

**By hand,** if you'd rather. The Pico shows up as `/dev/ttyACM0` (check with `ls /dev/ttyACM*`):

```bash
screen /dev/ttyACM0 115200      # Ctrl+A, then K, then Y to quit
```

Nothing there? See the table below: it's almost always the cable in the wrong socket.

The game waits a second at start-up for the log to connect, so if `tools/picocalc_log.sh` is already running, you see everything from the first line. (Anything printed while nothing's listening is gone.)

---

## Your own game

The game lives in `game/`, and it's yours: change `main.c`, add more `.c` and `.h` files beside it, and every build picks them up. Keep to:

* the functions in `shim/include/raylib.h`
* an **800 x 600** world (the shim scales it to the screen)
* movement by `GetFrameTime()`, so it plays at the same speed anywhere

**Pictures and sounds** go in `game/resources/`, loaded as `LoadTexture("resources/hero.png")` (the desktop build runs from `game/`, so the paths work there too). Build them in for the PicoCalc whenever they change:

```bash
python3 tools/convert_assets.py game
```

That rewrites `game/shim_assets.c`, which the PicoCalc build needs even when there's nothing in it. Then the same three steps as before.

**How big can it be?** Pictures are shrunk by 25/64 and stored with 16 colours (one of them see-through), so a 1152 x 3712 sprite sheet takes about 320 KB of the Pico's 2 MB of flash. Sounds are 8-bit at 11 kHz: about 11 KB a second. `convert_assets.py` tells you the total.

**Keeping more than one game?** Put each in its own folder (with a copy of `game/Makefile`) and point the builds at it with `GAME_DIR`: `make -C sim GAME_DIR=pong run`, `cmake -S picocalc -B build/pong -DGAME_DIR=pong`.

---

## Why does it look like that?

**Chunky pixels.** 800 x 600 squeezed into 312 x 234. A 40-unit square is 15 pixels on the PicoCalc. Design big.

**Fewer colours.** One byte a pixel (RGB332: 3 bits red, 3 green, 2 blue), so 256 colours. Gentle gradients come out in steps.

**The odd tear.** Watch a fast-moving sprite closely and now and then you'll see it split: the top half where it was, the bottom half where it is. The LCD redraws itself from its own memory about 60 times a second, and nothing tells the Pico where it's up to: ClockworkPi didn't wire up the LCD's *tearing effect* pin. Catch it mid-write and you see both. Every PicoCalc game has this. All we can do is make each write short, which is why only the rectangles that changed are sent, not the whole screen (see `PlatformPresent()` in `picocalc/platform_picocalc.c`).

**Frame rate.** The starter game should hold its 60 frames a second, since little changes each frame. A busier game (a full-screen background, sprites, a HUD) came in at about 20 to 27 in testing. The starter game shows its FPS in the corner.

---

## When things go wrong

| You see… | What's going on | Fix |
|---|---|---|
| `implicit declaration of function 'DrawCircle'` | Your game uses something the shim doesn't have | Draw it another way, or add it to the shim (see challenges) |
| `No shim_assets.c in game` | The build needs the asset list, even an empty one | `python3 tools/convert_assets.py game` |
| `SHIM: [resources/x.png] isn't built in` (in the log) | The picture's not in `shim_assets.c` | Run `convert_assets.py` again |
| `PICO_SDK_PATH` not set | CMake can't find the SDK | `export PICO_SDK_PATH=~/pico-sdk` (and in your `~/.bashrc`) |
| `arm-none-eabi-gcc` not found, or `nosys.specs` missing | The ARM compiler, or its C library, isn't installed | The tools line for your distro, under *You'll need* |
| No **RPI-RP2** drive | Not in BOOTSEL mode, or a charge-only cable | Toothpick on "reset" *while* plugging in; try another cable |
| Black screen | Running from the micro-USB alone | Plug in the USB-C and press power |
| No `/dev/ttyACM0` | The log's on the micro-USB, not the USB-C; or the game isn't running | Plug the micro-USB into the computer too, and switch the PicoCalc on |
| `Permission denied` on `/dev/ttyACM0` | Your user can't read serial ports yet | `sudo usermod -aG dialout $USER` (Debian-based) or `uucp` (Arch-based), then log out and in |
| Specks, or bits of text in the wrong place | The LCD missing data | Lower `PICOCALC_LCD_SPI_HZ` in `picocalc/board_config.h` |
| Simulator: `GLXBadFBConfig` | The graphics driver, not the simulator (often a driver update waiting for a restart) | Restart, or `LIBGL_ALWAYS_SOFTWARE=1 make -C sim run` |

---

## Challenges

Roughly in order of how much you'll learn about the inside of a computer:

1. **Make it yours.** Change the starter game into something else: Pong, Snake, a maze. Run it on the PicoCalc.
2. **Add a shape.** Add `DrawCircle()` to the shim (`shim/include/raylib.h`, `shim/src/rl_draw.c`, and a line in `shim/include/shim_rename.h` for the simulator). The Pico has no floating-point hardware and no divide instruction: see how `DrawRectangleRounded()` gets away with one square root per row.
3. **Count the cost.** `TraceLog` how long `EndDrawing()` takes (`GetTime()` before and after) and watch it in `tools/picocalc_log.sh`. Where does a frame's time go?
4. **Fewer tears.** `PlatformPresent()` sends one rectangle for each run of changed rows, wide enough for all of them. When the player and a coin share rows, that's one wide rectangle. Split it into two, and measure the difference.
5. **Two cores.** The Pico has two. One draws the next frame while the other sends this one to the LCD? You'll need a second framebuffer, and you'll need to think hard about who's allowed to touch what, when.

---

## Thanks and licences

* [raylib](https://www.raylib.com) by Ramon Santamaria: `shim/include/raymath.h` and the default font (`shim/src/rl_font_data.h`) are raylib's (zlib licence).
* `shim/src/stb_perlin.h` by Sean Barrett (public domain).
* The LCD start-up sequence, pins and keyboard protocol come from ClockworkPi's examples at [github.com/clockworkpi/PicoCalc](https://github.com/clockworkpi/PicoCalc).
* Everything else: see `LICENSE`.
