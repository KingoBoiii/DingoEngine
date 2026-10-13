#!/bin/sh
# Usage: ./install-desktop-entry.sh [--remove]
#
# Adds the program in this folder to the desktop's application menu, with its icon: the .desktop
# file beside this script names the folder as @DIR@, since a desktop entry can't hold a relative
# path, so this writes a copy with the folder's absolute path into
# ~/.local/share/applications. Run it again after moving the folder; --remove takes the entry out.
set -eu
dir=$(cd "$(dirname "$0")" && pwd)
apps=${XDG_DATA_HOME:-$HOME/.local/share}/applications
# Desktop-entry strings escape a backslash; inside Exec's quoted argument ", `, $ and \ also take
# one first, and % is doubled. Each result is then escaped again as a sed replacement.
replacement() {
	sed 's/[\\&|]/\\&/g'
}
dir_value=$(printf '%s' "$dir" | sed 's/\\/\\\\/g' | replacement)
exec_value=$(printf '%s' "$dir" | sed 's/[\\"`$]/\\&/g; s/\\/\\\\/g; s/%/%%/g' | replacement)

found=0
for template in "$dir"/*.desktop; do
	[ -f "$template" ] || continue
	found=1
	target="$apps/dingo-$(basename "$template")"
	if [ "${1:-}" = "--remove" ]; then
		rm -f "$target"
		echo "Removed $target"
	else
		mkdir -p "$apps"
		sed "s|@EXECDIR@|$exec_value|g; s|@DIR@|$dir_value|g" "$template" > "$target"
		echo "Installed $target"
	fi
done
[ "$found" = 1 ] || { echo "No .desktop file beside $0" >&2; exit 1; }
command -v update-desktop-database > /dev/null && update-desktop-database "$apps" 2> /dev/null || true
