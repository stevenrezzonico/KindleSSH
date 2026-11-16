# Building Kindle apps with Claude: what is known

Facts in this file were measured on the owner's Kindle through KindleSSH on 2026-11-16, unless a
line says "not checked". Read it before starting a new Kindle app. The working example of
everything here is this repository (`build.sh`, `src/`, `tests/`).

## 1. The device

| Item | Value |
|---|---|
| Model | Kindle Paperwhite 12th generation (FBInk name "PaperWhite 6"), jailbroken |
| Firmware | 5.19.6 |
| Kernel / CPU | Linux 5.15.41-lab126, `armv7l` (32-bit, hard float), MediaTek |
| C library on device | glibc 2.35 (owner's statement). Our programs do not use it. |
| Screen | 1272 x 1696, 8-bit gray, rotation 0, e-ink |
| Touch | `/dev/input/event1`, name `fts_ts`, multi-touch protocol B, X 0..1272, Y 0..1696, same axes as the screen |
| Power key | `/dev/input/event0`, name `bd71828-pwrkey` |
| Shell | busybox `sh`. Scriptlets and SSH sessions run as root (uid 0). |
| User storage | `/mnt/us` (the USB/MTP drive). Programs run from it. Files show as `rwxrwxrwx`. Assume no symlinks and no permissions (not checked). |
| RAM state | `/tmp` and `/var/tmp`. Cleared at restart. |

## 2. Getting access (every session)

1. The owner opens KindleSSH on the Kindle and taps TURN SSH ON.
2. The owner gives the WI-FI address and the 8-character password. The user is `root`, the
   port is 22 unless changed. **The password is new at every TURN SSH ON**, so ask again after any
   OFF/ON or restart.
3. The Kindle does not sleep while SSH is on.

Log in without a terminal prompt (WSL shown; Windows `ssh.exe` works the same with a `.cmd`
ask-pass file and `DISPLAY` set):

```bash
printf '#!/bin/sh\necho THE_PASSWORD\n' > ~/kpass && chmod 700 ~/kpass
export SSH_ASKPASS=~/kpass SSH_ASKPASS_REQUIRE=force
O="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o PubkeyAuthentication=no -o NumberOfPasswordPrompts=1 -o ConnectTimeout=10"
ssh -n $O root@ADDRESS 'command'
printf 'put local /mnt/us/App/file\n' | sftp $O root@ADDRESS     # not "sftp -b": -b turns password login off
```

Delete the ask-pass file at the end of the session.

Network facts:

- WSL2 reaches the Kindle's Wi-Fi address directly.
- **Plugging the USB cable in puts the Kindle in MTP mode and Wi-Fi goes off.** Work over Wi-Fi with
  the cable out, or use USB network mode.
- USB network mode (KindleSSH button; unplug, tap ON, plug in): Kindle is `169.254.15.244`. Reached
  from Windows and from inside WSL with no setup.
- `sshctl.sh` on the Kindle (`sh /mnt/us/KindleSSH/bin/sshctl.sh status`) prints `ssh=`, `port=`,
  `password=`, `usb=`. `off` ends your own session.

## 3. Building programs

Rule: **static musl ARM binaries**, so nothing depends on the Kindle's libraries.

```bash
zig cc -target arm-linux-musleabihf -mcpu=cortex_a7 -Os -static -s -o app app.c
```

- zig 0.17.1 is installed in the owner's WSL Ubuntu 26.04. `gcc`, `make`, `git`, `curl` too.
- Strip with `-s` at link time. `zig objcopy --strip-all` is not implemented.
- For autoconf projects use wrapper scripts as `CC`/`AR`/`RANLIB` (`zig cc ...`, `zig ar`,
  `zig ranlib`) and `--host=arm-linux-musleabihf`. See `build.sh`.
- No `sudo` without a password in WSL. Get packages with `apt download NAME` and `dpkg -x`.
- Run ARM binaries on the PC with `qemu-arm` (from the `qemu-user` package, unpacked the same way;
  after a build it is at `~/kssh-build/out/qemu-arm`). An ARM program that starts another ARM
  program does not work under it.
- Size reference: the whole KindleSSH `dist/` is 2.1 MB.

## 4. Drawing on the screen

Use **FBInk** (https://github.com/NiLuJe/FBInk), commit `ded92fa510c957e0157c92ae83bd5c51af710c05`
or newer. Release v1.25.0 does not know this model.

```bash
git submodule update --init --depth 1 stb i2c-tools
make staticlib KINDLE=1 MINIMAL=1 IMAGE=1 CC=arm-cc AR=arm-ar RANLIB=arm-ranlib STRIP=true
# link: Release/libfbink.a libi2c-staged/lib/libi2c.a -lm
```

Pattern that works (`src/main.c`, `src/ui.c`):

1. Draw the whole screen into a `1272*1696` byte buffer (0 = black, 255 = white).
2. `fbink_open()`, `fbink_init(fd, &cfg)`, then
   `fbink_print_raw_data(fd, buf, 1272, 1696, 1272*1696, 0, 0, &cfg)`.
3. `cfg.is_flashing = true` gives a full flash refresh; false gives a normal one.

Keep all drawing and tap logic free of device code. Then the same code compiles on the PC, writes
PNG screenshots, and Claude can look at the UI before the owner does (`tests/preview.c`). Text:
`stb_truetype` with a font file shipped in the app folder (Inter and JetBrains Mono are in
`dist/KindleSSH/font/`, both open-license). FBInk is GPL-3.0, so a program linked with it is too.

Not explored: partial-region refresh, waveform modes, FBInk's own text functions, rotation.

## 5. Touch input

Open `/dev/input/event1` (better: scan `event*` for `ABS_MT_POSITION_X`, as `open_touch()` does).
Read `struct input_event`: track `ABS_MT_POSITION_X/Y`; a tap ends on `ABS_MT_TRACKING_ID` = -1 or
`BTN_TOUCH` = 0, then `SYN_REPORT`. Ignore a second "up" within 300 ms. Scale by the range from
`EVIOCGABS`. Take the device with `ioctl(fd, EVIOCGRAB, 1)` so the Kindle UI below gets no taps;
release it at exit.

## 6. Launching: scriptlets

A shell script in `/mnt/us/documents/` appears in the library and runs as root when tapped.

```sh
#!/bin/sh
# Name: My App
# Author: Me
# Icon: /mnt/us/MyApp/icon.png
# DontUseFBInk
```

- `# Icon:` takes an absolute path (or a `data:image/png;base64,` string). 300 x 450 gray PNG works.
- Without `# DontUseFBInk` the script's output is printed on the screen.
- Unix line endings only.
- Set `PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH`.
- Start the app in the background (`( ... ) </dev/null >/dev/null 2>&1 &`) and write a log file under
  `/mnt/us/MyApp/`. Whether the library waits for or kills a long script: not checked.
- Docs: https://kindlemodding.org/kindle-dev/scriptlets.html

## 7. Full-screen app: keep the Kindle UI off the screen

Same steps KOReader uses; they work on this Kindle (`src/KindleSSH.sh`).

```sh
# before the app
lipc-set-prop com.lab126.pillow disableEnablePillow disable
[ -f /etc/upstart/statusbar.conf ] && stop statusbar
killall -STOP awesome
# after the app, ALWAYS (use: trap restore EXIT; trap 'exit 1' TERM INT HUP)
killall -CONT awesome
[ -f /etc/upstart/statusbar.conf ] && start statusbar
lipc-set-prop com.lab126.pillow disableEnablePillow enable
lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home
```

Safety rules for any full-screen app:

- If the app dies without the restore steps, the Kindle looks frozen. Always restore in a trap.
- Give the app a way out that does not need touch (KindleSSH closes itself after 5 idle minutes).
- Last resort for the owner: hold the power button about 40 seconds to restart.
- When testing over SSH, `killall APPNAME` triggers the launcher's restore.
- Behaviour when the Kindle sleeps and wakes under a full-screen app: not checked.

## 8. System tools found on the Kindle

| Need | Command |
|---|---|
| Keep awake / allow sleep | `lipc-set-prop com.lab126.powerd preventScreenSaver 1` / `0` |
| Inspect a service's properties | `lipc-probe com.lab126.volumd`, `lipc-get-prop SERVICE PROP` |
| System settings store | `kdb get PATH`, `kdb ls PATH` |
| Services (upstart) | `start NAME`, `stop NAME`, `status NAME`, `initctl list`; jobs in `/etc/upstart/` |
| Logs | `/var/log/messages`, `dmesg`, `logger` |
| Firmware version | `/etc/prettyversion.txt` |
| Kernel config | `zcat /proc/config.gz` |
| Firewall | `iptables` (INPUT policy was ACCEPT; `/etc/sysconfig/iptables` holds the boot rules) |
| Network | `ifconfig`, `udhcpd`, `modprobe`, `rmmod`, `lsmod` |

USB internals: gadget through configfs at `/sys/kernel/config/usb_gadget/`, controller
`11211000.usb`, MTP by the upstart job `mtp` (`tizen-mtp`, `/usr/bin/mtp.sh`). Kernel modules
present: `usb_f_rndis`, `usb_f_ecm`, `usb_f_acm`, `usb_f_serial`, `usb_f_mass_storage`, `g_ether`.
The working USB network recipe is `usb_on()` in `src/sshctl.sh`.

## 9. Seeing and touching the screen from SSH

Both were tested on the Kindle. Together they let Claude run a tap-and-look test with no person
at the device.

**Screenshot.** `/dev/fb0` holds what the screen shows, for our apps and for the Kindle's own UI.
8 bits per pixel, 1272 bytes per row, 1272 x 3392 virtual (two pages; the visible one is first).

```bash
ssh -n $O root@ADDRESS 'cd /sys/class/graphics/fb0; echo "$(head -1 modes) $(cat bits_per_pixel) $(cat stride)"; cat /dev/fb0' \
    | python3 tools/fb2png.py shot.png
```

Then read `shot.png`. A screenshot of KindleSSH shows the SSH password, and one of the home screen
shows the owner's library: keep screenshots out of the repository.

**Tap.** `ktap` writes finger events into the touch device. Coordinates are screen pixels.

```bash
ssh -n $O root@ADDRESS '/mnt/us/KindleSSH/bin/ktap 184 560'        # one tap
ssh -n $O root@ADDRESS '/mnt/us/KindleSSH/bin/ktap 360 615 910 1260' # two taps, 0.5 s apart
```

- The taps reach an app that holds the touch device with `EVIOCGRAB`, and they reach the Kindle's
  own UI.
- To start an app from the library: screenshot the home screen, find the icon, tap it. The app is
  up about 5 seconds later.
- Taps only. Swipes and long presses are not built.

## 10. Work loop for a new app

1. Write logic and drawing as PC-testable code. Check screenshots and tests on the PC.
2. Cross-compile static. Smoke-run under `qemu-arm`.
3. Upload with SFTP to `/mnt/us/AppName/`. To replace a running program: upload as `name.new`,
   `killall name`, `mv name.new name`.
4. Run it over SSH. Drive it with `ktap`, look with a screenshot (section 9), and read its log
   (KindleSSH logs every tap with coordinates).
5. Put the final files in a `dist/` folder laid out like the Kindle drive, for install by USB copy.

## 11. Limits set by the owner

- Nothing on the system partition. No boot jobs. Everything under `/mnt/us/AppName/` plus one
  scriptlet in `/mnt/us/documents/`.
- Programs must not depend on the Kindle's own libraries. Calling the Kindle's own command-line
  tools (`lipc-set-prop`, `iptables`, `start`/`stop`) is accepted.
- Ask before any change that alters the device mode (USB mode, stopping services, restart).

Not checked anywhere in this project: writing to the system partition, behaviour after a restart,
other Kindle models or firmware versions.
