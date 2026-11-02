#!/usr/bin/env bash
# Usage: scripts/ci/crash-reporting-check.sh <Debug|Release|Distribution>
#
# End-to-end check of a --crash-reporting build on Linux against a local mock Sentry server: FlappyBird
# crashes with reporting on, and the next launch must send that crash once (with its log attached)
# plus the demo's error event; a third launch must not send it again; a crash from a run with
# reporting off must never be sent. Needs an X display (DISPLAY) and python3; logs go to
# build/crash-reporting/<config>/.
set -uo pipefail

CONFIG=${1:?usage: $0 <Debug|Release|Distribution>}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
EXE=$ROOT/build/bin/$CONFIG-linux-x86_64/FlappyBird/FlappyBird
OUT=$ROOT/build/crash-reporting/$CONFIG
PORT=${CRASH_CHECK_PORT:-18765}
DSN="http://0123456789abcdef0123456789abcdef@127.0.0.1:$PORT/1"
REQUESTS=$OUT/requests.txt
rm -rf "$OUT" && mkdir -p "$OUT"
: > "$REQUESTS"

python3 - "$PORT" "$REQUESTS" <<'EOF' &
import http.server, sys
port, out = int(sys.argv[1]), sys.argv[2]
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode("latin1")
        tags = [name for name, marker in (("crash", "crash.previous_run"), ("error", "Test error from Flappy Bird"),
                                          ("attachment", '"type":"attachment"')) if marker in body]
        with open(out, "a") as f:
            f.write(f"{self.path} bytes={len(body)} {' '.join(tags)}\n")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", "2")
        self.end_headers()
        self.wfile.write(b"{}")
    def log_message(self, *args):
        pass
http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
EOF
server=$!
trap 'kill $server 2>/dev/null' EXIT
sleep 1

failures=0
fail() { failures=$((failures + 1)); echo "::error title=Crash reporting ($CONFIG)::$1"; echo "FAIL  $1"; }

# name, expected exit, log dir, then FlappyBird's arguments.
launch() {
	local name=$1 expected=$2 dir=$3; shift 3
	(cd "$ROOT/examples/FlappyBird" && exec timeout -k 5 60 "$EXE" "--log-dir=$dir" "$@") > "$OUT/$name.log" 2>&1
	local status=$?
	if [[ $status -ne $expected ]]; then fail "$name: exit $status, expected $expected"; tail -20 "$OUT/$name.log"; fi
}
count() { grep -c -- "$1" "$REQUESTS"; }

on=$OUT/reporting-on
launch crash 139 "$on" --sentry-dsn="$DSN" --sentry-attachments --crash-demo=crash
grep -q '^\*\*\* Crash reporting: on' "$on/Dingo.log" || fail "crash: no '*** Crash reporting: on' line in the log"
grep -q 'End of backtrace' "$on/Dingo.log" || fail "crash: no finished crash report in the log"
[[ $(count crash) -eq 0 ]] || fail "crash: the crashing process sent the crash itself"

launch upload 0 "$on" --sentry-dsn="$DSN" --sentry-attachments --crash-demo=error
[[ $(count crash) -eq 1 ]] || fail "upload: $(count crash) crash events, expected 1"
grep crash "$REQUESTS" | grep -q attachment || fail "upload: the crash event carries no attachment"
[[ $(count error) -ge 1 ]] || fail "upload: no error event"
grep -q . "$on/Dingo-reported-crashes.txt" 2>/dev/null || fail "upload: Dingo-reported-crashes.txt is empty"

launch relaunch 0 "$on" --sentry-dsn="$DSN" --crash-demo=error
[[ $(count crash) -eq 1 ]] || fail "relaunch: the crash was sent again ($(count crash) crash events)"

off=$OUT/reporting-off
launch crash-off 139 "$off" --crash-demo=crash
grep -q '^\*\*\* Crash reporting: on' "$off/Dingo.log" && fail "crash-off: the log claims reporting was on"
launch upload-off 0 "$off" --sentry-dsn="$DSN" --crash-demo=error
[[ $(count crash) -eq 1 ]] || fail "upload-off: a crash from a run with reporting off was sent"

echo "---- requests ----"
cat "$REQUESTS"
if ((failures)); then
	echo "$failures check(s) failed; logs in $OUT"
	exit 1
fi
echo "ok    crash reporting ($CONFIG)"
