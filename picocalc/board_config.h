/**
 * board_config.h : The ClockworkPi PicoCalc (Raspberry Pi Pico 1, 320 x 320 LCD)
 *
 * Games are written for an 800 x 600 world, as on the desktop. The shim draws
 * it at 25/64 scale: 312 x 234 pixels, in the middle of the 320 x 320 screen.
 *
 * The Pico 1 has 264 KB of RAM: the framebuffer (312 x 234, one byte per
 * pixel) takes 73 KB, and a copy of the last frame another 73 KB (so only
 * what changed is sent to the LCD). Pictures and sounds live in flash (2 MB).
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ---- The shim ----------------------------------------------------------------

#define SHIM_FB_WIDTH 312		// 800 x 25/64
#define SHIM_FB_HEIGHT 234		// 600 x 25/64
#define SHIM_SCALE_X (25.0f / 64.0f)
#define SHIM_SCALE_Y (25.0f / 64.0f)
#define SHIM_IMAGE_SCALE 0.25f	// GenImage...() pictures stored at quarter size (an 800 x 600 one: 78 x 59) and stretched
#define SHIM_SPRITE_CELL 25		// convert_assets.py: pictures shrunk by 25/64 (a 64 px sprite cell becomes 25 px)
#define SHIM_PCM_RATE 11025		// convert_assets.py: sounds as 8-bit samples at 11 kHz (PWM to the speakers)

// ---- The PicoCalc ------------------------------------------------------------

// Where the 312 x 234 picture sits on the 320 x 320 LCD
#define PICOCALC_VIEW_X 4
#define PICOCALC_VIEW_Y 43

// LCD on SPI1 (from ClockworkPi's lcdspi.h)
#define PICOCALC_LCD_SCK 10
#define PICOCALC_LCD_MOSI 11
#define PICOCALC_LCD_MISO 12
#define PICOCALC_LCD_CS 13
#define PICOCALC_LCD_DC 14
#define PICOCALC_LCD_RST 15
#define PICOCALC_LCD_SPI_HZ 25000000 // ClockworkPi's speed. 33 MHz drops the odd pixel; try 20000000 if you still see specks
#define PICOCALC_REFRESH_ROWS 8      // Rows re-sent every frame whether they changed or not (see PlatformPresent)

// Keyboard on I2C1, address 0x1F (from ClockworkPi's i2ckbd.h)
#define PICOCALC_KBD_SDA 6
#define PICOCALC_KBD_SCL 7
#define PICOCALC_KBD_ADDR 0x1F
#define PICOCALC_KBD_HZ 10000

// Speakers: PWM on GPIO 26 (left) and 27 (right)
#define PICOCALC_AUDIO_LEFT 26
#define PICOCALC_AUDIO_RIGHT 27

#endif // BOARD_CONFIG_H
