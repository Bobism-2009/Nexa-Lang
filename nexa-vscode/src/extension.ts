import * as vscode from "vscode";
import { indexNexaDefinitions, maskComments, type IndexedDef } from "./nexaIndex";
import {
  keywordCompletions,
  typeCompletions,
  moduleMemberCompletions,
  stringMethodCompletions,
  linePrefix,
  HOVER_DOCS,
  MODULE_MEMBERS,
} from "./nexaCompletions";
import { runNexaFile } from "./nexaRun";
import { registerSemanticTokens } from "./nexaSemantic";

const WORD = (name: string) => new RegExp(`\\b${escapeRe(name)}\\b`, "g");

function escapeRe(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

const RESERVED = new Set(
  [
    "if", "else", "while", "for", "switch", "case", "default", "return",
    "break", "continue", "let", "const", "fn", "extern", "struct", "enum",
    "try", "catch", "throw", "true", "false", "null", "new", "delete", "sizeof",
  ]
);

function isReservedWordForLookup(name: string): boolean {
  return RESERVED.has(name);
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
  if (d.kind === "variable") {
    return vscode.SymbolKind.Variable;
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
    return new vscode.Location(document.uri, defs[0]!.nameRange);
  }
  return defs.map((d) => new vscode.Location(document.uri, d.nameRange));
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
    return found[0]!;
  }
  return found;
}

function provideCompletions(
  document: vscode.TextDocument,
  position: vscode.Position
): vscode.CompletionItem[] {
  const prefix = linePrefix(document, position);
  const items: vscode.CompletionItem[] = [];

  const modDot = prefix.match(/(?:^|\s)([a-z]+)\.\s*([A-Za-z_]*)$/);
  if (modDot) {
    return moduleMemberCompletions(modDot[1]!);
  }

  const strDot = prefix.match(/\.([A-Za-z_]*)$/);
  if (strDot) {
    return stringMethodCompletions();
  }

  items.push(...keywordCompletions());
  items.push(...typeCompletions());

  for (const d of indexNexaDefinitions(document)) {
    const kind =
      d.kind === "function"
        ? vscode.CompletionItemKind.Function
        : d.kind === "variable"
          ? vscode.CompletionItemKind.Variable
          : d.kind === "struct"
            ? vscode.CompletionItemKind.Struct
            : vscode.CompletionItemKind.Enum;
    const c = new vscode.CompletionItem(d.name, kind);
    c.detail = `Nexa ${d.kind} in this file`;
    items.push(c);
  }

  for (const mod of Object.keys(MODULE_MEMBERS)) {
    const c = new vscode.CompletionItem(mod, vscode.CompletionItemKind.Module);
    c.detail = `${mod}.* standard module`;
    c.insertText = new vscode.SnippetString(`${mod}.$0`);
    items.push(c);
  }

  return items;
}

function provideHover(
  document: vscode.TextDocument,
  position: vscode.Position
): vscode.Hover | null {
  const wordRange = document.getWordRangeAtPosition(
    position,
    /[A-Za-z_][A-Za-z0-9_]*/
  );
  if (!wordRange) {
    return null;
  }
  const word = document.getText(wordRange);
  const line = document.lineAt(position.line).text;

  const modCall = line.match(
    new RegExp(`\\b(${Object.keys(MODULE_MEMBERS).join("|")})\\.${escapeRe(word)}\\b`)
  );
  if (modCall) {
    const key = `${modCall[1]}.${word}`;
    const members = MODULE_MEMBERS[modCall[1]!];
    const member = members?.find((m) => m.name === word);
    const docText = HOVER_DOCS[key] ?? member?.detail ?? key;
    return new vscode.Hover(docText, wordRange);
  }

  if (word === "null" && HOVER_DOCS.null) {
    return new vscode.Hover(HOVER_DOCS.null, wordRange);
  }

  if (line.includes("extern fn") && word !== "extern" && word !== "fn") {
    const defs = indexNexaDefinitions(document).filter(
      (d) => d.name === word && d.kind === "function"
    );
    if (defs.length) {
      return new vscode.Hover("extern fn — C FFI declaration", defs[0]!.nameRange);
    }
  }

  return null;
}

export function activate(context: vscode.ExtensionContext): void {
  registerSemanticTokens(context);

  const docFilter: vscode.DocumentSelector = [
    { language: "nexa", scheme: "file" },
  ];

  context.subscriptions.push(
    vscode.commands.registerCommand("nexa.runFile", () => {
      const editor = vscode.window.activeTextEditor;
      if (editor?.document.languageId === "nexa") {
        void runNexaFile(editor.document, true);
      }
    }),
    vscode.commands.registerCommand("nexa.buildFile", () => {
      const editor = vscode.window.activeTextEditor;
      if (editor?.document.languageId === "nexa") {
        void runNexaFile(editor.document, false);
      }
    })
  );

  context.subscriptions.push(
    vscode.languages.registerDefinitionProvider(docFilter, {
      provideDefinition(document, position, token) {
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
      provideReferences(document, position, refContext, token) {
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
          if (refContext.includeDeclaration) {
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
      provideDocumentSymbols(document) {
        return indexNexaDefinitions(document).map(
          (d) =>
            new vscode.DocumentSymbol(
              d.name,
              d.kind,
              defKindToSymbol(d),
              d.lineRange,
              d.nameRange
            )
        );
      },
    })
  );

  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(
      docFilter,
      {
        provideCompletionItems(document, position) {
          return provideCompletions(document, position);
        },
      },
      ".",
      " "
    )
  );

  context.subscriptions.push(
    vscode.languages.registerHoverProvider(docFilter, {
      provideHover(document, position) {
        return provideHover(document, position);
      },
    })
  );
}

export function deactivate(): void {}
