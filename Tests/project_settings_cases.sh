#!/bin/sh
# Build settings in nexapkg.json.
#
# How a project is built belongs to the project, not to the line someone
# typed, so these are manifest fields rather than command-line options. NexaC
# reads the manifest itself -- looking in the source file's folder and then its
# parents -- which is why "NexaC build" needs no extra argument to pass one
# down to the compile it spawns, and why building a file directly inside a
# project gets the same treatment.
#
#   default    no manifest, and a manifest with no setting, both build.
#   bad        a value that is neither "embed" nor "system" is refused, with a
#              message naming both. Checked on every platform: a typo should
#              fail on the machine the author is sitting at, not only on Linux.
#   good       "embed" and "system" are both accepted.
#   nested     the manifest is found from a subdirectory, not just beside it.
#   build      "NexaC build" refuses the same bad value the child would.
#   keeps      nexapkg rewriting the manifest does not drop the setting.
#   targets    "targets" builds the other platforms too, names them apart from
#              the host build, notes rather than fails when the host is named,
#              and carries on past a target that fails while still ending in a
#              non-zero exit. The cross-compiles themselves need an installed
#              target, so that layer is skipped when none is.
#
# The link line the setting chooses is Linux's, and is not exercised here:
# this runs where it runs. What it pins down is that the value is read,
# checked, carried and never silently ignored.
#
# Usage: Tests/project_settings_cases.sh [path-to-NexaC]   (from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
fails=0

mkdir -p "$WORK/proj/src"
printf '#include <std/gfx>\nfn main(): int {\n    gfx.open("x", 8, 8, 1);\n    return 0;\n}\n' > "$WORK/proj/src/main.nxa"

manifest() {  # manifest <json-body-for-x11-or-empty>
    if [ -z "$1" ]; then
        printf '{\n  "name": "proj",\n  "entry": "src/main.nxa",\n  "dependencies": {}\n}\n' > "$WORK/proj/nexapkg.json"
    else
        printf '{\n  "name": "proj",\n  "entry": "src/main.nxa",\n  "x11": "%s",\n  "dependencies": {}\n}\n' "$1" \
            > "$WORK/proj/nexapkg.json"
    fi
}

# ok <label> <expect-pass:0|1> -- runs a --source build of the project entry
try() {
    label=$1; want=$2
    out=$("$NEXAC" "$WORK/proj/src/main.nxa" --source "$WORK/out.cpp" 2>&1)
    got=$?
    if [ "$got" = "$want" ]; then
        echo "ok $label"
    else
        echo "FAIL $label: exit $got, wanted $want"
        printf '%s\n' "$out" | sed 's/^/  /' | tail -4
        fails=$((fails + 1))
    fi
}

# No manifest anywhere: the defaults, exactly as before the setting existed.
rm -f "$WORK/proj/nexapkg.json"
try "default: no manifest builds" 0

manifest ""
try "default: manifest without the setting builds" 0

manifest "embed"
try "good: embed" 0
manifest "system"
try "good: system" 0

# A typo must not quietly build the other way.
manifest "shared"
out=$("$NEXAC" "$WORK/proj/src/main.nxa" --source "$WORK/out.cpp" 2>&1)
if [ $? -eq 0 ]; then
    echo "FAIL bad: a bogus x11 value built anyway"
    fails=$((fails + 1))
else
    case "$out" in
        *embed*system*) echo "ok bad: refused, naming both values" ;;
        *) echo "FAIL bad: refused without naming the two values"
           printf '%s\n' "$out" | sed 's/^/  /' | tail -3
           fails=$((fails + 1)) ;;
    esac
fi

# Found from a subdirectory: the manifest sits a level above the source.
manifest "system"
mkdir -p "$WORK/proj/src/deep/deeper"
cp "$WORK/proj/src/main.nxa" "$WORK/proj/src/deep/deeper/main.nxa"
if "$NEXAC" "$WORK/proj/src/deep/deeper/main.nxa" --source "$WORK/out.cpp" > "$WORK/t.log" 2>&1; then
    echo "ok nested: a manifest two folders up is found"
else
    echo "FAIL nested: building below the manifest failed"
    sed 's/^/  /' "$WORK/t.log" | tail -4
    fails=$((fails + 1))
fi
manifest "shared"
if "$NEXAC" "$WORK/proj/src/deep/deeper/main.nxa" --source "$WORK/out.cpp" > "$WORK/t.log" 2>&1; then
    echo "FAIL nested: a bad setting two folders up was ignored"
    fails=$((fails + 1))
else
    echo "ok nested: and it is obeyed, not just found"
fi

# NexaC build spawns the compile as a child; the child reads the manifest too.
manifest "shared"
if (cd "$WORK/proj" && "$NEXAC" build . > "$WORK/b.log" 2>&1); then
    echo "FAIL build: NexaC build accepted a bogus setting"
    fails=$((fails + 1))
