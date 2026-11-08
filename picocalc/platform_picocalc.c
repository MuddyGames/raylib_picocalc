/**
 * platform_picocalc.c : The raylib shim's board jobs on the ClockworkPi PicoCalc
 * (Raspberry Pi Pico 1 / RP2040).
 *
 *   Screen    320 x 320 LCD on SPI1, 18-bit colour (ClockworkPi's init sequence,
 *             from Code/picocalc_helloworld/lcdspi in github.com/clockworkpi/PicoCalc)
 *   Keyboard  The keyboard's own chip on I2C1, address 0x1F, read on the
 *             Pico's second core so the game never waits for it
 *   Sound     8-bit samples to PWM on GPIO 26/27, from a repeating timer
 *   Log       TraceLog over the Pico's micro-USB (/dev/ttyACM0 on Linux)
 *
 * Pins are in board_config.h.
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/rand.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"

#include "raylib.h"
#include "platform.h"

#define LCD_SPI spi1
#define KBD_I2C i2c1

// ---------------------------------------------------------------------------
// LCD
// ---------------------------------------------------------------------------

// One command and its data, with CS already low
static void LcdSend(uint8_t command, const uint8_t *data, size_t length)
{
	gpio_put(PICOCALC_LCD_DC, 0);
	spi_write_blocking(LCD_SPI, &command, 1);
	gpio_put(PICOCALC_LCD_DC, 1);
	if (length)
		spi_write_blocking(LCD_SPI, data, length);
}

// One command on its own. CS has to stay high for a moment between commands,
// or the LCD can miss the next one: at 200 MHz, two gpio_put()s in a row are
// only a few nanoseconds apart, too quick for it to notice.
static void LcdCommand(uint8_t command, const uint8_t *data, size_t length)
{
	gpio_put(PICOCALC_LCD_CS, 0);
	LcdSend(command, data, length);
	gpio_put(PICOCALC_LCD_CS, 1);
	busy_wait_us(1);
}

#define LCD(cmd, ...)                                                  \
	do                                                                 \
	{                                                                  \
		static const uint8_t d[] = {__VA_ARGS__};                      \
		LcdCommand(cmd, d, sizeof(d));                                 \
	} while (0)

static void LcdInit(void)
{
	spi_init(LCD_SPI, PICOCALC_LCD_SPI_HZ);
	gpio_set_function(PICOCALC_LCD_SCK, GPIO_FUNC_SPI);
	gpio_set_function(PICOCALC_LCD_MOSI, GPIO_FUNC_SPI);
	gpio_set_function(PICOCALC_LCD_MISO, GPIO_FUNC_SPI);

	gpio_init(PICOCALC_LCD_CS);
	gpio_init(PICOCALC_LCD_DC);
	gpio_init(PICOCALC_LCD_RST);
	gpio_set_dir(PICOCALC_LCD_CS, GPIO_OUT);
	gpio_set_dir(PICOCALC_LCD_DC, GPIO_OUT);
	gpio_set_dir(PICOCALC_LCD_RST, GPIO_OUT);
	gpio_put(PICOCALC_LCD_CS, 1);

	// Reset
	gpio_put(PICOCALC_LCD_RST, 1);
	sleep_ms(10);
	gpio_put(PICOCALC_LCD_RST, 0);
	sleep_ms(10);
	gpio_put(PICOCALC_LCD_RST, 1);
	sleep_ms(200);

	// ClockworkPi's sequence (lcdspi.c, pico_lcd_init)
	LCD(0xE0, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F); // Positive gamma
	LCD(0xE1, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F); // Negative gamma
	LCD(0xC0, 0x17, 0x15);		 // Power control 1
	LCD(0xC1, 0x41);			 // Power control 2
	LCD(0xC5, 0x00, 0x12, 0x80); // VCOM
	LCD(0x36, 0x48);			 // Memory access: MX, BGR
	LCD(0x3A, 0x66);			 // 18-bit colour over SPI
	LCD(0xB0, 0x00);			 // Interface mode
	LCD(0xB1, 0xA0);			 // Frame rate
	LcdCommand(0x21, NULL, 0);	 // Inversion on
	LCD(0xB4, 0x02);			 // Inversion control
	LCD(0xB6, 0x02, 0x02, 0x3B); // Display function
	LCD(0xB7, 0xC6);			 // Entry mode
	LCD(0xE9, 0x00);
	LCD(0xF7, 0xA9, 0x51, 0x2C, 0x82); // Adjust control 3
	LcdCommand(0x11, NULL, 0);		   // Sleep out
	sleep_ms(120);
	LcdCommand(0x29, NULL, 0); // Display on
	sleep_ms(120);
}

// Start writing pixels into a rectangle of the LCD. CS goes low once and stays
// low for the whole lot (where, then the pixels, until LcdEnd), so the LCD
// can't miss where they're meant to go. When it did, rows turned up wherever
// the last ones went: streaks, and text drawn on top of other text.
static void LcdBegin(int x0, int y0, int x1, int y1)
{
	uint8_t column[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
	uint8_t page[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};

	gpio_put(PICOCALC_LCD_CS, 0);
	LcdSend(0x2A, column, 4); // Columns
	LcdSend(0x2B, page, 4);	  // Rows
	LcdSend(0x2C, NULL, 0);	  // Pixels follow (DC stays high)
}

static void LcdEnd(void)
{
	while (spi_is_busy(LCD_SPI))
		tight_loop_contents();
	gpio_put(PICOCALC_LCD_CS, 1);
	busy_wait_us(1);
}

// RGB332 to the LCD's three bytes (R, G, B, top 6 bits used)
static uint8_t rgbLookup[256][3];

static int dmaChannel;
static uint8_t lineBuffers[2][SHIM_FB_WIDTH * 3];

static void ClearWholeScreen(void)
{
	memset(lineBuffers[0], 0, sizeof(lineBuffers[0]));
	LcdBegin(0, 0, 319, 319);
	for (int y = 0; y < 320; y++)
	{
		spi_write_blocking(LCD_SPI, lineBuffers[0], SHIM_FB_WIDTH * 3);
		spi_write_blocking(LCD_SPI, lineBuffers[0], (320 - SHIM_FB_WIDTH) * 3);
	}
	LcdEnd();
}

// ---------------------------------------------------------------------------
// Sending frames: only what changed
// ---------------------------------------------------------------------------
//
// The SPI link to the LCD is the slow part: a whole frame is 219 KB, about
// 70 ms at 25 MHz. So each frame is compared with the last one sent, and only
// the rectangles that changed go out. A square walking across the screen is
// a few dozen pixels a row, not 312.
//
// That helps with tearing too. The LCD redraws itself from its own memory
// about 60 times a second, top to bottom, and nothing tells us where it's up
// to (ClockworkPi didn't wire up the LCD's "tearing effect" pin). If it passes
// a sprite while we're halfway through writing it, you see half of the old
// sprite and half of the new one for a moment. The shorter each write, the
// less often that happens.

// What the LCD is showing now (word aligned, so rows compare 4 pixels at a time)
static uint8_t lastFrame[SHIM_FB_WIDTH * SHIM_FB_HEIGHT] __attribute__((aligned(4)));
static bool screenKnown = false; // false until the first frame has gone out in full
static int refreshRow = 0;		  // Where the rolling refresh is up to

_Static_assert(SHIM_FB_WIDTH % 4 == 0, "rows are compared 4 pixels at a time");

// Which part of row y differs from what the LCD has: false if none of it.
// [*x0, *x1) in pixels, to a multiple of 4.
static bool ChangedSpan(const uint8_t *fb, int y, int *x0, int *x1)
{
	const uint32_t *now = (const uint32_t *)(fb + y * SHIM_FB_WIDTH);
	const uint32_t *was = (const uint32_t *)(lastFrame + y * SHIM_FB_WIDTH);
	const int words = SHIM_FB_WIDTH / 4;

	int first = 0;
	while (first < words && now[first] == was[first])
		first++;
	if (first == words)
		return false;

	int last = words - 1;
	while (now[last] == was[last])
		last--;

	*x0 = first * 4;
	*x1 = (last + 1) * 4;
	return true;
}

// Is row y one of this frame's rolling refresh rows? A few rows every frame
// go out whatever happened, working down the screen, so a glitch on the SPI
// wires is painted over within a second, rather than sitting there until
// something walks over it.
static bool RefreshRow(int y)
{
	return (y - refreshRow + SHIM_FB_HEIGHT) % SHIM_FB_HEIGHT < PICOCALC_REFRESH_ROWS;
}

// Send a rectangle: rows first up to (not including) last, columns x0 to x1
static void SendRect(const uint8_t *fb, int x0, int x1, int first, int last)
{
	int width = x1 - x0;
	LcdBegin(PICOCALC_VIEW_X + x0, PICOCALC_VIEW_Y + first, PICOCALC_VIEW_X + x1 - 1, PICOCALC_VIEW_Y + last - 1);

	// Convert one line while DMA sends the last one
	for (int y = first; y < last; y++)
	{
		uint8_t *line = lineBuffers[y & 1];
		const uint8_t *src = fb + y * SHIM_FB_WIDTH + x0;
		for (int x = 0; x < width; x++)
		{
			const uint8_t *rgb = rgbLookup[src[x]];
			line[x * 3 + 0] = rgb[0];
			line[x * 3 + 1] = rgb[1];
			line[x * 3 + 2] = rgb[2];
		}

		dma_channel_wait_for_finish_blocking(dmaChannel);
		dma_channel_transfer_from_buffer_now(dmaChannel, line, (uint32_t)width * 3);

		// The LCD will have this row now
		memcpy(lastFrame + y * SHIM_FB_WIDTH + x0, src, (size_t)width);
	}

	dma_channel_wait_for_finish_blocking(dmaChannel);
	LcdEnd();
}

void PlatformPresent(const uint8_t *fb)
{
	int y = 0;
	while (y < SHIM_FB_HEIGHT)
	{
		// What needs sending on this row?
		int x0 = 0, x1 = SHIM_FB_WIDTH;
		bool send = !screenKnown || RefreshRow(y) || ChangedSpan(fb, y, &x0, &x1);
		if (!screenKnown || RefreshRow(y))
		{
			x0 = 0;
			x1 = SHIM_FB_WIDTH;
		}
		if (!send)
		{
			y++;
			continue;
		}

		// Rows below that need sending too go in the same rectangle, made
		// wide enough for all of them
		int first = y++;
		while (y < SHIM_FB_HEIGHT)
		{
			int a = 0, b = SHIM_FB_WIDTH;
			if (screenKnown && !RefreshRow(y) && !ChangedSpan(fb, y, &a, &b))
				break;
			if (a < x0)
				x0 = a;
			if (b > x1)
				x1 = b;
			y++;
		}
		SendRect(fb, x0, x1, first, y);
	}

	screenKnown = true;
	refreshRow = (refreshRow + PICOCALC_REFRESH_ROWS) % SHIM_FB_HEIGHT;
}

// ---------------------------------------------------------------------------
// Keyboard: read on core 1, so core 0 (the game) never waits for it
// ---------------------------------------------------------------------------

// PicoCalc key codes (ClockworkPi's keyboard_define.h)
#define PC_KEY_ESC 0xB1
#define PC_KEY_LEFT 0xB4
#define PC_KEY_UP 0xB5
#define PC_KEY_DOWN 0xB6
#define PC_KEY_RIGHT 0xB7
#define PC_KEY_ENTER 0x0A

// Which raylib key each PicoCalc key code is
static int RaylibKey(uint8_t code)
{
	switch (code)
	{
	case PC_KEY_UP: return KEY_UP;
	case PC_KEY_DOWN: return KEY_DOWN;
	case PC_KEY_LEFT: return KEY_LEFT;
	case PC_KEY_RIGHT: return KEY_RIGHT;
	case PC_KEY_ESC: return KEY_ESCAPE;
	case PC_KEY_ENTER: return KEY_ENTER;
	case ' ': return KEY_SPACE;
	default: break;
	}
	if (code >= 'a' && code <= 'z')
		return KEY_A + (code - 'a');
	if (code >= 'A' && code <= 'Z')
		return KEY_A + (code - 'A');
	if (code >= '0' && code <= '9')
		return KEY_ZERO + (code - '0');
	return -1;
}

// Written by core 1, read by core 0 (one byte each: no locking needed)
static volatile uint8_t keyHeld[SHIM_KEY_COUNT];

static void KeyboardLoop(void)
{
	for (;;)
	{
		// Ask for the next event in the keyboard's queue: [state, key code]
		uint8_t reg = 0x09, event[2] = {0, 0};
		if (i2c_write_timeout_us(KBD_I2C, PICOCALC_KBD_ADDR, &reg, 1, false, 500000) != 1)
		{
			sleep_ms(50);
			continue;
		}
		sleep_ms(16); // ClockworkPi's examples wait this long for the answer
		if (i2c_read_timeout_us(KBD_I2C, PICOCALC_KBD_ADDR, event, 2, false, 500000) != 2)
			continue;

		uint8_t state = event[0], code = event[1];
		if (state == 0 && code == 0)
			continue; // Queue empty

		int key = RaylibKey(code);
		if (key < 0)
			continue;

		// 1 = pressed, 2 = held, 3 = released
		keyHeld[key] = (state == 1 || state == 2);
	}
}

void PlatformReadKeys(bool down[SHIM_KEY_COUNT])
{
	for (int i = 0; i < SHIM_KEY_COUNT; i++)
		down[i] = keyHeld[i];
}

// ---------------------------------------------------------------------------
// Sound: 8-bit samples to PWM, one sample per timer tick
// ---------------------------------------------------------------------------

static repeating_timer_t soundTimer;
static const ShimSound *volatile playing = NULL;
static volatile uint32_t position = 0;
static uint sliceLeft, sliceRight;

static bool SoundTick(repeating_timer_t *timer)
{
	(void)timer;
	const ShimSound *sound = playing;
	uint8_t level = 128; // Silence: half way

	if (sound && position < sound->pcmLength)
		level = sound->pcm[position++];
	else
		playing = NULL;

	pwm_set_gpio_level(PICOCALC_AUDIO_LEFT, level);
	pwm_set_gpio_level(PICOCALC_AUDIO_RIGHT, level);
	return true; // Keep going
}

bool PlatformAudioInit(void)
{
	gpio_set_function(PICOCALC_AUDIO_LEFT, GPIO_FUNC_PWM);
	gpio_set_function(PICOCALC_AUDIO_RIGHT, GPIO_FUNC_PWM);
	sliceLeft = pwm_gpio_to_slice_num(PICOCALC_AUDIO_LEFT);
	sliceRight = pwm_gpio_to_slice_num(PICOCALC_AUDIO_RIGHT);

	// 8-bit PWM, well above hearing (125 MHz / 256 = 488 kHz)
	pwm_config config = pwm_get_default_config();
	pwm_config_set_wrap(&config, 255);
	pwm_init(sliceLeft, &config, true);
	if (sliceRight != sliceLeft)
		pwm_init(sliceRight, &config, true);

	// One sample per tick (negative: from the start of one tick to the next)
	return add_repeating_timer_us(-1000000 / 11025, SoundTick, NULL, &soundTimer);
}

void PlatformAudioClose(void)
{
	cancel_repeating_timer(&soundTimer);
	pwm_set_enabled(sliceLeft, false);
	pwm_set_enabled(sliceRight, false);
}

void PlatformPlaySound(const ShimSound *sound)
{
	if (!sound->pcm)
		return;
	playing = NULL; // Stop the timer reading the old one
	position = 0;
	playing = sound;
}

// ---------------------------------------------------------------------------
// Start-up, time, log
// ---------------------------------------------------------------------------

void PlatformInit(void)
{
	stdio_init_all(); // printf to the PicoCalc's USB-C and the Pico's micro-USB

	// RGB332 -> 3 bytes, once
	for (int p = 0; p < 256; p++)
	{
		rgbLookup[p][0] = (uint8_t)(((p >> 5) & 7) * 255 / 7);
		rgbLookup[p][1] = (uint8_t)(((p >> 2) & 7) * 255 / 7);
		rgbLookup[p][2] = (uint8_t)((p & 3) * 255 / 3);
	}

	LcdInit();

	// DMA feeds the LCD's SPI, a line at a time
	dmaChannel = dma_claim_unused_channel(true);
	dma_channel_config dma = dma_channel_get_default_config(dmaChannel);
	channel_config_set_transfer_data_size(&dma, DMA_SIZE_8);
	channel_config_set_dreq(&dma, spi_get_dreq(LCD_SPI, true));
	channel_config_set_read_increment(&dma, true);
	channel_config_set_write_increment(&dma, false);
	dma_channel_configure(dmaChannel, &dma, &spi_get_hw(LCD_SPI)->dr, lineBuffers[0], 0, false);

	ClearWholeScreen(); // Black bars round the game's picture

	// Keyboard on core 1
	i2c_init(KBD_I2C, PICOCALC_KBD_HZ);
	gpio_set_function(PICOCALC_KBD_SDA, GPIO_FUNC_I2C);
	gpio_set_function(PICOCALC_KBD_SCL, GPIO_FUNC_I2C);
	gpio_pull_up(PICOCALC_KBD_SDA);
	gpio_pull_up(PICOCALC_KBD_SCL);
	multicore_launch_core1(KeyboardLoop);
}

uint64_t PlatformMicros(void)
{
	return time_us_64();
}

void PlatformSleepMicros(uint32_t micros)
{
	sleep_us(micros);
}

void PlatformLog(const char *line)
{
	// fputs, not printf: the line is already formatted, and the stack is small
	fputs(line, stdout);
	fputs("\n", stdout);
}

uint32_t PlatformRandomSeed(void)
{
	return get_rand_32();
}
