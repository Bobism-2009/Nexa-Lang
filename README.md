<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="Logo/nexa-logo-on-dark.svg">
    <img src="Logo/nexa-logo-on-light.svg" alt="Nexa" width="360">
  </picture>
</p>

<p align="center">
  A small systems language with C++-like syntax, compiled to tiny, fast native programs.
</p>

<p align="center">
  <a href="#get-nexac">Get NexaC</a> ·
  <a href="#hello-world">Hello world</a> ·
  <a href="SYNTAX/">Syntax reference</a> ·
  <a href="Examples/">Examples</a>
</p>

**NexaC** is Nexa's compiler: it parses `.nxa` files, transpiles to a single C++ translation unit, and invokes **clang++** (falling back to **g++** when clang is not installed) to produce a native executable or shared library. Set `NEXA_CXX` to force a specific compiler.

| | |
|---|---|
| **Version** | 0.1.17 (`NexaC --version`) |
| **Sources** | `*.nxa` |
| **Pipeline** | Nexa → C++ → native binary, or **`--wasm`** → WebAssembly |
| **Host tooling** | C++17 compiler on `PATH` |
| **Syntax reference** | [`SYNTAX/`](SYNTAX/) (authoritative) |

---

## Requirements

- **Windows:** a **MinGW-targeted** `clang++` on your `PATH` — preferably [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (`MartinStorsjo.LLVM-MinGW.UCRT` via winget). Official MSVC-target LLVM alone is not enough. The installer installs llvm-mingw when that MinGW clang++ is missing, and will not upgrade tools you already have.
- **Linux:** `clang++` (or `g++`) and normal build tools. The installer / `make install-deps`
  installs compile-time packages only (compiler, git, X11 *headers/static libs*
  for `std/gfx`). It does **not** install the wasm toolchain. Executables embed
  libstdc++, libgcc, and static X11; the resulting binary does not need
  `libX11.so`. libc stays dynamic. HTTPS uses the system `libssl.so` at run time
  (`dlopen`), so no OpenSSL headers are required to compile.
- **macOS:** Apple Command Line Tools (`xcode-select --install`); see [`MACOS.md`](MACOS.md).

The generated C++ uses the standard library (`std::string`, `std::vector`, threads, chrono, etc.) and platform APIs where modules need them (e.g. `std/os` on Windows).

**`--wasm`** needs [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (`em++` on `PATH`, or `EMSDK` set). If it is missing, NexaC asks to install Emscripten into `~/emsdk` and will also install missing Git/Python (and Node for `--run`). An existing `~/emsdk` or winget/LLVM/MinGW install is reused — NexaC does not upgrade tools that are already present. WASI-SDK (`WASI_SDK_PATH`) is a fallback for programs that do not use `std/network`'s http.* or `std/thread`. (`udp.*` is not available on wasm at all.) Override the compiler with `NEXA_WASM_CXX`. Run with `NexaC file.nxa --wasm --run` (`node` for Emscripten, `wasmtime`/`wasmer` for WASI).

---

## Get NexaC

From source, in the repo root. `build.cmd` / `build.sh` pick `clang++` (else `g++`); name a
different compiler as their argument (`sh build.sh g++`).

**Windows** (cmd.exe; or `sh build.sh` from Git Bash or MSYS2):

```bat
build.cmd
```

This builds the Nexa icon into `NexaC.exe` from `NexaC.rc` (with `windres` for a MinGW compiler,
`llvm-rc` for an MSVC-target clang++), and with MinGW links **statically**, so the exe runs on a
clean PC (no MinGW/LLVM C++ runtime DLLs). Without either tool it builds the same compiler with no
icon. There is also a [WIN/Makefile](WIN/Makefile) (`make` in MSYS2, or a Linux cross-compiler).

**Linux:**

```bash
make install-deps && make && make install
```

`make install-deps` installs the compiler and the X11 headers `std/gfx` needs (apt, dnf, pacman,
zypper or apk), `make` builds `NexaC`, and `make install` puts `NexaC`, `nexac` and `nexapkg` in
`~/.local/bin`. To only build it, with what you already have: `sh build.sh`.

**Raspberry Pi and other 64-bit ARM Linux**, on the Pi itself (Raspberry Pi OS already has clang):

```bash
make install-deps && make && make install
```

(To build *for* a Pi from another machine, use the [`arm64-linux` target](https://github.com/Bobism-2009/Nexa-Targets) instead: `NexaC hello.nxa --target arm64-linux`.)

**macOS**, after Apple's Command Line Tools:

```bash
xcode-select --install
make && make install
```

This installs `NexaC`, `nexac`, and `nexapkg` to `~/.local/bin` by default. Details: [`MACOS.md`](MACOS.md).

Check the toolchain:

```bash
NexaC --version
NexaC upgrade          # install a newer GitHub release if one exists
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
| `NexaC file.nxa --debug` / `-g` | Debuggable binary: `-g -O0`, nothing stripped, implies `--preserve-names`, plus sanitizer checks when the C++ compiler can link them. The debugger steps through your `.nxa` source, not the generated C++. With `--run` the binary is kept. Not valid with `--small` or `--wasm` |
| `NexaC --dll` / `--shared` | Build DLL / `.so` (Linux) / `.dylib` (macOS) |
| `NexaC --static-lib` | Build a static archive (`.a` Linux / `.lib` Windows) from a `.nxa` |
| `NexaC file.nxa --object` / `-c` | Write an object file (`.o`) and stop before linking; `--assembly` / `-S` writes `.s`, `--llvm-ir` writes `.ll` |
| `NexaC file.nxa --link lib.a` | Statically link an archive/object into the executable (repeatable) |
| `NexaC --no-console` | Windows subsystem without console (executables only) |
| `NexaC file.nxa --wasm` | WebAssembly via **em++**: a `.js` loader with the `.wasm` baked in, plus a `.html` that loads it — so the page opens over `file://`. WASI-SDK emits a bare `.wasm` |
| `NexaC file.nxa --wasm --wasm-split` | The same, as three separate files (`.html`, `.js`, `.wasm`). Needs a server: a browser will not fetch a separate `.wasm` over `file://` |
| `NexaC file.nxa --target arm64-linux` | Build for a platform installed with `nexapkg target install <name>` rather than built in — see [Nexa-Targets](https://github.com/Bobism-2009/Nexa-Targets). Source only, compiled by your clang; the first build compiles the platform's runtime and caches it |
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

Library `fn`s are exported as `extern "C"` symbols. If you have a C header, include it and call
the functions directly. Otherwise declare the symbol with `extern fn` (no `inline_cpp!`):

```nexa
#include "mathlib.h"

fn main() {
    let n = add(2, 3);
}
```

```nexa
extern fn add(a: int, b: int): int;

fn main() {
    let n = add(2, 3);
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
| `nexapkg.json` | Manifest: `name`, `entry`, `output`, optional `dll` / `dllOutput`, `x11`, `targets`, `dependencies` |
| `nexapkg.lock` | Resolved git commit per dependency — commit it for reproducible builds |

A `user/repo@tag` dependency is reproducible by tag; the lockfile additionally records the exact
commit, so a fresh `nexapkg install` restores the same revision. Local (`file:`) dependencies are
re-copied on every install so edits propagate during development.

`targets` names extra platforms `NexaC build` should produce as well as this machine's
(`"windows"`, `"wasm"`, or a name from `nexapkg target list`). `x11` is Linux-only and
says how a `std/gfx` or `std/ui` executable gets Xlib: `"embed"` (the default, a static
X11, about 1.1MB, no `libX11.so` needed) or `"system"` (about 23KB, and `libX11.so` must
be there at run time). Full field reference: [`SYNTAX/Modules.txt`](SYNTAX/Modules.txt) (PACKAGES).

---

## Repository layout

| Path | Purpose |
|------|---------|
| [`NexaC.cpp`](NexaC.cpp) | Driver: parse, transpile, compile, temp file handling |
| [`include/`](include/) | Lexer, parser, transpiler, modules, package tool headers |
| [`SYNTAX/`](SYNTAX/) | Language & compiler reference (`Core`, `Modules`, `CLI`, …) |
| `Tests/` | Self-checking `.nxa` programs for language, stdlib, and compiler features |
| [`Examples/`](Examples/) | Larger samples |
| [`nexa-vscode/`](nexa-vscode/) | VS Code extension (syntax / tooling) |
| [`Installer/`](Installer/) | Installer-related Nexa sources |
| [`MACOS.md`](MACOS.md) | Native macOS (Apple silicon / Intel) build notes |

---

## Learn the language

1. **Entry & types:** [`SYNTAX/Core.txt`](SYNTAX/Core.txt) — functions, `let`, structs, enums, `fn main()`, `fn main(args: []string)`, core string methods (`s.upper()`, `s.split(",")`, `s.contains(...)`, …).
2. **Control flow:** [`SYNTAX/ControlFlow.txt`](SYNTAX/ControlFlow.txt).
3. **Standard modules:** [`SYNTAX/Modules.txt`](SYNTAX/Modules.txt) — `#include <std/io>`, `std/os`, `std/file`, `std/math`, `std/random`, `std/time`, `std/thread`, `std/term` (colour, cursor, keys), `std/gfx` (window, fullscreen, PCM audio), `std/gfx3d` (3D window: OpenGL 1.1 by default, Vulkan 1.4 on Windows, WebGL in the browser), etc.
4. **Includes & packages:** [`SYNTAX/Includes.txt`](SYNTAX/Includes.txt), **nexapkg** for third-party deps.