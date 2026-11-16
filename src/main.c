/* KindleSSH device side: e-ink output through FBInk, touch input from evdev, sshctl.sh for the work.
 * Everything on stderr goes to log.txt (the launcher redirects it). */
#include "ui.h"
#include "fbink.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/input.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define IDLE_CLOSE_S 300 /* the app closes itself: a dead touch screen must never lock the Kindle UI */

static const char *dir = "/mnt/us/KindleSSH";
static Ui ui;
static unsigned char px[UI_W * UI_H];
static int fbfd = -1;
static FBInkConfig cfg;
static volatile sig_atomic_t quit;

static void on_signal(int s) { (void) s; quit = 1; }

/* Runs sshctl.sh and reads the state it prints. */
static void ctl(const char *args)
{
	char cmd[300], line[128];
	snprintf(cmd, sizeof cmd, "sh %s/bin/sshctl.sh %s 2>&1", dir, args);
	FILE *p = popen(cmd, "r");
	if (!p) { perror("popen"); return; }
	while (fgets(line, sizeof line, p)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!strncmp(line, "ssh=", 4)) ui.ssh_on = !strcmp(line + 4, "on");
		else if (!strncmp(line, "port=", 5)) { if (ui.ssh_on || !ui.port) ui.port = atoi(line + 5); }
		else if (!strncmp(line, "password=", 9)) snprintf(ui.password, sizeof ui.password, "%s", line + 9);
		else if (!strncmp(line, "usb=", 4)) snprintf(ui.usb, sizeof ui.usb, "%s", line + 4);
		else fprintf(stderr, "sshctl %s: %s\n", args, line);
	}
	pclose(p);
}

/* First IPv4 address of an interface that is up and is not loopback or USB. */
static void find_ip(void)
{
	struct ifaddrs *list, *a;
	ui.ip[0] = 0;
	if (getifaddrs(&list)) return;
	for (a = list; a && !ui.ip[0]; a = a->ifa_next)
		if (a->ifa_addr && a->ifa_addr->sa_family == AF_INET && (a->ifa_flags & IFF_UP) && !(a->ifa_flags & IFF_LOOPBACK)
		    && strncmp(a->ifa_name, "usb", 3))
			inet_ntop(AF_INET, &((struct sockaddr_in *) a->ifa_addr)->sin_addr, ui.ip, sizeof ui.ip);
	freeifaddrs(list);
}

static void draw(int flash)
{
	ui_render(&ui, px);
	cfg.is_flashing = flash;
	int rc = fbink_print_raw_data(fbfd, px, UI_W, UI_H, sizeof px, 0, 0, &cfg);
	if (rc < 0) fprintf(stderr, "fbink_print_raw_data: %d\n", rc);
}

static int max_x, max_y;

/* Finds the multi-touch device, takes it for ourselves so the Kindle UI below gets no taps. */
static int open_touch(void)
{
	for (int i = 0; i < 16; i++) {
		char path[32], name[80] = "?";
		unsigned long bits[(ABS_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = { 0 };
		struct input_absinfo ax, ay;
		snprintf(path, sizeof path, "/dev/input/event%d", i);
		int fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0) continue;
		ioctl(fd, EVIOCGNAME(sizeof name), name);
		if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof bits), bits) >= 0
		    && (bits[ABS_MT_POSITION_X / (8 * sizeof(long))] >> (ABS_MT_POSITION_X % (8 * sizeof(long))) & 1)
		    && ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ax) >= 0 && ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ay) >= 0
		    && ax.maximum > 0 && ay.maximum > 0) {
			max_x = ax.maximum; max_y = ay.maximum;
			fprintf(stderr, "touch: %s \"%s\" x 0..%d y 0..%d grab=%d\n", path, name, max_x, max_y, ioctl(fd, EVIOCGRAB, 1));
			return fd;
		}
		fprintf(stderr, "input: %s \"%s\" is not the touch screen\n", path, name);
		close(fd);
	}
	return -1;
}

static void refresh_state(void) { ctl("status"); find_ip(); }

