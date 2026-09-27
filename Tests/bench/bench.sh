#!/bin/sh
# How fast a Nexa program runs: time Tests/bench/bench.nxa built two ways.
#
#   sh Tests/bench/bench.sh ./NexaC                  slim runtime vs stock
#   sh Tests/bench/bench.sh ./NexaC-old ./NexaC      one NexaC vs another
#
# With one NexaC it compares that NexaC's normal build with NEXA_NO_SLIM=1, the
# check that the size work costs no speed. With two it compares their builds of
# the same program: a baseline, then a change.
#
# Each workload in bench.nxa times itself. The two programs run in turn, RUNS
# times each (default 9), so drift on the machine lands on both; the table shows
# each workload's median and the change from the first build to the second.
#
# Not part of Tests/run_tests.sh: a timing is not a pass or a fail. Differences
# under about 5% are within what code layout alone moves (an unrelated function
# that shifts the alignment of a hot loop); rerun before believing one. A build
# that computed something different -- the checksum on the last line -- is an
# error, and so is a failed build.
#
# Usage: Tests/bench/bench.sh [--runs N] NEXAC [NEXAC2]

set -u
# Set at all, even empty, NEXA_NO_SLIM builds with the stock runtime.
unset NEXA_NO_SLIM

RUNS=9
if [ "${1:-}" = "--runs" ]; then RUNS=$2; shift 2; fi
if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi

abspath() {
    case "$1" in
        /*) echo "$1" ;;
        *) echo "$(pwd)/$1" ;;
    esac
}
A=$(abspath "$1")
B=$(abspath "${2:-$1}")
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexabench)
trap 'rm -rf "$WORK"' EXIT

EXE=""
case "$(uname -s 2>/dev/null)" in MINGW*|MSYS*|CYGWIN*) EXE=".exe" ;; esac

if [ $# -eq 1 ]; then
    NAME_A="stock"; NAME_B="slim"
    ENV_A="NEXA_NO_SLIM=1"; ENV_B="NEXA_BENCH=1"
else
    NAME_A="first"; NAME_B="second"
    ENV_A="NEXA_BENCH=1"; ENV_B="NEXA_BENCH=1"
fi

build() {  # build NEXAC ENV OUT
    if ! env "$2" "$1" "$HERE/bench.nxa" -o "$3" >"$3.log" 2>&1; then
        echo "build failed with $1:"
        tail -5 "$3.log"
        exit 1
    fi
}
build "$A" "$ENV_A" "$WORK/a$EXE"
build "$B" "$ENV_B" "$WORK/b$EXE"

i=0
while [ "$i" -lt "$RUNS" ]; do
    "$WORK/a$EXE" </dev/null | tr -d '\r' >>"$WORK/a.times"
    "$WORK/b$EXE" </dev/null | tr -d '\r' >>"$WORK/b.times"
    i=$((i + 1))
done

ca=$(grep '^check ' "$WORK/a.times" | sort -u)
cb=$(grep '^check ' "$WORK/b.times" | sort -u)

median() {  # median WORKLOAD FILE
    grep "^$1 " "$2" | cut -d' ' -f2 | sort -n |
        awk '{ v[NR] = $1 } END { if (NR % 2) print v[(NR + 1) / 2]; else print (v[NR / 2] + v[NR / 2 + 1]) / 2 }'
}

echo "$RUNS runs each, median ms"
printf '%-10s %10s %10s %8s\n' workload "$NAME_A" "$NAME_B" change
for w in $(grep -v '^check ' "$WORK/a.times" | cut -d' ' -f1 | awk '!seen[$0]++'); do
    ma=$(median "$w" "$WORK/a.times")
    mb=$(median "$w" "$WORK/b.times")
    awk -v w="$w" -v a="$ma" -v b="$mb" \
        'BEGIN { printf "%-10s %10.1f %10.1f %+7.1f%%\n", w, a, b, (a > 0 ? (b - a) / a * 100 : 0) }'
done
sa=$(wc -c <"$WORK/a$EXE" | tr -d ' ')
sb=$(wc -c <"$WORK/b$EXE" | tr -d ' ')
printf '%-10s %10s %10s\n' size "$sa" "$sb"

if [ "$ca" != "$cb" ] || [ "$(echo "$ca" | wc -l)" -ne 1 ]; then
    echo "ERROR: the builds computed different results: [$ca] vs [$cb]"
    exit 1
fi
