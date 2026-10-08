#!/usr/bin/env bash
# Usage: scripts/ci/linux-smoke-test.sh <Debug|Debug-ASan|Release|Distribution>
#
# Runs every Dingo-TestFramework case, every example and Marionette's --check from an existing
# build, each from its own source directory. Apps that don't exit by themselves are closed after
# SMOKE_SECONDS (default 20) through WM_DELETE_WINDOW, so teardown runs too. A run fails on a
# non-zero exit, a hang, a [FAIL] self-check, a Vulkan validation error, an AddressSanitizer
# report or an spdlog LOG ERROR. Needs an X display (DISPLAY) and python3-xlib; logs go to
# build/smoke/<config>/.
set -uo pipefail

CONFIG=${1:?usage: $0 <Debug|Debug-ASan|Release|Distribution>}
SECONDS_PER_RUN=${SMOKE_SECONDS:-20}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BIN=$ROOT/build/bin/$CONFIG-linux-x86_64
LOGS=$ROOT/build/smoke/$CONFIG
CLOSE="python3 $ROOT/scripts/ci/close-windows.py"
mkdir -p "$LOGS"

# Mesa and libxcb leak a few hundred bytes at swap-chain creation and X11 teardown.
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}

failures=0

run() {
	local name=$1 cwd=$2 exe=$3; shift 3
	local log=$LOGS/$name.log status problems=""

	(cd "$cwd" && exec "$exe" "$@") > "$log" 2>&1 &
	local pid=$!
	for ((i = 0; i < SECONDS_PER_RUN; i++)); do
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

	[[ $status -ne 0 ]] && problems+=" exit=$status"
	grep -q '\[FAIL\]' "$log" && problems+=" failed-checks=$(grep -c '\[FAIL\]' "$log")"
	grep -qE 'Validation Error|VUID-' "$log" && problems+=" validation-errors=$(grep -cE 'Validation Error|VUID-' "$log")"
	grep -q 'ERROR: AddressSanitizer' "$log" && problems+=" asan"
	grep -q 'LOG ERROR' "$log" && problems+=" log-error"

	local passed
	passed=$(grep -c '\[PASS\]' "$log")
	if [[ -n $problems ]]; then
		failures=$((failures + 1))
		echo "::error title=Smoke test $name ($CONFIG)::$problems"
		echo "FAIL  $name ($passed passed):$problems"
		grep -E '\[FAIL\]|Validation Error|VUID-|AddressSanitizer|LOG ERROR|Assertion failed' "$log" | head -20
	else
		echo "ok    $name ($passed passed)"
	fi
}

mapfile -t tests < <(grep -oP '^\s*m_Tests\.push_back\(\{ "\K[^"]+' "$ROOT/test/src/TestLayer.cpp")
for test in "${tests[@]}"; do
	run "test-${test//[^A-Za-z0-9]/_}" "$ROOT/test" "$BIN/Dingo-TestFramework/Dingo-TestFramework" "--test=$test"
done

for dir in "$ROOT"/examples/*/; do
	example=$(basename "$dir")
	run "example-$example" "$dir" "$BIN/$example/$example"
done

run "marionette-check" "$ROOT/examples/Marionette" "$BIN/Marionette/Marionette" --check

echo "${#tests[@]} test cases, $(ls -d "$ROOT"/examples/*/ | wc -l) examples, Marionette --check: $failures failed"
exit $((failures > 0))
