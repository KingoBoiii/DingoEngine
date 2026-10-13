#!/usr/bin/env bash
# Usage: scripts/package-linux-programs.sh <Config> <output dir> [name suffix]
#
# Packs each example and the test framework of an existing build into
# <output dir>/<Program>[-suffix]-linux-x86_64.tar.gz, holding <Program>/. An example ships its
# executable beside <Program>.dpak (its assets, packed and encrypted with its dpak.key) and the
# licence files at the root of its assets, and starts from any directory. The test framework, a
# development tool, keeps its loose assets/ and must be started from its own directory. Each
# program also gets its icon and the .desktop template premake's exeMetadata() wrote, which
# install-desktop-entry.sh fills in on the player's machine.
set -euo pipefail

CONFIG=${1:?usage: $0 <Config> <output dir> [name suffix]}
OUT=${2:?usage: $0 <Config> <output dir> [name suffix]}
SUFFIX=${3:+-$3}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=$ROOT/build/bin/$CONFIG-linux-x86_64

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$OUT"

desktop() {
	cp "$2/icon.png" "$work/$1/$1.png"
	cp "$ROOT/build/generated/$1/$1.desktop" "$ROOT/scripts/linux-desktop/install-desktop-entry.sh" "$work/$1/"
}

archive() {
	tar -czf "$OUT/$1$SUFFIX-linux-x86_64.tar.gz" -C "$work" "$1"
}

for dir in "$ROOT"/examples/*/; do
	program=$(basename "$dir")
	mkdir -p "$work/$program"
	cp "$BIN/$program/$program" "$work/$program/"
	"$ROOT/scripts/pack-assets.sh" "$program" "$work/$program/$program.dpak" "$CONFIG" > /dev/null
	find "$dir/assets" -maxdepth 1 -type f -iname '*licen[cs]e*' -exec cp {} "$work/$program/" \;
	cp "$ROOT/THIRD-PARTY-NOTICES.md" "$work/$program/"
	desktop "$program" "$dir"
	archive "$program"
done

mkdir -p "$work/Dingo-TestFramework"
cp "$BIN/Dingo-TestFramework/Dingo-TestFramework" "$work/Dingo-TestFramework/"
cp -r "$ROOT/test/assets" "$ROOT/test/imgui.ini" "$ROOT/THIRD-PARTY-NOTICES.md" "$work/Dingo-TestFramework/"
desktop Dingo-TestFramework "$ROOT/test"
archive Dingo-TestFramework
