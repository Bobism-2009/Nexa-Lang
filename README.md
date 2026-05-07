# Nexa & NexaC

**Nexa** is a small systems-friendly language with C++-like surface syntax. **NexaC** is its compiler: it parses `.nxa` files, transpiles to a single C++ translation unit, and invokes **clang++** (or **g++** on Windows as a fallback) to produce a native executable or shared library.

Current compiler version string: **0.1.4** (`NexaC --version`).

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

Example project: [`Tests/cli_args_demo.nxa`](Tests/cli_args_demo.nxa).

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
| `NexaC --no-console` | Windows subsystem without console (executables only) |

---

## Repository layout

| Path | Purpose |
|------|---------|
| [`NexaC.cpp`](NexaC.cpp) | Driver: parse, transpile, compile, temp file handling |
| [`include/`](include/) | Lexer, parser, transpiler, modules, package tool headers |
| [`SYNTAX/`](SYNTAX/) | Language & compiler reference (`Core`, `Modules`, `CLI`, …) |
| [`Tests/`](Tests/) | Small programs and harnesses (e.g. CLI demo, optimizations) |
| [`Examples/`](Examples/) | Larger samples |
| [`nexa-vscode/`](nexa-vscode/) | VS Code extension (syntax / tooling) |
| [`Installer/`](Installer/) | Installer-related Nexa sources |

---

## Learn the language

1. **Entry & types:** [`SYNTAX/Core.txt`](SYNTAX/Core.txt) — functions, `let`, structs, enums, `fn main()`, `fn main(args: []string)`.
2. **Control flow:** [`SYNTAX/ControlFlow.txt`](SYNTAX/ControlFlow.txt).
3. **Standard modules:** [`SYNTAX/Modules.txt`](SYNTAX/Modules.txt) — `#include <std/io>`, `std/os`, `std/file`, `std/time`, `std/thread`, etc.
4. **Includes & packages:** [`SYNTAX/Includes.txt`](SYNTAX/Includes.txt), **nexapkg** for third-party deps.