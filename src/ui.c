#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

typedef struct { int x, y, w, h; } Rect;

enum { BLACK = 0, DARK = 0x50, GRAY = 0x80, LIGHT = 0xB8, PALE = 0xE6, WHITE = 0xFF };

static stbtt_fontinfo sans, mono;
static unsigned char *P;          /* buffer being drawn */
static int BW = UI_W, BH = UI_H;  /* its size */

/* ---- layout: one place, used by drawing and by tap handling ---- */
#define M 64
static const Rect R_TOPBTN = { UI_W - M - 240, 34, 240, 92 };          /* CLOSE / CANCEL */
static const Rect R_CARD   = { M, 210, UI_W - 2 * M, 710 };
static const Rect R_MAIN   = { M, 960, UI_W - 2 * M, 170 };
static const Rect R_USB    = { M, 1160, UI_W - 2 * M, 150 };
static const Rect R_PORT   = { M, 1340, UI_W - 2 * M, 150 };
static const Rect R_ENTRY  = { 236, 230, 800, 190 };
static const char KEYS[12][4] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "DEL", "0", "OK" };
static Rect key_rect(int i) { Rect r = { 236 + (i % 3) * 275, 520 + (i / 3) * 215, 250, 190 }; return r; }

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

enum { FILLED, OUTLINE, DISABLED };
static void button(Rect r, int style, float px, const char *label, const char *sub)
{
	int fg = style == FILLED ? WHITE : style == OUTLINE ? BLACK : LIGHT;
	if (style == FILLED) rrect(r, 28, BLACK);
	else frame(r, 28, style == OUTLINE ? 6 : 4, fg, WHITE);
	text_mid(&sans, px, r, sub ? -22 : 0, label, fg, 2);
	if (sub) text_mid(&sans, 32, r, 40, sub, style == DISABLED ? LIGHT : GRAY, 0);
}

static void header(const char *title, const char *btn)
{
	Rect bar = { 0, 0, UI_W, 160 };
	rrect(bar, 0, BLACK);
	text(&sans, 72, M, 106, title, WHITE, 0);
	frame(R_TOPBTN, 22, 4, WHITE, BLACK);
	text_mid(&sans, 40, R_TOPBTN, 0, btn, WHITE, 2);
}

static void row(int base, const char *label, const char *value, int color)
{
	text(&sans, 34, R_CARD.x + 56, base - 6, label, GRAY, 3);
	text(&mono, 60, R_CARD.x + 380, base, value, color, 0);
}

static void render_main(const Ui *ui)
{
	char buf[96];
	const Rect c = R_CARD;
	header("KindleSSH", "CLOSE");

	frame(c, 34, 5, BLACK, WHITE);
	Rect dot = { c.x + 60, c.y + 62, 64, 64 };
	if (ui->ssh_on) rrect(dot, 32, BLACK); else frame(dot, 32, 7, GRAY, WHITE);
	text(&sans, 84, c.x + 156, c.y + 124, ui->ssh_on ? "SSH is on" : "SSH is off", ui->ssh_on ? BLACK : GRAY, 0);
	Rect line = { c.x + 40, c.y + 186, c.w - 80, 3 };
	rrect(line, 0, LIGHT);

	row(c.y + 280, "WI-FI", ui->ip[0] ? ui->ip : "no network", ui->ip[0] ? BLACK : GRAY);
	row(c.y + 380, "USB", ui->usb[0] ? ui->usb : "off", ui->usb[0] ? BLACK : GRAY);
	snprintf(buf, sizeof buf, "%d", ui->port);
	row(c.y + 480, "PORT", buf, BLACK);

	Rect pw = { c.x + 40, c.y + 530, c.w - 80, 150 };
	rrect(pw, 24, PALE);
	text(&sans, 34, pw.x + 16, pw.y + 86, "PASSWORD", DARK, 3);
	Rect pwv = { pw.x + 230, pw.y, pw.w - 230, pw.h };
	if (ui->ssh_on) text_mid(&mono, 104, pwv, 0, ui->password, BLACK, 10);
	else text_mid(&sans, 40, pwv, 0, "made new at every TURN SSH ON", GRAY, 0);

	if (ui->busy == A_USB) button(R_USB, DISABLED, 46, "PLEASE WAIT", "this takes some seconds");
	else if (ui->usb[0]) button(R_USB, OUTLINE, 46, "TURN USB NETWORK OFF", "now: USB is a network link, no file transfer");
	else button(R_USB, OUTLINE, 46, "TURN USB NETWORK ON", "unplug the cable first, plug it in after");

	if (ui->busy == A_SSH) button(R_MAIN, DISABLED, 64, "PLEASE WAIT", NULL);
	else if (ui->ssh_on) button(R_MAIN, OUTLINE, 64, "TURN SSH OFF", NULL);
	else button(R_MAIN, FILLED, 64, "TURN SSH ON", NULL);

	snprintf(buf, sizeof buf, "now: %d", ui->port);
	button(R_PORT, ui->ssh_on ? DISABLED : OUTLINE, 46, "CHANGE PORT", ui->ssh_on ? "turn SSH off first" : buf);

	Rect foot = { 0, 1522, UI_W, 60 };
	const char *host = ui->ip[0] ? ui->ip : ui->usb;
	if (ui->ssh_on && host[0]) {
		if (ui->port == 22) snprintf(buf, sizeof buf, "ssh root@%s", host);
		else snprintf(buf, sizeof buf, "ssh -p %d root@%s", ui->port, host);
		text_mid(&mono, 44, foot, 0, buf, BLACK, 0);
	}
	foot.y += 62;
	text_mid(&sans, 32, foot, 0, "The server stays on after CLOSE, until TURN SSH OFF or a restart.", GRAY, 0);
	foot.y += 46;
	text_mid(&sans, 32, foot, 0, "The Kindle does not sleep while SSH or USB network is on.", GRAY, 0);
}

