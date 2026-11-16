/* ktap X Y [X Y ...]: taps the Kindle touch screen at screen pixels X,Y, as a finger would.
 * For tests over SSH. It writes touch events into the touch device.
 * shortcut: one finger, taps only (no swipe); add moves between down and up when an app needs them. */
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int fd = -1;

static void emit(int type, int code, int value)
{
	struct input_event ev;
	memset(&ev, 0, sizeof ev);
	ev.type = type; ev.code = code; ev.value = value;
	if (write(fd, &ev, sizeof ev) != sizeof ev) { perror("ktap: write"); exit(1); }
}

int main(int argc, char **argv)
{
	struct input_absinfo ax, ay;
	if (argc < 3 || argc % 2 != 1) { fprintf(stderr, "usage: ktap X Y [X Y ...]\n"); return 2; }
	for (int i = 0; i < 16 && fd < 0; i++) {
		char path[32];
		snprintf(path, sizeof path, "/dev/input/event%d", i);
		fd = open(path, O_RDWR);
		if (fd >= 0 && (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ax) < 0 || ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ay) < 0
		                || ax.maximum <= 0 || ay.maximum <= 0)) { close(fd); fd = -1; }
	}
	if (fd < 0) { fprintf(stderr, "ktap: no touch screen found\n"); return 1; }

	for (int i = 1; i < argc; i += 2) {
		int x = atoi(argv[i]), y = atoi(argv[i + 1]);
		if (x < 0 || y < 0 || x > ax.maximum || y > ay.maximum) { fprintf(stderr, "ktap: %d,%d is off the screen\n", x, y); return 2; }
		emit(EV_ABS, ABS_MT_SLOT, 0);
		emit(EV_ABS, ABS_MT_TRACKING_ID, i);
		emit(EV_ABS, ABS_MT_POSITION_X, x);
		emit(EV_ABS, ABS_MT_POSITION_Y, y);
		emit(EV_KEY, BTN_TOUCH, 1);
		emit(EV_SYN, SYN_REPORT, 0);
		usleep(80000);
		emit(EV_ABS, ABS_MT_TRACKING_ID, -1);
		emit(EV_KEY, BTN_TOUCH, 0);
		emit(EV_SYN, SYN_REPORT, 0);
		usleep(500000); /* apps ignore a second tap that comes too fast */
	}
	return 0;
}
