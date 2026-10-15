# moscar

A small TCP port scanner for the command line, written in C.

`moscar` tries to open a TCP connection to each port in a range and reports
which ports accept it. It uses non-blocking sockets and `poll()`, so a silent
port costs a short timeout instead of hanging the whole scan.

**Status: v0.1.** It is simple on purpose: one target, one range, one probe at
a time. See [Limitations](#limitations) and the [Roadmap](#roadmap).

> **Use it only on systems you own or have explicit permission to scan.**
> Scanning other people's machines can break their rules and, depending on
> where you are, the law. Good targets for testing: `127.0.0.1`, your own VMs
> and containers, and `scanme.nmap.org` (which allows scanning).

## Build

Requires Linux and a C compiler (gcc).

```
make            # builds ./moscar
make test       # builds a sanitizer binary and runs the test suite
make lint       # optimized build with extra warnings and gcc's static analyzer
make clean
```

## Usage

```
moscar <ipv4> [-p N | -p N-M]
moscar --help
```

| Option | Meaning |
|---|---|
| `-p`, `--port` | Port or port range to scan: `N` or `N-M` (1-65535) |
| `-h`, `--help` | Show the help text and exit |

The target must be an IPv4 address. Options can come before or after it.

### Examples

Start a throwaway listener in one terminal:

```
python3 -m http.server 18090 --bind 127.0.0.1
```

Scan a range around it:

```
$ ./moscar 127.0.0.1 -p 18089-18091
127.0.0.1 is a valid IPv4 address.
port 18090 is open on 127.0.0.1

SUMMARY: ports scanned[3] { open ports[1], closed ports[2], filtered ports[0], unreachable ports[0]} --> time taken: [0.00 s]
```

Only open ports are listed. Everything else is counted in the summary.

A single closed port:

```
$ ./moscar 127.0.0.1 -p 5
127.0.0.1 is a valid IPv4 address.

SUMMARY: ports scanned[1] { open ports[0], closed ports[1], filtered ports[0], unreachable ports[0]} --> time taken: [0.00 s]
```

No `-p` at all scans the default range 20-80:

```
$ ./moscar 127.0.0.1
No port range is set.
Setting port range to [20,80].
...
```

### Reading the results

| Result | Meaning |
|---|---|
| **open** | The connection was accepted. |
| **closed** | The host actively refused the connection. |
| **filtered** | No answer within 300 ms. A firewall may be dropping packets, but see the note below. |
| **unreachable** | The network or host could not be reached (no route). |

The four counters add up to `ports scanned`. The exception is a probe that
ends in an unexpected error: it is reported on screen but not counted.

> **A "filtered" result is a weak signal.** It only means no reply arrived
> within 300 ms. A single lost packet can cause it, and moscar does not retry
> yet. If a port you expect to be open shows up as filtered, run the scan
> again.

## Parsing rules

Port parsing is deliberately forgiving:

- Without `-p`, or with a bare `-p` that has no value, the range is **20-80**.
- If `-p` is given more than once, **the last one wins**.
- A reversed range is swapped: `90-80` scans 80-90.
- Values above 65535 are clamped to 65535: `65536` scans port 65535.
- Trailing text after a number is ignored: `80abc` scans port 80, and
  `1-2-3` scans 1-2.
- A leading minus is ignored: `-5` scans port 5.
- A value with no usable number (`abc`, an empty string, or another option
  such as `-h` used as the value) is **rejected** with an error, and nothing is
  scanned.

## Exit codes

| Code | Meaning |
|---|---|
| `0` | The scan ran to completion. |
| `1` | Help was shown, or an error occurred: bad arguments, unknown option, invalid port value, or an unexpected system error while probing a port (the scan stops). |

Help and errors share exit code 1, so a script cannot tell them apart by the
code alone. The process never exits with `-1` (a shell would show that as 255).

### Internal return values (for contributors)

Option handlers (`help_opt`, `port_ran`, ...) and the argument-parsing
functions use their own three-way convention:

| Value | Meaning |
|---|---|
| `0` | Success, keep going. |
| `1` | Stop cleanly (for example after `--help`). |
| `-1` | Error, stop. |

`main` turns any non-zero result into exit code 1. These values are internal
and are not the process exit codes above.

## Testing

```
make test
```

`run_tests.sh` starts real listeners on localhost (ports 18080-18082), scans
them, and checks the results against what it opened and against `ss -ltn`. It
also checks option handling, rejected input, the timeout path, the no-route
path (using `unshare -rn`, skipped if unavailable), and that every summary adds
up. Each run is checked for hangs, exit codes, and sanitizer reports.

It needs `bash`, `python3`, `ss` (iproute2), and `timeout`. The binary under
test is built with AddressSanitizer and UndefinedBehaviorSanitizer, so memory
errors show up as test failures. Pass `BIN=./other-binary` to test a different
build.

## Limitations

- IPv4 only. IPv6 listeners are invisible to it.
- TCP connect scan only. No SYN scan, no UDP.
- One probe at a time. A silent (filtered) port costs 300 ms, so 1,000 of them
  take about five minutes.
- Fixed 300 ms timeout and no retries.
- One target per run: no hostnames, no CIDR ranges, no port lists.
- No service or version detection.

## Roadmap

- **v0.2:** comma-separated port lists (`22,80,443`), retries for
  unanswered probes, a configurable timeout.
- **Later:** concurrent probing, hostnames and CIDR targets, JSON output,
  SYN scanning.

## License

MIT. See [LICENSE](LICENSE).
