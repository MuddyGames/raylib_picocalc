/**
 * platform.h : What each board has to provide for the raylib shim.
 *
 * The shim (shim/) does all the drawing into a small framebuffer, keeps
 * track of keys and time, and finds the built-in sprite sheets and sounds.
 * A board only has to do the jobs below, in its own platform_<board>.c:
 *
 *     show a frame        PlatformPresent()
 *     read the controls   PlatformReadKeys()
 *     make a noise        PlatformAudioInit(), PlatformPlaySound()
 *     tell the time       PlatformMicros(), PlatformSleepMicros()
 *     print a log line    PlatformLog()
 *
 * Porting to a new board means writing these ten functions and a
 * board_config.h, and nothing else.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "shim_types.h"

// Set up the screen, the controls and anything else the board needs
// (called once, from InitWindow())
void PlatformInit(void);

// Send a finished frame to the screen.
// fb is SHIM_FB_WIDTH x SHIM_FB_HEIGHT pixels, one byte each, in RGB332
// (rrrgggbb). The board converts it to whatever its screen wants.
void PlatformPresent(const uint8_t *fb);

// Which keys are down right now? Set down[key] for each raylib KEY_ value
// that's held (KEY_UP, KEY_SPACE, ...). The array is cleared before each call.
void PlatformReadKeys(bool down[SHIM_KEY_COUNT]);

// Time since the board started, in microseconds
uint64_t PlatformMicros(void);

// Wait (used to hold the frame rate down to SetTargetFPS)
void PlatformSleepMicros(uint32_t micros);

// Print one line of log (no newline at the end: the board adds it)
void PlatformLog(const char *line);

// A seed for GetRandomValue(), different every time if the board can manage it
uint32_t PlatformRandomSeed(void);

// Sound. PlatformAudioInit() returns false if the board has no sound at all.
bool PlatformAudioInit(void);
void PlatformAudioClose(void);
void PlatformPlaySound(const ShimSound *sound);

#endif // PLATFORM_H
