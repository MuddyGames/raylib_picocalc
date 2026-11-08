/**
 * rl_draw.c : The raylib shim's drawing.
 *
 * Everything the game draws arrives in its own units (InitWindow's width x
 * height, as on the desktop) and lands on the board's framebuffer here:
 *
 *     screen x = shimOffsetX + x * shimScale
 *     screen y = shimOffsetY + y * shimScale
 *
 * A game written at the screen's own size gets scale 1 and no offset:
 * exactly what it asked for. A bigger world is shrunk to fit, keeping its
 * shape, with bars where it doesn't fill the screen.
 *
 * The framebuffer is one byte per pixel, RGB332 (rrrgggbb): 256 colours,
 * plenty for a small screen, and a whole frame in as few bytes as it has pixels.
 *
 * Pictures from convert_assets.py were shrunk to the game's scale before
 * they were built in, so they're usually drawn pixel for pixel; if not, they're
 * resized here. Text is raylib's default font at whole multiples of its 10
 * pixel size, never smaller.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shim_internal.h"
#include "rl_font_data.h"

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

// Word aligned, so a board can read it 4 bytes at a time
uint8_t shimFramebuffer[SHIM_FB_WIDTH * SHIM_FB_HEIGHT] __attribute__((aligned(4)));

// ---------------------------------------------------------------------------
// Pixels
// ---------------------------------------------------------------------------

// 3 (or 2) bits spread back over 8, so white stays white: a table, because
// the Pico's Cortex-M0+ has no divide instruction and "* 255 / 7" would call one
static const uint8_t spread3[8] = {0, 36, 72, 109, 145, 182, 218, 255};
static const uint8_t spread2[4] = {0, 85, 170, 255};

Color ShimUnpack(uint8_t p)
{
	return (Color){spread3[(p >> 5) & 7], spread3[(p >> 2) & 7], spread2[p & 3], 255};
}

// x / 255 without dividing, exact for anything a blend can make (0 to 65025)
static inline int Div255(int x)
{
	return (x + 1 + (x >> 8)) >> 8;
}

// c over d, by c's alpha
static inline uint8_t Blend(Color c, uint8_t under)
{
	Color d = ShimUnpack(under);
	int a = c.a, b = 255 - a;
	d.r = (unsigned char)Div255(c.r * a + d.r * b);
	d.g = (unsigned char)Div255(c.g * a + d.g * b);
	d.b = (unsigned char)Div255(c.b * a + d.b * b);
	return ShimPack(d);
}

// World units to framebuffer pixels
static inline int ToScreenX(float x) { return shimOffsetX + (int)floorf(x * shimScale); }
static inline int ToScreenY(float y) { return shimOffsetY + (int)floorf(y * shimScale); }

static inline void PutPixel(int x, int y, Color c)
{
	if (x < 0 || y < 0 || x >= SHIM_FB_WIDTH || y >= SHIM_FB_HEIGHT || c.a == 0)
		return;

	uint8_t *p = &shimFramebuffer[y * SHIM_FB_WIDTH + x];

	if (c.a >= 250)
	{
		*p = ShimPack(c);
		return;
	}

	// See-through: mix with what's already there
	*p = Blend(c, *p);
}

// See-through colours: what each of the 256 framebuffer colours becomes under
// colour c. Worked out once per shape, so each pixel is then just a lookup.
static void BlendTable(Color c, uint8_t table[256])
{
	for (int p = 0; p < 256; p++)
		table[p] = Blend(c, (uint8_t)p);
}

// One row of pixels, x0 up to (not including) x1. table: NULL for a solid colour
static inline void FillSpan(int y, int x0, int x1, uint8_t solid, const uint8_t *table)
{
	if (y < 0 || y >= SHIM_FB_HEIGHT)
		return;
	if (x0 < 0)
		x0 = 0;
	if (x1 > SHIM_FB_WIDTH)
		x1 = SHIM_FB_WIDTH;
	if (x0 >= x1)
		return;

	uint8_t *p = &shimFramebuffer[y * SHIM_FB_WIDTH + x0];
	if (table)
		for (int n = x1 - x0; n > 0; n--, p++)
			*p = table[*p];
	else
		memset(p, solid, (size_t)(x1 - x0));
}

// Does c have to be mixed with what's underneath?
static bool NeedsBlend(Color c)
{
	return c.a < 250;
}

static void FillRect(int x0, int y0, int x1, int y1, Color c)
{
	if (c.a == 0)
		return;

	// Tiny and see-through (a pixel of shadowed text): mixing each pixel is
	// quicker than working out a whole table
	if (NeedsBlend(c) && (x1 - x0) * (y1 - y0) < 256)
	{
		for (int y = y0; y < y1; y++)
			for (int x = x0; x < x1; x++)
				PutPixel(x, y, c);
		return;
	}

	uint8_t table[256];
	if (NeedsBlend(c))
		BlendTable(c, table);

	for (int y = y0; y < y1; y++)
		FillSpan(y, x0, x1, ShimPack(c), NeedsBlend(c) ? table : NULL);
}

// A world rectangle in framebuffer pixels: at least one pixel each way, so
// thin things (a 10 unit health bar on a tiny screen) don't vanish
static void ScreenRect(float x, float y, float w, float h, int *x0, int *y0, int *x1, int *y1)
{
	*x0 = ToScreenX(x);
	*y0 = ToScreenY(y);
	*x1 = ToScreenX(x + w);
	*y1 = ToScreenY(y + h);
	if (w > 0 && *x1 <= *x0)
		*x1 = *x0 + 1;
	if (h > 0 && *y1 <= *y0)
		*y1 = *y0 + 1;
}

// ---------------------------------------------------------------------------
// Colours
// ---------------------------------------------------------------------------

Color ColorAlpha(Color color, float alpha)
{
	if (alpha < 0.0f)
		alpha = 0.0f;
	if (alpha > 1.0f)
		alpha = 1.0f;
	color.a = (unsigned char)(alpha * 255.0f);
	return color;
}

Color Fade(Color color, float alpha)
{
	return ColorAlpha(color, alpha);
}

// ---------------------------------------------------------------------------
// Shapes
// ---------------------------------------------------------------------------

void ClearBackground(Color color)
{
	memset(shimFramebuffer, ShimPack(color), sizeof(shimFramebuffer));
}

void DrawRectangle(int posX, int posY, int width, int height, Color color)
{
	int x0, y0, x1, y1;
	ScreenRect((float)posX, (float)posY, (float)width, (float)height, &x0, &y0, &x1, &y1);
	FillRect(x0, y0, x1, y1, color);
}

// Is (px, py) inside a rectangle with corners rounded by radius r? (pixels)
// Which part of row y is inside a rounded rectangle (x0, y0)-(x1, y1) with
// corner radius r. Only the corner rows need the circle, once per row.
static bool RoundedSpan(int y, int x0, int y0, int x1, int y1, float r, int *left, int *right)
{
	if (y < y0 || y >= y1)
		return false;

	float py = y + 0.5f, dy = 0.0f;
	if (py < y0 + r)
		dy = y0 + r - py;
	else if (py > y1 - r)
		dy = py - (y1 - r);

	int inset = 0;
	if (dy > 0.0f)
	{
		if (dy > r)
			return false;
		inset = (int)(r - sqrtf(r * r - dy * dy) + 0.5f);
	}

	*left = x0 + inset;
	*right = x1 - inset;
	return *left < *right;
}

static float CornerRadius(float roundness, int x0, int y0, int x1, int y1)
{
	float shorter = (float)((x1 - x0) < (y1 - y0) ? (x1 - x0) : (y1 - y0));
	return roundness * shorter * 0.5f;
}

void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color)
{
	(void)segments;
	if (color.a == 0)
		return;

	int x0, y0, x1, y1;
	ScreenRect(rec.x, rec.y, rec.width, rec.height, &x0, &y0, &x1, &y1);
	float r = CornerRadius(roundness, x0, y0, x1, y1);

	uint8_t table[256];
	if (NeedsBlend(color))
		BlendTable(color, table);

	for (int y = y0; y < y1; y++)
	{
		int left, right;
		if (RoundedSpan(y, x0, y0, x1, y1, r, &left, &right))
			FillSpan(y, left, right, ShimPack(color), NeedsBlend(color) ? table : NULL);
	}
}

void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float lineThick, Color color)
{
	(void)segments;
	if (color.a == 0)
		return;

	int x0, y0, x1, y1;
	ScreenRect(rec.x, rec.y, rec.width, rec.height, &x0, &y0, &x1, &y1);
	float r = CornerRadius(roundness, x0, y0, x1, y1);

	int t = (int)(lineThick * shimScale + 0.5f);
	if (t < 1)
		t = 1;
	float ri = (r - t > 0.0f) ? r - t : 0.0f;

	uint8_t table[256];
	if (NeedsBlend(color))
		BlendTable(color, table);
	const uint8_t *blend = NeedsBlend(color) ? table : NULL;

	// The border: each row of the outer shape, less the inner shape's row
	for (int y = y0; y < y1; y++)
	{
		int left, right, innerLeft, innerRight;
		if (!RoundedSpan(y, x0, y0, x1, y1, r, &left, &right))
			continue;

		if (RoundedSpan(y, x0 + t, y0 + t, x1 - t, y1 - t, ri, &innerLeft, &innerRight))
		{
			FillSpan(y, left, innerLeft, ShimPack(color), blend);
			FillSpan(y, innerRight, right, ShimPack(color), blend);
		}
		else
			FillSpan(y, left, right, ShimPack(color), blend);
	}
}

void DrawRing(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color)
{
	(void)segments;

	float cx = shimOffsetX + center.x * shimScale, cy = shimOffsetY + center.y * shimScale;
	float ox = outerRadius * shimScale, oy = ox;
	float ix = innerRadius * shimScale, iy = ix;

	// At least a pixel thick
	if (ox - ix < 1.0f)
		ix = ox - 1.0f;
	if (oy - iy < 1.0f)
		iy = oy - 1.0f;

	bool fullCircle = (endAngle - startAngle) >= 360.0f;

	// 1 / radius squared, once: multiplying is far cheaper than dividing
	// on a chip with no floating point hardware
	float kox = 1.0f / (ox * ox), koy = 1.0f / (oy * oy);
	bool hasInner = (ix > 0.0f && iy > 0.0f);
	float kix = hasInner ? 1.0f / (ix * ix) : 0.0f, kiy = hasInner ? 1.0f / (iy * iy) : 0.0f;

	for (int y = (int)(cy - oy) - 1; y <= (int)(cy + oy) + 1; y++)
		for (int x = (int)(cx - ox) - 1; x <= (int)(cx + ox) + 1; x++)
		{
			float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
			float dx2 = dx * dx, dy2 = dy * dy;
			float outer = dx2 * kox + dy2 * koy;
			float inner = hasInner ? dx2 * kix + dy2 * kiy : 2.0f;

			if (outer > 1.0f || inner < 1.0f)
				continue;

			if (!fullCircle)
			{
				float angle = atan2f(dy, dx) * RAD2DEG;
				if (angle < 0.0f)
					angle += 360.0f;
				if (angle < startAngle || angle > endAngle)
					continue;
			}

			PutPixel(x, y, color);
		}
}

// ---------------------------------------------------------------------------
// Text: raylib's default font, 10 pixels tall at size 1
// ---------------------------------------------------------------------------

// Whole-number font size for a world font size (raylib's default is 10 tall)
static int FontPixelScale(int fontSize)
{
	int s = (int)(fontSize * shimScale / RL_FONT_HEIGHT + 0.5f);
	return (s < 1) ? 1 : s;
}

static int GlyphIndex(unsigned char c)
{
	return (c >= RL_FONT_FIRST) ? c - RL_FONT_FIRST : '?' - RL_FONT_FIRST;
}

static void DrawGlyph(int index, int x, int y, int scale, Color color)
{
	int gx = rlFontPos[index][0], gy = rlFontPos[index][1], gw = rlFontWidth[index];

	for (int py = 0; py < RL_FONT_HEIGHT; py++)
		for (int px = 0; px < gw; px++)
		{
			int bit = (gy + py) * 128 + gx + px;
			if (rlFontBits[bit / 32] & (1u << (bit % 32)))
				FillRect(x + px * scale, y + py * scale, x + (px + 1) * scale, y + (py + 1) * scale, color);
		}
}

void DrawText(const char *text, int posX, int posY, int fontSize, Color color)
{
	int scale = FontPixelScale(fontSize);
	int x = ToScreenX((float)posX), y = ToScreenY((float)posY);
	int startX = x;

	for (const unsigned char *c = (const unsigned char *)text; *c; c++)
	{
		if (*c == '\n')
		{
			x = startX;
			y += (RL_FONT_HEIGHT + 2) * scale;
			continue;
		}

		int index = GlyphIndex(*c);
		DrawGlyph(index, x, y, scale, color);
		x += (rlFontWidth[index] + 1) * scale; // 1 pixel between letters, like raylib
	}
}

int MeasureText(const char *text, int fontSize)
{
	int scale = FontPixelScale(fontSize);
	int width = 0, widest = 0;

	for (const unsigned char *c = (const unsigned char *)text; *c; c++)
	{
		if (*c == '\n')
		{
			widest = (width > widest) ? width : widest;
			width = 0;
			continue;
		}
		if (width > 0)
			width += scale; // The gap
		width += rlFontWidth[GlyphIndex(*c)] * scale;
	}
	widest = (width > widest) ? width : widest;

	// Back to world units, so the game's layout maths still works
	return (int)ceilf(widest / shimScale);
}

// ---------------------------------------------------------------------------
// Images (RGBA in RAM) and textures
// ---------------------------------------------------------------------------

Image GenImagePerlinNoise(int width, int height, int offsetX, int offsetY, float scale)
{
	// A board can't hold a big image, so the pixels are stored smaller
	// (SHIM_IMAGE_SCALE of the screen) but sampled from the same noise as the
	// desktop. width/height are the stored size, so code that loops over
	// image.width * image.height stays inside the image.
	int dataWidth = (int)(width * shimScale * SHIM_IMAGE_SCALE + 0.5f);
	int dataHeight = (int)(height * shimScale * SHIM_IMAGE_SCALE + 0.5f);
	if (dataWidth < 1)
		dataWidth = 1;
	if (dataHeight < 1)
		dataHeight = 1;

	Color *pixels = (Color *)malloc((size_t)dataWidth * (size_t)dataHeight * sizeof(Color));
	Image image = {pixels, dataWidth, dataHeight, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, width, height};
	if (!pixels)
	{
		TraceLog(LOG_ERROR, "SHIM: No room for a %i x %i image", dataWidth, dataHeight);
		image.width = image.height = 0;
		return image;
	}

	float aspectRatio = (float)width / (float)height;

	for (int y = 0; y < dataHeight; y++)
		for (int x = 0; x < dataWidth; x++)
		{
			// Same maths as raylib's GenImagePerlinNoise, in world pixels
			float wx = (x + 0.5f) * width / dataWidth;
			float wy = (y + 0.5f) * height / dataHeight;
			float nx = (wx + offsetX) * (scale / (float)width);
			float ny = (wy + offsetY) * (scale / (float)height);
			if (width > height)
				nx *= aspectRatio;
			else
				ny /= aspectRatio;

			float p = stb_perlin_fbm_noise3(nx, ny, 1.0f, 2.0f, 0.5f, 6);
			if (p < -1.0f)
				p = -1.0f;
			if (p > 1.0f)
				p = 1.0f;

			unsigned char intensity = (unsigned char)((p + 1.0f) * 0.5f * 255.0f);
			pixels[y * dataWidth + x] = (Color){intensity, intensity, intensity, 255};
		}

	return image;
}

void ImageFormat(Image *image, int newFormat)
{
	(void)image;
	(void)newFormat; // Images are always RGBA here
}

void UnloadImage(Image image)
{
	free(image.data);
}

// Texture ids: which kind, so UnloadTexture knows what to free
#define TEXTURE_BUILT_IN 1
#define TEXTURE_FROM_IMAGE 2

Texture2D LoadTexture(const char *fileName)
{
	Texture2D texture = {0};
	const ShimTexture *data = (const ShimTexture *)ShimFindAsset(fileName, SHIM_ASSET_TEXTURE);
	if (data)
	{
		texture.id = TEXTURE_BUILT_IN;
		texture.width = data->worldWidth;
		texture.height = data->worldHeight;
		texture.shim = data;
		TraceLog(LOG_INFO, "TEXTURE: [%s] built in (%i x %i, stored %i x %i)", fileName,
				 data->worldWidth, data->worldHeight, data->dataWidth, data->dataHeight);
	}
	return texture;
}

Texture2D LoadTextureFromImage(Image image)
{
	Texture2D texture = {0};
	if (!image.data)
		return texture;

	// The texture keeps its own copy of the pixels, like raylib (which copies to the GPU)
	size_t bytes = (size_t)image.width * (size_t)image.height * 4;
	ShimTexture *data = (ShimTexture *)malloc(sizeof(ShimTexture) + bytes);
	if (!data)
	{
		TraceLog(LOG_ERROR, "SHIM: No room for a texture");
		return texture;
	}

	uint8_t *pixels = (uint8_t *)(data + 1);
	memcpy(pixels, image.data, bytes);

	*data = (ShimTexture){SHIM_TEX_RGBA, pixels, NULL, image.width, image.height,
						  image.worldWidth, image.worldHeight, false};

	texture.id = TEXTURE_FROM_IMAGE;
	texture.width = image.worldWidth; // World size, as on the desktop
	texture.height = image.worldHeight;
	texture.shim = data;
	return texture;
}

void UnloadTexture(Texture2D texture)
{
	if (texture.id == TEXTURE_FROM_IMAGE)
		free((void *)texture.shim); // Built-in ones live in flash: nothing to free
}

// One stored pixel as a Color (alpha 0 = transparent)
static inline Color TexturePixel(const ShimTexture *t, int x, int y)
{
	if (t->format == SHIM_TEX_RGBA)
	{
		const uint8_t *p = t->pixels + ((size_t)y * (size_t)t->dataWidth + (size_t)x) * 4;
		return (Color){p[0], p[1], p[2], p[3]};
	}

	// PAL4: two pixels per byte, rows padded to a whole byte
	int rowBytes = (t->dataWidth + 1) / 2;
	uint8_t byte = t->pixels[y * rowBytes + x / 2];
	uint8_t index = (x & 1) ? (byte & 0x0F) : (byte >> 4);
	const uint8_t *c = t->palette + index * 4;
	return (Color){c[0], c[1], c[2], c[3]};
}

static inline Color Tint(Color c, Color tint)
{
	if (tint.r == 255 && tint.g == 255 && tint.b == 255 && tint.a == 255)
		return c;
	return (Color){(unsigned char)(c.r * tint.r / 255), (unsigned char)(c.g * tint.g / 255),
				   (unsigned char)(c.b * tint.b / 255), (unsigned char)(c.a * tint.a / 255)};
}

void DrawTextureRec(Texture2D texture, Rectangle source, Vector2 position, Color tint)
{
	const ShimTexture *t = (const ShimTexture *)texture.shim;
	if (!t)
		return;

	// The source rectangle is in the original image's pixels (as on the
	// desktop); this is where it is in the stored pixels
	float toDataX = (float)t->dataWidth / (float)t->worldWidth;
	float toDataY = (float)t->dataHeight / (float)t->worldHeight;
	int sx0 = (int)(source.x * toDataX + 0.5f), sy0 = (int)(source.y * toDataY + 0.5f);
	int sw = (int)(source.width * toDataX + 0.5f), sh = (int)(source.height * toDataY + 0.5f);

	if (t->sprite)
	{
		// Sprites: the size the game's scale makes them, centred where the
		// desktop would centre them. convert_assets.py stored them at that
		// size, so usually it's pixel for pixel; if not, nearest neighbour.
		int tw = (int)(source.width * shimScale + 0.5f), th = (int)(source.height * shimScale + 0.5f);
		if (abs(tw - sw) <= 1 && abs(th - sh) <= 1)
		{
			tw = sw;
			th = sh;
		}
		if (tw < 1 || th < 1 || sw < 1 || sh < 1)
			return;

		int cx = ToScreenX(position.x + source.width / 2.0f);
		int cy = ToScreenY(position.y + source.height / 2.0f);
		int dx0 = cx - tw / 2, dy0 = cy - th / 2;

		for (int y = 0; y < th; y++)
		{
			int sy = sy0 + ((th == sh) ? y : y * sh / th);
			for (int x = 0; x < tw; x++)
			{
				int sx = sx0 + ((tw == sw) ? x : x * sw / tw);
				Color c = TexturePixel(t, sx, sy);
				if (c.a == 0)
					continue;
				PutPixel(dx0 + x, dy0 + y, Tint(c, tint));
			}
		}
		return;
	}

	// Everything else: stretched to fill its world rectangle
	int x0, y0, x1, y1;
	ScreenRect(position.x, position.y, source.width, source.height, &x0, &y0, &x1, &y1);
	int dw = x1 - x0, dh = y1 - y0;
	if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
		return;

	// Which source column each screen column comes from: once, not per pixel
	static int16_t columnFrom[SHIM_FB_WIDTH];
	for (int x = x0; x < x1; x++)
		if (x >= 0 && x < SHIM_FB_WIDTH)
			columnFrom[x] = (int16_t)(sx0 + (x - x0) * sw / dw);

	bool plain = (tint.r == 255 && tint.g == 255 && tint.b == 255 && tint.a == 255);
	int cx0 = (x0 < 0) ? 0 : x0, cx1 = (x1 > SHIM_FB_WIDTH) ? SHIM_FB_WIDTH : x1;

	for (int y = y0; y < y1; y++)
	{
		if (y < 0 || y >= SHIM_FB_HEIGHT)
			continue;
		int sy = sy0 + (y - y0) * sh / dh;

		if (plain && t->format == SHIM_TEX_RGBA)
		{
			// The usual case (the background). Each stored pixel covers a few
			// screen pixels, usually over the same colour, so the last mix is
			// kept and reused while neither changes.
			const uint8_t *row = t->pixels + (size_t)sy * (size_t)t->dataWidth * 4;
			uint8_t *out = &shimFramebuffer[y * SHIM_FB_WIDTH];
			const uint8_t *lastPixel = NULL;
			uint8_t lastUnder = 0, lastResult = 0;

			for (int x = cx0; x < cx1; x++)
			{
				const uint8_t *p = row + columnFrom[x] * 4;
				if (p[3] == 0)
					continue;
				if (p != lastPixel || out[x] != lastUnder)
				{
					lastPixel = p;
					lastUnder = out[x];
					lastResult = (p[3] >= 250) ? ShimPack((Color){p[0], p[1], p[2], 255})
											   : Blend((Color){p[0], p[1], p[2], p[3]}, out[x]);
				}
				out[x] = lastResult;
			}
			continue;
		}

		for (int x = cx0; x < cx1; x++)
			PutPixel(x, y, Tint(TexturePixel(t, columnFrom[x], sy), tint));
	}
}

void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
	DrawTextureRec(texture, (Rectangle){0, 0, (float)texture.width, (float)texture.height},
				   (Vector2){(float)posX, (float)posY}, tint);
}
