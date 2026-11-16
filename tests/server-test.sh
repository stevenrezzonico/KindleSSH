#!/bin/bash
# Runs the ARM dropbear and sftp-server under qemu and logs in with the real ssh/sftp clients.
# Usage: tests/server-test.sh BUILD_DIR   (BUILD_DIR holds dropbearmulti, sftp-server, qemu-arm)
set -u
B=$1; Q=$B/qemu-arm; T=$(mktemp -d); PORT=2299; fail=0
check() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' want '$3'"; fail=1; fi; }

$Q $B/dropbearmulti dropbearkey -t ed25519 -f $T/hostkey >/dev/null 2>&1
echo "Abc23xyz" > $T/password
KSSH_PASSFILE=$T/password $Q $B/dropbearmulti dropbear -F -p 127.0.0.1:$PORT -r $T/hostkey 2>$T/log &
pid=$!; sleep 1

printf '#!/bin/sh\ncat %s/try\n' $T > $T/askpass; chmod +x $T/askpass
login() { # $1 = password to try; prints the remote output
  echo "$1" > $T/try
  SSH_ASKPASS=$T/askpass SSH_ASKPASS_REQUIRE=force ssh -p $PORT -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null -o PubkeyAuthentication=no -o NumberOfPasswordPrompts=1 \
    -o LogLevel=ERROR 127.0.0.1 'echo in' 2>/dev/null
}
check "right password logs in" "$(login Abc23xyz)" "in"
check "wrong password fails"   "$(login Abc23xyZ)" ""
check "empty password fails"   "$(login '')" ""
rm $T/password
check "no password file fails" "$(login Abc23xyz)" ""
kill $pid

echo "payload $$" > $T/src
printf 'put %s/src %s/dst\nget %s/dst %s/back\n' $T $T $T $T | sftp -q -D "$Q $B/sftp-server" -b - >/dev/null 2>&1
check "sftp put/get round trip" "$(cat $T/back 2>/dev/null)" "payload $$"

rm -rf $T
exit $fail
