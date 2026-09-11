# Nexa language regression suite

Programs written **in Nexa** that pin what the language does, so a NexaC change
that moves the language surface fails a test instead of quietly shipping.

Run everything from the repo root:

```sh
./Tests/run_tests.sh            # all four phases
make test                       # same thing
```

Useful flags: `--filter strings` (one test), `--phase run`, `--jobs 4`,
`--nexac ./build/NexaC`, `--keep` (leave the work dir for inspection).

## Layout

| Path | What it is | How it passes |
| --- | --- | --- |
| `Tests/Lang/*.nxa` | must compile and run | stdout matches the sibling `.expected` byte for byte |
| `Tests/Lang/errors/*.nxa` | must **not** compile | NexaC exits non-zero and prints every line of the sibling `.expected` |
| `Tests/Lang/known_bugs/*.nxa` | filed NexaC bugs | expected to fail; a pass is an `XPASS` and fails the run |

The harness also runs the pre-existing `Tests/*_cases.sh` suites as a final
phase, so one command covers the whole tree.

## Writing a test

Print one `label=value` per line. The label is what makes a failure readable —
the harness shows a unified diff, so `mod_neg_lhs=-1` vs `mod_neg_lhs=1` says
exactly which rule moved.

Derive the `.expected` from `SYNTAX/`, not from what NexaC currently prints.
A test that just records today's output cannot tell a fix from a regression.
Where `SYNTAX/` is genuinely silent (bool rendering under `+`, for one), say so
in a comment next to the case.

Avoid anything that is undefined by design — division by zero, out-of-range
indexing, reading an uninitialized local, popping an empty slice
(`SYNTAX/Core.txt`, COMPILE-TIME CHECKS). Those have no right answer to pin.

### Error cases

Each line of a `.expected` under `errors/` is a substring that must appear in
NexaC's output; `#` lines are comments. The harness additionally requires that
the diagnostic names the `.nxa` file and that no raw C++ compiler `error:`
leaks through — diagnosing in Nexa is the point, so clang never gets to speak.

Assert the line number when NexaC reports it correctly. When it does not, leave
the number out and say why in a `#` comment.

### Known bugs

`known_bugs/` holds a minimal reproduction of a NexaC bug that has been filed
but not fixed. Its `.expected` is the **correct** output — what `SYNTAX/` says
should happen — so the case fails today and turns into a loud `XPASS` the day
it is fixed. The file's header comment says what is wrong, quotes the compiler
diagnostic if there is one, and names the workaround the rest of the suite uses.

When a bug is fixed: move the `.nxa`/`.expected` pair up into `Tests/Lang/`,
fold its cases into the topic test they belong to, and drop the workaround
comments that point at it.
