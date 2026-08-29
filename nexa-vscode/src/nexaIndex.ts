import * as path from "path";
import * as vscode from "vscode";

const FN_RE = /\b(?:extern\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(/g;
const STRUCT_RE = /\bstruct\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const ENUM_RE = /\benum\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const LET_RE = /^\s*let\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:[:=;]|$)/gm;

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

export type DefKind = "function" | "struct" | "enum" | "variable";

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
  const masked = maskComments(doc.getText());
  const defs = [
    ...runPattern(doc, masked, FN_RE, "function"),
    ...runPattern(doc, masked, STRUCT_RE, "struct"),
    ...runPattern(doc, masked, ENUM_RE, "enum"),
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
