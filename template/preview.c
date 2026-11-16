/* PC tool: checks tap handling, then writes each screen and the icon as PNG.
 * Usage: preview FONT_DIR OUT_DIR */
#include "ui.h"
#include <assert.h>
#include <stdio.h>

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
	char path[256];
	assert(argc == 3 && ui_fonts(argv[1]) == 0);

	Ui ui = { 0 };
	shot(&ui, argv[2], "start");
	assert(ui_tap(&ui, 636, 1000) == A_NONE && ui.count == 1);  /* the big button counts */
	assert(ui_tap(&ui, 636, 400) == A_NONE && ui.count == 1);   /* the number is not a button */
	assert(ui_tap(&ui, 1100, 80) == A_CLOSE);
	shot(&ui, argv[2], "one");

	ui_icon(px);
	snprintf(path, sizeof path, "%s/icon.png", argv[2]);
	assert(stbi_write_png(path, 300, 450, 1, px, 300));
	puts("preview: all checks passed");
	return 0;
}
