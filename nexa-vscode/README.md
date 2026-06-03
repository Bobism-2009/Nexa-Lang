# Nexa Syntax Highlighting

VS Code / Cursor extension for Nexa language syntax highlighting.

## Installation

### Option 1: Copy to extensions folder

First build the extension (the manifest's `main` is `out/extension.js`, produced by compiling):

```bash
cd nexa-vscode
npm install
npm run compile
```

**Windows (PowerShell):**

```powershell
Copy-Item -Recurse nexa-vscode "$env:USERPROFILE\.cursor\extensions\nexac.nexa-0.1.5"
```

**macOS / Linux:**

```bash
cp -r nexa-vscode ~/.cursor/extensions/nexac.nexa-0.1.5
# or: ~/.vscode/extensions/nexac.nexa-0.1.5
```

Restart VS Code/Cursor (or **Developer: Reload Window**). `.nxa` files use the Nexa grammar.

### Option 2: Run from folder (development)

1. Open the `nexa-vscode` folder in VS Code
2. Press F5 to launch Extension Development Host
3. Open a `.nxa` file in the new window

### Package as VSIX (optional)

```bash
npm install -g @vscode/vsce
cd nexa-vscode
npm install
vsce package
```

Then install the generated `.vsix` file via **Extensions: Install from VSIX...**.

## Features

- Syntax highlighting: keywords, `fn` / function names, `io.*` / `os.*` / `dll.*` / `file.*` / `random.*` / `time.*` / `thread.*`, std module includes (`#include <std/...>`), types, `#include`, strings, char literals, floats
- Snippets: `main`, `fn`, `incio`, `println`, `while`, `init`, etc.
- Comments: `//` and `/* */`
- Bracket matching, auto-closing, basic brace indentation
