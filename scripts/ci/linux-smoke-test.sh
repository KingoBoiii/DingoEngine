#!/usr/bin/env bash
# Usage: scripts/ci/linux-smoke-test.sh <Debug|Debug-ASan|Release|Distribution>
#
# Runs the unit tests, then every Dingo-TestFramework case, every example, the --check of Marionette
# and of Hearthvale (headless, then windowed) from an existing build, each from its own source
# directory, then the test app headless with DISPLAY unset (--graphics=headless), then the test
# app's deliberate crashes (--crash=), which must leave their report in the log, then Marionette's
# --check and Candlewick packed (a copy of the executable beside its .dpak, started from
# elsewhere). A hung app's frame index (twice, 5 s apart), whether it and GLFW saw the close,
# its per-thread CPU use, the X server's CPU use and wait channel, both ends of the
# app's sockets' queues and its thread stacks are recorded before it is killed. Apps that don't
# exit by themselves are closed after SMOKE_SECONDS (default 20) through WM_DELETE_WINDOW, so
# teardown runs too. A run fails on an unexpected exit code, a hang, a [FAIL] self-check, a Vulkan
# validation error, an AddressSanitizer or LeakSanitizer report or an spdlog LOG ERROR.
# SMOKE_TOURNAMENT=1 adds Marionette's tournament (about 5 minutes in Release). Needs an X display
# (DISPLAY) and python3-xlib; logs go to build/smoke/<config>/.
set -uo pipefail

CONFIG=${1:?usage: $0 <Debug|Debug-ASan|Release|Distribution>}
SECONDS_PER_RUN=${SMOKE_SECONDS:-20}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BIN=$ROOT/build/bin/$CONFIG-linux-x86_64
LOGS=$ROOT/build/smoke/$CONFIG
CLOSE="python3 $ROOT/scripts/ci/close-windows.py"
mkdir -p "$LOGS"

# Leak checks are opt-in: the Debug-ASan CI job turns them on with scripts/ci/lsan.supp, which
# holds the leaks Mesa and libxcb make at swap-chain creation and X11 teardown.
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}

# The Vulkan loader unloads Mesa's llvmpipe ICD before LeakSanitizer looks, so what its threads
# hold reads as leaked from frames no suppression can name; preloaded, it stays mapped and the run
# is clean. libasan must come first.
APP_PRELOAD=""
[[ $CONFIG == Debug-ASan ]] && APP_PRELOAD="$(gcc -print-file-name=libasan.so) libvulkan_lvp.so"

failures=0

# gdb isn't the run's ancestor, so Yama's ptrace_scope 1 needs root to let it attach.
dump_threads() {
	command -v gdb > /dev/null || return 0
	local exe
	exe=$(readlink "/proc/$1/exe") || return 0
	local gdb=(gdb)
	sudo -n true 2> /dev/null && gdb=(sudo -n gdb)
	timeout -k 10 120 "${gdb[@]}" -p "$1" "$exe" -batch -nx -ex 'set pagination off' -ex 'thread apply all -ascending bt' 2>&1 | grep -vE '^\[New LWP|^\[Thread debugging' | head -2000
}

# Frames that still advance with the close unseen by GLFW mean the request was lost on the way;
# a frame index that stands still means the frame loop itself stalled. Needs the engine's
# symbols (Debug, or --keep-symbols); GLFW's line also needs its debug info.
app_state() {
	command -v gdb > /dev/null || return 0
	local exe
	exe=$(readlink "/proc/$1/exe") || return 0
	local gdb=(gdb)
	sudo -n true 2> /dev/null && gdb=(sudo -n gdb)
	timeout -k 10 60 "${gdb[@]}" -p "$1" "$exe" -batch -nx \
		-ex "printf \"frame index %lu\\n\", (unsigned long) 'Dingo::Renderer::GetFrameIndex'()" \
		-ex "printf \"application running %d\\n\", (int) 'Dingo::Application::s_Instance'->m_IsRunning" \
		-ex 'printf "glfw should close %d\n", (int) _glfw.windowListHead->shouldClose' 2>&1 \
		| grep -E '^(frame index|application running|glfw should close) |^No symbol|^Cannot access|unknown (return )?type|ptrace|Could not attach' \
		|| echo "(no state: gdb was denied, timed out or found no symbols)"
	return 0
}

