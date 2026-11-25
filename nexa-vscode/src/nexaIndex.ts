import * as path from "path";
import * as vscode from "vscode";

const FN_RE = /\b(?:extern\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*[(\[]/g;
const STRUCT_RE = /\bstruct\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const ENUM_RE = /\benum\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const TYPE_RE = /^[ \t]*(?:priv\s+)?type\s+([A-Za-z_][A-Za-z0-9_]*)/gm;
const SHAPE_RE = /\bshape\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const LET_RE = /^[ \t]*(?:priv\s+)?(?:let|const)\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:[:=;]|$)/gm;
const DEFINE_RE = /^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)/gm;

/** Replace // and / ** / with spaces. Same length as input for offset mapping. */
export function maskComments(s: string): string {
  const a = s.split("");
  for (let i = 0; i < a.length; i++) {
    if (i < a.length - 1 && a[i] === "/" && a[i + 1] === "/") {
      a[i] = " ";
      a[i + 1] = " ";
      i += 2;
      while (i < a.length && a[i] !== "\n" && a[i] !== "\r") {
        a[i] = " ";
        i++;
      }
      i--;
      continue;
    }
    if (i < a.length - 1 && a[i] === "/" && a[i + 1] === "*") {
      a[i] = " ";
      a[i + 1] = " ";
      i += 2;
      while (i < a.length - 1) {
        if (a[i] === "*" && a[i + 1] === "/") {
          a[i] = " ";
          a[i + 1] = " ";
          i += 1;
          break;
        }
        a[i] = " ";
        i++;
      }
      continue;
    }
  }
  return a.join("");
}

// Blanks string and char literals (keeping f-string {holes}, which hold code), so a
// word inside quotes is never taken for a variable. Lengths are kept: columns match.
export function maskStrings(s: string): string {
  const a = s.split("");
  let i = 0;
  const blank = (from: number, to: number) => {
    for (let k = from; k < to && k < a.length; k++) if (a[k] !== "\n" && a[k] !== "\r") a[k] = " ";
  };
  while (i < a.length) {
    const c = a[i];
    if (c === "R" && a[i + 1] === '"' && !/[A-Za-z0-9_]/.test(a[i - 1] ?? "")) {
      const open = s.indexOf("(", i + 2);
      if (open > 0 && open - i < 20) {
        const close = s.indexOf(")" + s.slice(i + 2, open) + '"', open);
        const end = close < 0 ? a.length : close + (open - i - 2) + 2;
        blank(i, end);
        i = end;
        continue;
      }
    }
    if (c === '"' || c === "'") {
      const interp = c === '"' && a[i - 1] === "f" && !/[A-Za-z0-9_]/.test(a[i - 2] ?? "");
      a[i] = " ";
      i++;
      let depth = 0;
      while (i < a.length && a[i] !== "\n") {
        if (depth === 0 && a[i] === "\\") { a[i] = " "; if (i + 1 < a.length && a[i + 1] !== "\n") a[i + 1] = " "; i += 2; continue; }
        if (depth === 0 && a[i] === c) { a[i] = " "; i++; break; }
        if (interp && a[i] === "{") {
          if (depth === 0 && a[i + 1] === "{") { a[i] = " "; a[i + 1] = " "; i += 2; continue; }
          depth++;
          i++;
          continue;
        }
        if (interp && a[i] === "}" && depth > 0) { depth--; i++; continue; }
        if (depth === 0) a[i] = " ";
        i++;
      }
      continue;
    }
    i++;
  }
  return a.join("");
}

/** Comments and string/char literals blanked, same length as the input. */
export function maskCode(s: string): string {
  return maskStrings(maskComments(s));
}

export type DefKind = "function" | "struct" | "enum" | "variable" | "macro" | "type" | "shape";

export interface IndexedDef {
  name: string;
  kind: DefKind;
  nameRange: vscode.Range;
  lineRange: vscode.Range;
  defLine: number;
}

function runPattern(
  doc: vscode.TextDocument,
  masked: string,
  re: RegExp,
  kind: DefKind
): IndexedDef[] {
  const out: IndexedDef[] = [];
  re.lastIndex = 0;
  let m: RegExpExecArray | null;
  while ((m = re.exec(masked)) !== null) {
    const name = m[1]!;
    if (!name) {
      continue;
    }
    const nameOffset = m.index + m[0]!.indexOf(name);
    const defStart = m.index;
    const defStartPos = doc.positionAt(defStart);
    const line = doc.lineAt(defStartPos.line);
    if (kind === "function" && !/\b(?:extern\s+)?fn\s/.test(line.text)) {
      continue;
    }
    if (kind === "struct" && !/\bstruct\s/.test(line.text)) {
      continue;
    }
    if (kind === "enum" && !/\benum\s/.test(line.text)) {
      continue;
    }
    if (kind === "macro" && !/^\s*#\s*define\s/.test(line.text)) {
      continue;
    }
    if (kind === "shape" && !/\bshape\s/.test(line.text)) {
      continue;
    }
    const nameStart = doc.positionAt(nameOffset);
    const nameEnd = doc.positionAt(nameOffset + name.length);
    out.push({
      name,
      kind,
      nameRange: new vscode.Range(nameStart, nameEnd),
      lineRange: new vscode.Range(
        defStartPos.line,
        0,
        defStartPos.line,
        line.text.length
      ),
      defLine: defStartPos.line,
    });
  }
  return out;
}

export function indexNexaDefinitions(doc: vscode.TextDocument): IndexedDef[] {
  const masked = maskCode(doc.getText());
  const defs = [
    ...runPattern(doc, masked, FN_RE, "function"),
    ...runPattern(doc, masked, STRUCT_RE, "struct"),
    ...runPattern(doc, masked, ENUM_RE, "enum"),
    ...runPattern(doc, masked, TYPE_RE, "type"),
    ...runPattern(doc, masked, SHAPE_RE, "shape"),
    ...runPattern(doc, masked, DEFINE_RE, "macro"),
    ...indexLetBindings(doc, masked),
  ];
  return uniqueByLine(defs);
}

function indexLetBindings(
  doc: vscode.TextDocument,
  masked: string
): IndexedDef[] {
  const out: IndexedDef[] = [];
  LET_RE.lastIndex = 0;
  let m: RegExpExecArray | null;
  while ((m = LET_RE.exec(masked)) !== null) {
    const name = m[1]!;
    const nameOffset = m.index + m[0]!.indexOf(name);
    const defStartPos = doc.positionAt(m.index);
    const line = doc.lineAt(defStartPos.line);
    const nameStart = doc.positionAt(nameOffset);
    const nameEnd = doc.positionAt(nameOffset + name.length);
    out.push({
      name,
      kind: "variable",
      nameRange: new vscode.Range(nameStart, nameEnd),
      lineRange: new vscode.Range(
        defStartPos.line,
        0,
        defStartPos.line,
        line.text.length
      ),
      defLine: defStartPos.line,
    });
  }
  return out;
}

function uniqueByLine(defs: IndexedDef[]): IndexedDef[] {
  const seen = new Set<string>();
  return defs.filter((d) => {
    const k = `${d.defLine}\0${d.name}\0${d.kind}`;
    if (seen.has(k)) {
      return false;
    }
    seen.add(k);
    return true;
  });
}

export type IncludeKind = "quote" | "angle";

export interface IncludeSpec {
  kind: IncludeKind;
  path: string;
}

export interface LocatedDef {
  def: IndexedDef;
  document: vscode.TextDocument;
}

const INCLUDE_LINE_RE = /^\s*#include\s*(?:<([^>]+)>|"([^"]+)")/gm;

export function extractIncludes(masked: string): IncludeSpec[] {
  const out: IncludeSpec[] = [];
  INCLUDE_LINE_RE.lastIndex = 0;
  let m: RegExpExecArray | null;
  while ((m = INCLUDE_LINE_RE.exec(masked)) !== null) {
    if (m[2]) {
      out.push({ kind: "quote", path: m[2] });
    } else if (m[1]) {
      out.push({ kind: "angle", path: m[1] });
    }
  }
  return out;
}

function shouldFollowInclude(p: string): boolean {
  const norm = p.replace(/\\/g, "/");
  if (norm.startsWith("std/")) {
    return false;
  }
  if (/\.(h|hh|hpp|hxx|c|cc|cpp|cxx)$/i.test(norm)) {
    return false;
  }
  return true;
}

async function uriExists(uri: vscode.Uri): Promise<boolean> {
  try {
    await vscode.workspace.fs.stat(uri);
    return true;
  } catch {
    return false;
  }
}

function withNxaIfNeeded(p: string): string[] {
  const norm = p.replace(/\\/g, "/");
  if (/\.[A-Za-z0-9]+$/.test(norm)) {
    return [p];
  }
  return [p, `${p}.nxa`];
}

function packageSearchRoots(from: vscode.Uri): vscode.Uri[] {
  const roots: vscode.Uri[] = [];
  const folder = vscode.workspace.getWorkspaceFolder(from);
  if (folder) {
    roots.push(folder.uri);
    roots.push(vscode.Uri.joinPath(folder.uri, ".nexa", "packages"));
  }
  const home = process.env.USERPROFILE || process.env.HOME;
  if (home) {
    roots.push(vscode.Uri.file(path.join(home, ".nexa", "packages")));
  }
  return roots;
}

export async function resolveIncludeUri(
  from: vscode.TextDocument,
  spec: IncludeSpec
): Promise<vscode.Uri | null> {
  if (!shouldFollowInclude(spec.path)) {
    return null;
  }
  const parts = spec.path.replace(/\\/g, "/").split("/").filter((p) => p.length > 0);
  if (parts.length === 0) {
    return null;
  }
  if (spec.kind === "quote") {
    const baseDir = path.dirname(from.uri.fsPath);
    for (const rel of withNxaIfNeeded(spec.path)) {
      const abs = path.isAbsolute(rel)
        ? path.normalize(rel)
        : path.normalize(path.join(baseDir, rel));
      const uri = vscode.Uri.file(abs);
      if (await uriExists(uri)) {
        return uri;
      }
    }
    return null;
  }
  for (const root of packageSearchRoots(from.uri)) {
    for (const rel of withNxaIfNeeded(spec.path)) {
      const relParts = rel.replace(/\\/g, "/").split("/").filter((p) => p.length > 0);
      const uri = vscode.Uri.joinPath(root, ...relParts);
      if (await uriExists(uri)) {
        return uri;
      }
    }
  }
  return null;
}

export async function collectIncludedDefinitions(
  document: vscode.TextDocument,
  token?: vscode.CancellationToken
): Promise<LocatedDef[]> {
  const out: LocatedDef[] = [];
  const visited = new Set<string>([document.uri.toString()]);
  const queue: vscode.TextDocument[] = [document];
  let scanned = 0;
  while (queue.length > 0) {
    if (token?.isCancellationRequested || scanned > 100) {
      break;
    }
    const current = queue.shift()!;
    scanned += 1;
    const specs = extractIncludes(maskComments(current.getText()));
    for (const spec of specs) {
      if (token?.isCancellationRequested) {
        break;
      }
      const uri = await resolveIncludeUri(current, spec);
      if (!uri) {
        continue;
      }
      const key = uri.toString();
      if (visited.has(key)) {
        continue;
      }
      visited.add(key);
      let included: vscode.TextDocument;
      try {
        included = await vscode.workspace.openTextDocument(uri);
      } catch {
        continue;
      }
      for (const def of indexNexaDefinitions(included)) {
        out.push({ def, document: included });
      }
      queue.push(included);
    }
  }
  return out;
}

export function includeSpecAtPosition(
  document: vscode.TextDocument,
  position: vscode.Position
): IncludeSpec | null {
  const line = document.lineAt(position.line).text;
  const quote = /^\s*#include\s*"([^"]+)"/.exec(line);
  if (quote) {
    const start = line.indexOf('"') + 1;
    const end = start + quote[1]!.length;
    if (position.character >= start && position.character <= end) {
      return { kind: "quote", path: quote[1]! };
    }
  }
  const angle = /^\s*#include\s*<([^>]+)>/.exec(line);
  if (angle) {
    const start = line.indexOf("<") + 1;
    const end = start + angle[1]!.length;
    if (position.character >= start && position.character <= end) {
      return { kind: "angle", path: angle[1]! };
    }
  }
  return null;
}

/** The `}` that closes the `{` at `open`, or -1. */
export function matchBrace(masked: string, open: number): number {
  let depth = 0;
  for (let i = open; i < masked.length; i++) {
    const c = masked[i];
    if (c === "{") {
      depth++;
    } else if (c === "}") {
      depth--;
      if (depth === 0) {
        return i;
      }
    }
  }
  return -1;
}

/**
 * From just after a definition's name: where its head stops (the body's `{`, or the
 * `;` or `=` of a definition with no body) and the body's braces, if any.
 */
export function scanHead(
  masked: string,
  from: number
): { stop: number; open: number; close: number } {
  let paren = 0;
  let bracket = 0;
  for (let i = from; i < masked.length; i++) {
    const c = masked[i];
    if (c === "(") {
      paren++;
    } else if (c === ")") {
      paren--;
    } else if (c === "[") {
      bracket++;
    } else if (c === "]") {
      bracket--;
    } else if (paren <= 0 && bracket <= 0) {
      if (c === ";" || c === "=") {
        return { stop: i, open: -1, close: -1 };
      }
      if (c === "{") {
        return { stop: i, open: i, close: matchBrace(masked, i) };
      }
    }
  }
  return { stop: masked.length, open: -1, close: -1 };
}

/** A definition's head as one line: `fn add(a: int, b: int): int`, `struct Point`. */
export function definitionHead(doc: vscode.TextDocument, d: IndexedDef, masked?: string): string {
  const text = doc.getText();
  const start = doc.offsetAt(new vscode.Position(d.defLine, 0));
  const nameEnd = doc.offsetAt(d.nameRange.end);
  let end: number;
  if (d.kind === "function") {
    end = scanHead(masked ?? maskCode(text), nameEnd).stop;
  } else {
    end = text.indexOf("\n", nameEnd);
    if (end < 0) {
      end = text.length;
    }
  }
  let head = text.slice(start, end).replace(/\s+/g, " ").trim();
  if (d.kind === "struct" || d.kind === "enum" || d.kind === "shape") {
    head = head.replace(/\s*\{.*$/, "");
  }
  return head.replace(/\s*;$/, "");
}

/** The // lines or /* block *\/ written just above line `line`, as plain text. */
export function docComment(doc: vscode.TextDocument, line: number): string {
  let l = line - 1;
  if (l >= 0 && /\*\/\s*$/.test(doc.lineAt(l).text)) {
    const block: string[] = [];
    while (l >= 0) {
      const t = doc.lineAt(l).text;
      block.unshift(t);
      l--;
      if (/\/\*/.test(t)) {
        break;
      }
    }
    return block
      .join("\n")
      .replace(/^\s*\/\*+\s?/, "")
      .replace(/\s*\*+\/\s*$/, "")
      .split("\n")
      .map((s) => s.replace(/^\s*\*\s?/, ""))
      .join("\n")
      .trim();
  }
  const out: string[] = [];
  while (l >= 0 && /^\s*\/\//.test(doc.lineAt(l).text)) {
    out.unshift(doc.lineAt(l).text.replace(/^\s*\/\/+\s?/, ""));
    l--;
  }
  return out.join("\n").trim();
}

export interface BodyMember {
  name: string;
  kind: "field" | "variant";
  /** Offset of the name in the document. */
  offset: number;
  /** A field's type, or a variant's payload. */
  detail: string;
}

/** The body between `open` and `close` with everything nested deeper blanked. */
function shallowBody(masked: string, open: number, close: number): string {
  const out: string[] = [];
  let depth = 0;
  for (let i = open + 1; i < close; i++) {
    const c = masked[i]!;
    if (c === "{" || c === "(" || c === "[") {
      depth++;
      out.push(depth === 1 ? c : " ");
    } else if (c === "}" || c === ")" || c === "]") {
      out.push(depth === 1 ? c : " ");
      depth--;
    } else {
      out.push(depth > 0 && c !== "\n" ? " " : c);
    }
  }
  return out.join("");
}

/** A struct's or shape's fields: `name: type;` at the top of its body (methods are fn defs). */
export function structFields(masked: string, open: number, close: number): BodyMember[] {
  const body = shallowBody(masked, open, close);
  const out: BodyMember[] = [];
  const re = /(^|[;{}])(\s*(?:priv\s+)?)([A-Za-z_][A-Za-z0-9_]*)\s*:\s*([^;{}=]*)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(body)) !== null) {
    const at = m.index + m[1]!.length + m[2]!.length;
    if (m[3] === "fn") {
      continue;
    }
    const typeAt = m.index + m[0].length - m[4]!.length;
    const type = masked.slice(open + 1 + typeAt, open + 1 + typeAt + m[4]!.length).replace(/\s+/g, " ").trim();
    out.push({ name: m[3]!, kind: "field", offset: open + 1 + at, detail: type });
  }
  return out;
}

/**
 * An enum's variants: the names that start its body's statements, as in
 * `enum Color { Red; Green; }` or `enum Event { Click { x: int; }; Quit; }`.
 */
export function enumVariants(masked: string, open: number, close: number): BodyMember[] {
  const body = shallowBody(masked, open, close);
  const out: BodyMember[] = [];
  const re = /(^|[;,}])(\s*)([A-Za-z_][A-Za-z0-9_]*)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(body)) !== null) {
    const at = m.index + m[1]!.length + m[2]!.length;
    const after = masked.slice(open + 1 + at + m[3]!.length, close);
    const payload = /^\s*(\{[^}]*\}|\([^)]*\)|=\s*[^,;}]+)/.exec(after);
    out.push({
      name: m[3]!,
      kind: "variant",
      offset: open + 1 + at,
      detail: payload ? payload[1]!.replace(/\s+/g, " ").trim() : "",
    });
  }
  return out;
}
