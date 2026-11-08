/**
 * shim_internal.h : Shared between the shim's own .c files. Not for game code.
 */

#ifndef SHIM_INTERNAL_H
#define SHIM_INTERNAL_H

#include <stdint.h>

#include "raylib.h"
#include "platform.h"
#include "shim_types.h"

// ---- Defaults for anything a board_config.h leaves out ----------------------

#ifndef SHIM_IMAGE_SCALE
#define SHIM_IMAGE_SCALE 1.0f // Run-time images (the background) at full framebuffer size
#endif

// ---- Where the game's world lands on the screen (rl_core.c) ----------------
// InitWindow(width, height) sets the world's size. It's scaled to fit the
// screen, keeping its shape, and centred:  screen = offset + world x scale.
// A game written at the screen's own size gets scale 1: pixel for pixel.

extern float shimScale;
extern int shimOffsetX, shimOffsetY;

// ---- The framebuffer (rl_draw.c) -------------------------------------------

extern uint8_t shimFramebuffer[SHIM_FB_WIDTH * SHIM_FB_HEIGHT];

// RGB332 (rrrgggbb), one byte per pixel
static inline uint8_t ShimPack(Color c)
{
	return (uint8_t)((c.r & 0xE0) | ((c.g & 0xE0) >> 3) | (c.b >> 6));
}

// Back to 8 bits per channel
Color ShimUnpack(uint8_t p);

// ---- Assets (rl_core.c) ------------------------------------------------------

const void *ShimFindAsset(const char *fileName, ShimAssetType type);

#endif // SHIM_INTERNAL_H
