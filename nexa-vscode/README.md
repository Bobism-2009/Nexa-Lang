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
Copy-Item -Recurse nexa-vscode "$env:USERPROFILE\.cursor\extensions\nexac.nexa-0.2.2"
# or: $env:USERPROFILE\.vscode\extensions\nexac.nexa-0.2.2
```

**macOS / Linux:**

```bash
cp -r nexa-vscode ~/.cursor/extensions/nexac.nexa-0.2.0
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
- **Syntax highlighting** — keywords, types (`unsigned int`, `size_t`, pointers), `extern fn`, `null`, `->`, operators, std modules (`io`, `os`, `file`, `math`, `crypto`, `http`, …), structs, enums, strings/chars, `#include`
- **Snippets** — `main`, `fn`, `extern`, `struct`, `enum`, `forin`, module includes, `inlinecpp`, etc.
- **Bracket matching** and indent for `{}`, `case`/`default`

### IntelliSense
- **Completions** — keywords, types, functions/variables/structs/enums in the file and in `#include`d `.nxa` files, `module.` member lists (`io.`, `os.`, …), string methods after `.`
- **Hover** — brief docs for std module calls; definition line for locals and included symbols
- **Go to definition** — functions (including `extern fn`), structs, enums, `let` bindings, and `#include "file.nxa"` paths
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
