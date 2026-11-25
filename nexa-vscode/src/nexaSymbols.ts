import * as vscode from "vscode";
import {
  definitionHead,
  enumVariants,
  indexNexaDefinitions,
  maskCode,
  scanHead,
  structFields,
  type IndexedDef,
} from "./nexaIndex";

function symbolKind(d: IndexedDef, inStruct: boolean): vscode.SymbolKind {
  switch (d.kind) {
    case "function":
      return inStruct ? vscode.SymbolKind.Method : vscode.SymbolKind.Function;
    case "struct":
      return vscode.SymbolKind.Struct;
    case "enum":
      return vscode.SymbolKind.Enum;
    case "shape":
      return vscode.SymbolKind.Interface;
    case "type":
      return vscode.SymbolKind.TypeParameter;
    case "macro":
      return vscode.SymbolKind.Constant;
    case "variable":
      return vscode.SymbolKind.Variable;
  }
}

interface Node {
  sym: vscode.DocumentSymbol;
  start: number;
  end: number;
  def: IndexedDef;
}

/**
 * The outline as a tree: a struct or shape holds its fields and methods, an enum its
 * variants, a function its locals. Each symbol spans its whole body, so the
 * breadcrumbs follow the cursor into it.
 */
export function documentSymbols(document: vscode.TextDocument): vscode.DocumentSymbol[] {
  const masked = maskCode(document.getText());
  const defs = indexNexaDefinitions(document).sort(
    (a, b) => document.offsetAt(a.nameRange.start) - document.offsetAt(b.nameRange.start)
  );
  const nodes: Node[] = [];
  for (const d of defs) {
    const nameStart = document.offsetAt(d.nameRange.start);
    const lineStart = document.offsetAt(d.lineRange.start);
    let end = document.offsetAt(d.lineRange.end);
    let body: { open: number; close: number } | null = null;
    if (d.kind === "function" || d.kind === "struct" || d.kind === "enum" || d.kind === "shape") {
      const head = scanHead(masked, document.offsetAt(d.nameRange.end));
      if (head.open >= 0 && head.close >= 0) {
        body = { open: head.open, close: head.close };
        end = head.close + 1;
      }
    }
    const detail =
      d.kind === "function"
        ? definitionHead(document, d, masked).replace(/^.*?\bfn\s+[A-Za-z_][A-Za-z0-9_]*\s*/, "")
        : d.kind === "type"
          ? definitionHead(document, d, masked).replace(/^.*?=\s*/, "= ")
          : "";
    const sym = new vscode.DocumentSymbol(
      d.name,
      detail,
      symbolKind(d, false),
      new vscode.Range(document.positionAt(lineStart), document.positionAt(end)),
      d.nameRange
    );
    if (body && (d.kind === "struct" || d.kind === "shape")) {
      for (const f of structFields(masked, body.open, body.close)) {
        const at = document.positionAt(f.offset);
        const r = new vscode.Range(at, at.translate(0, f.name.length));
        sym.children.push(new vscode.DocumentSymbol(f.name, f.detail, vscode.SymbolKind.Field, r, r));
      }
    }
    if (body && d.kind === "enum") {
      for (const v of enumVariants(masked, body.open, body.close)) {
        const at = document.positionAt(v.offset);
        const r = new vscode.Range(at, at.translate(0, v.name.length));
        sym.children.push(new vscode.DocumentSymbol(v.name, v.detail, vscode.SymbolKind.EnumMember, r, r));
      }
    }
    nodes.push({ sym, start: nameStart, end, def: d });
  }

  // Nest each symbol in the innermost function, struct or shape whose body holds it.
  const containers = nodes.filter((n) => n.sym.range.end.line > n.sym.range.start.line);
  const top: vscode.DocumentSymbol[] = [];
  for (const n of nodes) {
    let parent: Node | null = null;
    for (const c of containers) {
      if (c === n || n.start <= c.start || n.start >= c.end) continue;
      if (!parent || c.end - c.start < parent.end - parent.start) parent = c;
    }
    if (parent) {
      const inStruct = parent.def.kind === "struct" || parent.def.kind === "shape";
      n.sym.kind = symbolKind(n.def, inStruct);
      parent.sym.children.push(n.sym);
    } else {
      top.push(n.sym);
    }
  }
  for (const n of nodes) {
    n.sym.children.sort((a, b) => a.range.start.compareTo(b.range.start));
  }
  return top;
}
