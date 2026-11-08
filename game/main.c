/**
 * main.c : About the smallest game there is. WASD moves a square; walk into
 * the gold one and it hops somewhere else. Score goes up. That's it.
 *
 * The same file builds for the desktop and for the PicoCalc: on the PicoCalc,
 * #include <raylib.h> finds the shim (shim/) instead of the real raylib,
 * which shrinks the 800 x 600 world onto the little screen.
 *
 *     Desktop:    make -C game run
 *     Simulator:  make -C sim run
 *     PicoCalc:   cmake -S picocalc -B build && cmake --build build -j   (see README.md)
 */

#include <raylib.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PLAYER_SIZE 40
#define PLAYER_SPEED 300.0f // World units a second: the same speed on any machine
#define COIN_SIZE 24

// Somewhere for the coin, not hard against the edges
static Rectangle NewCoin(void)
{
	return (Rectangle){(float)GetRandomValue(20, SCREEN_WIDTH - 20 - COIN_SIZE),
					   (float)GetRandomValue(60, SCREEN_HEIGHT - 20 - COIN_SIZE), COIN_SIZE, COIN_SIZE};
}

int main(void)
{
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "WASD");
	SetTargetFPS(60);

	Rectangle player = {SCREEN_WIDTH / 2.0f - PLAYER_SIZE / 2.0f, SCREEN_HEIGHT / 2.0f - PLAYER_SIZE / 2.0f,
						PLAYER_SIZE, PLAYER_SIZE};
	Rectangle coin = NewCoin();
	int score = 0;

	while (!WindowShouldClose())
	{
		// Update: move by time, not by frame, so a slow board isn't a slow game
		float step = PLAYER_SPEED * GetFrameTime();
		if (IsKeyDown(KEY_W)) player.y -= step;
		if (IsKeyDown(KEY_S)) player.y += step;
		if (IsKeyDown(KEY_A)) player.x -= step;
		if (IsKeyDown(KEY_D)) player.x += step;

		// Stay on the screen
		if (player.x < 0) player.x = 0;
		if (player.y < 0) player.y = 0;
		if (player.x > SCREEN_WIDTH - PLAYER_SIZE) player.x = SCREEN_WIDTH - PLAYER_SIZE;
		if (player.y > SCREEN_HEIGHT - PLAYER_SIZE) player.y = SCREEN_HEIGHT - PLAYER_SIZE;

		// Got it? (Two rectangles overlap when they overlap on both axes)
		if (player.x < coin.x + coin.width && player.x + player.width > coin.x &&
			player.y < coin.y + coin.height && player.y + player.height > coin.y)
		{
			score++;
			coin = NewCoin();
			TraceLog(LOG_INFO, "[WASD] Score %i", score);
		}

		// Draw
		BeginDrawing();
		ClearBackground(DARKBLUE);
		DrawRectangle((int)coin.x, (int)coin.y, COIN_SIZE, COIN_SIZE, GOLD);
		DrawRectangle((int)player.x, (int)player.y, PLAYER_SIZE, PLAYER_SIZE, RAYWHITE);
		DrawText(TextFormat("Score: %i", score), 20, 20, 30, YELLOW);
		DrawText(TextFormat("%i FPS", GetFPS()), SCREEN_WIDTH - 120, 20, 30, LIGHTGRAY);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
