/* Touches the Kindle screen as one finger would. Coordinates are screen pixels. For tests over SSH.
 *   ktap X Y [X Y ...]              taps, 0.5 s apart
 *   ktap swipe X1 Y1 X2 Y2 [MS]     drag from 1 to 2 in MS milliseconds (default 300)
 *   ktap hold X Y [MS]              long press for MS milliseconds (default 1000)
 * It writes touch events into the touch device. */
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int fd = -1, id;
static struct input_absinfo ax, ay;

static void emit(int type, int code, int value)
{
	struct input_event ev;
	memset(&ev, 0, sizeof ev);
	ev.type = type; ev.code = code; ev.value = value;
	if (write(fd, &ev, sizeof ev) != sizeof ev) { perror("ktap: write"); exit(1); }
}

static int coord(const char *s, int max)
{
	char *end;
	long v = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || v < 0 || v > max) { fprintf(stderr, "ktap: %s is not on the screen (0..%d)\n", s, max); exit(2); }
	return (int)v;
}

/* Finger down at x1,y1, move to x2,y2 in ms milliseconds, finger up. */
static void stroke(int x1, int y1, int x2, int y2, int ms)
{
	int steps = (x1 == x2 && y1 == y2) ? 1 : ms / 20 + 1;
	emit(EV_ABS, ABS_MT_SLOT, 0);
	emit(EV_ABS, ABS_MT_TRACKING_ID, ++id);
	emit(EV_ABS, ABS_MT_POSITION_X, x1);
	emit(EV_ABS, ABS_MT_POSITION_Y, y1);
	emit(EV_KEY, BTN_TOUCH, 1);
	emit(EV_SYN, SYN_REPORT, 0);
	for (int i = 1; i <= steps; i++) {
		usleep(ms * 1000 / steps);
		if (steps == 1) break;
		emit(EV_ABS, ABS_MT_POSITION_X, x1 + (x2 - x1) * i / steps);
		emit(EV_ABS, ABS_MT_POSITION_Y, y1 + (y2 - y1) * i / steps);
		emit(EV_SYN, SYN_REPORT, 0);
	}
	emit(EV_ABS, ABS_MT_TRACKING_ID, -1);
	emit(EV_KEY, BTN_TOUCH, 0);
	emit(EV_SYN, SYN_REPORT, 0);
}

int main(int argc, char **argv)
{
	int swipe = argc > 1 && !strcmp(argv[1], "swipe"), hold = argc > 1 && !strcmp(argv[1], "hold");
	if (swipe ? (argc != 6 && argc != 7) : hold ? (argc != 4 && argc != 5) : (argc < 3 || argc % 2 != 1)) {
		fprintf(stderr, "usage: ktap X Y [X Y ...] | ktap swipe X1 Y1 X2 Y2 [MS] | ktap hold X Y [MS]\n");
		return 2;
	}
	for (int i = 0; i < 16 && fd < 0; i++) {
		char path[32];
		snprintf(path, sizeof path, "/dev/input/event%d", i);
		fd = open(path, O_RDWR);
		if (fd >= 0 && (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ax) < 0 || ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ay) < 0
		                || ax.maximum <= 0 || ay.maximum <= 0)) { close(fd); fd = -1; }
	}
	if (fd < 0) { fprintf(stderr, "ktap: no touch screen found\n"); return 1; }

	if (swipe) {
		stroke(coord(argv[2], ax.maximum), coord(argv[3], ay.maximum), coord(argv[4], ax.maximum), coord(argv[5], ay.maximum),
		       argc == 7 ? coord(argv[6], 60000) : 300);
	} else if (hold) {
		int x = coord(argv[2], ax.maximum), y = coord(argv[3], ay.maximum);
		stroke(x, y, x, y, argc == 5 ? coord(argv[4], 60000) : 1000);
	} else {
		for (int i = 1; i < argc; i += 2) {
			int x = coord(argv[i], ax.maximum), y = coord(argv[i + 1], ay.maximum);
			stroke(x, y, x, y, 80);
			usleep(500000); /* apps ignore a second tap that comes too fast */
		}
	}
	return 0;
}
