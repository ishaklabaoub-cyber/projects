#!/usr/bin/env bash
#
# run_tests.sh - end-to-end tests for moscar (scan_port, parsing, summary).
#
# It starts real listeners on localhost, scans them, and checks the output against
# known truth: the ports this script opened, and `ss -ltn`.
#
# Every scan run is checked for: a hang, a non-zero exit code, a sanitizer report,
# and a SUMMARY that adds up:
#     scanned == open + closed + filtered + unreachable
#
# Usage:
#   make test                                  # builds ./moscar_asan and runs this script
#   BIN=./moscar_asan bash run_tests.sh        # run it by hand
#   BIN=./moscar bash run_tests.sh             # test the normal build instead
#
# Needs: bash, python3, ss (iproute2), timeout. The no-route test also needs
# `unshare -rn` and is skipped when unavailable.
#
# Expected SUMMARY shape (edit the *_LABEL variables if yours changes):
#   SUMMARY: ports scanned[3] { open ports[2], closed ports[1], filtered ports[0],
#            unreachable ports[0]} --> time taken: [0.00 s]

BIN=${BIN:-./moscar_asan}
HOST=127.0.0.1

SCANNED_LABEL="ports scanned"
OPEN_LABEL="open ports"
CLOSED_LABEL="closed ports"
FILTERED_LABEL="filtered ports"
UNREACHABLE_LABEL="unreachable ports"

# Decisions about YOUR program's behavior. Change them here when you change the program.
HELP_EXIT=1             # exit code after --help / -h (many tools use 0)
ERR_EXIT=1              # exit code for rejected input (bad IP, bad option, bad port value)
DEFAULT_RANGE_COUNT=61  # ports scanned when -p is given with no value (default 20-80)
NO_P_COUNT=61           # ports scanned when NO -p is given at all (default range 20-80)

# Test ports: A and C get listeners; B and the rest of the range stay closed.
PORT_A=18080
PORT_B=18081
PORT_C=18082
SCAN_LO=$PORT_A
SCAN_HI=$((PORT_A + 9))

PASS=0
FAIL=0
RUNNER=""                # optional command prefix, e.g. "unshare -rn"
declare -A LISTENER_PID

# ---------------------------------------------------------------- helpers

pass() { PASS=$((PASS + 1)); printf '  ok    %s\n' "$1"; }
fail() {
    FAIL=$((FAIL + 1))
    printf '  FAIL  %s\n' "$1"
    [ -n "$2" ] && printf '        -> %s\n' "$2"
}

port_in_use() { ss -ltnH "sport = :$1" | grep -q .; }

start_listener() {
    python3 -m http.server "$1" --bind "$HOST" >/dev/null 2>&1 &
    LISTENER_PID[$1]=$!
    for _ in $(seq 1 50); do
        port_in_use "$1" && return 0
        sleep 0.1
    done
    echo "could not start a listener on port $1" >&2
    exit 2
}

stop_listener() {
    local pid=${LISTENER_PID[$1]}
    [ -z "$pid" ] && return
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
    unset 'LISTENER_PID[$1]'
    for _ in $(seq 1 50); do
        port_in_use "$1" || return 0
        sleep 0.1
    done
}

cleanup() {
    for p in "${LISTENER_PID[@]}"; do
        kill "$p" 2>/dev/null
    done
    wait 2>/dev/null
}
trap cleanup EXIT

# Number inside e.g. "open ports[2]" from the scanner's output.
field() { grep -o "$1\[[0-9]*\]" <<<"$OUT" | head -n1 | grep -o '[0-9]*'; }

# run_scan <args...>  ->  sets OUT ERR CODE ELAPSED S O C F U
run_scan() {
    local errf t0 t1
    errf=$(mktemp)
    t0=$(date +%s.%N)
    # shellcheck disable=SC2086   # RUNNER is meant to word-split ("unshare -rn")
    OUT=$(timeout 30 $RUNNER "$BIN" "$@" 2>"$errf" </dev/null)
    CODE=$?
    t1=$(date +%s.%N)
    ERR=$(<"$errf")
    rm -f "$errf"
    ELAPSED=$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f", b - a }')
    S=$(field "$SCANNED_LABEL")
    O=$(field "$OPEN_LABEL")
    C=$(field "$CLOSED_LABEL")
    F=$(field "$FILTERED_LABEL")
    U=$(field "$UNREACHABLE_LABEL")
}

sanitizer_hit() { grep -qE "Sanitizer|runtime error" <<<"$ERR"; }

