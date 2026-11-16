# KindleSSH

A home-screen app for a jailbroken Kindle Paperwhite (12th generation, firmware 5.19.x) that turns a
root SSH + SFTP server on and off.

## Install

1. Connect the Kindle to the PC with USB.
2. Copy the contents of `dist/` to the top folder of the Kindle drive. This adds `KindleSSH/` and
   `documents/KindleSSH.sh`.
3. Eject the Kindle. "KindleSSH" appears in the library. Tap it.

## Use

- **TURN SSH ON** starts the server and shows a new 8-character password. Log in as `root`:
  `ssh root@ADDRESS` (add `-p PORT` if the port is not 22). SFTP and `scp` use the same login.
- **TURN SSH OFF** stops the server and closes open sessions.
- **TURN USB NETWORK ON** makes the USB cable a network link. Unplug the cable first, tap the
  button, then plug the cable in. The Kindle is then at `169.254.15.244`: `ssh root@169.254.15.244`.
  Windows 11 needs no driver and no address setup. While it is on, the Kindle is not visible as a
  file-transfer (MTP) device. **TURN USB NETWORK OFF** or a restart brings file transfer back.
- The Kindle does not sleep while SSH or USB network is on (sleep would turn Wi-Fi off). It can
  sleep again when both are off. Turn them off when you are done: an awake Kindle uses more battery.
- **CHANGE PORT** opens a keypad. It is available while SSH is off.
- **CLOSE** returns to the Kindle home screen. The server stays on until TURN SSH OFF or a restart.
- The app closes itself after 5 minutes without a tap.

Nothing is written to the system partition and nothing starts at boot. The password and the server
state are kept in RAM (`/tmp/kindlessh`). The host key and the last port are kept in
`KindleSSH/etc/`.

Use this on a network you trust: it is a root login protected by a short password.

## If something goes wrong

- The app writes `KindleSSH/log.txt` at every start. It records the screen, the touch device and
  every tap. The server log is `/tmp/kindlessh/dropbear.log` (lost at restart).
- If the screen stays on the app and does not react, wait 5 minutes: the app closes itself. If the
  Kindle home screen does not come back, hold the power button for about 40 seconds to restart.

## Build

In WSL Ubuntu, with `zig`, `gcc`, `make`, `git` and `curl` installed:

```bash
bash build.sh
```

The script builds static ARM programs (Dropbear, OpenSSH `sftp-server`, the UI with FBInk), runs the
tests, and writes `dist/` and the screenshots in `shots/`. Work files go to `~/kssh-build`.

| Path | Content |
|---|---|
| `src/ui.c`, `src/ui.h` | Screen drawing and tap handling. Runs on the PC too. |
| `src/main.c` | Kindle side: e-ink output (FBInk), touch input. |
| `src/sshctl.sh` | Server control: `on [PORT]`, `off`, `status`, `usb on|off`. |
| `src/KindleSSH.sh` | The scriptlet that the library shows. |
| `patches/` | Dropbear change: password from a file, and build options. |
| `tests/` | UI test and screenshots, control script test, server test under qemu. |

USB networking uses the firmware's own switch (`volumd useUsbForNetwork`) to keep the Kindle out of
MTP mode, then replaces the stock `g_ether` driver (Windows takes it for a serial port) with an
RNDIS gadget built through configfs. If the cable is plugged in while USB network is off, the Kindle
enters MTP mode and turns Wi-Fi off.
