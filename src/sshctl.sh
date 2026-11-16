#!/bin/sh
# KindleSSH server control. Runs on the Kindle's busybox sh.
#   sshctl.sh on [PORT] | off | status | usb on|off
# State lives in RAM ($R), so a reboot clears it. Only the host key and the last port are kept on disk.
D=${KSSH_DIR:-/mnt/us/KindleSSH}
R=${KSSH_RUN:-/tmp/kindlessh}
DB=$D/bin/dropbearmulti

G=/sys/kernel/config/usb_gadget/kssh
USB_IP=169.254.15.244   # link-local: Windows reaches it with no setup on the PC

usb_running() { [ -n "$(cat $G/UDC 2>/dev/null)" ]; }

usb_off() {
	echo "" >$G/UDC
	rm -f $G/os_desc/c.1 $G/configs/c.1/rndis.usb0
	rmdir $G/configs/c.1/strings/0x409 $G/configs/c.1 $G/functions/rndis.usb0 $G/strings/0x409 $G
	lipc-set-prop com.lab126.volumd useUsbForNetwork 0
} 2>/dev/null

# Must run with the cable unplugged: on plug-in the Kindle gives the USB port to MTP.
usb_on() {
	usb_off
	# The firmware's own switch keeps the Kindle out of MTP mode. It loads g_ether, which Windows
	# takes for a serial port, so we replace it with an RNDIS gadget that Windows knows.
	lipc-set-prop com.lab126.volumd useUsbForNetwork 1
	i=0; while [ $i -lt 8 ] && ! ifconfig usb0 2>/dev/null | grep -q 'inet addr'; do sleep 1; i=$((i + 1)); done
	rmmod g_ether 2>/dev/null
	modprobe usb_f_rndis
	mkdir $G && cd $G || return 1
	echo 0x1d6b >idVendor; echo 0x0104 >idProduct; echo 0x0200 >bcdUSB
	mkdir strings/0x409
	echo KindleSSH >strings/0x409/manufacturer; echo "Kindle USB network" >strings/0x409/product
	echo KSSH0001 >strings/0x409/serialnumber   # fixed, so Windows keeps one network adapter for it
	mkdir functions/rndis.usb0
	echo e0 >functions/rndis.usb0/class; echo 01 >functions/rndis.usb0/subclass; echo 03 >functions/rndis.usb0/protocol
	echo ee:19:00:00:00:00 >functions/rndis.usb0/dev_addr; echo ee:49:00:00:00:00 >functions/rndis.usb0/host_addr
	echo RNDIS >functions/rndis.usb0/os_desc/interface.rndis/compatible_id
	echo 5162001 >functions/rndis.usb0/os_desc/interface.rndis/sub_compatible_id
	mkdir -p configs/c.1/strings/0x409; echo RNDIS >configs/c.1/strings/0x409/configuration
	ln -s $G/functions/rndis.usb0 configs/c.1/
	echo 1 >os_desc/use; echo 0xcd >os_desc/b_vendor_code; echo MSFT100 >os_desc/qw_sign
	ln -s $G/configs/c.1 os_desc/
	ls /sys/class/udc >UDC
	cd /
	ifconfig usb0 $USB_IP netmask 255.255.0.0
}

# Sleep turns Wi-Fi off, so the Kindle stays awake while the server or the USB link is on.
keep_awake() {
	if running || usb_running; then v=1; else v=0; fi
	lipc-set-prop com.lab126.powerd preventScreenSaver $v
}

running() { [ -f $R/pid ] && kill -0 "$(cat $R/pid)" 2>/dev/null; }

stop() {
	[ -f $R/pid ] && kill "$(cat $R/pid)" 2>/dev/null
	killall dropbearmulti 2>/dev/null   # open sessions are separate processes
	[ -f $R/port ] && iptables -D INPUT -p tcp --dport "$(cat $R/port)" -j ACCEPT 2>/dev/null
	rm -f $R/pid $R/password $R/port
}

case "$1" in
on)
	port=${2:-$(cat $D/etc/port 2>/dev/null)}; port=${port:-22}
	case "$port" in ''|*[!0-9]*) echo "bad port" >&2; exit 2;; esac
	[ "$port" -ge 1 ] && [ "$port" -le 65535 ] || { echo "bad port" >&2; exit 2; }
	stop
	umask 077
	mkdir -p $R $D/etc
	[ -s $D/etc/hostkey ] || $DB dropbearkey -t ed25519 -f $D/etc/hostkey >/dev/null 2>&1
	# 8 characters, without the look-alikes 0 O 1 l I
	tr -dc 'a-km-zA-HJ-NP-Z2-9' </dev/urandom 2>/dev/null | head -c 8 >$R/password
	[ "$(wc -c <$R/password)" -eq 8 ] || { echo "no password" >&2; exit 1; }
	echo "$port" >$R/port
	echo "$port" >$D/etc/port
	iptables -I INPUT -p tcp --dport "$port" -j ACCEPT
	KSSH_PASSFILE=$R/password $DB dropbear -F -p "$port" -r $D/etc/hostkey \
		>>$R/dropbear.log 2>&1 </dev/null &
	echo $! >$R/pid
	sleep 1
	running || { stop; keep_awake; echo "server did not start" >&2; exit 1; }
	;;
off)
	stop
	;;
usb)
	case "$2" in
	on) usb_on; usb_running || { usb_off; keep_awake; echo "USB network did not start" >&2; exit 1; };;
	off) usb_off;;
	*) exit 2;;
	esac
	;;
status)
	;;
*)
	echo "usage: $0 on [PORT] | off | status | usb on|off" >&2; exit 2
	;;
esac

[ "$1" = status ] || keep_awake

# Every command ends with the state, for the UI to read.
if running; then
	echo "ssh=on"; echo "port=$(cat $R/port)"; echo "password=$(cat $R/password)"
else
	echo "ssh=off"; echo "port=$(cat $D/etc/port 2>/dev/null || echo 22)"; echo "password="
fi
usb_running && echo "usb=$USB_IP" || echo "usb="
