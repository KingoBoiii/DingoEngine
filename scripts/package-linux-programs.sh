#!/usr/bin/env bash
# Usage: scripts/package-linux-programs.sh <Config> <output dir> [name suffix]
#
# Packs each example and the test framework of an existing build into
# <output dir>/<Program>[-suffix]-linux-x86_64.tar.gz, holding <Program>/ with the executable
# beside its assets/, which is where it must be started from.
set -euo pipefail

CONFIG=${1:?usage: $0 <Config> <output dir> [name suffix]}
OUT=${2:?usage: $0 <Config> <output dir> [name suffix]}
SUFFIX=${3:+-$3}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=$ROOT/build/bin/$CONFIG-linux-x86_64

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$OUT"

pack() {
	local program=$1 source=$2; shift 2
	mkdir -p "$work/$program"
	cp "$BIN/$program/$program" "$work/$program/"
	cp -r "$source/assets" "$@" "$work/$program/"
	tar -czf "$OUT/$program$SUFFIX-linux-x86_64.tar.gz" -C "$work" "$program"
}

for dir in "$ROOT"/examples/*/; do
	pack "$(basename "$dir")" "$dir"
done
pack Dingo-TestFramework "$ROOT/test" "$ROOT/test/imgui.ini"
