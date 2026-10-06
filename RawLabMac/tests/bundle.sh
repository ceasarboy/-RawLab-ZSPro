#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
APP="${1:-$ROOT/build/RawLab Mac.app}"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}"
for binary in "$APP/Contents/MacOS/RawLabMac" "$APP"/Contents/Frameworks/*.dylib; do
    while IFS= read -r dependency; do
        case "$dependency" in
            /System/*|/usr/lib/*) ;;
            @rpath/*)
                test -f "$APP/Contents/Frameworks/${dependency#@rpath/}" || {
                    echo "FAIL: missing bundled dependency $dependency"; exit 1;
                }
                ;;
            *) echo "FAIL: external dependency $dependency in $binary"; exit 1 ;;
        esac
    done < <(otool -L "$binary" | awk 'NR>1 {print $1}')
    codesign --verify --strict "$binary"
done
codesign --verify --deep --strict "$APP"
echo 'PASS: all non-system dynamic dependencies are bundled and signed'
