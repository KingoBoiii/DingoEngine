#!/usr/bin/env bash
# Usage: scripts/linux-tester/make-bundle.sh <Config> <output.tar.gz>
#
# Packs an existing build's test app and examples, each beside its assets/ as in the repo, with
# this directory's scripts and README, into one archive a tester unpacks and runs.
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
grep -oP '^\s*m_Tests\.push_back\(\{ "\K[^"]+' "$ROOT/test/src/TestLayer.cpp" > "$pkg/tests.txt"

cp "$BIN/Dingo-TestFramework/Dingo-TestFramework" "$pkg/test/"
cp -r "$ROOT/test/assets" "$pkg/test/"
for dir in "$ROOT"/examples/*/; do
	example=$(basename "$dir")
	mkdir -p "$pkg/examples/$example"
	cp "$BIN/$example/$example" "$pkg/examples/$example/"
	cp -r "$dir/assets" "$pkg/examples/$example/"
done

mkdir -p "$(dirname "$OUTPUT")"
tar -czf "$OUTPUT" -C "$work" DingoEngine-linux-tester
