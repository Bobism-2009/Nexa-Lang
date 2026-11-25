import * as vscode from "vscode";
import {
  collectIncludedDefinitions,
  definitionHead,
  indexNexaDefinitions,
  maskCode,
  matchBrace,
  scanHead,
  structFields,
  type IndexedDef,
} from "./nexaIndex";

const ID = "[A-Za-z_][A-Za-z0-9_]*";
const TYPE = `(?:enum\\s+)?((?:\\[\\]\\s*)*\\*?\\s*${ID}(?:\\[[^\\]\\n]*\\])?)`;

function escapeRe(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

/**
 * The type the code before `before` gives `name`: from `let name: T`, `let name = T{`,
 * a parameter `name: T`, or `for (name in xs)` over a `[]T`. `self` is the struct or
 * shape around it. The nearest declaration wins. Null when nothing says.
 */
export function typeOfName(masked: string, name: string, before: number, depth = 0): string | null {
  if (name === "self") {
    const re = /\b(?:struct|shape)\s+([A-Za-z_][A-Za-z0-9_]*)[^{;]*\{/g;
    let best: string | null = null;
    let m: RegExpExecArray | null;
    while ((m = re.exec(masked)) !== null && m.index < before) {
      const close = matchBrace(masked, m.index + m[0].length - 1);
      if (close < 0 || close > before) {
        best = m[1]!;
      }
    }
    return best;
  }
  const n = escapeRe(name);
  const patterns = [
    new RegExp(`\\b(?:let|const)\\s+(?:const\\s+)?${n}\\s*:\\s*${TYPE}`, "g"),
    new RegExp(`\\b(?:let|const)\\s+(?:const\\s+)?${n}\\s*=\\s*(?:new\\s+)?(${ID})\\s*(?:\\[[^\\]\\n]*\\]\\s*)?\\{`, "g"),
    new RegExp(`[(,]\\s*${n}\\s*:\\s*${TYPE}`, "g"),
  ];
  let bestAt = -1;
  let type: string | null = null;
  for (const re of patterns) {
    let m: RegExpExecArray | null;
    while ((m = re.exec(masked)) !== null && m.index < before) {
      if (m.index > bestAt) {
        bestAt = m.index;
        type = m[1]!.replace(/\s+/g, "");
      }
    }
  }
  const loop = new RegExp(`\\bfor\\s*\\(\\s*${n}\\s+in\\s+(${ID})\\s*\\)`, "g");
  let m: RegExpExecArray | null;
  while ((m = loop.exec(masked)) !== null && m.index < before) {
    if (m.index > bestAt && depth < 3) {
      const of = typeOfName(masked, m[1]!, m.index, depth + 1);
      if (of && of.startsWith("[]")) {
        bestAt = m.index;
        type = of.slice(2);
      }
    }
  }
  return type;
}

/** `*Point` and `Box[int]` name the struct Point and Box. */
export function bareTypeName(type: string): string {
  return type.replace(/^\*+/, "").replace(/\[.*$/, "");
}

export interface MemberInfo {
  name: string;
  kind: "field" | "method";
  /** A field's type, or a method's head. */
  detail: string;
  document: vscode.TextDocument;
  range: vscode.Range;
  /** The method's definition, for its signature and doc comment. */
  def?: IndexedDef;
}

/** The fields and methods of the struct or shape `typeName`, here or in an include. */
export async function membersOf(
  document: vscode.TextDocument,
  typeName: string,
  token?: vscode.CancellationToken
): Promise<MemberInfo[] | null> {
  const bare = bareTypeName(typeName);
  if (!bare || typeName.startsWith("[]")) {
    return null;
  }
  const isContainer = (d: IndexedDef) => d.name === bare && (d.kind === "struct" || d.kind === "shape");
  let doc = document;
  let def = indexNexaDefinitions(document).find(isContainer);
  if (!def) {
    const hit = (await collectIncludedDefinitions(document, token)).find((h) => isContainer(h.def));
    if (!hit) {
      return null;
    }
    doc = hit.document;
    def = hit.def;
  }
  const masked = maskCode(doc.getText());
  const body = scanHead(masked, doc.offsetAt(def.nameRange.end));
  if (body.open < 0 || body.close < 0) {
    return [];
  }
  const out: MemberInfo[] = [];
  for (const f of structFields(masked, body.open, body.close)) {
    const start = doc.positionAt(f.offset);
    out.push({
      name: f.name,
      kind: "field",
      detail: f.detail,
      document: doc,
      range: new vscode.Range(start, start.translate(0, f.name.length)),
    });
  }
  for (const d of indexNexaDefinitions(doc)) {
    const at = doc.offsetAt(d.nameRange.start);
    if (d.kind === "function" && at > body.open && at < body.close) {
      out.push({
        name: d.name,
        kind: "method",
        detail: definitionHead(doc, d, masked),
        document: doc,
        range: d.nameRange,
        def: d,
      });
    }
  }
  return out;
}

/** The members of whatever `recv` is at `before` in `document`, or null. */
export async function membersOfReceiver(
  document: vscode.TextDocument,
  recv: string,
  before: number,
  token?: vscode.CancellationToken
): Promise<{ type: string; members: MemberInfo[] | null } | null> {
  const type = typeOfName(maskCode(document.getText()), recv, before);
  if (!type) {
    return null;
  }
  return { type, members: await membersOf(document, type, token) };
}