# Problems that apply to every SUCCESSFUL scan run. Prints a description, or nothing.
base_checks() {
    local p="" total
    [ "$CODE" -eq 124 ] && p+="HANG (timeout); "
    [ "$CODE" -ne 0 ] && [ "$CODE" -ne 124 ] && p+="exit code $CODE (wanted 0); "
    sanitizer_hit && p+="sanitizer report on stderr; "
    if [ -z "$S" ]; then
        p+="no SUMMARY line found; "
    else
        total=$(( ${O:-0} + ${C:-0} + ${F:-0} + ${U:-0} ))
        [ "$S" -ne "$total" ] &&
            p+="summary does not add up: scanned=$S but open+closed+filtered+unreachable=$total; "
    fi
    printf '%s' "$p"
}

# expect <name> <scanned> <open> <closed>   (use "-" to skip a field)
expect() {
    local name=$1 es=$2 eo=$3 ec=$4 problems
    problems=$(base_checks)
    [ "$es" != "-" ] && [ "$S" != "$es" ] && problems+="scanned=$S (wanted $es); "
    [ "$eo" != "-" ] && [ "$O" != "$eo" ] && problems+="open=$O (wanted $eo); "
    [ "$ec" != "-" ] && [ "$C" != "$ec" ] && problems+="closed=$C (wanted $ec); "
    if [ -z "$problems" ]; then pass "$name"; else fail "$name" "$problems"; fi
}

# expect_scan <name> <ports-scanned> <args...> : succeeds and scans exactly that many ports
expect_scan() {
    local name=$1 n=$2
    shift 2
    run_scan "$@"
    expect "$name" "$n" - -
}

# expect_reject <name> <args...> : bad input must exit with ERR_EXIT and must NOT start a scan
expect_reject() {
    local name=$1 p=""
    shift
    run_scan "$@"
    [ "$CODE" -eq 124 ] && p+="HANG (timeout); "
    [ "$CODE" -ne "$ERR_EXIT" ] && [ "$CODE" -ne 124 ] && p+="exit code $CODE (wanted $ERR_EXIT); "
    sanitizer_hit && p+="sanitizer report on stderr; "
    grep -q "SUMMARY" <<<"$OUT" && p+="a scan ran despite bad input; "
    if [ -z "$p" ]; then pass "$name"; else fail "$name" "$p"; fi
}

# expect_help <name> <args...> : prints usage, exits with HELP_EXIT, does not scan
expect_help() {
    local name=$1 p=""
    shift
    run_scan "$@"
    [ "$CODE" -eq 124 ] && p+="HANG (timeout); "
    [ "$CODE" -ne "$HELP_EXIT" ] && p+="exit code $CODE (wanted $HELP_EXIT); "
    sanitizer_hit && p+="sanitizer report on stderr; "
    grep -qi "usage" <<<"$OUT" || p+="no usage text printed; "
    grep -q "SUMMARY" <<<"$OUT" && p+="a scan ran after help; "
    if [ -z "$p" ]; then pass "$name"; else fail "$name" "$p"; fi
}

# Sorted, space-separated list of ports the scanner reported open.
scanner_open_ports() {
    grep -oE 'port [0-9]+ is open' <<<"$OUT" | awk '{print $2}' | sort -n | tr '\n' ' '
}

# Same, from `ss`. Only IPv4-reachable listeners count (the scanner is IPv4 only).
ss_open_ports() {
    ss -ltnH "( sport >= :$1 and sport <= :$2 )" |
        awk '$4 ~ /^(127\.0\.0\.1|0\.0\.0\.0|\*):/ {print $4}' |
        sed 's/.*://' | sort -nu | tr '\n' ' '
}

# ------------------------------------------------------------ preflight

if [ ! -x "$BIN" ]; then
    echo "binary '$BIN' not found or not executable (run 'make test', or build it first)" >&2
    exit 2
fi
for tool in ss python3 timeout; do
    command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 2; }
done
for p in $(seq "$SCAN_LO" "$SCAN_HI"); do
    if port_in_use "$p"; then
        echo "port $p is already in use; change PORT_A in this script" >&2
        exit 2
    fi
done

start_listener "$PORT_A"
start_listener "$PORT_C"

# ----------------------------------------------------------------- tests

echo "== single ports =="
run_scan "$HOST" -p "$PORT_A"
expect "open port is counted open" 1 1 0
grep -q "port $PORT_A is open" <<<"$OUT" \
    && pass "open port is printed by number" \
    || fail "open port is printed by number" "no 'port $PORT_A is open' line"
grep -qE 'time taken: \[[0-9.]+ s\]' <<<"$OUT" \
    && pass "summary reports time taken" \
    || fail "summary reports time taken" "no 'time taken: [N s]' in summary"

run_scan "$HOST" -p "$PORT_B"
expect "closed port is counted closed" 1 0 1
grep -q "is open" <<<"$OUT" \
    && fail "closed port prints nothing as open" "an 'is open' line was printed" \
    || pass "closed port prints nothing as open"

