/**
 * raylib.h (board shim) : the part of raylib this game uses, for boards that
 * can't run raylib itself, like the PicoCalc.
 *
 * Same names, same values, same behaviour as far as the game can tell, so the
 * game's own code doesn't change. Underneath, it draws into a small framebuffer
 * and hands it to the board (see platform.h), scaling the game's world
 * (SCREEN_WIDTH x SCREEN_HEIGHT, 800 x 600) down to the board's screen.
 *
 * Only what the game calls is here. If you call something else, you'll get
 * "implicit declaration": add it to the shim (shim/src), or don't use it.
 *
 * The enums and colours below are copied from raylib 6.x, so every KEY_ and
 * colour has the same value as on the desktop.
 * raylib is Copyright (c) 2013-2026 Ramon Santamaria (@raysan5), zlib/libpng licence.
 */

#ifndef RAYLIB_H
#define RAYLIB_H

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

#define RAYLIB_SHIM 1 // So code can tell it's on a board: #ifdef RAYLIB_SHIM

// The simulator links the real raylib for its window, so it builds the game
// and the shim with these names prefixed "Shim" (see shim_rename.h)
#ifdef SHIM_RENAME
#include "shim_rename.h"
#endif

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define DEG2RAD (PI / 180.0f)
#define RAD2DEG (180.0f / PI)

#define CLITERAL(type) (type)

// ---------------------------------------------------------------------------
// Types (raymath.h checks these RL_*_TYPE guards, so it uses ours)
// ---------------------------------------------------------------------------
typedef struct Vector2 { float x; float y; } Vector2;
typedef struct Vector3 { float x; float y; float z; } Vector3;
typedef struct Vector4 { float x; float y; float z; float w; } Vector4;
typedef Vector4 Quaternion;
typedef struct Matrix {
    float m0, m4, m8, m12;
    float m1, m5, m9, m13;
    float m2, m6, m10, m14;
    float m3, m7, m11, m15;
} Matrix;
typedef struct Color { unsigned char r; unsigned char g; unsigned char b; unsigned char a; } Color;
typedef struct Rectangle { float x; float y; float width; float height; } Rectangle;

#define RL_COLOR_TYPE
#define RL_RECTANGLE_TYPE
#define RL_VECTOR2_TYPE
#define RL_VECTOR3_TYPE
#define RL_VECTOR4_TYPE
#define RL_QUATERNION_TYPE
#define RL_MATRIX_TYPE

// An image in RAM, always RGBA (4 bytes per pixel).
// width x height is how many pixels are stored. A board can't hold an
// 800 x 600 image, so GenImagePerlinNoise() stores fewer, and remembers the
// size it stands for in the world (worldWidth x worldHeight): a texture
// made from it still covers 800 x 600 when drawn.
typedef struct Image {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
    int worldWidth;
    int worldHeight;
} Image;

// A texture: either one built into the program (sprite sheets, made by
// tools/convert_assets.py, read straight from flash) or one made
// at run time from an Image (the background).
typedef struct Texture {
    unsigned int id;     // 0 = not loaded
    int width;           // In the game's world units (same as on the desktop)
    int height;
    int mipmaps;
    int format;
    const void *shim;    // The shim's own data (ShimTexture, see shim_internal.h)
} Texture;
typedef Texture Texture2D;

typedef struct Font { int baseSize; } Font;

typedef struct Sound {
    unsigned int frameCount;
    const void *shim;    // The shim's own data (ShimSound), NULL if not loaded
} Sound;

typedef struct Music {
    unsigned int frameCount;
    bool looping;
    const void *shim;    // Always NULL: no music streaming on the boards
} Music;

