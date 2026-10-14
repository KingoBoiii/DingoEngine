#!/bin/sh
# Usage: scripts/icons/make-icons.sh
#
# Draws every executable's placeholder icon (a coloured tile with initials in Inter Bold) into
# icon.png (256 px, the Linux .desktop icon) and icon.ico (16-256 px, the Windows resource) beside
# its premake5.lua. Needs ImageMagick. Replace a project's two files with real art at any time.
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
font="$root/test/assets/fonts/Inter-Bold.ttf"

grep -v '^#' "$root/scripts/icons/icons.txt" | while read -r dir colour text size; do
	[ -n "$dir" ] || continue
	out="$root/$dir"
	convert -size 256x256 xc:none -fill "$colour" -draw "roundrectangle 8,8 247,247 44,44" \
		-font "$font" -fill white -pointsize "$size" -gravity center -annotate +0+4 "$text" \
		-strip -depth 8 "PNG32:$out/icon.png"
	convert "$out/icon.png" -define icon:auto-resize=256,128,64,48,32,24,16 "$out/icon.ico"
	echo "make-icons: $dir"
done
