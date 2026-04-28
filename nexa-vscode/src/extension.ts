import * as vscode from "vscode";
import { indexNexaDefinitions, maskComments, type IndexedDef } from "./nexaIndex";

const WORD = (name: string) => new RegExp(`\\b${escapeRe(name)}\\b`, "g");

function escapeRe(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

/** Only keywords / declarations that are never a user-defined identifier for lookup. */
const RESERVED = new Set(
  "if,else,while,for,switch,case,default,return,break,continue,let,const,fn"
    .split(",")
    .map((s) => s.trim())
);

function isReservedWordForLookup(name: string): boolean {
  if (RESERVED.has(name)) {
    return true;
  }
  if (name === "struct" || name === "enum" || name === "false" || name === "true") {
    return true;
  }
  return false;
}

function allRefsInDocument(
  doc: vscode.TextDocument,
  name: string
): vscode.Location[] {
  if (isReservedWordForLookup(name)) {
    return [];
  }
  const masked = maskComments(doc.getText());
  const re = WORD(name);
  re.lastIndex = 0;
  const out: vscode.Location[] = [];
  let m: RegExpExecArray | null;
  while ((m = re.exec(masked)) !== null) {
    const start = doc.positionAt(m.index);
    const end = doc.positionAt(m.index + name.length);
    out.push(new vscode.Location(doc.uri, new vscode.Range(start, end)));
  }
  return out;
}

async function allRefsInWorkspace(
  document: vscode.TextDocument,
  name: string,
  token: vscode.CancellationToken
): Promise<vscode.Location[]> {
  if (isReservedWordForLookup(name)) {
    return [];
  }
  if (!vscode.workspace.getWorkspaceFolder(document.uri)) {
    return allRefsInDocument(document, name);
  }
  const uris = await vscode.workspace.findFiles(
    "**/*.nxa",
    "{**/node_modules/**,**/.git/**}",
    2000
  );
  const out: vscode.Location[] = [];
  for (const uri of uris) {
    if (token.isCancellationRequested) {
      return out;
    }
    let doc: vscode.TextDocument;
    try {
      doc = await vscode.workspace.openTextDocument(uri);
    } catch {
      continue;
    }
    const re = new RegExp(`\\b${escapeRe(name)}\\b`, "g");
    const masked = maskComments(doc.getText());
    let m: RegExpExecArray | null;
    while ((m = re.exec(masked)) !== null) {
      const start = doc.positionAt(m.index);
      const end = doc.positionAt(m.index + name.length);
      out.push(new vscode.Location(doc.uri, new vscode.Range(start, end)));
    }
  }
  return out;
}

function defKindToSymbol(d: IndexedDef): vscode.SymbolKind {
  if (d.kind === "struct") {
    return vscode.SymbolKind.Struct;
  }
  if (d.kind === "enum") {
    return vscode.SymbolKind.Enum;
  }
  return vscode.SymbolKind.Function;
}

function isDefNameRange(
  doc: vscode.TextDocument,
  name: string,
  range: vscode.Range
): boolean {
  for (const d of indexNexaDefinitions(doc)) {
    if (d.name === name && d.nameRange.isEqual(range)) {
      return true;
    }
  }
  return false;
}

function locationsFromIndexedDefs(
  document: vscode.TextDocument,
  defs: IndexedDef[]
): vscode.Location | vscode.Location[] {
  if (defs.length === 1) {
    return new vscode.Location(
      document.uri,
      (defs[0] as IndexedDef).nameRange
    );
  }
  return defs.map(
    (d) => new vscode.Location(document.uri, d.nameRange)
  );
}

async function findDefinitionInOtherFiles(
  name: string,
  ownUri: vscode.Uri,
  token: vscode.CancellationToken
): Promise<vscode.Location | vscode.Location[] | null> {
  if (isReservedWordForLookup(name)) {
    return null;
  }
  if (!vscode.workspace.getWorkspaceFolder(ownUri)) {
    return null;
  }
  const uris = await vscode.workspace.findFiles(
    "**/*.nxa",
    "{**/node_modules/**,**/.git/**}",
    500
  );
  const found: vscode.Location[] = [];
  for (const uri of uris) {
    if (token.isCancellationRequested) {
      return null;
    }
    if (uri.toString() === ownUri.toString()) {
      continue;
    }
    let doc: vscode.TextDocument;
    try {
      doc = await vscode.workspace.openTextDocument(uri);
    } catch {
      continue;
    }
    for (const d of indexNexaDefinitions(doc)) {
      if (d.name === name) {
        found.push(new vscode.Location(doc.uri, d.nameRange));
      }
    }
  }
  if (found.length === 0) {
    return null;
  }
  if (found.length === 1) {
    return found[0] as vscode.Location;
  }
  return found;
}

export function activate(context: vscode.ExtensionContext): void {
  const docFilter: vscode.DocumentSelector = [
    { language: "nexa", scheme: "file" },
  ];

  context.subscriptions.push(
    vscode.languages.registerDefinitionProvider(docFilter, {
      provideDefinition(
        document: vscode.TextDocument,
        position: vscode.Position,
        token: vscode.CancellationToken
      ) {
        return (async () => {
          const idRange = document.getWordRangeAtPosition(
            position,
            /[A-Za-z_][A-Za-z0-9_]*/
          );
          if (!idRange) {
            return null;
          }
          const name = document.getText(idRange);
          if (isReservedWordForLookup(name)) {
            return null;
          }
          const local = indexNexaDefinitions(document).filter((d) => d.name === name);
          if (local.length) {
            return locationsFromIndexedDefs(document, local);
          }
          return findDefinitionInOtherFiles(name, document.uri, token);
        })();
      },
    })
  );

  context.subscriptions.push(
    vscode.languages.registerReferenceProvider(docFilter, {
      provideReferences(
        document: vscode.TextDocument,
        position: vscode.Position,
        context: vscode.ReferenceContext,
        token: vscode.CancellationToken
      ) {
        return (async () => {
          const idRange = document.getWordRangeAtPosition(
            position,
            /[A-Za-z_][A-Za-z0-9_]*/
          );
          if (!idRange) {
            return null;
          }
          const name = document.getText(idRange);
          if (isReservedWordForLookup(name)) {
            return null;
          }
          const refs = await allRefsInWorkspace(document, name, token);
          if (context.includeDeclaration) {
            return refs;
          }
          const out: vscode.Location[] = [];
          for (const loc of refs) {
            if (token.isCancellationRequested) {
              return out;
            }
            const doc = await vscode.workspace.openTextDocument(loc.uri);
            if (isDefNameRange(doc, name, loc.range)) {
              continue;
            }
            out.push(loc);
          }
          return out;
        })();
      },
    })
  );

  context.subscriptions.push(
    vscode.languages.registerDocumentSymbolProvider(docFilter, {
      provideDocumentSymbols(document: vscode.TextDocument) {
        return indexNexaDefinitions(document).map((d) => {
          return new vscode.DocumentSymbol(
            d.name,
            d.kind,
            defKindToSymbol(d),
            d.lineRange,
            d.nameRange
          );
        });
      },
    })
  );

  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(docFilter, {
      provideCompletionItems(document: vscode.TextDocument) {
        const items: vscode.CompletionItem[] = [];
        for (const d of indexNexaDefinitions(document)) {
          if (d.kind === "function") {
            const c = new vscode.CompletionItem(
              d.name,
              vscode.CompletionItemKind.Function
            );
            c.detail = "Nexa function in this file";
            items.push(c);
          }
        }
        return items;
      },
    })
  );
}

export function deactivate(): void {}