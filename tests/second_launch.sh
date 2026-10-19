#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# 0.9.5-2 C-2: with Lunduke City running, a second `lunduke-city` (plain, or
# with a .cty as a file manager passes it) forwards to the running City and
# exits 0. Before 0.9.5-2 every second launch held itself open as a hidden
# process that ignored SIGTERM.
# usage: second_launch.sh <lunduke-city> <asset dir>
set -u
if [ "${1:-}" = "--inner" ]; then
    BIN=$2
    T=$3
    "$BIN" > "$T/primary.log" 2>&1 &
    P1=$!
    up=0
    i=0
    while [ $i -lt 100 ]; do
        if gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
            --method org.freedesktop.DBus.NameHasOwner com.lunduke.LundukeCity 2>/dev/null | grep -q true; then
            up=1
            break
        fi
        sleep 0.1
        i=$((i + 1))
    done
    sleep 1
    rc1=99
    rc2=99
    if [ $up -eq 1 ]; then
        timeout -k 2 20 "$BIN" > "$T/second.log" 2>&1
        rc1=$?
        timeout -k 2 20 "$BIN" "$T/x.cty" > "$T/third.log" 2>&1
        rc2=$?
    fi
    alive=0
    kill -0 "$P1" 2>/dev/null && alive=1
    kill -TERM "$P1" 2>/dev/null
    sleep 1
    kill -KILL "$P1" 2>/dev/null
    wait "$P1" 2>/dev/null
    echo "primary registered: $up"
    echo "second launch (no file) exit: $rc1"
    echo "second launch (with .cty) exit: $rc2"
    echo "primary still running afterwards: $alive"
    if [ $up -eq 1 ] && [ $rc1 -eq 0 ] && [ $rc2 -eq 0 ] && [ $alive -eq 1 ]; then
        echo "PASS: second launches forwarded and exited"
        exit 0
    fi
    echo "FAIL: a second launch did not exit on its own (124/137 = still running after 20 s)"
    exit 1
fi

BIN=$1
if [ -z "${DISPLAY:-}" ]; then
    echo "no DISPLAY; this GUI test must fail rather than skip"
    exit 1
fi
export LUNDUKE_CITY_ASSETS="$2"
T=$(mktemp -d /tmp/lunduke-second-launch-XXXXXX) || exit 1
export HOME="$T" XDG_CONFIG_HOME="$T/config" XDG_STATE_HOME="$T/state" XDG_DATA_HOME="$T/data"
export XDG_CACHE_HOME="$T/cache" NO_AT_BRIDGE=1
printf 'not a city file\n' > "$T/x.cty"
unset DBUS_SESSION_BUS_ADDRESS
# A private bus, so another City on the session bus cannot take the opens.
dbus-run-session -- sh "$0" --inner "$BIN" "$T"
rc=$?
rm -rf "$T"
exit $rc
