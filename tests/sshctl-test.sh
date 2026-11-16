#!/bin/bash
# Tests src/sshctl.sh on the PC with stub programs for dropbear, iptables, lipc and killall.
set -u
S=$(cd "$(dirname "$0")/.." && pwd)/src/sshctl.sh
T=$(mktemp -d); fail=0
check() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' want '$3'"; fail=1; fi; }
get() { echo "$out" | sed -n "s/^$1=//p"; }

mkdir -p $T/app/bin $T/stub
cat >$T/app/bin/dropbearmulti <<'EOF'
#!/bin/sh
[ "$1" = dropbearkey ] && { echo key >"$5"; exit 0; }
exec sleep 600
EOF
sed -i "2i echo \"\$@\" >>$T/dropbear.log" $T/app/bin/dropbearmulti
printf '#!/bin/sh\necho "$@" >>%s/iptables.log\n' $T >$T/stub/iptables
printf '#!/bin/sh\necho "$@" >>%s/lipc.log\n[ "$2" = preventScreenSaver ] && echo "$3" >%s/sleepstate\n' $T $T >$T/stub/lipc-set-prop
printf '#!/bin/sh\ncat %s/sleepstate 2>/dev/null\n' $T >$T/stub/lipc-get-prop
printf '#!/bin/sh\necho "$@" >>%s/killall.log\n' $T >$T/stub/killall
chmod +x $T/app/bin/* $T/stub/*
run() { out=$(PATH=$T/stub:$PATH KSSH_DIR=$T/app KSSH_RUN=$T/run sh $S "$@" 2>&1); rc=$?; }

run status;        check "starts off" "$(get ssh)" off; check "default port 22" "$(get port)" 22
run on 2222;       check "on" "$(get ssh)" on; check "port shown" "$(get port)" 2222
p1=$(get password)
check "password is 8 allowed chars" "$(echo "$p1" | grep -cE '^[a-km-zA-HJ-NP-Z2-9]{8}$')" 1
check "password file is private" "$(stat -c %a $T/run/password)" 600
check "firewall opened" "$(tail -1 $T/iptables.log)" "-I INPUT -p tcp --dport 2222 -j ACCEPT"
run status;        check "status repeats password" "$(get password)" "$p1"
run on;            check "port remembered" "$(get port)" 2222
[ "$(get password)" != "$p1" ]; check "new password at every on" $? 0
run off;           check "off" "$(get ssh)" off; check "password gone" "$(get password)" ""
check "firewall closed" "$(tail -1 $T/iptables.log)" "-D INPUT -p tcp --dport 2222 -j ACCEPT"
check "server process gone" "$(pgrep -f "$T/app/bin/dropbearmulti" | wc -l)" 0
for bad in 0 65536 22x -5; do run on $bad; check "port $bad rejected" $rc 2; done
run on;            check "kept awake while SSH is on" "$(cat $T/sleepstate)" 1
echo 1 >$T/sleepstate
run status;        check "status does not change sleep" "$(cat $T/sleepstate)" 1
run off;           check "sleep allowed again after off" "$(cat $T/sleepstate)" 0
run status;        check "usb off (the gadget needs a real Kindle)" "$(get usb)" ""
run on;            check "key file folder given to the server" "$(tail -1 $T/dropbear.log | grep -c -- "-D $T/app/etc")" 1
run rescue myapp;  check "rescue kills the app" "$(grep -c '^myapp$' $T/killall.log)" 1
check "rescue wakes the Kindle UI" "$(grep -c -- '-CONT awesome' $T/killall.log)" 1
check "rescue asks for the home screen" "$(grep -c 'appmgrd start app://com.lab126.booklet.home' $T/lipc.log)" 1
check "rescue leaves SSH on" "$(get ssh)" on
run off

rm -rf $T
exit $fail