/* Returns 1 when the app must close. */
static int tap(int x, int y)
{
	char args[32];
	int act = ui_tap(&ui, x, y);
	fprintf(stderr, "tap %d,%d action %d\n", x, y, act);
	if (act == A_CLOSE) return 1;
	if (act == A_SSH || act == A_USB) {
		ui.busy = act;
		draw(0);
		if (act == A_USB) snprintf(args, sizeof args, "usb %s", ui.usb[0] ? "off" : "on");
		else if (ui.ssh_on) snprintf(args, sizeof args, "off");
		else snprintf(args, sizeof args, "on %d", ui.port);
		ctl(args);
		ui.busy = 0;
	}
	find_ip();
	draw(0); /* also for A_NONE: repaints over anything the Kindle drew on us, such as after a wake-up */
	return 0;
}

int main(void)
{
	if (getenv("KSSH_DIR")) dir = getenv("KSSH_DIR");
	char fonts[256];
	snprintf(fonts, sizeof fonts, "%s/font", dir);
	if (ui_fonts(fonts)) { fprintf(stderr, "cannot load fonts from %s\n", fonts); return 1; }

	FBInkState st;
	cfg.is_quiet = true;
	if ((fbfd = fbink_open()) < 0 || fbink_init(fbfd, &cfg) < 0) { fprintf(stderr, "cannot open the screen\n"); return 1; }
	fbink_get_state(&cfg, &st);
	fprintf(stderr, "screen: %s %ux%u bpp %u rota %u\n", st.device_name, st.screen_width, st.screen_height, st.bpp, st.current_rota);
	if (st.screen_width != UI_W || st.screen_height != UI_H) fprintf(stderr, "warning: the UI is drawn for %dx%d\n", UI_W, UI_H);

	int tfd = open_touch();
	if (tfd < 0) fprintf(stderr, "no touch screen found: the app closes after %d s\n", IDLE_CLOSE_S);

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGHUP, on_signal);

	refresh_state();
	draw(1);

	struct pollfd pfd = { tfd, POLLIN, 0 };
	int idle = 0, x = 0, y = 0, up = 0;
	struct timespec last = { 0, 0 }, now;
	while (!quit) {
		int n = poll(&pfd, tfd < 0 ? 0 : 1, 5000);
		if (n < 0) break;
		if (n == 0) { /* every 5 s: follow changes made elsewhere, such as Wi-Fi coming up */
			Ui before = ui;
			if ((idle += 5) >= IDLE_CLOSE_S) { fprintf(stderr, "idle: closing\n"); break; }
			refresh_state();
			if (memcmp(&before, &ui, sizeof ui)) draw(0);
			continue;
		}
		struct input_event ev[32];
		ssize_t got = read(tfd, ev, sizeof ev);
		for (int i = 0; i < (int) (got / sizeof ev[0]) && !quit; i++) {
			if (ev[i].type == EV_ABS && ev[i].code == ABS_MT_POSITION_X) x = ev[i].value;
			else if (ev[i].type == EV_ABS && ev[i].code == ABS_MT_POSITION_Y) y = ev[i].value;
			else if (ev[i].type == EV_ABS && ev[i].code == ABS_MT_TRACKING_ID && ev[i].value == -1) up = 1;
			else if (ev[i].type == EV_KEY && ev[i].code == BTN_TOUCH && ev[i].value == 0) up = 1;
			else if (ev[i].type == EV_SYN && ev[i].code == SYN_REPORT && up) {
				up = 0;
				clock_gettime(CLOCK_MONOTONIC, &now);
				long ms = (now.tv_sec - last.tv_sec) * 1000 + (now.tv_nsec - last.tv_nsec) / 1000000;
				if (ms < 300) continue; /* one finger lift can arrive as two reports */
				idle = 0;
				fprintf(stderr, "raw %d,%d ", x, y);
				quit |= tap((int) ((long) x * UI_W / (max_x + 1)), (int) ((long) y * UI_H / (max_y + 1)));
				while (read(tfd, ev, sizeof ev) > 0) {} /* drop taps made while we were busy */
				clock_gettime(CLOCK_MONOTONIC, &last);
				break;
			}
		}
	}
	if (tfd >= 0) ioctl(tfd, EVIOCGRAB, 0);
	fbink_close(fbfd);
	return 0;
}
