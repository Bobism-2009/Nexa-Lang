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
- **Semantic colours** — function names are purple where `fn` defines them and yellow where they are called; variables and parameters the file declares are light blue wherever they are used; a `#define` name keeps its blue everywhere it is used
- **Folding** — braces, `#if` / `#else` / `#endif` branches, `// region` blocks, comment blocks and runs of `#include`
- **Snippets** — `main`, `fn`, `fngen`, `fnshape`, `type`, `shape`, `privfn`, `letdest`, `defer`, `match`, `matchopt`, `forrange`, `some`, `filter`, `define`, `definefn`, `undef`, module includes, `inlinecpp`, `inlineasm`, etc.
- **Bracket matching** and indent for `{}`, `case`/`default`

### IntelliSense
- **Completions** — keywords, types, functions/variables/structs/enums/`#define` macros in the file and in `#include`d `.nxa` files, `module.` member lists (`io.`, `os.`, `term.` with its termios flags, …), string, slice, map and `Option` methods after `.` (`xs.filter`, `m.get`, `o.or`, …), every directive after `#` (`#ifdef` and `#if` come with their `#endif`), and the predefined macros (`_WIN32`, `__aarch64__`, `__LINE__`, …)
- **Struct members** — after `p.`, the fields and methods of `p`'s struct or shape (from `let p: Point`, `let p = Point{...}`, a parameter, `for (p in points)` or `self`)
- **Parameter hints** — while typing a call's arguments: your functions and methods (with their doc comments), `#define` macros with parameters, and every std module function
- **Hover** — a function's whole signature and the `//` or `/* */` comment above it; a field's type; std module calls; keywords and directives (`match`, `step`, `#line`, …)
- **Go to definition** — functions (including `extern fn`), structs, enums, `type`, `shape`, `let` bindings, `#define` macros (also from included files), and `#include "file.nxa"` paths
- **Find references** — workspace-wide for `.nxa` files
- **Rename** (F2) — a local (`let`, a parameter, a loop or `catch` variable) only in its scope; a function, struct, type or global across the workspace's `.nxa` files; a field also through `p.x`, `self.x` and struct literals. Strings and comments are left alone
- **Outline and breadcrumbs** — structs with their fields and methods, enums with their variants, functions with their locals

### NexaC integration
- **Nexa: Run Current File** — runs `NexaC path.nxa --run` in the integrated terminal (editor run button or `Ctrl+Shift+R` / `Cmd+Shift+R`)
- **Nexa: Build Current File** — runs `NexaC path.nxa`
- **Errors as you save** — each file is checked by NexaC when it opens and each time it is saved (`NexaC file.nxa --source`, so only the Nexa checks run, not the C++ compiler); errors and warnings show in the editor and the Problems panel. A file that other workspace files `#include` is checked through the files at the top of those includes, so it can rely on what they include before it, and an error inside it is shown in it. **Nexa: Check Current File** runs it on demand
- **Nexa: Show Generated C++** — the C++ NexaC makes from the file, beside it (also in the editor's right-click menu)
- Setting **`nexa.nexacPath`** — path to `NexaC` (default: `NexaC` on PATH)
- Setting **`nexa.checkOnSave`** — turn the checking off (default: on)

## Development

1. Open the `nexa-vscode` folder
2. Press **F5** to launch Extension Development Host
3. Open a `.nxa` file in the new window

```bash
npm run watch   # recompile on save
```

## Requirements

[NexaC](https://github.com/Bobism-2009/Nexa-Lang) on PATH (or set `nexa.nexacPath`) for run/build commands and for checking files as you save.
