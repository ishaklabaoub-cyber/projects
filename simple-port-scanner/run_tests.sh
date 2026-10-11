#!/usr/bin/env bash
#
# run_tests.sh - end-to-end tests for the port scanner (and its scan_port logic).
#
# It starts real listeners on localhost, scans them, and checks the scanner's
# output against known truth: the ports this script opened, and `ss -ltn`.
# Every run is also checked for: hangs, a non-zero exit code, sanitizer
# reports, and a SUMMARY line that adds up (scanned == open + closed + filtered).
#
# Usage:
#   gcc -g -Wall -Wextra -fsanitize=address,undefined simple-port-scanner.c -o scanner
#   bash run_tests.sh                      # uses ./scanner
#   BIN=./scanner_plain bash run_tests.sh  # use another binary
#
# If your SUMMARY line is worded differently, edit the *_LABEL variables below.
# Expected summary shape:  SUMMARY: ports scanned[3] , open ports[2], closed ports[1]
# (a "filtered ports[N]" counter is optional until you add it)

BIN=${BIN:-./scanner}
HOST=127.0.0.1

SCANNED_LABEL="ports scanned"
OPEN_LABEL="open ports"
CLOSED_LABEL="closed ports"
FILTERED_LABEL="filtered ports"

# Test ports: A and C get listeners, B and the rest stay closed.
PORT_A=18080
PORT_B=18081
PORT_C=18082
SCAN_LO=$PORT_A
SCAN_HI=$((PORT_A + 9))

PASS=0
FAIL=0
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

# run_scan <args...>  ->  sets OUT ERR CODE ELAPSED S O C F
run_scan() {
    local errf t0 t1
    errf=$(mktemp)
    t0=$(date +%s.%N)
    OUT=$(timeout 30 "$BIN" "$@" 2>"$errf" </dev/null)
    CODE=$?
    t1=$(date +%s.%N)
    ERR=$(<"$errf")
    rm -f "$errf"
    ELAPSED=$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f", b - a }')
    S=$(field "$SCANNED_LABEL")
    O=$(field "$OPEN_LABEL")
    C=$(field "$CLOSED_LABEL")
    F=$(field "$FILTERED_LABEL")
}

# Problems that apply to EVERY scan run. Prints a description, or nothing.
base_checks() {
    local p="" total
    [ "$CODE" -eq 124 ] && p+="HANG (timeout); "
    [ "$CODE" -ne 0 ] && [ "$CODE" -ne 124 ] && p+="exit code $CODE (wanted 0); "
    grep -qE "Sanitizer|runtime error" <<<"$ERR" && p+="sanitizer report on stderr; "
    if [ -z "$S" ]; then
        p+="no SUMMARY line found; "
    else
        total=$(( ${O:-0} + ${C:-0} + ${F:-0} ))
        [ "$S" -ne "$total" ] &&
            p+="summary does not add up: scanned=$S but open+closed+filtered=$total; "
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
    echo "scanner binary '$BIN' not found or not executable (build it first)" >&2
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

run_scan "$HOST" -p "$PORT_B"
expect_reject "junk argument after a valid option" "$HOST" -p 80 abc
expect "closed port is counted closed" 1 0 1
grep -q "is open" <<<"$OUT" \
    && fail "closed port prints nothing as open" "an 'is open' line was printed" \
    || pass "closed port prints nothing as open"

echo "== ranges =="
run_scan "$HOST" -p "$PORT_A-$PORT_C"
expect "range with 2 open + 1 closed" 3 2 1

# Design choice: reversed ranges are swapped. Delete this test if you
# decide to reject them instead.
run_scan "$HOST" -p "$PORT_C-$PORT_A"
expect "reversed range is normalized" 3 2 1

run_scan "$HOST" -p "$PORT_A-$PORT_A"
expect "range of one port (N-N)" 1 1 0

echo "== boundaries =="
run_scan "$HOST" -p 1
expect "lowest port (1) is scanned" 1 - -
run_scan "$HOST" -p 65535
expect "highest port (65535) is scanned" 1 - -
run_scan "$HOST" -p 1-3
expect "ports 1-3 counted" 3 - -

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

echo "== listener goes away =="
stop_listener "$PORT_C"
run_scan "$HOST" -p "$PORT_C"
expect "stopped listener is now closed" 1 0 1

# ---------------------------------------------------------------- result

echo
echo "passed: $PASS   failed: $FAIL"
[ "$FAIL" -eq 0 ]
