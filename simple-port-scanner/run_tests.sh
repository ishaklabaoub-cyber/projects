#!/bin/bash
BIN=./scanner

run() {
    echo "=== $*"
    timeout 10 "$BIN" "$@" >/tmp/out.txt 2>/tmp/err.txt </dev/null
    code=$?
    [ $code -eq 124 ] && echo "  !! HANG (timeout)"
    grep -qE "Sanitizer|runtime error" /tmp/err.txt && echo "  !! SANITIZER REPORT"
    echo "  exit code: $code"
    head -n 3 /tmp/out.txt | sed 's/^/  out: /'
    head -n 3 /tmp/err.txt | sed 's/^/  err: /'
}

# --- arguments / target ---
run
run ""
run 999.1.1.1
run abc
run 127.0.0.1.5
run 127.0.0.1 127.0.0.2

# --- options ---
run --help
run -h
run 127.0.0.1 -h
run -h 127.0.0.1
run -h -h
run -x
run 127.0.0.1 -x
run 127.0.0.1 --foo
run 127.0.0.1 -
run 127.0.0.1 --
run 127.0.0.1 -pp 80
run 127.0.0.1 --helpme
run 127.0.0.1 --$(python3 -c "print('a'*5000)")

# --- port values ---
run 127.0.0.1 -p
run -p 80-90 127.0.0.1
run 127.0.0.1 -p 80
run 127.0.0.1 -p 80-80
run 127.0.0.1 -p 90-80
run 127.0.0.1 -p 0
run 127.0.0.1 -p 0-5
run 127.0.0.1 -p 65535
run 127.0.0.1 -p 65536
run 127.0.0.1 -p 1-70000
run 127.0.0.1 -p -5
run 127.0.0.1 -p abc
run 127.0.0.1 -p 80abc
run 127.0.0.1 -p ""
run 127.0.0.1 -p 80-
run 127.0.0.1 -p ,
run 127.0.0.1 -p 1-2-3
run 127.0.0.1 -p "22 80"
run 127.0.0.1 -p 20-22 -p 40-42
run 127.0.0.1 --port 20-22
run 127.0.0.1