// ---------------------------------------------------------------------------
// Colours (same values as raylib)
// ---------------------------------------------------------------------------
#define LIGHTGRAY   CLITERAL(Color){ 200, 200, 200, 255 }
#define GRAY        CLITERAL(Color){ 130, 130, 130, 255 }
#define DARKGRAY    CLITERAL(Color){ 80, 80, 80, 255 }
#define YELLOW      CLITERAL(Color){ 253, 249, 0, 255 }
#define GOLD        CLITERAL(Color){ 255, 203, 0, 255 }
#define ORANGE      CLITERAL(Color){ 255, 161, 0, 255 }
#define PINK        CLITERAL(Color){ 255, 109, 194, 255 }
#define RED         CLITERAL(Color){ 230, 41, 55, 255 }
#define MAROON      CLITERAL(Color){ 190, 33, 55, 255 }
#define GREEN       CLITERAL(Color){ 0, 228, 48, 255 }
#define LIME        CLITERAL(Color){ 0, 158, 47, 255 }
#define DARKGREEN   CLITERAL(Color){ 0, 117, 44, 255 }
#define SKYBLUE     CLITERAL(Color){ 102, 191, 255, 255 }
#define BLUE        CLITERAL(Color){ 0, 121, 241, 255 }
#define DARKBLUE    CLITERAL(Color){ 0, 82, 172, 255 }
#define PURPLE      CLITERAL(Color){ 200, 122, 255, 255 }
#define VIOLET      CLITERAL(Color){ 135, 60, 190, 255 }
#define DARKPURPLE  CLITERAL(Color){ 112, 31, 126, 255 }
#define BEIGE       CLITERAL(Color){ 211, 176, 131, 255 }
#define BROWN       CLITERAL(Color){ 127, 106, 79, 255 }
#define DARKBROWN   CLITERAL(Color){ 76, 63, 47, 255 }
#define WHITE       CLITERAL(Color){ 255, 255, 255, 255 }
#define BLACK       CLITERAL(Color){ 0, 0, 0, 255 }
#define BLANK       CLITERAL(Color){ 0, 0, 0, 0 }
#define MAGENTA     CLITERAL(Color){ 255, 0, 255, 255 }
#define RAYWHITE    CLITERAL(Color){ 245, 245, 245, 255 }

// ---------------------------------------------------------------------------
// Enums (copied from raylib, same values)
// ---------------------------------------------------------------------------
typedef enum {
    LOG_ALL = 0,        // Display all logs
    LOG_TRACE,          // Trace logging, intended for internal use only
    LOG_DEBUG,          // Debug logging, used for internal debugging, it should be disabled on release builds
    LOG_INFO,           // Info logging, used for program execution info
    LOG_WARNING,        // Warning logging, used on recoverable failures
    LOG_ERROR,          // Error logging, used on unrecoverable failures
    LOG_FATAL,          // Fatal logging, used to abort program: exit(EXIT_FAILURE)
    LOG_NONE            // Disable logging
} TraceLogLevel;

