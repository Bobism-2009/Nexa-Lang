#!/bin/sh
# --target and `nexapkg target`: platforms installed as packages.
#
# Hermetic. HOME and USERPROFILE point at a scratch directory, so nothing here
# reads or writes the real ~/.nexa, and the target installed is a fixture made
# on the spot -- a target.json and an empty source list. No runtime is ever
# compiled: every case stops at --source or is refused before the build, which
# is where all of --target's decisions are made. Building a real target's
# runtime takes half a minute and a recent clang, and belongs to the target
# repository's own checks rather than to every run of this suite.
#
# Usage: Tests/target_cases.sh [path-to-NexaC]      (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
HOMEDIR="$WORK/home"
mkdir -p "$HOMEDIR"

fails=0

nexac() { HOME="$HOMEDIR" USERPROFILE="$HOMEDIR" "$NEXAC" "$@"; }

# expect <label> <text-the-output-must-contain> <command...>
expect() {
    label=$1; want=$2; shift 2
    out=$("$@" 2>&1)
    case "$out" in
        *"$want"*) ;;
        *) echo "FAIL $label: output lacks: $want"; printf '%s\n' "$out" | sed 's/^/  /' | head -8; fails=$((fails + 1)) ;;
    esac
}

printf '#include <std/io>\nfn main() {\n    io.println("hi");\n}\n' > "$WORK/io.nxa"
printf '#include <std/gfx>\nfn main() {\n    gfx.open("x", 10, 10, 1);\n}\n' > "$WORK/gfx.nxa"
printf '#include <std/io>\nfn main() {\n    io.println("n=" + io.to_int("42"));\n}\n' > "$WORK/exc.nxa"

# A fixture target: everything --target reads, and nothing to compile.
make_fixture() {
    d=$1; name=$2; exc=$3
    mkdir -p "$d/lists" "$d/src"
    : > "$d/lists/none.txt"
    cat > "$d/target.json" <<EOF
{
  "name": "$name",
  "version": "0.0.1",
  "description": "a fixture for Tests/target_cases.sh",
  "os": "linux",
  "triple": "aarch64-linux-musl",
  "exceptions": $exc,
  "modules": ["std/io"],
  "libraries": [ { "name": "c", "root": "src", "files": "lists/none.txt", "flags": [] } ]
}
EOF
}
make_fixture "$WORK/repo/fixture" fixture false

# --- before anything is installed ------------------------------------------------

# The four built in need no --target, and naming one is a mistake with a clear
# answer -- not a search of ~/.nexa/targets that finds nothing.
expect "built-in name"      "is built into NexaC, so it needs no --target"  nexac "$WORK/io.nxa" --target windows
expect "built-in: browser"  "For the browser, use --wasm"                    nexac "$WORK/io.nxa" --target wasm
expect "not installed"      "no target named 'fixture' is installed"         nexac "$WORK/io.nxa" --target fixture
expect "not installed: how" "nexapkg target install fixture"                 nexac "$WORK/io.nxa" --target fixture
expect "list, empty"        "No targets installed"                           nexac nexapkg target list

# --- installing one from a local directory -----------------------------------------

expect "install --from"     "Installed target fixture 0.0.1"  nexac nexapkg target install fixture --from "$WORK/repo"
if [ ! -f "$HOMEDIR/.nexa/targets/fixture/target.json" ]; then
    echo "FAIL install: nothing at ~/.nexa/targets/fixture/target.json"
    fails=$((fails + 1))
fi
expect "list"               "fixture  0.0.1"                  nexac nexapkg target list
expect "install twice"      "already installed"               nexac nexapkg target install fixture --from "$WORK/repo"

# --- what --target decides ----------------------------------------------------------

