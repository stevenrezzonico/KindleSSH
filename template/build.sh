#!/bin/bash
# Builds the app into dist/ (laid out like the Kindle drive). Run in WSL:  bash build.sh
# Needs the KindleSSH build first (bash build.sh in the KindleSSH repository): it leaves the compiler
# wrappers, FBInk and the fonts in ~/kssh-build.
# To start a new app: copy this folder, set APP and BIN below, edit ui.c / ui.h / preview.c.
set -euo pipefail
APP=MyApp     # folder on the Kindle (/mnt/us/MyApp) and the name in the library
BIN=myapp     # program name
P=$(cd "$(dirname "$0")" && pwd)
B=${KSSH_BUILD:-$HOME/kssh-build}
export PATH=$B/bin:$PATH
mkdir -p $P/out

# PC: tap tests, screenshots in shots/ (look at them before the Kindle sees the app), the icon.
mkdir -p $P/shots
gcc -O2 -Wall -Wextra -I$P -isystem $B/FBInk/stb -o $P/out/preview $P/preview.c $P/ui.c -lm
$P/out/preview $B/out/font $P/shots

# Kindle: static ARM program, needs no library from the Kindle.
arm-cc -Os -Wall -Wextra -DAPP_DIR="\"/mnt/us/$APP\"" -I$P -I$B/FBInk -isystem $B/FBInk/stb -o $P/out/$BIN $P/main.c $P/ui.c \
	$B/FBInk/Release/libfbink.a $B/FBInk/libi2c-staged/lib/libi2c.a -lm -static -s

rm -rf $P/dist
mkdir -p $P/dist/documents $P/dist/$APP/bin $P/dist/$APP/font
sed "s/@APP@/$APP/g; s/@BIN@/$BIN/g" $P/launcher.sh >$P/dist/documents/$APP.sh
cp $P/out/$BIN $P/dist/$APP/bin/
cp $B/out/font/Inter-Bold.otf $P/dist/$APP/font/
cp $P/shots/icon.png $P/dist/$APP/
du -sh $P/dist
