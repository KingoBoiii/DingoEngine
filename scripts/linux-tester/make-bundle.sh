#!/usr/bin/env bash
# Usage: scripts/linux-tester/make-bundle.sh <Config> <output.tar.gz>
#
# Packs an existing build's test app beside its assets/ and each example beside its encrypted
# <Example>.dpak, as a release ships them, with this directory's scripts and README, into one
# archive a tester unpacks and runs.
set -euo pipefail

CONFIG=${1:?usage: $0 <Config> <output.tar.gz>}
OUTPUT=${2:?usage: $0 <Config> <output.tar.gz>}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BIN=$ROOT/build/bin/$CONFIG-linux-x86_64

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
pkg=$work/DingoEngine-linux-tester
mkdir -p "$pkg/test" "$pkg/examples"

cp "$ROOT"/scripts/linux-tester/{README.md,run-checks.sh,play.sh} "$ROOT/scripts/ci/close-windows.py" "$pkg/"
grep -oP '^\s*m_Tests\.push_back\(\{ "\K[^"]+' "$ROOT/test/src/TestLayer.cpp" > "$pkg/tests.txt" || true
count=$(wc -l < "$pkg/tests.txt")
if (( count < 20 )); then
	echo "found $count test cases in test/src/TestLayer.cpp, expected at least 20" >&2
	exit 1
fi

cp "$BIN/Dingo-TestFramework/Dingo-TestFramework" "$pkg/test/"
cp -r "$ROOT/test/assets" "$ROOT/test/imgui.ini" "$pkg/test/"
for dir in "$ROOT"/examples/*/; do
	example=$(basename "$dir")
	mkdir -p "$pkg/examples/$example"
	cp "$BIN/$example/$example" "$pkg/examples/$example/"
	"$ROOT/scripts/pack-assets.sh" "$example" "$pkg/examples/$example/$example.dpak" "$CONFIG" > /dev/null
	find "$dir/assets" -maxdepth 1 -type f -iname '*licen[cs]e*' -exec cp {} "$pkg/examples/$example/" \;
done

mkdir -p "$(dirname "$OUTPUT")"
tar -czf "$OUTPUT" -C "$work" DingoEngine-linux-tester
