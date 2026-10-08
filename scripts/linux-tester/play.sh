#!/usr/bin/env bash
# Usage: ./play.sh <program> [arguments...]
#
# Starts a program from its own directory, where it finds its assets/, and keeps its log in
# logs/<program>-<time>.log beside this script as well as printing it.
set -uo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
name=${1:-}

if [[ -z $name ]]; then
	echo "Usage: ./play.sh <program> [arguments...]"
	echo
	echo "Programs:"
	echo "  TestFramework"
	for dir in "$HERE"/examples/*/; do echo "  $(basename "$dir")"; done
	exit 1
fi
shift

if [[ ${name,,} == testframework || ${name,,} == test ]]; then
	dir=$HERE/test exe=Dingo-TestFramework
else
	dir=$(find "$HERE/examples" -mindepth 1 -maxdepth 1 -type d -iname "$name" | head -1)
	[[ -z $dir ]] && { echo "No program called '$name'. Run ./play.sh to list them." >&2; exit 1; }
	exe=$(basename "$dir")
fi

mkdir -p "$HERE/logs"
log=$HERE/logs/$exe-$(date +%Y%m%d-%H%M%S).log
cd "$dir" && "./$exe" "$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
echo "Exit code $status; log saved to $log"
exit "$status"
