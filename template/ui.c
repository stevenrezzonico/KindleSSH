/* Example screen: a counter with one button. Replace the state, the layout and the drawing with your own.
 * The drawing helpers (rrect, frame, text, text_mid) are all you need for a clean e-ink UI. */
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

typedef struct { int x, y, w, h; } Rect;

enum { BLACK = 0, GRAY = 0x80, LIGHT = 0xB8, WHITE = 0xFF };

static stbtt_fontinfo sans;
static unsigned char *P;          /* buffer being drawn */
static int BW = UI_W, BH = UI_H;  /* its size */

/* ---- layout: one place, used by drawing and by tap handling ---- */
#define M 64
static const Rect R_CLOSE = { UI_W - M - 240, 34, 240, 92 };
static const Rect R_COUNT = { M, 210, UI_W - 2 * M, 500 };
static const Rect R_MAIN  = { M, 900, UI_W - 2 * M, 200 };

static int inside(Rect r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

/* ---- drawing ---- */
static void blend(int x, int y, int color, float a)
{
	if (x < 0 || y < 0 || x >= BW || y >= BH || a <= 0) return;
	unsigned char *d = &P[y * BW + x];
	*d = (unsigned char) (*d + (color - *d) * (a > 1 ? 1 : a));
}

/* Filled rectangle with rounded, anti-aliased corners. rad 0 = plain rectangle. */
static void rrect(Rect r, int rad, int color)
{
	for (int y = r.y; y < r.y + r.h; y++)
		for (int x = r.x; x < r.x + r.w; x++) {
			float dx = fmaxf(fmaxf(r.x + rad - x - 0.5f, x + 0.5f - (r.x + r.w - rad)), 0);
			float dy = fmaxf(fmaxf(r.y + rad - y - 0.5f, y + 0.5f - (r.y + r.h - rad)), 0);
			blend(x, y, color, dx > 0 && dy > 0 ? rad - sqrtf(dx * dx + dy * dy) + 0.5f : 1);
		}
}

static void frame(Rect r, int rad, int thick, int border, int fill)
{
	Rect in = { r.x + thick, r.y + thick, r.w - 2 * thick, r.h - 2 * thick };
	rrect(r, rad, border);
	rrect(in, rad - thick, fill);
}

static int text_w(const stbtt_fontinfo *f, float px, const char *s, int track)
{
	float sc = stbtt_ScaleForPixelHeight(f, px), w = 0;
	for (; *s; s++) {
		int adv, lsb;
		stbtt_GetCodepointHMetrics(f, *s, &adv, &lsb);
		w += adv * sc + track + (s[1] ? stbtt_GetCodepointKernAdvance(f, s[0], s[1]) * sc : 0);
	}
	return (int) (w - track + 0.5f);
}

/* Draws s with its left edge at x and its baseline at base. */
static void text(const stbtt_fontinfo *f, float px, int x, int base, const char *s, int color, int track)
{
	float sc = stbtt_ScaleForPixelHeight(f, px), pen = x;
	for (; *s; s++) {
		int adv, lsb, w, h, xo, yo;
		unsigned char *g = stbtt_GetCodepointBitmap(f, sc, sc, *s, &w, &h, &xo, &yo);
		for (int j = 0; g && j < h; j++)
			for (int i = 0; i < w; i++)
				blend((int) pen + xo + i, base + yo + j, color, g[j * w + i] / 255.0f);
		stbtt_FreeBitmap(g, NULL);
		stbtt_GetCodepointHMetrics(f, *s, &adv, &lsb);
		pen += adv * sc + track + (s[1] ? stbtt_GetCodepointKernAdvance(f, s[0], s[1]) * sc : 0);
	}
}

/* Centers s in r. dy moves it down. Capital letters are about 0.72 of the pixel height. */
static void text_mid(const stbtt_fontinfo *f, float px, Rect r, int dy, const char *s, int color, int track)
{
	text(f, px, r.x + (r.w - text_w(f, px, s, track)) / 2, r.y + r.h / 2 + (int) (px * 0.36f) + dy, s, color, track);
}

void ui_render(const Ui *ui, unsigned char *px)
{
	char num[16];
	Rect bar = { 0, 0, UI_W, 160 };
	P = px; BW = UI_W; BH = UI_H;
	memset(px, WHITE, UI_W * UI_H);

	rrect(bar, 0, BLACK);
	text(&sans, 72, M, 106, "My App", WHITE, 0);
	frame(R_CLOSE, 22, 4, WHITE, BLACK);
	text_mid(&sans, 40, R_CLOSE, 0, "CLOSE", WHITE, 2);

	frame(R_COUNT, 36, 6, BLACK, WHITE);
	snprintf(num, sizeof num, "%d", ui->count);
	text_mid(&sans, 260, R_COUNT, 0, num, BLACK, 0);

	rrect(R_MAIN, 28, BLACK);
	text_mid(&sans, 64, R_MAIN, 0, "TAP ME", WHITE, 2);
}

int ui_tap(Ui *ui, int x, int y)
{
	if (inside(R_CLOSE, x, y)) return A_CLOSE;
	if (inside(R_MAIN, x, y)) ui->count++;
	return A_NONE;
}

void ui_icon(unsigned char *px)
{
	Rect f = { 24, 60, 252, 330 };
	P = px; BW = 300; BH = 450;
	memset(px, WHITE, 300 * 450);
	frame(f, 30, 10, BLACK, WHITE);
	text_mid(&sans, 90, f, 0, "APP", BLACK, 2);
}

static int load(stbtt_fontinfo *f, const char *dir, const char *name)
{
	char path[256];
	snprintf(path, sizeof path, "%s/%s", dir, name);
	FILE *fp = fopen(path, "rb");
	if (!fp) return -1;
	fseek(fp, 0, SEEK_END);
	long n = ftell(fp);
	rewind(fp);
	unsigned char *data = malloc(n); /* kept for the life of the program: stb reads from it */
	int ok = data && fread(data, 1, n, fp) == (size_t) n && stbtt_InitFont(f, data, 0);
	fclose(fp);
	return ok ? 0 : -1;
}

int ui_fonts(const char *dir) { return load(&sans, dir, "Inter-Bold.otf"); }
