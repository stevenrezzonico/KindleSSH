#!/bin/sh
# Name: @APP@
# Author: @APP@
# Icon: /mnt/us/@APP@/icon.png
# DontUseFBInk

D=/mnt/us/@APP@
export PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

# The app runs detached, so the library does not wait for it.
(
	pidof @BIN@ >/dev/null && exit 0
	exec >$D/log.txt 2>&1
	echo "start $(date)"

	# Keep the Kindle UI from drawing over the app. ALWAYS undone at exit, or the Kindle looks frozen.
	restore() {
		trap - EXIT
		killall -CONT awesome
		[ -f /etc/upstart/statusbar.conf ] && start statusbar
		lipc-set-prop com.lab126.pillow disableEnablePillow enable
		lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home
		echo "restored $(date)"
	}
	trap restore EXIT
	trap 'exit 1' TERM INT HUP
	lipc-set-prop com.lab126.pillow disableEnablePillow disable
	[ -f /etc/upstart/statusbar.conf ] && stop statusbar
	killall -STOP awesome

	APP_DIR=$D $D/bin/@BIN@
	echo "exit code $?"
) </dev/null >/dev/null 2>&1 &
