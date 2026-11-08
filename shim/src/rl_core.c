/**
 * rl_core.c : The raylib shim's window, timing, input, logging and audio.
 *
 * "Window" is a stretch on a microcontroller: InitWindow() starts the board's
 * screen, and EndDrawing() sends each frame to it. Everything is in the
 * game's world units (800 x 600), like the desktop; rl_draw.c does the scaling.
 */

#include <stdio.h>
#include <string.h>

#include "shim_internal.h"

// ---------------------------------------------------------------------------
// Window and timing
// ---------------------------------------------------------------------------

static int worldWidth = 800;
static int worldHeight = 600;

static uint64_t frameTarget = 0; // Microseconds per frame (0 = as fast as it goes)
static uint64_t frameStart = 0;	 // When the current frame started
static float frameTime = 1.0f / 60.0f;

static int fpsCount = 0; // Frames counted this second, for GetFPS()
static uint64_t fpsStart = 0;
static int fps = 0;

static uint32_t randomState = 1;

// Keys: this frame and last frame (IsKeyPressed compares the two)
static bool keyDown[SHIM_KEY_COUNT];
static bool keyWasDown[SHIM_KEY_COUNT];

static int logLevel = LOG_INFO;

void InitWindow(int width, int height, const char *title)
{
	worldWidth = width;
	worldHeight = height;

	PlatformInit();

	randomState = PlatformRandomSeed();
	if (randomState == 0)
		randomState = 0x9E3779B9u; // xorshift can't start from 0

	frameStart = PlatformMicros();
	fpsStart = frameStart;

	TraceLog(LOG_INFO, "SHIM: %s, world %i x %i on a %i x %i screen", title, width, height, SHIM_FB_WIDTH, SHIM_FB_HEIGHT);
}

void CloseWindow(void)
{
	TraceLog(LOG_INFO, "SHIM: Window closed");
}

bool WindowShouldClose(void)
{
	return false; // A board has no close button: the game runs until the power goes
}

int GetScreenWidth(void) { return worldWidth; }
int GetScreenHeight(void) { return worldHeight; }

void SetTargetFPS(int targetFps)
{
	frameTarget = (targetFps > 0) ? (1000000u / (uint64_t)targetFps) : 0;
}

float GetFrameTime(void) { return frameTime; }
double GetTime(void) { return (double)PlatformMicros() / 1000000.0; }
int GetFPS(void) { return fps; }

// xorshift32: small, fast and good enough for a game
int GetRandomValue(int min, int max)
{
	if (min > max)
	{
		int t = min;
		min = max;
		max = t;
	}

	randomState ^= randomState << 13;
	randomState ^= randomState >> 17;
	randomState ^= randomState << 5;

	return min + (int)(randomState % (uint32_t)(max - min + 1));
}

// ---------------------------------------------------------------------------
// Frames: BeginDrawing() / EndDrawing()
// ---------------------------------------------------------------------------

void BeginDrawing(void)
{
}

