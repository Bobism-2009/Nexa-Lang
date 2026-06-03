import * as vscode from "vscode";

const FN_RE = /\bfn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(/g;
const STRUCT_RE = /\bstruct\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const ENUM_RE = /\benum\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;

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

export type DefKind = "function" | "struct" | "enum";

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
    if (kind === "function" && !/\bfn\s/.test(line.text)) {
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
  ];
  return uniqueByLine(defs);
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
