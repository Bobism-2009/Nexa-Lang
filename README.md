# Nexa & NexaC

**Nexa** is a small systems-friendly language with C++-like surface syntax. **NexaC** is its compiler: it parses `.nxa` files, transpiles to a single C++ translation unit, and invokes **clang++** (or **g++** on Windows as a fallback) to produce a native executable or shared library.

Current compiler version string: **0.1.6** (`NexaC --version`).

| | |
|---|---|
| **Sources** | `*.nxa` |
| **Pipeline** | Nexa → C++ → native binary |
| **Host tooling** | C++17 compiler on `PATH` |
| **Syntax reference** | [`SYNTAX/`](SYNTAX/) (authoritative) |

---

## Requirements

- **Windows:** [LLVM/Clang](https://releases.llvm.org/) or **MinGW-w64** (`clang++` / `g++`) on your `PATH`.
- **Linux:** `clang++` and normal build tools.

The generated C++ uses the standard library (`std::string`, `std::vector`, threads, chrono, etc.) and platform APIs where modules need them (e.g. `std/os` on Windows).

---

## Get NexaC

**From source (repo root):**

**Windows:** link **statically** so `NexaC.exe` runs on a clean PC (no MinGW/LLVM C++ runtime DLLs):

```bash
clang++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ NexaC.cpp -o NexaC.exe
```

If that fails with your Clang (e.g. MSVC-target toolchain), build with **MinGW g++** instead, same flags:

```bash
g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ NexaC.cpp -o NexaC.exe
```

**Linux / macOS:**

```bash
clang++ -std=c++17 -O2 NexaC.cpp -o NexaC
# Windows: adds .exe — e.g. NexaC.exe
```

There is also a [WIN/Makefile](WIN/Makefile) for building `NexaC.exe` in the `WIN/` folder (`make` in MSYS2 or similar).

Check the toolchain:

```bash
NexaC --version
```

---

## Hello world

`hello.nxa`:

```nexa
#include <std/io>

fn main(): void {
    io.println("Hello, Nexa");
}
```

Build and run:

```bash
NexaC hello.nxa -o hello
./hello
```

On Windows, `-o hello` yields `hello.exe`.

---

## Command-line arguments

Optional slice parameter (name is yours); `args[0]` is the program path, same idea as `argv[0]` in C:

```nexa
#include <std/io>

fn main(args: []string): void {
    io.println("argc = " + len(args));
    if (len(args) > 1 && args[1] == "--help") {
        io.println("Usage: …");
        return;
    }
}
```

The **`if` condition must be one parenthesized expression** — use `if (a && b)` or `if ((a) && (b))`, not `if (a) && (b)`.

Example project: [`Examples/cli_args_demo.nxa`](Examples/cli_args_demo.nxa).

---

## NexaC CLI (summary)

Full detail: [`SYNTAX/CLI.txt`](SYNTAX/CLI.txt) or `NexaC --help`.

| Command | Meaning |
|--------|---------|
| `NexaC file.nxa` | Compile to `file.exe` (Windows) or `file` (no extension on Unix default) |
| `NexaC file.nxa -o out` | Set output name |
| `NexaC file.nxa --source out.cpp` | Emit C++ only |
| `NexaC init [dir]` | Scaffold a project |
| `NexaC build [dir]` | Build entry `.nxa` in directory |
| `NexaC --run` / `-r` | Build to temp binary and run |
| `NexaC -p` / `--preserve-names` | Keep readable C++ symbol names |
| `NexaC --dll` / `--shared` | Build DLL / `.so` |
| `NexaC --static-lib` | Build a static archive (`.a` Linux / `.lib` Windows) from a `.nxa` |
| `NexaC file.nxa --link lib.a` | Statically link an archive/object into the executable (repeatable) |
| `NexaC --no-console` | Windows subsystem without console (executables only) |
| `nexapkg <cmd>` / `NexaC nexapkg <cmd>` | Package manager (see [Packages](#packages-nexapkg)) |

---

## Static libraries

Build a `.nxa` into a static archive and bake it into an executable (no runtime `.so`/`.dll` to ship):

```bash
# 1. Compile a library source to a static archive (.a on Linux, .lib on Windows)
NexaC mathlib.nxa --static-lib -o libmath.a

# 2. Statically link it into an executable
NexaC app.nxa --link libmath.a -o app
```

Library `fn`s are exported as `extern "C"` symbols. Call them from the executable through a
file-scope `inline_cpp!` block (an `extern "C"` declaration must be at file scope, not inside `fn main`):

```nexa
#include <std/inline>

inline_cpp! {
extern "C" int add(int, int);
}

fn main() {
    inline_cpp! { add(2, 3); }
}
```

`--link` is repeatable; `.a`/`.o` inputs are baked in, while `.so`/`.dll` link dynamically.

---

## Packages (nexapkg)

`nexapkg` (an alias of `NexaC`, also runnable as `NexaC nexapkg <cmd>`) manages `.nxa` dependencies.
Packages are folders of `.nxa` modules resolved from `./.nexa/packages/` then `~/.nexa/packages/`.

```bash
nexapkg install user/repo          # add + install a GitHub package (auto-creates manifest)
nexapkg add user/repo@v1.2.0       # pin a dependency to a tag or branch
nexapkg add ./local/lib            # add a local file or directory
nexapkg add as net/http user/http  # install under a custom include path
nexapkg install                    # install everything in nexapkg.json
nexapkg install --global           # install into the shared ~/.nexa/packages/
nexapkg update [name]              # re-fetch git deps and refresh the lock
nexapkg remove <name>              # drop a dependency (alias: rm, uninstall)
nexapkg list                       # show dependencies and locked commits
```

Use an installed package with an angle-bracket include:

```nexa
#include <user/repo>      // or <name/module> for a specific file in the package
```

| File | Purpose |
|------|---------|
| `nexapkg.json` | Manifest: project `name` + `dependencies` (`"include/path": "source"`) |
| `nexapkg.lock` | Resolved git commit per dependency — commit it for reproducible builds |

A `user/repo@tag` dependency is reproducible by tag; the lockfile additionally records the exact
commit, so a fresh `nexapkg install` restores the same revision. Local (`file:`) dependencies are
re-copied on every install so edits propagate during development.

---

## Repository layout

| Path | Purpose |
|------|---------|
| [`NexaC.cpp`](NexaC.cpp) | Driver: parse, transpile, compile, temp file handling |
| [`include/`](include/) | Lexer, parser, transpiler, modules, package tool headers |
| [`SYNTAX/`](SYNTAX/) | Language & compiler reference (`Core`, `Modules`, `CLI`, …) |
| `Tests/` | Small programs and harnesses (e.g. optimizations, arg slicing) |
| [`Examples/`](Examples/) | Larger samples |
| [`nexa-vscode/`](nexa-vscode/) | VS Code extension (syntax / tooling) |
| [`Installer/`](Installer/) | Installer-related Nexa sources |

---

## Learn the language

1. **Entry & types:** [`SYNTAX/Core.txt`](SYNTAX/Core.txt) — functions, `let`, structs, enums, `fn main()`, `fn main(args: []string)`, core string methods (`s.upper()`, `s.split(",")`, `s.contains(...)`, …).
2. **Control flow:** [`SYNTAX/ControlFlow.txt`](SYNTAX/ControlFlow.txt).
3. **Standard modules:** [`SYNTAX/Modules.txt`](SYNTAX/Modules.txt) — `#include <std/io>`, `std/os`, `std/file`, `std/math`, `std/random`, `std/time`, `std/thread`, etc.
4. **Includes & packages:** [`SYNTAX/Includes.txt`](SYNTAX/Includes.txt), **nexapkg** for third-party deps.