typedef enum {
    KEY_NULL            = 0,        // Key: NULL, used for no key pressed
    // Alphanumeric keys
    KEY_APOSTROPHE      = 39,       // Key: '
    KEY_COMMA           = 44,       // Key: ,
    KEY_MINUS           = 45,       // Key: -
    KEY_PERIOD          = 46,       // Key: .
    KEY_SLASH           = 47,       // Key: /
    KEY_ZERO            = 48,       // Key: 0
    KEY_ONE             = 49,       // Key: 1
    KEY_TWO             = 50,       // Key: 2
    KEY_THREE           = 51,       // Key: 3
    KEY_FOUR            = 52,       // Key: 4
    KEY_FIVE            = 53,       // Key: 5
    KEY_SIX             = 54,       // Key: 6
    KEY_SEVEN           = 55,       // Key: 7
    KEY_EIGHT           = 56,       // Key: 8
    KEY_NINE            = 57,       // Key: 9
    KEY_SEMICOLON       = 59,       // Key: ;
    KEY_EQUAL           = 61,       // Key: =
    KEY_A               = 65,       // Key: A | a
    KEY_B               = 66,       // Key: B | b
    KEY_C               = 67,       // Key: C | c
    KEY_D               = 68,       // Key: D | d
    KEY_E               = 69,       // Key: E | e
    KEY_F               = 70,       // Key: F | f
    KEY_G               = 71,       // Key: G | g
    KEY_H               = 72,       // Key: H | h
    KEY_I               = 73,       // Key: I | i
    KEY_J               = 74,       // Key: J | j
    KEY_K               = 75,       // Key: K | k
    KEY_L               = 76,       // Key: L | l
    KEY_M               = 77,       // Key: M | m
    KEY_N               = 78,       // Key: N | n
    KEY_O               = 79,       // Key: O | o
    KEY_P               = 80,       // Key: P | p
    KEY_Q               = 81,       // Key: Q | q
    KEY_R               = 82,       // Key: R | r
    KEY_S               = 83,       // Key: S | s
    KEY_T               = 84,       // Key: T | t
    KEY_U               = 85,       // Key: U | u
    KEY_V               = 86,       // Key: V | v
    KEY_W               = 87,       // Key: W | w
    KEY_X               = 88,       // Key: X | x
    KEY_Y               = 89,       // Key: Y | y
    KEY_Z               = 90,       // Key: Z | z
    KEY_LEFT_BRACKET    = 91,       // Key: [
    KEY_BACKSLASH       = 92,       // Key: '\'
    KEY_RIGHT_BRACKET   = 93,       // Key: ]
    KEY_GRAVE           = 96,       // Key: `
    // Function keys
    KEY_SPACE           = 32,       // Key: Space
    KEY_ESCAPE          = 256,      // Key: Esc
    KEY_ENTER           = 257,      // Key: Enter
    KEY_TAB             = 258,      // Key: Tab
    KEY_BACKSPACE       = 259,      // Key: Backspace
    KEY_INSERT          = 260,      // Key: Ins
    KEY_DELETE          = 261,      // Key: Del
    KEY_RIGHT           = 262,      // Key: Cursor right
    KEY_LEFT            = 263,      // Key: Cursor left
    KEY_DOWN            = 264,      // Key: Cursor down
    KEY_UP              = 265,      // Key: Cursor up
    KEY_PAGE_UP         = 266,      // Key: Page up
    KEY_PAGE_DOWN       = 267,      // Key: Page down
    KEY_HOME            = 268,      // Key: Home
    KEY_END             = 269,      // Key: End
    KEY_CAPS_LOCK       = 280,      // Key: Caps lock
    KEY_SCROLL_LOCK     = 281,      // Key: Scroll down
    KEY_NUM_LOCK        = 282,      // Key: Num lock
    KEY_PRINT_SCREEN    = 283,      // Key: Print screen
    KEY_PAUSE           = 284,      // Key: Pause
    KEY_F1              = 290,      // Key: F1
    KEY_F2              = 291,      // Key: F2
    KEY_F3              = 292,      // Key: F3
    KEY_F4              = 293,      // Key: F4
    KEY_F5              = 294,      // Key: F5
    KEY_F6              = 295,      // Key: F6
    KEY_F7              = 296,      // Key: F7
    KEY_F8              = 297,      // Key: F8
    KEY_F9              = 298,      // Key: F9
    KEY_F10             = 299,      // Key: F10
    KEY_F11             = 300,      // Key: F11
    KEY_F12             = 301,      // Key: F12
    KEY_LEFT_SHIFT      = 340,      // Key: Shift left
    KEY_LEFT_CONTROL    = 341,      // Key: Control left
    KEY_LEFT_ALT        = 342,      // Key: Alt left
    KEY_LEFT_SUPER      = 343,      // Key: Super left
    KEY_RIGHT_SHIFT     = 344,      // Key: Shift right
    KEY_RIGHT_CONTROL   = 345,      // Key: Control right
    KEY_RIGHT_ALT       = 346,      // Key: Alt right
    KEY_RIGHT_SUPER     = 347,      // Key: Super right
    KEY_KB_MENU         = 348,      // Key: KB menu
    // Keypad keys
    KEY_KP_0            = 320,      // Key: Keypad 0
    KEY_KP_1            = 321,      // Key: Keypad 1
    KEY_KP_2            = 322,      // Key: Keypad 2
    KEY_KP_3            = 323,      // Key: Keypad 3
    KEY_KP_4            = 324,      // Key: Keypad 4
    KEY_KP_5            = 325,      // Key: Keypad 5
    KEY_KP_6            = 326,      // Key: Keypad 6
    KEY_KP_7            = 327,      // Key: Keypad 7
    KEY_KP_8            = 328,      // Key: Keypad 8
    KEY_KP_9            = 329,      // Key: Keypad 9
    KEY_KP_DECIMAL      = 330,      // Key: Keypad .
    KEY_KP_DIVIDE       = 331,      // Key: Keypad /
    KEY_KP_MULTIPLY     = 332,      // Key: Keypad *
    KEY_KP_SUBTRACT     = 333,      // Key: Keypad -
    KEY_KP_ADD          = 334,      // Key: Keypad +
    KEY_KP_ENTER        = 335,      // Key: Keypad Enter
    KEY_KP_EQUAL        = 336,      // Key: Keypad =
    // Android key buttons
    KEY_BACK            = 4,        // Key: Android back button
    KEY_MENU            = 5,        // Key: Android menu button
    KEY_VOLUME_UP       = 24,       // Key: Android volume up button
    KEY_VOLUME_DOWN     = 25        // Key: Android volume down button
} KeyboardKey;

