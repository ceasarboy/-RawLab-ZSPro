#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${ANDROID_HOME:?Set ANDROID_HOME to your Android SDK}"
APK="${1:-$ROOT/app/build/outputs/apk/debug/app-debug.apk}"
case "$(uname -s)" in
  Darwin) HOST=darwin-x86_64 ;;
  Linux) HOST=linux-x86_64 ;;
  *) printf 'Run this check from macOS or Linux.\n' >&2; exit 1 ;;
esac
READELF="$SDK/ndk/27.2.12479018/toolchains/llvm/prebuilt/$HOST/bin/llvm-readelf"
TEMP="$(mktemp -d)"
trap 'rm -rf "$TEMP"' EXIT
"$SDK/build-tools/35.0.0/zipalign" -c -P 16 4 "$APK"
unzip -q "$APK" 'lib/*' -d "$TEMP"
for library in "$TEMP"/lib/*/*.so; do
  "$READELF" -lW "$library" > "$TEMP/headers.txt"
  awk '$1 == "LOAD" { count++; if ($NF != "0x4000" && $NF != "0x10000") bad=1 } END { exit (bad || count == 0) }' "$TEMP/headers.txt"
  printf '16 KB aligned: %s\n' "${library#"$TEMP/"}"
done
