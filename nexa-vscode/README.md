# Nexa Language (VS Code / Cursor)

Syntax highlighting, IntelliSense, and NexaC integration for `.nxa` files.

## Installation

### Copy into extensions folder

```bash
cd nexa-vscode
npm install
npm run compile
```

**Windows (PowerShell):**

```powershell
Copy-Item -Recurse nexa-vscode "$env:USERPROFILE\.cursor\extensions\nexac.nexa-0.2.10"
# or: $env:USERPROFILE\.vscode\extensions\nexac.nexa-0.2.10
```

**macOS / Linux:**

```bash
cp -r nexa-vscode ~/.cursor/extensions/nexac.nexa-0.2.10
```

Reload the window (**Developer: Reload Window**).

### Package as VSIX (optional)

```bash
npm install -g @vscode/vsce
cd nexa-vscode
vsce package
```

Install via **Extensions: Install from VSIX...**.

## Features

### Language support
- **Syntax highlighting** — keywords (`match`, `priv`, `type`, `shape`, `defer`, `in`, `and`/`or`/`not`, …), types (`float32`/`float64`, `unsigned int`, pointers, `Option`), generic `fn name[T]`, `extern fn`, `Some`/`None`, `null`, `self`, operators (`=>`, ranges `..` and `..<`), std modules (`io`, `os`, `file`, `term`, `gfx`, `gfx3d`, `ui`, …), structs, enums, type aliases, shapes, strings/chars, `#include`, `#define` / `#undef` (macro name and parameters), `#if` / `#ifdef` / `#elif` / `#else` / `#endif` with `defined`, `#error` / `#warning`, `#` and `##`, `__VA_ARGS__` / `__VA_OPT__` and the predefined macros (`_WIN32`, `__linux__`, `__LINE__`, …), `inline_cpp!` and `inline_asm!`
- **Snippets** — `main`, `fn`, `fngen`, `fnshape`, `type`, `shape`, `privfn`, `letdest`, `defer`, `match`, `matchopt`, `forrange`, `some`, `filter`, `define`, `definefn`, `undef`, module includes, `inlinecpp`, `inlineasm`, etc.
- **Bracket matching** and indent for `{}`, `case`/`default`

### IntelliSense
- **Completions** — keywords, types, functions/variables/structs/enums/`#define` macros in the file and in `#include`d `.nxa` files, `module.` member lists (`io.`, `os.`, `term.` with its termios flags, …), string, slice, map and `Option` methods after `.` (`xs.filter`, `m.get`, `o.or`, …), every directive after `#` (`#ifdef` and `#if` come with their `#endif`), and the predefined macros (`_WIN32`, `__aarch64__`, `__LINE__`, …)
- **Hover** — brief docs for std module calls; definition line for locals and included symbols
- **Go to definition** — functions (including `extern fn`), structs, enums, `let` bindings, `#define` macros (also from included files), and `#include "file.nxa"` paths
- **Find references** — workspace-wide for `.nxa` files
- **Document outline** — symbols in the file tree

### NexaC integration
- **Nexa: Run Current File** — runs `NexaC path.nxa --run` in the integrated terminal (editor run button or `Ctrl+Shift+R` / `Cmd+Shift+R`)
- **Nexa: Build Current File** — runs `NexaC path.nxa`
- Setting **`nexa.nexacPath`** — path to `NexaC` (default: `NexaC` on PATH)

## Development

1. Open the `nexa-vscode` folder
2. Press **F5** to launch Extension Development Host
3. Open a `.nxa` file in the new window

```bash
npm run watch   # recompile on save
```

## Requirements

[NexaC](https://github.com/Bobism-2009/Nexa-Lang) on PATH (or set `nexa.nexacPath`) for run/build commands.
