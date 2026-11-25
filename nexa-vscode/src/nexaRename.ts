import * as vscode from "vscode";
import { MODULE_MEMBERS, NEXA_KEYWORDS, NEXA_TYPES } from "./nexaCompletions";
import { maskCode, matchBrace } from "./nexaIndex";

const IDENT = /^[A-Za-z_][A-Za-z0-9_]*$/;
const RESERVED = new Set<string>([
  ...NEXA_KEYWORDS,
  ...NEXA_TYPES,
  "main", "unsigned", "map", "Option", "Result", "inline_cpp", "inline_asm",
  ...Object.keys(MODULE_MEMBERS),
]);

function escapeRe(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

/** Where a local declaration of a name lives: from the declaration to the end of its block. */
interface LocalScope {
  decl: number;
  start: number;
  end: number;
}

/** The `{` before `at` that nothing between closes, or -1 at the top level. */
function enclosingOpen(masked: string, at: number): number {
  let depth = 0;
  for (let i = at - 1; i >= 0; i--) {
    const c = masked[i];
    if (c === "}") {
      depth++;
    } else if (c === "{") {
      if (depth === 0) {
        return i;
      }
      depth--;
    }
  }
  return -1;
}

/** For a name declared in a `( ... )` (parameter, loop or catch variable): the block that follows. */
function blockAfterParens(masked: string, at: number): number {
  let depth = 0;
  for (let i = at; i < masked.length; i++) {
    const c = masked[i];
    if (c === "(") {
      depth++;
    } else if (c === ")") {
      if (depth === 0) {
        for (let j = i + 1; j < masked.length; j++) {
          const d = masked[j];
          if (d === "{") {
            return j;
          }
          if (d === ";") {
            return -1;
          }
        }
        return -1;
      }
      depth--;
    }
  }
  return -1;
}

/** Every declaration of `name` inside a block, with the stretch of code it covers. */
function localScopes(masked: string, name: string): LocalScope[] {
  const n = escapeRe(name);
  const out: LocalScope[] = [];
  const add = (decl: number, inParens: boolean) => {
    if (inParens) {
      const open = blockAfterParens(masked, decl + name.length);
      const end = open < 0 ? decl + name.length : matchBrace(masked, open);
      out.push({ decl, start: decl, end: end < 0 ? masked.length : end });
      return;
    }
    const open = enclosingOpen(masked, decl);
    if (open < 0) {
      return; // a top-level let: a global
    }
    const close = matchBrace(masked, open);
    out.push({ decl, start: decl, end: close < 0 ? masked.length : close });
  };
  let m: RegExpExecArray | null;
  const letRe = new RegExp(`\\b(?:let|const)\\s+(?:const\\s+)?(${n})\\b`, "g");
  while ((m = letRe.exec(masked)) !== null) {
    add(m.index + m[0].length - name.length, false);
  }
  const fnRe = /\bfn\b(?:\s+[A-Za-z_][A-Za-z0-9_]*)?\s*(?:\[[^\]]*\]\s*)?\(([^)]*)\)/g;
  while ((m = fnRe.exec(masked)) !== null) {
    const listAt = m.index + m[0].length - 1 - m[1]!.length;
    const p = new RegExp(`(^|,)\\s*(?:\\.\\.\\.\\s*)?(${n})\\s*(?=:|,|$)`, "g");
    let q: RegExpExecArray | null;
    while ((q = p.exec(m[1]!)) !== null) {
      add(listAt + q.index + q[0].length - name.length, true);
    }
  }
  const forRe = /\bfor\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)(?:\s*,\s*([A-Za-z_][A-Za-z0-9_]*))?\s+in\b/g;
  while ((m = forRe.exec(masked)) !== null) {
    for (const g of [m[1], m[2]]) {
      if (g === name) {
        const rel = new RegExp(`\\b${n}\\b`).exec(m[0])!.index;
        add(m.index + rel, true);
        break;
      }
    }
  }
  const catchRe = new RegExp(`\\bcatch\\s*\\(\\s*(${n})\\b`, "g");
  while ((m = catchRe.exec(masked)) !== null) {
    add(m.index + m[0].length - name.length, true);
  }
  return out;
}

function occurrences(masked: string, name: string): number[] {
  const re = new RegExp(`\\b${escapeRe(name)}\\b`, "g");
  const out: number[] = [];
  let m: RegExpExecArray | null;
  while ((m = re.exec(masked)) !== null) {
    out.push(m.index);
  }
  return out;
}

function afterDot(masked: string, at: number): boolean {
  let i = at - 1;
  while (i >= 0 && (masked[i] === " " || masked[i] === "\t")) i--;
  return masked[i] === "." && masked[i - 1] !== ".";
}

