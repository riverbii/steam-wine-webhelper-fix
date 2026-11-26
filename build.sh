#!/bin/bash
# Build the Steam webhelper fix (64-bit winmm.dll proxy) with mingw-w64.
#   ./build.sh [OUTPUT_DIR]   -> OUTPUT_DIR/winmm.dll (default: build)
# Exports are generated from mingw's winmm import library and forwarded to winmm_orig.dll.
set -euo pipefail
cd "$(dirname "$0")"
OUT="${1:-build}"
CC="${CC:-x86_64-w64-mingw32-gcc}"
NM="${NM:-x86_64-w64-mingw32-nm}"
command -v "$CC" >/dev/null || { echo "mingw-w64 compiler not found ($CC)" >&2; exit 1; }
LIB="$("$CC" -print-file-name=libwinmm.a)"
[ -f "$LIB" ] || { echo "libwinmm.a not found" >&2; exit 1; }
mkdir -p "$OUT"
DEF="$OUT/winmm.def"
{
  echo "LIBRARY winmm.dll"
  echo "EXPORTS"
  # import-library symbols look like "__imp_timeGetTime"; forward each to winmm_orig
  "$NM" -g "$LIB" 2>/dev/null | awk '$2 == "I" || $3 ~ /^__imp_/ { print $NF }' \
    | sed -n 's/^__imp_//p' | sort -u \
    | while read -r name; do echo "  $name=winmm_orig.$name"; done
} > "$DEF"
COUNT="$(grep -c '=winmm_orig\.' "$DEF")"
[ "$COUNT" -gt 100 ] || { echo "too few forwarded exports ($COUNT)" >&2; exit 1; }
"$CC" -shared -O2 -s -o "$OUT/winmm.dll" proxy.c "$DEF" \
  -lkernel32 -luser32 -Wl,--enable-stdcall-fixup
echo "built $OUT/winmm.dll ($COUNT forwarded exports)"
