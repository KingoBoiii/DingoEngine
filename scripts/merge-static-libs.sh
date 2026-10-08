#!/bin/sh
# Usage: merge-static-libs.sh <output.a> <input.a>...
#
# The Linux twin of the lib.exe merge that builds the distributable DingoEngine.lib: every member
# of every input archive goes into one archive. ar's MRI script keeps members whose names repeat
# across inputs, which extracting and re-archiving would overwrite, but can't quote a path with a
# space, so the inputs are reached through numbered links in a scratch directory.
set -eu

out=$1
shift
mkdir -p "$(dirname "$out")"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

i=0
{
	echo "CREATE merged.a"
	for lib in "$@"; do
		i=$((i + 1))
		ln -s "$(realpath "$lib")" "$work/$i.a"
		echo "ADDLIB $i.a"
	done
	echo "SAVE"
	echo "END"
} > "$work/merge.mri"

(cd "$work" && ar -M < merge.mri && ranlib merged.a)
mv -f "$work/merged.a" "$out"
