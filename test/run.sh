#!/bin/bash
# Runs the proxy under real Wine: the browser process of "steamwebhelper.exe" must get
# --in-process-gpu; child processes (--type=...) and other programs must not; winmm calls must work.
set -euo pipefail
cd "$(dirname "$0")"
OUT="$(cd .. && pwd)/build"
WORK="$(mktemp -d)"
trap 'wineserver -k 2>/dev/null || true; rm -rf "$WORK"' EXIT
x86_64-w64-mingw32-gcc -O2 -o "$WORK/steamwebhelper.exe" cmdline.c -lwinmm
cp "$WORK/steamwebhelper.exe" "$WORK/other.exe"
ORIG="$(find /usr/lib -path '*wine/x86_64-windows/winmm.dll' 2>/dev/null | head -n 1)"
[ -n "$ORIG" ] || { echo "Wine's winmm.dll not found"; exit 1; }
cp "$OUT/winmm.dll" "$WORK/winmm.dll"
cp "$ORIG" "$WORK/winmm_orig.dll"
export WINEPREFIX="$WORK/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winmm=n,b"
wineboot -i >/dev/null 2>&1 || true
run() { (cd "$WORK" && wine64 "$@" 2>/dev/null | tr -d '\r'); }
fail() { echo "FAIL: $*"; exit 1; }

out="$(run steamwebhelper.exe --lang=en)"; echo "$out"
echo "$out" | grep -q -- '--lang=en --in-process-gpu' || fail "browser process didn't get --in-process-gpu"
echo "$out" | grep -q 'TIME_OK=1' || fail "forwarded winmm call failed"
out="$(run steamwebhelper.exe --type=renderer)"; echo "$out"
echo "$out" | grep -q -- '--in-process-gpu' && fail "child process was changed"
out="$(run other.exe --x)"; echo "$out"
echo "$out" | grep -q -- '--in-process-gpu' && fail "another program was changed"
out="$(run steamwebhelper.exe --in-process-gpu)"
[ "$(echo "$out" | grep -o -- '--in-process-gpu' | wc -l)" = 1 ] || fail "flag added twice"
echo "webhelper fix: all checks passed"