thread_cpu() {
	local stat
	for stat in /proc/$1/task/*/stat; do
		sed -E 's/^([0-9]+) \((.*)\) (.*)$/\1\t\2\t\3/' "$stat" 2> /dev/null
	done | awk -F'\t' '{ split($3, f, " "); printf "%s\t%s\t%s\t%d\n", $1, $2, f[1], f[12] + f[13] }'
}

# A deadlocked app's threads use no CPU between two samples; a slow one's render or llvmpipe threads do.
thread_activity() {
	local before after
	before=$(thread_cpu "$1")
	sleep 5
	after=$(thread_cpu "$1")
	echo "tid	name	state	cpu ticks in 5 s"
	awk -F'\t' 'NF < 4 { next } NR == FNR { b[$1] = $4; next } { printf "%s\t%s\t%s\t%s\n", $1, $2, $3, ($1 in b) ? $4 - b[$1] : "new" }' <(echo "$before") <(echo "$after")
}

x_server_pid() {
	[[ -n ${DISPLAY:-} ]] || return 1
	pgrep -n -f "^Xvfb ${DISPLAY%.*}( |\$)" || pgrep -n -x Xvfb
}

app_sockets() {
	command -v ss > /dev/null || { echo "(ss is not installed)"; return 0; }
	ss -xpn 2> /dev/null | awk -v app="pid=$1," 'NR == 1 { print; next } { row[NR] = $0; own[NR] = $6; peer[NR] = $8 } index($0, app) { mine[$6] = 1 }
		END { for (i = 2; i <= NR; i++) if ((own[i] in mine) || (peer[i] in mine)) print row[i] }'
}

x_server_ticks() {
	sed -E 's/^.*\) //' "/proc/$1/stat" 2> /dev/null | awk '{ print $12 + $13 }'
}

# The main thread and Mesa's WSI thread can both block on the X socket (#186); whether the X server
# still runs and which end's queue stays full shows who stopped reading.
x_server_activity() {
	local xpid before after
	xpid=$(x_server_pid) || { echo "no X server found for DISPLAY=${DISPLAY:-}"; return 0; }
	echo "X server $xpid: $({ tr '\0' ' ' < "/proc/$xpid/cmdline"; } 2> /dev/null)"
	before=$(x_server_ticks "$xpid")
	echo "-- before --"
	app_sockets "$1"
	sleep 5
	after=$(x_server_ticks "$xpid")
	echo "-- 5 s later --"
	app_sockets "$1"
	if [[ -n $before && -n $after ]]; then
		echo "X server: state $(awk '{ print $3 }' "/proc/$xpid/stat" 2> /dev/null), wchan $(cat "/proc/$xpid/wchan" 2> /dev/null), cpu ticks in 5 s $((after - before))"
	else
		echo "X server: gone"
	fi
}

# RUN_SECONDS, EXPECT_EXIT (default 0), EXPECT_LOG (a pattern that must be in EXPECT_LOG_FILE,
# default the run's output) and NO_DISPLAY=1 (DISPLAY and WAYLAND_DISPLAY unset) apply to one call.
run() {
	local name=$1 cwd=$2 exe=$3; shift 3
	local log=$LOGS/$name.log status problems=""
	local seconds=${RUN_SECONDS:-$SECONDS_PER_RUN} expected=${EXPECT_EXIT:-0}

	# Line-buffered, so a hung run's log tail is what it last logged: spdlog flushes only warnings.
	(cd "$cwd" && { [[ -z ${NO_DISPLAY:-} ]] || unset DISPLAY WAYLAND_DISPLAY; } && LD_PRELOAD=$APP_PRELOAD exec stdbuf -oL "$exe" "$@") > "$log" 2>&1 &
	local pid=$!
	for ((i = 0; i < seconds; i++)); do
		kill -0 $pid 2>/dev/null || break
		sleep 1
	done
	if kill -0 $pid 2>/dev/null; then
		local delivered=1
		$CLOSE $pid || delivered=0
		for ((i = 0; i < 60; i++)); do
			kill -0 $pid 2>/dev/null || break
			sleep 1
		done
		if kill -0 $pid 2>/dev/null; then
			x_server_activity $pid > "$log.x11" &
			thread_activity $pid > "$log.activity"
			wait $!
			# After the samples above: the attach and the inferior call interrupt the app's waits.
			{ app_state $pid; sleep 5; echo "-- 5 s later --"; app_state $pid; } > "$log.state"
			dump_threads $pid > "$log.threads"
			kill -9 $pid 2>/dev/null
			problems+=" hung"
			((delivered)) || problems+=" close-not-delivered"
		fi
	fi
	wait $pid
	status=$?

	[[ $status -ne $expected ]] && problems+=" exit=$status"
	if [[ -n ${EXPECT_LOG:-} ]] && ! grep -qE "$EXPECT_LOG" "${EXPECT_LOG_FILE:-$log}" 2>/dev/null; then
		problems+=" missing:'$EXPECT_LOG'"
	fi
	grep -q '\[FAIL\]' "$log" && problems+=" failed-checks=$(grep -c '\[FAIL\]' "$log")"
	grep -qE 'Validation Error|VUID-' "$log" && problems+=" validation-errors=$(grep -cE 'Validation Error|VUID-' "$log")"
	grep -q 'ERROR: AddressSanitizer' "$log" && problems+=" asan"
	grep -q 'ERROR: LeakSanitizer' "$log" && problems+=" leaks"
	grep -q 'LOG ERROR' "$log" && problems+=" log-error"

	local passed
	passed=$(grep -c '\[PASS\]' "$log")
	if [[ -n $problems ]]; then
		failures=$((failures + 1))
		echo "::error title=Smoke test $name ($CONFIG)::$problems"
		echo "FAIL  $name ($passed passed):$problems"
		grep -E '\[FAIL\]|Validation Error|VUID-|AddressSanitizer|LeakSanitizer|LOG ERROR|Assertion failed' "$log" | head -20
		if [[ $problems == *hung* ]]; then
			echo "---- the last lines it logged ----"
			tail -25 "$log"
			grep -q 'Window close requested' "$log" && echo "---- it logged the close request ----" \
				|| echo "---- it never logged the close request (or the log was still buffered) ----"
			echo "---- frame loop and close state, 5 s apart ----"
			cat "$log.state"
			echo "---- CPU per thread over 5 s ----"
			cat "$log.activity"
			echo "---- the X server and the app's sockets over the same 5 s ----"
			cat "$log.x11"
			echo "---- its threads ----"
			grep -E '^Thread|^#' "$log.threads" | head -1000
		fi
	else
		echo "ok    $name ($passed passed)"
	fi
}

unit_log=$LOGS/unit-tests.log
if "$BIN/Dingo-UnitTests/Dingo-UnitTests" > "$unit_log" 2>&1; then
	echo "ok    unit-tests ($(grep -oP 'test cases:\s*\K\d+' "$unit_log") cases)"
else
	failures=$((failures + 1))
	echo "::error title=Unit tests ($CONFIG)::see $unit_log"
	echo "FAIL  unit-tests"
	grep -E 'ERROR|Status' "$unit_log" | head -20
fi

mapfile -t tests < <(grep -oP '^\s*m_Tests\.push_back\(\{ "\K[^"]+' "$ROOT/test/src/TestLayer.cpp")
if (( ${#tests[@]} < 20 )); then
	echo "::error::found ${#tests[@]} test cases in test/src/TestLayer.cpp, expected at least 20"
	exit 1
fi
for test in "${tests[@]}"; do
	run "test-${test//[^A-Za-z0-9]/_}" "$ROOT/test" "$BIN/Dingo-TestFramework/Dingo-TestFramework" "--test=$test"
done

# The Throughput case moves a static mesh more than once; the scene warns about the first move only.
moved_log=$LOGS/test-Throughput_Test.log
moved_warnings=$(grep -c 'a MeshRendererComponent with Static set moved' "$moved_log" 2> /dev/null)
if [[ ${moved_warnings:-0} -ne 1 ]]; then
	failures=$((failures + 1))
	echo "::error title=Smoke test test-Throughput_Test ($CONFIG)::${moved_warnings:-0} static-moved warnings, expected 1"
	echo "FAIL  test-Throughput_Test: ${moved_warnings:-0} static-moved warnings, expected exactly 1"
fi

for dir in "$ROOT"/examples/*/; do
	example=$(basename "$dir")
	run "example-$example" "$dir" "$BIN/$example/$example"
done

run "marionette-check" "$ROOT/examples/Marionette" "$BIN/Marionette/Marionette" --check
run "hearthvale-check-headless" "$ROOT/examples/Hearthvale" "$BIN/Hearthvale/Hearthvale" --check --graphics=headless
RUN_SECONDS=${SMOKE_CHECK_SECONDS:-150} EXPECT_LOG='\[Checks\] [0-9]+ passed, 0 failed' \
	run "hearthvale-check" "$ROOT/examples/Hearthvale" "$BIN/Hearthvale/Hearthvale" --check

if [[ ${SMOKE_TOURNAMENT:-0} == 1 ]]; then
	RUN_SECONDS=1200 EXPECT_LOG='\[PASS\] tournament' \
		run "marionette-tournament" "$ROOT/examples/Marionette" "$BIN/Marionette/Marionette" --tournament=10 --seed=1 --player-tier=3
fi

NO_DISPLAY=1 EXPECT_LOG='Headless: [0-9]+ updates' \
	run "headless-check" "$ROOT/test" "$BIN/Dingo-TestFramework/Dingo-TestFramework" --graphics=headless

# The crash reports land in the log file, not only on stderr. ASan catches the signals itself.
crash() {
	local kind=$1 dir=$LOGS/crash-$1
	rm -rf "$dir"
	EXPECT_EXIT=$2 EXPECT_LOG=$3 EXPECT_LOG_FILE=$dir/Dingo.log \
		run "crash-$kind" "$ROOT/test" "$BIN/Dingo-TestFramework/Dingo-TestFramework" --test=Text "--crash=$kind" "--log-dir=$dir"
}
crash fatal 1 "Fatal error: The test app's --crash=fatal"
if [[ $CONFIG != Debug-ASan ]]; then
	crash segv 139 'End of backtrace'
	crash thread 139 'End of backtrace'
	crash engine-thread 139 'End of backtrace'
	crash engine-stack 139 'crashed: SIGSEGV'
	crash prepared-stack 139 'crashed: SIGSEGV'
	crash render-stack 139 'crashed: SIGSEGV'
	crash physics-stack 139 'crashed: SIGSEGV'
	crash threads 139 'End of backtrace'
	crash segv-after-info 139 'End of backtrace'
	if ! awk '/last info line/{i=NR} /crashed: SIGSEGV/{c=NR} END{exit !(i && i<c)}' "$LOGS/crash-segv-after-info/Dingo.log" 2>/dev/null; then
		failures=$((failures + 1))
		echo "::error title=Smoke test crash-segv-after-info ($CONFIG)::the last info line isn't before the report"
		echo "FAIL  crash-segv-after-info: the last info line isn't before the report"
	fi
	crash stack 139 'crashed: SIGSEGV'
	crash abort 134 'crashed: SIGABRT'
	crash terminate 134 "std::terminate called on an uncaught exception: the test app's --crash=terminate"
	crash throw 134 "std::terminate called on an uncaught exception: the test app's --crash=throw"
	crash engine-throw 134 "std::terminate called on an uncaught exception: the test app's engine-thread throw"
	crash prepared-throw 134 "std::terminate called on an uncaught exception: the test app's engine-thread throw"
	crash render-throw 134 "std::terminate called on an uncaught exception: the test app's engine-thread throw"
	crash asset-throw 134 "std::terminate called on an uncaught exception: the test app's engine-thread throw"
	if [[ $CONFIG == Distribution ]]; then
		crash verify 1 "Fatal error: Verify Failed the test app's --crash=verify"
	else
		crash verify 133 'crashed: SIGTRAP'
	fi
fi

# The loader's report of the validation layer reaches a Distribution log only through the
# engine's messenger.
if [[ $CONFIG == Distribution ]]; then
	rm -rf "$LOGS/validation"
	EXPECT_LOG='\[Vulkan\].*VK_LAYER_KHRONOS_validation' EXPECT_LOG_FILE=$LOGS/validation/Dingo.log \
		run "distribution-validation" "$ROOT/test" "$BIN/Dingo-TestFramework/Dingo-TestFramework" --test=Text --vulkan-validation "--log-dir=$LOGS/validation"
fi

# Shipped the way a release ships them: a copy of the executable beside its encrypted .dpak, no
# assets/ anywhere, started from another directory.
packed=$(mktemp -d)
trap 'rm -rf "$packed"' EXIT
mkdir -p "$packed/cwd"
for example in Marionette Candlewick; do
	mkdir -p "$packed/$example"
	cp "$BIN/$example/$example" "$packed/$example/"
	"$ROOT/scripts/pack-assets.sh" "$example" "$packed/$example/$example.dpak" "$CONFIG" > /dev/null
done
EXPECT_LOG="mounted package 'Marionette\\.dpak'" run "packed-marionette-check" "$packed/cwd" "$packed/Marionette/Marionette" --check
EXPECT_LOG="mounted package 'Candlewick\\.dpak'" run "packed-candlewick" "$packed/cwd" "$packed/Candlewick/Candlewick"

echo "Unit tests, ${#tests[@]} test cases, $(ls -d "$ROOT"/examples/*/ | wc -l) examples, Marionette and Hearthvale --check, the headless check, the crash reports, packed Marionette --check and Candlewick: $failures failed"
exit $((failures > 0))