# The C++ is the Linux slice, whatever the host: the target said "os": "linux".
if nexac "$WORK/io.nxa" --target fixture --source "$WORK/io.cpp" > "$WORK/src.log" 2>&1; then
    if grep -q 'windows.h' "$WORK/io.cpp"; then
        echo "FAIL --source: the Windows slice was emitted for a linux target"
        fails=$((fails + 1))
    fi
else
    echo "FAIL --source: NexaC refused a program the target supports"
    sed 's/^/  /' "$WORK/src.log"
    fails=$((fails + 1))
fi

# A module the target does not list is refused by name, before any build.
expect "unsupported module" "does not support std/gfx"         nexac "$WORK/gfx.nxa" --target fixture --source "$WORK/g.cpp"
expect "names what it has"  "(it supports: std/io)"            nexac "$WORK/gfx.nxa" --target fixture --source "$WORK/g.cpp"
# And a program that throws is refused by a target built without exceptions.
expect "no exceptions"      "needs C++ exceptions"             nexac "$WORK/exc.nxa" --target fixture --source "$WORK/e.cpp"

expect "--run"              "cannot run a program built for fixture" nexac "$WORK/io.nxa" --target fixture --run
expect "with --wasm"        "cannot be combined with --wasm"   nexac "$WORK/io.nxa" --target fixture --wasm
expect "with --dll"         "cannot be combined with"          nexac "$WORK/io.nxa" --target fixture --dll

# --- a target.json that is wrong ----------------------------------------------------

T="$HOMEDIR/.nexa/targets/fixture/target.json"
cp "$T" "$WORK/good.json"

# Broken JSON names the line, so it can be found.
printf '{\n  "name": "fixture",\n  "version": 1.0.0\n}\n' > "$T"
expect "bad json"           "line 3"                            nexac "$WORK/io.nxa" --target fixture --source "$WORK/b.cpp"
expect "bad json: fix"      "nexapkg target install fixture --force" nexac "$WORK/io.nxa" --target fixture --source "$WORK/b.cpp"

# A target.json saying it is some other target than the one it is installed as.
sed 's/"name": "fixture"/"name": "impostor"/' "$WORK/good.json" > "$T"
expect "name mismatch"      "installed as \"fixture\""          nexac "$WORK/io.nxa" --target fixture --source "$WORK/m.cpp"

# A library can be built only for programs that include a module ("when"),
# but not the one the start files come from: every program needs those.
cat > "$T" <<'EOF'
{
  "name": "fixture", "version": "0.0.1", "os": "linux", "triple": "aarch64-linux-musl",
  "modules": ["std/io"],
  "libraries": [ { "name": "c", "root": "src", "files": "lists/none.txt", "flags": [], "when": ["std/inline"] } ],
  "startfiles": { "library": "c", "before": [], "after": [] }
}
EOF
expect "conditional startfiles" "every program needs its start files" nexac "$WORK/io.nxa" --target fixture --source "$WORK/w.cpp"

# A byte-order mark is not an error: Windows editors put one on everything.
printf '\357\273\277' > "$T"
cat "$WORK/good.json" >> "$T"
if ! nexac "$WORK/io.nxa" --target fixture --source "$WORK/bom.cpp" > "$WORK/bom.log" 2>&1; then
    echo "FAIL BOM: a target.json starting with a UTF-8 byte-order mark was refused"
    sed 's/^/  /' "$WORK/bom.log" | head -4
    fails=$((fails + 1))
fi
cp "$WORK/good.json" "$T"

# --- removing it ----------------------------------------------------------------------

expect "remove"             "Removed target fixture"           nexac nexapkg target remove fixture
if [ -e "$HOMEDIR/.nexa/targets/fixture" ]; then
    echo "FAIL remove: ~/.nexa/targets/fixture is still there"
    fails=$((fails + 1))
fi
expect "remove twice"       "No target named 'fixture'"        nexac nexapkg target remove fixture

if [ "$fails" -eq 0 ]; then
    echo "target ok"
    exit 0
fi
echo "target: $fails failure(s)"
exit 1
