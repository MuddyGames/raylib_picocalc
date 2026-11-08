/**
 * platform_sim.c : The PicoCalc on your desktop, for trying a game without the
 * hardware. Its window, keys and sound come from the real raylib, the same
 * one the desktop build uses, so there's nothing extra to install.
 *
 * It shows exactly what the PicoCalc's screen would: the same 312 x 234
 * framebuffer, the same colours (one byte a pixel), the same scaling. Sounds
 * play as the PicoCalc plays them: 8-bit samples at 11 kHz.
 *
 *     make -C sim run
 *     make -C sim GAME_DIR=otherfolder run
 *
 * Two raylibs in one program? This file is the only one built against the
 * real raylib.h. The game and the shim are built with SHIM_RENAME, which turns
 * their InitWindow into ShimInitWindow and so on (see shim_rename.h), so the
 * two never meet. The game draws into the shim's framebuffer; this file hands
 * that framebuffer to the real raylib as a texture.
 *
 * ESC or the window's close button quits. For automatic testing, SIM_SCRIPT
 * runs a list of "frame action" pairs:
 *     SIM_SCRIPT="60 +D 120 -D 200 SHOT out.ppm 210 QUIT"
 * (+KEY / -KEY press and release, SHOT saves the screen, QUIT stops)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "raylib.h"	  // The real one: the Makefile puts raylib's src/ first
#include "platform.h" // The board's side of the shim

// The PicoCalc's whole 320 x 320 screen, and where the game's picture sits on it
#define SCREEN_W 320
#define SCREEN_H 320
#define VIEW_X PICOCALC_VIEW_X
#define VIEW_Y PICOCALC_VIEW_Y
#define ZOOM 2

static Color pixels[SCREEN_W * SCREEN_H];
static Texture2D screen;
static unsigned long frame = 0;

static void Quit(void)
{
	PlatformAudioClose();
	if (IsWindowReady())
		CloseWindow();
	exit(0);
}

// ---------------------------------------------------------------------------
// SIM_SCRIPT: scripted key presses and screenshots, for testing
// ---------------------------------------------------------------------------

static const char *script;
static bool scriptKeys[SHIM_KEY_COUNT];

static int KeyByName(const char *name)
{
	if (strcmp(name, "UP") == 0) return KEY_UP;
	if (strcmp(name, "DOWN") == 0) return KEY_DOWN;
	if (strcmp(name, "LEFT") == 0) return KEY_LEFT;
	if (strcmp(name, "RIGHT") == 0) return KEY_RIGHT;
	if (strcmp(name, "SPACE") == 0) return KEY_SPACE;
	if (strlen(name) == 1 && name[0] >= 'A' && name[0] <= 'Z') return name[0];
	return -1;
}

// A plain PPM: no libraries needed, and most image viewers open it
static void SaveScreenshot(const char *path)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
		fwrite(&pixels[i], 1, 3, f); // r, g, b (and leave out a)
	fclose(f);
}

// Run the script's actions for this frame
static void RunScript(void)
{
	const char *p = script;
	char action[64];
	unsigned long when;
	int used;

	while (p && sscanf(p, "%lu %63s%n", &when, action, &used) == 2)
	{
		p += used;
		if (strcmp(action, "SHOT") == 0)
		{
			char file[256];
			if (sscanf(p, "%255s%n", file, &used) != 1)
				break;
			p += used;
			if (when == frame)
				SaveScreenshot(file);
			continue;
		}
		if (when != frame)
			continue;
		if (strcmp(action, "QUIT") == 0)
			Quit();
		int key = KeyByName(action + 1);
		if (key >= 0)
			scriptKeys[key] = (action[0] == '+');
	}
}

// ---------------------------------------------------------------------------
// The board's jobs
// ---------------------------------------------------------------------------

void PlatformInit(void)
{
	SetTraceLogLevel(LOG_WARNING); // The game's own log is the one worth reading
	InitWindow(SCREEN_W * ZOOM, SCREEN_H * ZOOM, "PicoCalc (simulated)");
	if (!IsWindowReady())
	{
		// No window means no OpenGL: the desktop game wouldn't start either
		fprintf(stderr, "SIM: Couldn't open a window (raylib's warnings above say why).\n"
						"     Does the desktop game run (make run)? If not, it's the graphics driver.\n"
						"     To try without the GPU: LIBGL_ALWAYS_SOFTWARE=1 make -C sim run\n");
		exit(1);
	}

	// The board's screen, as a texture: filled in every frame by PlatformPresent()
	Image blank = GenImageColor(SCREEN_W, SCREEN_H, BLACK);
	screen = LoadTextureFromImage(blank);
	UnloadImage(blank);
	SetTextureFilter(screen, TEXTURE_FILTER_POINT); // Keep the pixels chunky

	script = getenv("SIM_SCRIPT");
}

void PlatformPresent(const uint8_t *fb)
{
	for (int y = 0; y < SHIM_FB_HEIGHT; y++)
		for (int x = 0; x < SHIM_FB_WIDTH; x++)
		{
			// RGB332: rrrgggbb
			uint8_t p = fb[y * SHIM_FB_WIDTH + x];
			int r = ((p >> 5) & 7) * 255 / 7, g = ((p >> 2) & 7) * 255 / 7, b = (p & 3) * 255 / 3;

			pixels[(y + VIEW_Y) * SCREEN_W + x + VIEW_X] = (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
		}

	UpdateTexture(screen, pixels);

	BeginDrawing();
	ClearBackground(BLACK);
	DrawTextureEx(screen, (Vector2){0, 0}, 0.0f, (float)ZOOM, WHITE);
	EndDrawing(); // Also picks up the keys for PlatformReadKeys()

	if (script)
		RunScript();
	frame++;
}

void PlatformReadKeys(bool down[SHIM_KEY_COUNT])
{
	if (WindowShouldClose()) // The window's close button, or ESC
		Quit();

	down[KEY_UP] = IsKeyDown(KEY_UP);
	down[KEY_DOWN] = IsKeyDown(KEY_DOWN);
	down[KEY_LEFT] = IsKeyDown(KEY_LEFT);
	down[KEY_RIGHT] = IsKeyDown(KEY_RIGHT);
	down[KEY_SPACE] = IsKeyDown(KEY_SPACE);
	for (int key = KEY_A; key <= KEY_Z; key++)
		down[key] = IsKeyDown(key);

	for (int i = 0; i < SHIM_KEY_COUNT; i++)
		down[i] = down[i] || scriptKeys[i];
}

uint64_t PlatformMicros(void)
{
	return IsWindowReady() ? (uint64_t)(GetTime() * 1000000.0) : 0;
}

void PlatformSleepMicros(uint32_t micros)
{
	WaitTime(micros / 1000000.0);
}

void PlatformLog(const char *line)
{
	printf("%s\n", line);
	fflush(stdout);
}

uint32_t PlatformRandomSeed(void)
{
	return (uint32_t)time(NULL);
}

// ---------------------------------------------------------------------------
// Sound: the board's own sounds (samples, or the buzzer's beeps), made into
// raylib Sounds the first time each one plays
// ---------------------------------------------------------------------------

#define SIM_RATE 22050
#define SIM_MAX_SOUNDS 16

static struct
{
	const ShimSound *from;
	Sound sound;
} cache[SIM_MAX_SOUNDS];
static int cached = 0;
static Sound *playing = NULL;

// The board's sound as 8-bit samples (128 = silence), loaded into raylib
static Sound MakeSound(const ShimSound *sound)
{
	Wave wave = {0};
	wave.sampleRate = SIM_RATE;
	wave.sampleSize = 8;
	wave.channels = 1;

	if (sound->pcm)
	{
		// Samples: play them at the rate they were made for
		wave.sampleRate = sound->pcmRate;
		wave.frameCount = sound->pcmLength;
		wave.data = (void *)sound->pcm; // LoadSoundFromWave() takes a copy
		return LoadSoundFromWave(wave);
	}

	// Tones: square waves, one after the other, like the buzzer
	for (int t = 0; t < sound->toneCount; t++)
		wave.frameCount += (unsigned int)SIM_RATE * sound->tones[t * 2 + 1] / 1000;

	uint8_t *buffer = malloc(wave.frameCount ? wave.frameCount : 1);
	if (!buffer)
		return (Sound){0};

	uint32_t at = 0;
	for (int t = 0; t < sound->toneCount; t++)
	{
		uint32_t frequency = sound->tones[t * 2], count = (uint32_t)SIM_RATE * sound->tones[t * 2 + 1] / 1000;
		for (uint32_t i = 0; i < count; i++)
			buffer[at++] = (frequency == 0) ? 128 : (((i * frequency * 2 / SIM_RATE) & 1) ? 160 : 96);
	}

	wave.data = buffer;
	Sound made = LoadSoundFromWave(wave);
	free(buffer);
	return made;
}

bool PlatformAudioInit(void)
{
	InitAudioDevice();
	return IsAudioDeviceReady();
}

void PlatformAudioClose(void)
{
	if (!IsAudioDeviceReady())
		return;
	for (int i = 0; i < cached; i++)
		UnloadSound(cache[i].sound);
	cached = 0;
	playing = NULL;
	CloseAudioDevice();
}

void PlatformPlaySound(const ShimSound *sound)
{
	// Find it, or make it
	Sound *found = NULL;
	for (int i = 0; i < cached && !found; i++)
		if (cache[i].from == sound)
			found = &cache[i].sound;
	if (!found)
	{
		if (cached == SIM_MAX_SOUNDS)
			return;
		cache[cached].from = sound;
		cache[cached].sound = MakeSound(sound);
		found = &cache[cached++].sound;
	}

	// One sound at a time, like the board
	if (playing)
		StopSound(*playing);
	PlaySound(*found);
	playing = found;
}