static void render_keypad(const Ui *ui)
{
	header("Port", "CANCEL");
	frame(R_ENTRY, 28, 6, BLACK, WHITE);
	text_mid(&mono, 120, R_ENTRY, 0, ui->entry, BLACK, 8);
	Rect hint = { 0, R_ENTRY.y + R_ENTRY.h + 10, UI_W, 60 };
	text_mid(&sans, 36, hint, 0, ui->entry_bad ? "Not a port. Type a number from 1 to 65535." : "A number from 1 to 65535",
	         ui->entry_bad ? BLACK : GRAY, 0);
	for (int i = 0; i < 12; i++)
		button(key_rect(i), i == 11 ? FILLED : OUTLINE, i == 9 || i == 11 ? 56 : 92, KEYS[i], NULL);
}

void ui_render(const Ui *ui, unsigned char *px)
{
	P = px; BW = UI_W; BH = UI_H;
	memset(px, WHITE, UI_W * UI_H);
	if (ui->keypad) render_keypad(ui); else render_main(ui);
}

void ui_icon(unsigned char *px)
{
	P = px; BW = 300; BH = 450;
	memset(px, WHITE, 300 * 450);
	Rect f = { 24, 60, 252, 330 }, bar = { 24, 60, 252, 86 }, sq = { 24, 110, 252, 36 }, body = { 0, 146, 300, 180 }, cap = { 0, 300, 300, 60 };
	frame(f, 30, 10, BLACK, WHITE);
	rrect(bar, 30, BLACK);
	rrect(sq, 0, BLACK); /* square off the bar's lower corners */
	for (int i = 0; i < 3; i++) { Rect d = { 50 + i * 34, 82, 20, 20 }; rrect(d, 10, WHITE); }
	text_mid(&sans, 112, body, 0, "SSH", BLACK, 2);
	text_mid(&mono, 26, cap, 0, "root@kindle", GRAY, 0);
}

int ui_port(const char *digits)
{
	if (!*digits || strlen(digits) > 5 || strspn(digits, "0123456789") != strlen(digits)) return 0;
	int v = atoi(digits);
	return v >= 1 && v <= 65535 ? v : 0;
}

int ui_tap(Ui *ui, int x, int y)
{
	if (!ui->keypad) {
		if (inside(R_TOPBTN, x, y)) return A_CLOSE;
		if (inside(R_MAIN, x, y)) return A_SSH;
		if (inside(R_USB, x, y)) return A_USB;
		if (inside(R_PORT, x, y) && !ui->ssh_on) { ui->keypad = 1; ui->entry[0] = 0; ui->entry_bad = 0; }
		return A_NONE;
	}
	if (inside(R_TOPBTN, x, y)) { ui->keypad = 0; return A_NONE; }
	for (int i = 0; i < 12; i++) {
		if (!inside(key_rect(i), x, y)) continue;
		size_t n = strlen(ui->entry);
		ui->entry_bad = 0;
		if (i == 9) { if (n) ui->entry[n - 1] = 0; }
		else if (i == 11) { int p = ui_port(ui->entry); if (p) { ui->port = p; ui->keypad = 0; } else ui->entry_bad = 1; }
		else if (n < 5) { ui->entry[n] = KEYS[i][0]; ui->entry[n + 1] = 0; }
	}
	return A_NONE;
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

int ui_fonts(const char *dir)
{
	return load(&sans, dir, "Inter-Bold.otf") || load(&mono, dir, "JetBrainsMonoNL-Bold.ttf") ? -1 : 0;
}
