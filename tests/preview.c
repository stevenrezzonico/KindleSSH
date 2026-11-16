/* PC tool: checks tap handling, then writes each screen and the icon as PNG.
 * Usage: preview FONT_DIR OUT_DIR */
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static unsigned char px[UI_W * UI_H];

static void shot(const Ui *ui, const char *dir, const char *name)
{
	char path[256];
	snprintf(path, sizeof path, "%s/%s.png", dir, name);
	ui_render(ui, px);
	assert(stbi_write_png(path, UI_W, UI_H, 1, px, UI_W));
}

int main(int argc, char **argv)
{
	assert(argc == 3 && ui_fonts(argv[1]) == 0);

	assert(ui_port("22") == 22 && ui_port("65535") == 65535 && ui_port("1") == 1);
	assert(!ui_port("") && !ui_port("0") && !ui_port("65536") && !ui_port("99999") && !ui_port("2a"));

	Ui ui = { .port = 22 };
	strcpy(ui.ip, "192.168.1.23");
	assert(ui_tap(&ui, 1100, 80) == A_CLOSE);
	assert(ui_tap(&ui, 636, 1060) == A_SSH);
	assert(ui_tap(&ui, 636, 1235) == A_USB);
	assert(ui_tap(&ui, 636, 500) == A_NONE && !ui.keypad);  /* the card is not a button */
	shot(&ui, argv[2], "off");

	assert(ui_tap(&ui, 900, 1420) == A_NONE && ui.keypad);  /* CHANGE PORT opens the keypad */
	ui_tap(&ui, 360, 615);   /* 1 */
	ui_tap(&ui, 360, 615);   /* 1 */
	ui_tap(&ui, 360, 1260);  /* DEL */
	assert(!strcmp(ui.entry, "1"));
	ui_tap(&ui, 360, 1260);
	ui_tap(&ui, 910, 1260);  /* OK with nothing typed */
	assert(ui.keypad && ui.entry_bad);
	shot(&ui, argv[2], "keypad-bad");
	ui_tap(&ui, 635, 615); ui_tap(&ui, 635, 615); ui_tap(&ui, 635, 615); ui_tap(&ui, 635, 615); /* 2222 */
	assert(!strcmp(ui.entry, "2222") && !ui.entry_bad);
	shot(&ui, argv[2], "keypad");
	ui_tap(&ui, 910, 1260);
	assert(!ui.keypad && ui.port == 2222);

	ui.keypad = 1;
	assert(ui_tap(&ui, 1100, 80) == A_NONE && !ui.keypad);  /* CANCEL */

	ui.busy = A_SSH;
	shot(&ui, argv[2], "busy");
	ui.busy = 0; ui.ssh_on = 1;
	strcpy(ui.password, "hT7mKq3w");
	assert(ui_tap(&ui, 900, 1420) == A_NONE && !ui.keypad); /* port is locked while SSH is on */
	shot(&ui, argv[2], "on");
	ui.ip[0] = 0;
	strcpy(ui.usb, "169.254.15.244");
	shot(&ui, argv[2], "on-usb");

	static unsigned char icon[300 * 450];
	char path[256];
	ui_icon(icon);
	snprintf(path, sizeof path, "%s/icon.png", argv[2]);
	assert(stbi_write_png(path, 300, 450, 1, icon, 300));
	puts("ok   ui taps, port check, screenshots");
	return 0;
}
