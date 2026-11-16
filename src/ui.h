/* KindleSSH screen: state, drawing into an 8-bit gray buffer, and tap handling. No device code here. */
#ifndef KSSH_UI_H
#define KSSH_UI_H

#define UI_W 1272
#define UI_H 1696

typedef struct {
	int  ssh_on, port;
	char password[16];
	char ip[48];     /* Wi-Fi address, "" = no network */
	char usb[48];    /* USB network address, "" = off */
	int  busy;       /* A_SSH or A_USB: that button shows PLEASE WAIT */
	int  keypad;     /* 1 = port keypad is shown */
	char entry[6];   /* digits typed on the keypad */
	int  entry_bad;  /* last OK was not a valid port */
} Ui;

enum { A_NONE, A_SSH, A_USB, A_CLOSE };

int  ui_fonts(const char *dir);                  /* loads the two fonts; 0 = ok */
void ui_render(const Ui *ui, unsigned char *px); /* px: UI_W * UI_H bytes */
int  ui_tap(Ui *ui, int x, int y);               /* returns an A_ action for the caller */
int  ui_port(const char *digits);                /* 1..65535, or 0 if not a valid port */
void ui_icon(unsigned char *px);                 /* px: 300 * 450 bytes */

#endif
