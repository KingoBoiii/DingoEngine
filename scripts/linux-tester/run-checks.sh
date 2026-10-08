#!/usr/bin/env bash
# Usage: ./run-checks.sh [seconds per program, default 15]
#
# Opens every test case and example of the bundle in turn on this desktop, closes each the way a
# user would, and packs the system details and every log into one archive to send back.
set -uo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
SECONDS_PER_RUN=${1:-15}
STAMP=$(date +%Y%m%d-%H%M%S)
REPORT=dingo-linux-report-$STAMP
OUT=$HERE/$REPORT
mkdir -p "$OUT/logs"

say() { echo "$*" | tee -a "$OUT/summary.txt"; }
have() { command -v "$1" >/dev/null 2>&1; }

if [[ -z ${DISPLAY:-} ]]; then
	echo "No DISPLAY: run this from a terminal on your desktop (X11, or Wayland with XWayland)." >&2
	exit 1
fi
libs=$( { ldconfig -p || /sbin/ldconfig -p; } 2>/dev/null)
if [[ -n $libs ]] && ! grep -q 'libvulkan\.so\.1' <<< "$libs"; then
	echo "The Vulkan loader (libvulkan.so.1) is missing. Install it first:" >&2
	echo "  Ubuntu/Debian: sudo apt install libvulkan1 mesa-vulkan-drivers vulkan-tools" >&2
	echo "  Fedora:        sudo dnf install vulkan-loader mesa-vulkan-drivers vulkan-tools" >&2
	echo "  Arch:          sudo pacman -S vulkan-icd-loader vulkan-tools (plus vulkan-radeon, vulkan-intel or nvidia-utils)" >&2
	exit 1
fi

close_with_xlib=1
if ! python3 -c 'import Xlib' 2>/dev/null; then
	close_with_xlib=0
	echo "python3-xlib is missing, so programs are stopped with SIGTERM and their shutdown goes unchecked."
	echo "  Install it for the full check: python3-xlib (Ubuntu, Debian, Fedora) or python-xlib (Arch)."
	echo
fi

distro=$( (. /etc/os-release && echo "$PRETTY_NAME") 2>/dev/null)

{
	echo "== System"
	echo "distro: $distro"
	echo "kernel: $(uname -r)"
	echo "glibc: $(ldd --version 2>/dev/null | head -1)"
	echo "session: ${XDG_SESSION_TYPE:-?} desktop: ${XDG_CURRENT_DESKTOP:-?} DISPLAY=${DISPLAY:-} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}"
	echo "scale: GDK_SCALE=${GDK_SCALE:-} QT_SCALE_FACTOR=${QT_SCALE_FACTOR:-}"
	echo
	echo "== GPU"
	have lspci && lspci | grep -iE 'vga|3d|display'
	ls /usr/share/vulkan/icd.d /etc/vulkan/icd.d 2>/dev/null
	if have vulkaninfo; then vulkaninfo --summary 2>&1; else echo "vulkaninfo not installed (package vulkan-tools)"; fi
	echo
	echo "== Monitors"
	if have xrandr; then xrandr --listmonitors 2>&1; else echo "xrandr not installed"; fi
	echo
	echo "== Audio"
	if have pactl; then pactl info 2>&1 | grep -E 'Server Name|Default Sink'; else echo "pactl not installed"; fi
	echo
	echo "== Input devices"
	ls /dev/input/by-id 2>/dev/null | grep -iE 'joystick|gamepad|event-joystick' || echo "no gamepad found"
} > "$OUT/system.txt" 2>&1

say "DingoEngine Linux checks, $(date)"
say "$distro, ${XDG_SESSION_TYPE:-unknown} session"
say "Each program runs for $SECONDS_PER_RUN s. Windows will open and close; please don't click into them."
say ""

failures=0
runs=0

run() {
	local name=$1 dir=$2 exe=$3; shift 3
	local log=$OUT/logs/$name.log status problems=""

	(cd "$HERE/$dir" && exec "./$exe" "$@") > "$log" 2>&1 &
	local pid=$!
	for ((i = 0; i < SECONDS_PER_RUN; i++)); do
		kill -0 $pid 2>/dev/null || break
		sleep 1
	done
	if kill -0 $pid 2>/dev/null; then
		if ((close_with_xlib)); then
			python3 "$HERE/close-windows.py" $pid || kill -TERM $pid 2>/dev/null
		else
			kill -TERM $pid 2>/dev/null
		fi
		for ((i = 0; i < 30; i++)); do
			kill -0 $pid 2>/dev/null || break
			sleep 1
		done
		if kill -0 $pid 2>/dev/null; then
			kill -9 $pid 2>/dev/null
			problems+=" hung-on-close"
		fi
	fi
	wait $pid
	status=$?
	runs=$((runs + 1))

	if [[ $status -eq 143 && $close_with_xlib -eq 0 ]]; then
		:
	elif [[ $status -ge 128 ]]; then
		problems+=" crashed(signal $((status - 128)))"
	elif [[ $status -ne 0 ]]; then
		problems+=" exit=$status"
	fi
	grep -q '\[FAIL\]' "$log" && problems+=" failed-checks=$(grep -c '\[FAIL\]' "$log")"
	grep -qE 'Validation Error|VUID-' "$log" && problems+=" validation-errors=$(grep -cE 'Validation Error|VUID-' "$log")"
	grep -q 'LOG ERROR' "$log" && problems+=" log-error"

	local passed
	passed=$(grep -c '\[PASS\]' "$log")
	if [[ -n $problems ]]; then
		failures=$((failures + 1))
		say "FAIL  $name ($passed checks passed):$problems"
	else
		say "ok    $name ($passed checks passed)"
	fi
}

while read -r test; do
	[[ -n $test ]] && run "test-${test//[^A-Za-z0-9]/_}" test Dingo-TestFramework "--test=$test"
done < "$HERE/tests.txt"

for dir in "$HERE"/examples/*/; do
	example=$(basename "$dir")
	run "example-$example" "examples/$example" "$example"
done

run "marionette-check" examples/Marionette Marionette --check
SECONDS_PER_RUN=$((SECONDS_PER_RUN > 30 ? SECONDS_PER_RUN : 30)) run "marionette-perf" examples/Marionette Marionette --autoplay --perf --vsync=off

say ""
say "GPU: $(grep -ohm1 'Selected Vulkan physical device: .*' "$OUT"/logs/*.log | head -1 | cut -d' ' -f5-)"
say "Performance: $(grep -oh '\[Perf\].*' "$OUT/logs/marionette-perf.log" || echo 'no [Perf] line')"
say "$runs runs, $failures with problems"

tar -czf "$HERE/$REPORT.tar.gz" -C "$HERE" "$REPORT"
echo
echo "Done. Please send back: $HERE/$REPORT.tar.gz"
echo "(It holds the summary above, system.txt and one log per program; nothing else from your machine.)"
