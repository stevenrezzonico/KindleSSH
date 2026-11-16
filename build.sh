#!/bin/bash
# Builds KindleSSH into dist/ (laid out like the Kindle USB root). Run in WSL Ubuntu:  bash build.sh
# Needs: zig, gcc, make, git, curl, apt (download only, no root). Work files go to ~/kssh-build.
set -euo pipefail
P=$(cd "$(dirname "$0")" && pwd)
B=${KSSH_BUILD:-$HOME/kssh-build}
OUT=$B/out
mkdir -p $B/bin $OUT/font
cd $B

DROPBEAR=dropbear-2026.94
DROPBEAR_SHA=e098034a843699200c8c977a991fff73159735bf795d5f72ef672c41a6b1ae81
OPENSSH=openssh-10.6p2
OPENSSH_SHA=9ae44872090cf2f06b3cf9ff147e7f16f309671340763ed2f99248398ae29d5b
FBINK_REV=ded92fa510c957e0157c92ae83bd5c51af710c05

# Static musl ARMv7 hard-float: the programs need no library from the Kindle.
printf '#!/bin/sh\nexec zig cc -target arm-linux-musleabihf -mcpu=cortex_a7 "$@"\n' >bin/arm-cc
printf '#!/bin/sh\nexec zig ar "$@"\n' >bin/arm-ar
printf '#!/bin/sh\nexec zig ranlib "$@"\n' >bin/arm-ranlib
chmod +x bin/*
export PATH=$B/bin:$PATH
TC="CC=arm-cc AR=arm-ar RANLIB=arm-ranlib STRIP=true"
NOLOG="--disable-lastlog --disable-utmp --disable-utmpx --disable-wtmp --disable-wtmpx"

fetch() { # url sha256
	local f=${1##*/}
	[ -f $f ] || curl -fsSLO "$1"
	echo "$2  $f" | sha256sum -c --quiet
}

if [ ! -x $OUT/dropbearmulti ]; then
	fetch https://matt.ucc.asn.au/dropbear/releases/$DROPBEAR.tar.bz2 $DROPBEAR_SHA
	rm -rf $DROPBEAR && tar xf $DROPBEAR.tar.bz2
	(cd $DROPBEAR
	 patch -p1 <$P/patches/dropbear-filepass.patch
	 cp $P/patches/localoptions.h .
	 ./configure --host=arm-linux-musleabihf $TC CFLAGS=-Os --enable-static --disable-zlib --disable-syslog \
		$NOLOG --disable-pututline --disable-pututxline >/dev/null
	 make -j8 PROGRAMS="dropbear dropbearkey" MULTI=1 LDFLAGS="-static -s" >make.log 2>&1
	 cp dropbearmulti $OUT/)
fi

if [ ! -x $OUT/sftp-server ]; then
	fetch https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/$OPENSSH.tar.gz $OPENSSH_SHA
	rm -rf $OPENSSH && tar xf $OPENSSH.tar.gz
	(cd $OPENSSH
	 ./configure --host=arm-linux-musleabihf $TC CFLAGS=-Os LDFLAGS="-static -s" --without-openssl --without-zlib \
		--without-pam --without-selinux --disable-strip $NOLOG >/dev/null
	 make -j8 sftp-server >make.log 2>&1
	 cp sftp-server $OUT/)
fi

if [ ! -f FBInk/Release/libfbink.a ]; then
	[ -d FBInk ] || git clone -q https://github.com/NiLuJe/FBInk.git
	(cd FBInk
	 git checkout -q $FBINK_REV
	 git submodule -q update --init --depth 1 stb i2c-tools
	 make staticlib KINDLE=1 MINIMAL=1 IMAGE=1 $TC >make.log 2>&1)
fi

# Fonts (SIL Open Font License) and qemu-arm for the tests come from Ubuntu packages, unpacked without root.
if [ ! -f $OUT/font/Inter-Bold.otf ] || [ ! -x $OUT/qemu-arm ]; then
	mkdir -p deb && (cd deb
	 apt download fonts-inter fonts-jetbrains-mono qemu-user >/dev/null 2>&1
	 for d in *.deb; do dpkg -x $d .; done
	 cp usr/share/fonts/opentype/inter/Inter-Bold.otf usr/share/fonts/truetype/jetbrains-mono/JetBrainsMonoNL-Bold.ttf $OUT/font/
	 cp usr/bin/qemu-arm $OUT/)
fi

arm-cc -Os -Wall -Wextra -I$P/src -IFBInk -isystem FBInk/stb -o $OUT/kindlessh-ui $P/src/main.c $P/src/ui.c \
	FBInk/Release/libfbink.a FBInk/libi2c-staged/lib/libi2c.a -lm -static -s

# Tests: UI logic and screenshots on the PC, control script with stubs, ARM server under qemu with real ssh/sftp.
mkdir -p $P/shots
gcc -O2 -Wall -Wextra -I$P/src -isystem FBInk/stb -o preview $P/tests/preview.c $P/src/ui.c -lm
./preview $OUT/font $P/shots
bash $P/tests/sshctl-test.sh
bash $P/tests/server-test.sh $OUT

rm -rf $P/dist
mkdir -p $P/dist/documents $P/dist/KindleSSH/bin $P/dist/KindleSSH/font
cp $P/src/KindleSSH.sh $P/dist/documents/
cp $OUT/kindlessh-ui $OUT/dropbearmulti $OUT/sftp-server $P/src/sshctl.sh $P/dist/KindleSSH/bin/
cp $OUT/font/* $P/dist/KindleSSH/font/
cp $P/shots/icon.png $P/dist/KindleSSH/
cp $P/LICENSES.txt $P/dist/KindleSSH/
du -sh $P/dist