/** True when `at` sits directly in a struct's or shape's body (a field or method name). */
function inStructBody(masked: string, at: number): boolean {
  const open = enclosingOpen(masked, at);
  if (open < 0) {
    return false;
  }
  const header = masked.slice(Math.max(0, open - 200), open);
  return /\b(?:struct|shape)\s+[A-Za-z_][A-Za-z0-9_]*[^{};]*$/.test(header);
}

function wordAt(document: vscode.TextDocument, position: vscode.Position) {
  const range = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
  if (!range) {
    return null;
  }
  return { range, name: document.getText(range), offset: document.offsetAt(range.start) };
}

function checkRenamable(document: vscode.TextDocument, position: vscode.Position) {
  const w = wordAt(document, position);
  if (!w) {
    throw new Error("Put the cursor on a name to rename it.");
  }
  const masked = maskCode(document.getText());
  if (masked.slice(w.offset, w.offset + w.name.length) !== w.name) {
    throw new Error("That is inside a string or a comment.");
  }
  if (RESERVED.has(w.name)) {
    throw new Error(`'${w.name}' is part of Nexa and can't be renamed.`);
  }
  const before = masked.slice(Math.max(0, w.offset - 40), w.offset);
  const mod = /([a-z][a-z0-9]*)\s*\.\s*$/.exec(before);
  if (mod && MODULE_MEMBERS[mod[1]!]) {
    throw new Error(`${mod[1]}.${w.name} belongs to the standard library and can't be renamed.`);
  }
  if (/^\s*#\s*(?:include|line|pragma)\b/.test(document.lineAt(position.line).text)) {
    throw new Error("Nothing on this line can be renamed.");
  }
  return { ...w, masked };
}

export async function provideRenameEdits(
  document: vscode.TextDocument,
  position: vscode.Position,
  newName: string,
  token: vscode.CancellationToken
): Promise<vscode.WorkspaceEdit> {
  const { name, offset, masked } = checkRenamable(document, position);
  if (!IDENT.test(newName)) {
    throw new Error(`'${newName}' is not a name: use letters, digits and _, not starting with a digit.`);
  }
  if (RESERVED.has(newName)) {
    throw new Error(`'${newName}' is part of Nexa and can't be a name.`);
  }
  const edit = new vscode.WorkspaceEdit();
  const scopes = localScopes(masked, name);
  const own = scopes
    .filter((s) => offset >= s.start && offset <= s.end)
    .sort((a, b) => a.end - a.start - (b.end - b.start))[0];

  if (own) {
    // A local: this declaration's stretch, minus inner ones that redeclare the name.
    const inner = scopes.filter((s) => s !== own && s.start > own.start && s.end <= own.end);
    for (const at of occurrences(masked, name)) {
      if (at < own.start || at > own.end || afterDot(masked, at)) continue;
      if (inner.some((s) => at >= s.start && at <= s.end)) continue;
      const start = document.positionAt(at);
      edit.replace(document.uri, new vscode.Range(start, start.translate(0, name.length)), newName);
    }
    return edit;
  }

  // Not a local: every file, leaving alone the locals that happen to share the name.
  let docs: vscode.TextDocument[] = [document];
  if (vscode.workspace.getWorkspaceFolder(document.uri)) {
    const uris = await vscode.workspace.findFiles("**/*.nxa", "{**/node_modules/**,**/.git/**}", 2000);
    docs = [];
    for (const uri of uris) {
      if (token.isCancellationRequested) {
        return edit;
      }
      try {
        docs.push(await vscode.workspace.openTextDocument(uri));
      } catch {
        // unreadable: skip
      }
    }
    if (!docs.some((d) => d.uri.toString() === document.uri.toString())) {
      docs.push(document);
    }
  }
  // A field or method (named after a dot, or declared in a struct or shape) is
  // renamed where it is reached through a dot, where it is declared, and as a key in
  // a struct literal. Anything else is renamed everywhere but after a dot.
  const member = afterDot(masked, offset) || inStructBody(masked, offset);
  for (const doc of docs) {
    const text = doc.uri.toString() === document.uri.toString() ? masked : maskCode(doc.getText());
    const locals = localScopes(text, name);
    for (const at of occurrences(text, name)) {
      const dot = afterDot(text, at);
      const inLocal = locals.some((s) => at >= s.start && at <= s.end);
      if (member) {
        const key = /^\s*:/.test(text.slice(at + name.length, at + name.length + 8)) && !inLocal;
        if (!dot && !inStructBody(text, at) && !key) continue;
      } else if (dot || inLocal) {
        continue;
      }
      const start = doc.positionAt(at);
      edit.replace(doc.uri, new vscode.Range(start, start.translate(0, name.length)), newName);
    }
  }
  return edit;
}

export function registerRename(context: vscode.ExtensionContext, selector: vscode.DocumentSelector): void {
  context.subscriptions.push(
    vscode.languages.registerRenameProvider(selector, {
      prepareRename(document, position) {
        return checkRenamable(document, position).range;
      },
      provideRenameEdits,
    })
  );
}
