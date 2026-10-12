#!/usr/bin/env bash
# Usage: scripts/ci/linux-smoke-test.sh <Debug|Debug-ASan|Release|Distribution>
#
# Runs the unit tests, then every Dingo-TestFramework case, every example and Marionette's --check
# from an existing build, each from its own source directory, then the test app's deliberate
# crashes (--crash=), which must leave their report in the log. Apps that don't exit by themselves
# are closed after SMOKE_SECONDS (default 20) through WM_DELETE_WINDOW, so teardown runs too. A run
# fails on an unexpected exit code, a hang, a [FAIL] self-check, a Vulkan validation error, an
# AddressSanitizer or LeakSanitizer report or an spdlog LOG ERROR. SMOKE_TOURNAMENT=1 adds
# Marionette's tournament (about 5 minutes in Release). Needs an X display (DISPLAY) and
# python3-xlib; logs go to build/smoke/<config>/.
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

failures=0

# RUN_SECONDS, EXPECT_EXIT (default 0) and EXPECT_LOG (a pattern that must be in EXPECT_LOG_FILE,
# default the run's output) apply to one call.
run() {
	local name=$1 cwd=$2 exe=$3; shift 3
	local log=$LOGS/$name.log status problems=""
	local seconds=${RUN_SECONDS:-$SECONDS_PER_RUN} expected=${EXPECT_EXIT:-0}

	(cd "$cwd" && exec "$exe" "$@") > "$log" 2>&1 &
	local pid=$!
	for ((i = 0; i < seconds; i++)); do
		kill -0 $pid 2>/dev/null || break
		sleep 1
	done
	if kill -0 $pid 2>/dev/null; then
		$CLOSE $pid || true
		for ((i = 0; i < 60; i++)); do
			kill -0 $pid 2>/dev/null || break
			sleep 1
		done
		if kill -0 $pid 2>/dev/null; then
			kill -9 $pid 2>/dev/null
			problems+=" hung"
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
for test in "${tests[@]}"; do
	run "test-${test//[^A-Za-z0-9]/_}" "$ROOT/test" "$BIN/Dingo-TestFramework/Dingo-TestFramework" "--test=$test"
done

for dir in "$ROOT"/examples/*/; do
	example=$(basename "$dir")
	run "example-$example" "$dir" "$BIN/$example/$example"
done

run "marionette-check" "$ROOT/examples/Marionette" "$BIN/Marionette/Marionette" --check

if [[ ${SMOKE_TOURNAMENT:-0} == 1 ]]; then
	RUN_SECONDS=1200 EXPECT_LOG='\[PASS\] tournament' \
		run "marionette-tournament" "$ROOT/examples/Marionette" "$BIN/Marionette/Marionette" --tournament=10 --seed=1 --player-tier=3
fi

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
	crash terminate 134 'std::terminate called on an uncaught exception'
	if [[ $CONFIG == Distribution ]]; then
		crash verify 1 "Fatal error: Verify Failed the test app's --crash=verify"
	else
		crash verify 133 'crashed: SIGTRAP'
	fi
fi

echo "Unit tests, ${#tests[@]} test cases, $(ls -d "$ROOT"/examples/*/ | wc -l) examples, Marionette --check and the crash reports: $failures failed"
exit $((failures > 0))