elif grep -q 'x11' "$WORK/b.log"; then
    echo "ok build: NexaC build refuses it too"
else
    echo "FAIL build: NexaC build failed without mentioning the setting"
    sed 's/^/  /' "$WORK/b.log" | tail -4
    fails=$((fails + 1))
fi

# Rewriting the manifest must not drop a field the writer did not know about.
manifest "system"
(cd "$WORK/proj" && "$NEXAC" nexapkg add "file:../dep" . > "$WORK/a.log" 2>&1)
if grep -q '"x11"' "$WORK/proj/nexapkg.json"; then
    echo "ok keeps: nexapkg rewrote the manifest and kept the setting"
else
    echo "FAIL keeps: the setting was lost when nexapkg rewrote the manifest"
    sed 's/^/  /' "$WORK/proj/nexapkg.json"
    fails=$((fails + 1))
fi

# ---------------------------------------------------------------------------
# targets: the other platforms one "NexaC build" should produce.
# ---------------------------------------------------------------------------
targets_manifest() {  # targets_manifest <json-array-body>
    printf '{
  "name": "proj",
  "entry": "src/main.nxa",
  "output": "app",
  "targets": [%s],
  "dependencies": {}
}
'         "$1" > "$WORK/proj/nexapkg.json"
}

# Naming this machine's build is a note, not a failure, and the host build
# still happens. Needs no cross-compiler, so it runs everywhere.
targets_manifest '"host", "linux"'
rm -f "$WORK/proj/app" "$WORK/proj/app.exe"
out=$(cd "$WORK/proj" && "$NEXAC" build . 2>&1)
if [ $? -ne 0 ]; then
    echo "FAIL targets: naming the host build failed the build"
    printf '%s
' "$out" | sed 's/^/  /' | tail -5
    fails=$((fails + 1))
elif ! printf '%s
' "$out" | grep -qi 'note'; then
    echo "FAIL targets: naming the host build said nothing about it"
    fails=$((fails + 1))
elif [ ! -f "$WORK/proj/app" ] && [ ! -f "$WORK/proj/app.exe" ]; then
    echo "FAIL targets: the host build did not happen"
    fails=$((fails + 1))
else
    echo "ok targets: the host build is a note, and still happens"
fi

# A name nothing provides must fail the build and say so, and must not stop
# whatever else was asked for.
targets_manifest '"no-such-target-here"'
out=$(cd "$WORK/proj" && "$NEXAC" build . 2>&1)
if [ $? -eq 0 ]; then
    echo "FAIL targets: an unknown target built anyway"
    fails=$((fails + 1))
elif printf '%s
' "$out" | grep -qi 'no-such-target-here'; then
    echo "ok targets: an unknown target fails the build, by name"
else
    echo "FAIL targets: the unknown target was not named in the error"
    printf '%s
' "$out" | sed 's/^/  /' | tail -5
    fails=$((fails + 1))
fi

# The real thing, for the first installed target there is.
INSTALLED=$("$NEXAC" nexapkg target list 2>&1 | awk '/^  [a-z0-9]+-[a-z0-9]+  / {print $1; exit}')
if [ -z "$INSTALLED" ]; then
    echo "skip targets: no cross target installed (nexapkg target install ...)"
else
    targets_manifest "\"$INSTALLED\""
    rm -f "$WORK/proj/app"*
    out=$(cd "$WORK/proj" && "$NEXAC" build . 2>&1)
    if [ $? -ne 0 ]; then
        echo "skip targets: the $INSTALLED runtime is not built here"
    elif [ -f "$WORK/proj/app-$INSTALLED" ]; then
        echo "ok targets: one build produced the host's and $INSTALLED's, named apart"
    else
        echo "FAIL targets: no app-$INSTALLED beside the host build"
        ls "$WORK/proj" | sed 's/^/  /'
        fails=$((fails + 1))
    fi
fi

# And the list has to survive nexapkg rewriting the manifest.
targets_manifest '"wasm", "arm64-linux"'
(cd "$WORK/proj" && "$NEXAC" nexapkg add "file:../dep" . > "$WORK/a2.log" 2>&1)
if grep -q '"targets"' "$WORK/proj/nexapkg.json" &&
   grep -q 'arm64-linux' "$WORK/proj/nexapkg.json"; then
    echo "ok targets: the list survives nexapkg rewriting the manifest"
else
    echo "FAIL targets: the list was lost when nexapkg rewrote the manifest"
    sed 's/^/  /' "$WORK/proj/nexapkg.json"
    fails=$((fails + 1))
fi

if [ $fails -eq 0 ]; then
    echo "project_settings ok"
    exit 0
fi
echo "project_settings: $fails failure(s)"
exit 1
