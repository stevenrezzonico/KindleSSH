/* App screen: state, drawing into an 8-bit gray buffer, and tap handling. No device code here,
 * so the same file runs on the PC for tests and screenshots (preview.c). */
#ifndef APP_UI_H
#define APP_UI_H

#define UI_W 1272
#define UI_H 1696

typedef struct {
	int count;  /* example state: replace with your own */
} Ui;

enum { A_NONE, A_CLOSE };

int  ui_fonts(const char *dir);                  /* loads the font; 0 = ok */
void ui_render(const Ui *ui, unsigned char *px); /* px: UI_W * UI_H bytes, 0 = black, 255 = white */
int  ui_tap(Ui *ui, int x, int y);               /* changes the state; returns an A_ action for main.c */
void ui_icon(unsigned char *px);                 /* px: 300 * 450 bytes, the library icon */

#endif
