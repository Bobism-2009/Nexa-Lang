import * as path from "path";
import * as vscode from "vscode";
import {
  collectIncludedDefinitions,
  includeSpecAtPosition,
  indexNexaDefinitions,
  maskComments,
  resolveIncludeUri,
  type DefKind,
  type IndexedDef,
  type LocatedDef,
} from "./nexaIndex";
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
  switch (d.kind) {
    case "struct":
      return vscode.SymbolKind.Struct;
    case "enum":
      return vscode.SymbolKind.Enum;
    case "variable":
      return vscode.SymbolKind.Variable;
    case "function":
      return vscode.SymbolKind.Function;
    default: {
      const _never: never = d.kind;
      return _never;
    }
  }
}

function defKindToCompletion(kind: DefKind): vscode.CompletionItemKind {
  switch (kind) {
    case "function":
      return vscode.CompletionItemKind.Function;
    case "variable":
      return vscode.CompletionItemKind.Variable;
    case "struct":
      return vscode.CompletionItemKind.Struct;
    case "enum":
      return vscode.CompletionItemKind.Enum;
    default: {
      const _never: never = kind;
      return _never;
    }
  }
}

function sourceLabel(doc: vscode.TextDocument): string {
  return path.basename(doc.uri.fsPath);
}

function hoverFromDef(doc: vscode.TextDocument, d: IndexedDef): vscode.Hover {
  const line = doc.lineAt(d.defLine).text.trim();
  const md = new vscode.MarkdownString();
  md.appendMarkdown(`**${d.kind}** \`${d.name}\` — ${sourceLabel(doc)}\n\n`);
  md.appendCodeblock(line, "nexa");
  return new vscode.Hover(md, d.nameRange);
}

function locationsFromLocated(hits: LocatedDef[]): vscode.Location | vscode.Location[] {
  if (hits.length === 1) {
    return new vscode.Location(hits[0]!.document.uri, hits[0]!.def.nameRange);
  }
  return hits.map((h) => new vscode.Location(h.document.uri, h.def.nameRange));
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

async function provideCompletions(
  document: vscode.TextDocument,
  position: vscode.Position,
  token: vscode.CancellationToken
): Promise<vscode.CompletionItem[]> {
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
    const c = new vscode.CompletionItem(d.name, defKindToCompletion(d.kind));
    c.detail = `Nexa ${d.kind} in this file`;
    c.sortText = "2" + d.name;
    items.push(c);
  }

  const included = await collectIncludedDefinitions(document, token);
  const seen = new Set(indexNexaDefinitions(document).map((d) => `${d.kind}:${d.name}`));
  for (const hit of included) {
    const key = `${hit.def.kind}:${hit.def.name}`;
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    const c = new vscode.CompletionItem(hit.def.name, defKindToCompletion(hit.def.kind));
    c.detail = `Nexa ${hit.def.kind} from ${sourceLabel(hit.document)}`;
    c.sortText = "3" + hit.def.name;
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

async function provideHover(
  document: vscode.TextDocument,
  position: vscode.Position,
  token: vscode.CancellationToken
): Promise<vscode.Hover | null> {
  const includeSpec = includeSpecAtPosition(document, position);
  if (includeSpec) {
    const uri = await resolveIncludeUri(document, includeSpec);
    if (uri) {
      return new vscode.Hover(`Included file \`${path.basename(uri.fsPath)}\``, document.lineAt(position.line).range);
    }
  }

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

  const local = indexNexaDefinitions(document).filter((d) => d.name === word);
  if (local.length) {
    return hoverFromDef(document, local[0]!);
  }
  const included = await collectIncludedDefinitions(document, token);
  const hit = included.find((h) => h.def.name === word);
  if (hit) {
    return hoverFromDef(hit.document, hit.def);
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
          const includeSpec = includeSpecAtPosition(document, position);
          if (includeSpec) {
            const uri = await resolveIncludeUri(document, includeSpec);
            if (uri) {
              return new vscode.Location(uri, new vscode.Position(0, 0));
            }
          }
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
          const included = await collectIncludedDefinitions(document, token);
          const hits = included.filter((h) => h.def.name === name);
          if (hits.length) {
            return locationsFromLocated(hits);
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
        provideCompletionItems(document, position, token) {
          return provideCompletions(document, position, token);
        },
      },
      ".",
      " "
    )
  );

  context.subscriptions.push(
    vscode.languages.registerHoverProvider(docFilter, {
      provideHover(document, position, token) {
        return provideHover(document, position, token);
      },
    })
  );
}

export function deactivate(): void {}