typedef enum {
    GAMEPAD_BUTTON_UNKNOWN = 0,         // Unknown button, for error checking
    GAMEPAD_BUTTON_LEFT_FACE_UP,        // Gamepad left DPAD up button
    GAMEPAD_BUTTON_LEFT_FACE_RIGHT,     // Gamepad left DPAD right button
    GAMEPAD_BUTTON_LEFT_FACE_DOWN,      // Gamepad left DPAD down button
    GAMEPAD_BUTTON_LEFT_FACE_LEFT,      // Gamepad left DPAD left button
    GAMEPAD_BUTTON_RIGHT_FACE_UP,       // Gamepad right button up (i.e. PS3: Triangle, Xbox: Y)
    GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,    // Gamepad right button right (i.e. PS3: Circle, Xbox: B)
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN,     // Gamepad right button down (i.e. PS3: Cross, Xbox: A)
    GAMEPAD_BUTTON_RIGHT_FACE_LEFT,     // Gamepad right button left (i.e. PS3: Square, Xbox: X)
    GAMEPAD_BUTTON_LEFT_TRIGGER_1,      // Gamepad top/back trigger left (first), it could be a trailing button
    GAMEPAD_BUTTON_LEFT_TRIGGER_2,      // Gamepad top/back trigger left (second), it could be a trailing button
    GAMEPAD_BUTTON_RIGHT_TRIGGER_1,     // Gamepad top/back trigger right (first), it could be a trailing button
    GAMEPAD_BUTTON_RIGHT_TRIGGER_2,     // Gamepad top/back trigger right (second), it could be a trailing button
    GAMEPAD_BUTTON_MIDDLE_LEFT,         // Gamepad center buttons, left one (i.e. PS3: Select)
    GAMEPAD_BUTTON_MIDDLE,              // Gamepad center buttons, middle one (i.e. PS3: PS, Xbox: XBOX)
    GAMEPAD_BUTTON_MIDDLE_RIGHT,        // Gamepad center buttons, right one (i.e. PS3: Start)
    GAMEPAD_BUTTON_LEFT_THUMB,          // Gamepad joystick pressed button left
    GAMEPAD_BUTTON_RIGHT_THUMB          // Gamepad joystick pressed button right
} GamepadButton;

typedef enum {
    GAMEPAD_AXIS_LEFT_X        = 0,     // Gamepad left stick X axis
    GAMEPAD_AXIS_LEFT_Y        = 1,     // Gamepad left stick Y axis
    GAMEPAD_AXIS_RIGHT_X       = 2,     // Gamepad right stick X axis
    GAMEPAD_AXIS_RIGHT_Y       = 3,     // Gamepad right stick Y axis
    GAMEPAD_AXIS_LEFT_TRIGGER  = 4,     // Gamepad back trigger left, pressure level: [1..-1]
    GAMEPAD_AXIS_RIGHT_TRIGGER = 5      // Gamepad back trigger right, pressure level: [1..-1]
} GamepadAxis;

typedef enum {
    PIXELFORMAT_UNCOMPRESSED_GRAYSCALE = 1, // 8 bit per pixel (no alpha)
    PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA,    // 8*2 bpp (2 channels)
    PIXELFORMAT_UNCOMPRESSED_R5G6B5,        // 16 bpp
    PIXELFORMAT_UNCOMPRESSED_R8G8B8,        // 24 bpp
    PIXELFORMAT_UNCOMPRESSED_R5G5B5A1,      // 16 bpp (1 bit alpha)
    PIXELFORMAT_UNCOMPRESSED_R4G4B4A4,      // 16 bpp (4 bit alpha)
    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,      // 32 bpp
    PIXELFORMAT_UNCOMPRESSED_R32,           // 32 bpp (1 channel - float)
    PIXELFORMAT_UNCOMPRESSED_R32G32B32,     // 32*3 bpp (3 channels - float)
    PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,  // 32*4 bpp (4 channels - float)
    PIXELFORMAT_UNCOMPRESSED_R16,           // 16 bpp (1 channel - half float)
    PIXELFORMAT_UNCOMPRESSED_R16G16B16,     // 16*3 bpp (3 channels - half float)
    PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,  // 16*4 bpp (4 channels - half float)
    PIXELFORMAT_COMPRESSED_DXT1_RGB,        // 4 bpp (no alpha)
    PIXELFORMAT_COMPRESSED_DXT1_RGBA,       // 4 bpp (1 bit alpha)
    PIXELFORMAT_COMPRESSED_DXT3_RGBA,       // 8 bpp
    PIXELFORMAT_COMPRESSED_DXT5_RGBA,       // 8 bpp
    PIXELFORMAT_COMPRESSED_ETC1_RGB,        // 4 bpp
    PIXELFORMAT_COMPRESSED_ETC2_RGB,        // 4 bpp
    PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA,   // 8 bpp
    PIXELFORMAT_COMPRESSED_PVRT_RGB,        // 4 bpp
    PIXELFORMAT_COMPRESSED_PVRT_RGBA,       // 4 bpp
    PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA,   // 8 bpp
    PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA    // 2 bpp
} PixelFormat;