echo "== ranges and argument order =="
expect_scan "range with 2 open + 1 closed" 3 "$HOST" -p "$PORT_A-$PORT_C"
expect "  ...and the counts are right" 3 2 1    # reuses that run's results
expect_scan "range of one port (N-N)" 1 "$HOST" -p "$PORT_A-$PORT_A"
expect_scan "long option --port" 3 "$HOST" --port "$PORT_A-$PORT_C"
expect_scan "target before options" 3 "$HOST" -p "$PORT_A-$PORT_C"
expect_scan "options before target" 3 -p "$PORT_A-$PORT_C" "$HOST"

echo "== boundaries =="
expect_scan "lowest port (1)" 1 "$HOST" -p 1
expect_scan "highest port (65535)" 1 "$HOST" -p 65535
expect_scan "ports 1-3" 3 "$HOST" -p 1-3

echo "== lenient parsing (design choices - edit the numbers if you change a rule) =="
expect_scan "reversed range is swapped (90-80)" 11 "$HOST" -p 90-80
expect_scan "trailing text ignored (80abc -> 80)" 1 "$HOST" -p 80abc
expect_scan "extra range part ignored (1-2-3 -> 1-2)" 2 "$HOST" -p 1-2-3
expect_scan "above 65535 is clamped (65536 -> 65535)" 1 "$HOST" -p 65536
expect_scan "leading minus ignored (-5 -> 5)" 1 "$HOST" -p -5
expect_scan "-p with no value uses the default range" "$DEFAULT_RANGE_COUNT" "$HOST" -p

echo "== help =="
expect_help "--help" --help
expect_help "-h" -h
expect_help "target then -h" "$HOST" -h
expect_help "-h then target" -h "$HOST"

echo "== bad input is rejected and never scans =="
expect_reject "no arguments"
expect_reject "invalid IP (999.1.1.1)" 999.1.1.1
expect_reject "not an IP (abc)" abc
expect_reject "unknown short option (-x)" "$HOST" -x
expect_reject "unknown long option (--foo)" "$HOST" --foo
expect_reject "port value 0" "$HOST" -p 0
expect_reject "port value abc" "$HOST" -p abc
expect_reject "empty port value" "$HOST" -p ""
expect_reject "two targets" "$HOST" 127.0.0.2 -p 80    # delete if you add support
expect_reject "option used as a port value (-p -p)" "$HOST" -p -p
expect_reject "option used as a port value (-p -h)" "$HOST" -p -h
expect_reject "junk argument after a valid option" "$HOST" -p 80 abc
expect_reject "unknown option after a valid one" "$HOST" -p 80 -x
expect_reject "unknown option before a valid one" "$HOST" -x -p 80

echo "== several options =="
expect_help "-h after another option" "$HOST" -p 80 -h
expect_help "--help after another option" "$HOST" -p 80 --help
expect_scan "-p twice: the last one wins" 2 "$HOST" -p 80 -p 90-91
expect_scan "-p then --port: the last one wins" 2 "$HOST" -p 80 --port 81-82
if [ -n "$NO_P_COUNT" ]; then
    expect_scan "no -p: default range is used" "$NO_P_COUNT" "$HOST"
else
    echo "  skip  no -p default (set NO_P_COUNT once you decide the default range)"
fi

echo "== ground truth (ss) =="
run_scan "$HOST" -p "$SCAN_LO-$SCAN_HI"
expect "10-port range completes and adds up" 10 - -
want=$(ss_open_ports "$SCAN_LO" "$SCAN_HI")
got=$(scanner_open_ports)
if [ "$want" = "$got" ]; then
    pass "open ports match ss  [${got% }]"
else
    fail "open ports match ss" "ss says [${want% }] but scanner says [${got% }]"
fi

echo "== no response (filtered / timeout path) =="
# 192.0.2.1 is reserved for documentation: nothing should ever answer.
run_scan 192.0.2.1 -p 80
expect "unanswered port: scanned 1, none open" 1 0 -
awk -v t="$ELAPSED" 'BEGIN { exit !(t < 5) }' \
    && pass "unanswered port finishes quickly (${ELAPSED}s)" \
    || fail "unanswered port finishes quickly" "took ${ELAPSED}s"

echo "== no route (unreachable path) =="
if unshare -rn true 2>/dev/null; then       # needs unprivileged user namespaces
    RUNNER="unshare -rn"
    run_scan 192.0.2.1 -p 80-82
    RUNNER=""
    expect "no route: scan completes and adds up" 3 0 0
    [ "$U" = "3" ] \
        && pass "no route: all 3 ports counted unreachable" \
        || fail "no route: all 3 ports counted unreachable" "unreachable=${U:-missing}"
else
    echo "  skip  (unshare -rn is not available on this system)"
fi

echo "== listener goes away =="
stop_listener "$PORT_C"
run_scan "$HOST" -p "$PORT_C"
expect "stopped listener is now closed" 1 0 1

# ---------------------------------------------------------------- result

echo
echo "passed: $PASS   failed: $FAIL"
[ "$FAIL" -eq 0 ]
