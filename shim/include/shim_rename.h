/**
 * shim_rename.h : Only for the simulator (sim/), which uses the real raylib
 * for its window. The game and the shim are compiled with SHIM_RENAME, so their
 * InitWindow, DrawText ... become ShimInitWindow, ShimDrawText ..., and the two
 * can be linked into one program without arguing over the names.
 *
 * Every function in the shim's raylib.h needs a line here: add one when you
 * add a function, or the simulator will complain of "multiple definition".
 */

#ifndef SHIM_RENAME_H
#define SHIM_RENAME_H

#define InitWindow ShimInitWindow
#define CloseWindow ShimCloseWindow
#define WindowShouldClose ShimWindowShouldClose
#define GetScreenWidth ShimGetScreenWidth
#define GetScreenHeight ShimGetScreenHeight
#define SetTargetFPS ShimSetTargetFPS
#define GetFrameTime ShimGetFrameTime
#define GetTime ShimGetTime
#define GetFPS ShimGetFPS
#define GetRandomValue ShimGetRandomValue
#define SetTraceLogLevel ShimSetTraceLogLevel
#define TraceLog ShimTraceLog
#define BeginDrawing ShimBeginDrawing
#define EndDrawing ShimEndDrawing
#define ClearBackground ShimClearBackground
#define DrawRectangle ShimDrawRectangle
#define DrawRectangleRounded ShimDrawRectangleRounded
#define DrawRectangleRoundedLinesEx ShimDrawRectangleRoundedLinesEx
#define DrawRing ShimDrawRing
#define DrawText ShimDrawText
#define MeasureText ShimMeasureText
#define TextFormat ShimTextFormat
#define Fade ShimFade
#define ColorAlpha ShimColorAlpha
#define GenImagePerlinNoise ShimGenImagePerlinNoise
#define ImageFormat ShimImageFormat
#define UnloadImage ShimUnloadImage
#define LoadTexture ShimLoadTexture
#define LoadTextureFromImage ShimLoadTextureFromImage
#define UnloadTexture ShimUnloadTexture
#define DrawTexture ShimDrawTexture
#define DrawTextureRec ShimDrawTextureRec
#define IsKeyDown ShimIsKeyDown
#define IsKeyPressed ShimIsKeyPressed
#define IsGamepadAvailable ShimIsGamepadAvailable
#define IsGamepadButtonDown ShimIsGamepadButtonDown
#define IsGamepadButtonPressed ShimIsGamepadButtonPressed
#define GetGamepadAxisMovement ShimGetGamepadAxisMovement
#define InitAudioDevice ShimInitAudioDevice
#define CloseAudioDevice ShimCloseAudioDevice
#define IsAudioDeviceReady ShimIsAudioDeviceReady
#define LoadSound ShimLoadSound
#define IsSoundValid ShimIsSoundValid
#define UnloadSound ShimUnloadSound
#define PlaySound ShimPlaySound
#define SetSoundPitch ShimSetSoundPitch
#define LoadMusicStream ShimLoadMusicStream
#define IsMusicValid ShimIsMusicValid
#define UnloadMusicStream ShimUnloadMusicStream
#define PlayMusicStream ShimPlayMusicStream
#define StopMusicStream ShimStopMusicStream
#define UpdateMusicStream ShimUpdateMusicStream
#define SetMusicVolume ShimSetMusicVolume

// raylib has stb_perlin inside it too
#define stb_perlin_noise3 shim_stb_perlin_noise3
#define stb_perlin_noise3_seed shim_stb_perlin_noise3_seed
#define stb_perlin_ridge_noise3 shim_stb_perlin_ridge_noise3
#define stb_perlin_fbm_noise3 shim_stb_perlin_fbm_noise3
#define stb_perlin_turbulence_noise3 shim_stb_perlin_turbulence_noise3
#define stb_perlin_noise3_wrap_nonpow2 shim_stb_perlin_noise3_wrap_nonpow2
#define stb_perlin_noise3_internal shim_stb_perlin_noise3_internal

#endif // SHIM_RENAME_H
