#!/usr/bin/env bash
# Usage: scripts/pack-assets.sh <Example> <out.dpak> [Config]
#
# Packs examples/<Example>/assets into <out.dpak>, encrypted with examples/<Example>/dpak.key,
# building DingoPack (Config, default Release) first if it isn't built. The game mounts
# <executable name>.dpak from its own directory, so put the package beside a copy of the
# executable, never beside the one in build/bin: there it would shadow the loose files you edit.
set -euo pipefail

EXAMPLE=${1:?usage: $0 <Example> <out.dpak> [Config]}
OUTPUT=${2:?usage: $0 <Example> <out.dpak> [Config]}
CONFIG=${3:-Release}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SOURCE=$ROOT/examples/$EXAMPLE
TOOL=$ROOT/build/bin/$CONFIG-linux-x86_64/DingoPack/DingoPack

[ -d "$SOURCE/assets" ] || { echo "pack-assets: no $SOURCE/assets" >&2; exit 1; }
[ -f "$SOURCE/dpak.key" ] || { echo "pack-assets: no $SOURCE/dpak.key (DingoPack genkey makes one)" >&2; exit 1; }

if [ ! -x "$TOOL" ]; then
	make -C "$ROOT" -j"$(nproc)" config="$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')" DingoPack
fi

mkdir -p "$(dirname "$OUTPUT")"
"$TOOL" pack "$SOURCE/assets" "$OUTPUT" --key "$SOURCE/dpak.key"
"$TOOL" verify "$OUTPUT" --key "$SOURCE/dpak.key" > /dev/null