void EndDrawing(void)
{
	// 1. Show the frame
	PlatformPresent(shimFramebuffer);

	// 2. Hold the frame rate down to SetTargetFPS (if the board's quick enough)
	uint64_t now = PlatformMicros();
	if (frameTarget > 0 && now - frameStart < frameTarget)
	{
		PlatformSleepMicros((uint32_t)(frameTarget - (now - frameStart)));
		now = PlatformMicros();
	}

	// 3. How long that frame took (the game moves things by this much)
	frameTime = (float)(now - frameStart) / 1000000.0f;
	if (frameTime > 0.1f)
		frameTime = 0.1f; // A hiccup (or a slow board) shouldn't send everyone flying
	frameStart = now;

	// 4. Frames per second, counted once a second
	fpsCount++;
	if (now - fpsStart >= 1000000u)
	{
		fps = fpsCount;
		fpsCount = 0;
		fpsStart = now;
	}

	// 5. Read the controls for the next frame (raylib does this here too)
	memcpy(keyWasDown, keyDown, sizeof(keyDown));
	memset(keyDown, 0, sizeof(keyDown));
	PlatformReadKeys(keyDown);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool IsKeyDown(int key)
{
	return key >= 0 && key < SHIM_KEY_COUNT && keyDown[key];
}

bool IsKeyPressed(int key)
{
	return key >= 0 && key < SHIM_KEY_COUNT && keyDown[key] && !keyWasDown[key];
}

// No gamepads: the board's buttons arrive as keys (see the board's PlatformReadKeys)
bool IsGamepadAvailable(int gamepad)
{
	(void)gamepad;
	return false;
}

bool IsGamepadButtonDown(int gamepad, int button)
{
	(void)gamepad;
	(void)button;
	return false;
}

bool IsGamepadButtonPressed(int gamepad, int button)
{
	(void)gamepad;
	(void)button;
	return false;
}

float GetGamepadAxisMovement(int gamepad, int axis)
{
	(void)gamepad;
	(void)axis;
	return 0.0f;
}

// ---------------------------------------------------------------------------
// Logging and text
// ---------------------------------------------------------------------------

void SetTraceLogLevel(int level)
{
	logLevel = level;
}

void TraceLog(int level, const char *text, ...)
{
	if (level < logLevel)
		return;

	static const char *prefix[] = {"", "TRACE: ", "DEBUG: ", "INFO: ", "WARNING: ", "ERROR: ", "FATAL: ", ""};
	static char line[256]; // static: off the stack, which is small on a microcontroller

	int used = snprintf(line, sizeof(line), "%s", (level >= 0 && level <= 7) ? prefix[level] : "");

	va_list args;
	va_start(args, text);
	vsnprintf(line + used, sizeof(line) - (size_t)used, text, args);
	va_end(args);

	PlatformLog(line);
}

// Same idea as raylib: a few buffers used in turn, so two or three
// TextFormat() results can be in use at once
#define TEXTFORMAT_BUFFERS 4
#define TEXTFORMAT_LENGTH 256

const char *TextFormat(const char *text, ...)
{
	static char buffers[TEXTFORMAT_BUFFERS][TEXTFORMAT_LENGTH];
	static int index = 0;

	char *buffer = buffers[index];
	index = (index + 1) % TEXTFORMAT_BUFFERS;

	va_list args;
	va_start(args, text);
	vsnprintf(buffer, TEXTFORMAT_LENGTH, text, args);
	va_end(args);

	return buffer;
}

// ---------------------------------------------------------------------------
// Built-in assets: LoadTexture("resources/x.png") finds the converted copy
// ---------------------------------------------------------------------------

const void *ShimFindAsset(const char *fileName, ShimAssetType type)
{
	for (int i = 0; i < shimAssetCount; i++)
	{
		if (shimAssets[i].type == type && strcmp(shimAssets[i].fileName, fileName) == 0)
			return shimAssets[i].data;
	}

	TraceLog(LOG_WARNING, "SHIM: [%s] isn't built in: run tools/convert_assets.py on the game's folder", fileName);
	return NULL;
}

// ---------------------------------------------------------------------------
// Audio: short sounds only
// ---------------------------------------------------------------------------

static bool audioReady = false;

void InitAudioDevice(void)
{
	audioReady = PlatformAudioInit();
	TraceLog(audioReady ? LOG_INFO : LOG_WARNING, "SHIM: Audio %s", audioReady ? "ready" : "not available on this board");
}

void CloseAudioDevice(void)
{
	if (audioReady)
		PlatformAudioClose();
	audioReady = false;
}

bool IsAudioDeviceReady(void) { return audioReady; }

Sound LoadSound(const char *fileName)
{
	Sound sound = {0};
	const ShimSound *data = (const ShimSound *)ShimFindAsset(fileName, SHIM_ASSET_SOUND);
	if (data)
	{
		sound.shim = data;
		sound.frameCount = data->pcmLength;
		TraceLog(LOG_INFO, "SOUND: [%s] built in", fileName);
	}
	return sound;
}

bool IsSoundValid(Sound sound) { return sound.shim != NULL; }

void UnloadSound(Sound sound)
{
	(void)sound; // Built into the program: nothing to free
}

void PlaySound(Sound sound)
{
	if (audioReady && sound.shim)
		PlatformPlaySound((const ShimSound *)sound.shim);
}

void SetSoundPitch(Sound sound, float pitch)
{
	(void)sound;
	(void)pitch;
}

// No music on the boards: an ogg needs more memory and time to decode than
// they can spare. IsMusicValid() says no, and the game carries on quietly.
Music LoadMusicStream(const char *fileName)
{
	Music music = {0};
	TraceLog(LOG_INFO, "SHIM: [%s] music isn't played on this board", fileName);
	return music;
}

bool IsMusicValid(Music music) { return music.shim != NULL; }
void UnloadMusicStream(Music music) { (void)music; }
void PlayMusicStream(Music music) { (void)music; }
void StopMusicStream(Music music) { (void)music; }
void UpdateMusicStream(Music music) { (void)music; }
void SetMusicVolume(Music music, float volume)
{
	(void)music;
	(void)volume;
}
