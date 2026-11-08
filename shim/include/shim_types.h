/**
 * shim_types.h : The shim's own data, shared by the shim, the boards and the
 * assets that tools/convert_assets.py generates.
 *
 * Game code never needs this file: it only sees raylib.h.
 */

#ifndef SHIM_TYPES_H
#define SHIM_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"

// raylib's KEY_ values all fit below this (KEY_KB_MENU = 348 is the largest)
#define SHIM_KEY_COUNT 512

// How a texture's pixels are stored
typedef enum
{
	SHIM_TEX_PAL4, // 4 bits per pixel, 16-colour palette, index 0 transparent (sprite sheets, in flash)
	SHIM_TEX_RGBA  // 4 bytes per pixel (made at run time, e.g. the background)
} ShimTexFormat;

// A texture's pixels and how they map onto the game's world
typedef struct
{
	ShimTexFormat format;
	const uint8_t *pixels;	 // PAL4: two pixels per byte, left pixel in the high nibble
	const uint8_t *palette;	 // PAL4: 16 x RGBA
	int dataWidth;			 // Stored size, in pixels
	int dataHeight;
	int worldWidth;			 // Size in the game's world (the original image's size)
	int worldHeight;
	bool sprite;			 // true: draw at stored size, centred (keeps the art's shape on any screen)
							 // false: stretch to fill its world rectangle (the background)
} ShimTexture;

// A sound, ready for the board to play (8-bit samples; tones are for boards
// with only a buzzer)
typedef struct
{
	const uint8_t *pcm;		 // 8-bit unsigned, mono (128 = silence), or NULL
	uint32_t pcmLength;		 // Samples
	uint16_t pcmRate;		 // Samples per second
	const uint16_t *tones;	 // Pairs: frequency in Hz (0 = rest), length in ms; or NULL
	uint16_t toneCount;		 // Pairs
} ShimSound;

// One file built into the program, found by the name the game asks for
typedef enum
{
	SHIM_ASSET_TEXTURE,
	SHIM_ASSET_SOUND
} ShimAssetType;

typedef struct
{
	const char *fileName; // e.g. "resources/player_sprite_sheet.png"
	ShimAssetType type;
	const void *data;	  // ShimTexture or ShimSound
} ShimAsset;

// In the game's shim_assets.c (written by tools/convert_assets.py)
extern const ShimAsset shimAssets[];
extern const int shimAssetCount;

#endif // SHIM_TYPES_H
