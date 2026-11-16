#!/bin/sh
# Name: KindleSSH
# Author: KindleSSH
# Icon: /mnt/us/KindleSSH/icon.png
# DontUseFBInk

D=/mnt/us/KindleSSH
export PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

# The app runs detached, so the library does not wait for it.
(
	pidof kindlessh-ui >/dev/null && exit 0
	exec >$D/log.txt 2>&1
	echo "start $(date) as $(id) on $(cat /etc/prettyversion.txt 2>/dev/null)"

	# Same steps KOReader uses on this firmware: keep the Kindle UI from drawing over the app.
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

	KSSH_DIR=$D $D/bin/kindlessh-ui
	echo "ui exit code $?"
) </dev/null >/dev/null 2>&1 &