// ---------------------------------------------------------------------------
// Window and timing
// ---------------------------------------------------------------------------
void InitWindow(int width, int height, const char *title); // width x height = the game's world
void CloseWindow(void);
bool WindowShouldClose(void);    // Always false on a board: there's no window to close
int GetScreenWidth(void);        // The world's width (800), not the board's screen
int GetScreenHeight(void);
void SetTargetFPS(int fps);
float GetFrameTime(void);
double GetTime(void);
int GetFPS(void);
int GetRandomValue(int min, int max);
void SetTraceLogLevel(int logLevel);
void TraceLog(int logLevel, const char *text, ...);

// ---------------------------------------------------------------------------
// Drawing (positions and sizes in world units, like the desktop)
// ---------------------------------------------------------------------------
void BeginDrawing(void);
void EndDrawing(void);           // Sends the frame to the screen, then reads the input
void ClearBackground(Color color);
void DrawRectangle(int posX, int posY, int width, int height, Color color);
void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color);
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float lineThick, Color color);
void DrawRing(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color);
void DrawText(const char *text, int posX, int posY, int fontSize, Color color);
int MeasureText(const char *text, int fontSize);   // In world units
const char *TextFormat(const char *text, ...);
Color Fade(Color color, float alpha);
Color ColorAlpha(Color color, float alpha);

// ---------------------------------------------------------------------------
// Images and textures
// ---------------------------------------------------------------------------
Image GenImagePerlinNoise(int width, int height, int offsetX, int offsetY, float scale);
void ImageFormat(Image *image, int newFormat);    // Images are always RGBA here
void UnloadImage(Image image);
Texture2D LoadTexture(const char *fileName);       // Finds a texture built into the program
Texture2D LoadTextureFromImage(Image image);
void UnloadTexture(Texture2D texture);
void DrawTexture(Texture2D texture, int posX, int posY, Color tint);
void DrawTextureRec(Texture2D texture, Rectangle source, Vector2 position, Color tint);

// ---------------------------------------------------------------------------
// Input (the board maps its keyboard or buttons onto these KEY_ values)
// ---------------------------------------------------------------------------
bool IsKeyDown(int key);
bool IsKeyPressed(int key);
bool IsGamepadAvailable(int gamepad);              // Always false on a board
bool IsGamepadButtonDown(int gamepad, int button);
bool IsGamepadButtonPressed(int gamepad, int button);
float GetGamepadAxisMovement(int gamepad, int axis);

// ---------------------------------------------------------------------------
// Audio (short sounds only; no music streaming on the boards)
// ---------------------------------------------------------------------------
void InitAudioDevice(void);
void CloseAudioDevice(void);
bool IsAudioDeviceReady(void);
Sound LoadSound(const char *fileName);             // Finds a sound built into the program
bool IsSoundValid(Sound sound);
void UnloadSound(Sound sound);
void PlaySound(Sound sound);
void SetSoundPitch(Sound sound, float pitch);      // Ignored on the boards
Music LoadMusicStream(const char *fileName);       // Returns an invalid Music: IsMusicValid() is false
bool IsMusicValid(Music music);
void UnloadMusicStream(Music music);
void PlayMusicStream(Music music);
void StopMusicStream(Music music);
void UpdateMusicStream(Music music);
void SetMusicVolume(Music music, float volume);

#endif // RAYLIB_H
