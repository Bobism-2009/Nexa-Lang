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
  directiveCompletions,
  predefinedMacroCompletions,
  linePrefix,
  HOVER_DOCS,
  MODULE_MEMBERS,
} from "./nexaCompletions";
import { runNexaFile } from "./nexaRun";
import { registerSemanticTokens } from "./nexaSemantic";
import { registerDiagnostics, showGeneratedCpp } from "./nexaDiagnostics";
import { provideSignatureHelp } from "./nexaSignature";
import { registerRename } from "./nexaRename";
import { registerFolding } from "./nexaFolding";
import { documentSymbols } from "./nexaSymbols";
import { definitionHead, docComment, maskCode } from "./nexaIndex";
import { membersOfReceiver, type MemberInfo } from "./nexaTypes";

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
    case "macro":
      return vscode.CompletionItemKind.Constant;
    case "type":
      return vscode.CompletionItemKind.TypeParameter;
    case "shape":
      return vscode.CompletionItemKind.Interface;
    default: {
      const _never: never = kind;
      return _never;
    }
  }
}

function sourceLabel(doc: vscode.TextDocument): string {
  return path.basename(doc.uri.fsPath);
}

/** The definition's head (a function's whole signature), its doc comment, and where it is. */
function hoverFromDef(doc: vscode.TextDocument, d: IndexedDef, at?: vscode.Range): vscode.Hover {
  const md = new vscode.MarkdownString();
  md.appendCodeblock(definitionHead(doc, d), "nexa");
  const comment = docComment(doc, d.defLine);
  if (comment) {
    md.appendMarkdown("\n\n");
    md.appendText(comment);
  }
  md.appendMarkdown(`\n\n*${d.kind}* — ${sourceLabel(doc)}:${d.defLine + 1}`);
  return new vscode.Hover(md, at);
}

function hoverFromMember(type: string, m: MemberInfo, at: vscode.Range): vscode.Hover {
  if (m.def) {
    const h = hoverFromDef(m.document, m.def, at);
    return h;
  }
  const md = new vscode.MarkdownString();
  md.appendCodeblock(`${m.name}: ${m.detail}`, "nexa");
  const comment = docComment(m.document, m.range.start.line);
  if (comment) {
    md.appendMarkdown("\n\n");
    md.appendText(comment);
  }
  md.appendMarkdown(`\n\n*field of ${type}* — ${sourceLabel(m.document)}:${m.range.start.line + 1}`);
  return new vscode.Hover(md, at);
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

  if (/^\s*#\s*[a-z]*$/.test(prefix)) {
    return directiveCompletions();
  }

  // io., os., ... list the module; any other name. (a string, a slice, a map) falls
  // through to the methods.
  const modDot = prefix.match(/(?:^|[^A-Za-z0-9_.])([a-z][a-z0-9]*)\.\s*([A-Za-z_]*)$/);
  if (modDot && MODULE_MEMBERS[modDot[1]!]) {
    return moduleMemberCompletions(modDot[1]!);
  }

  const strDot = prefix.match(/\.([A-Za-z_]*)$/);
  if (strDot) {
    // p. on a struct or shape: its fields and methods.
    const recv = prefix.match(/([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*[A-Za-z_]*$/);
    if (recv && !/\.\s*\.\s*[A-Za-z_]*$/.test(prefix)) {
      const before = document.offsetAt(position) - recv[0].length;
      const found = await membersOfReceiver(document, recv[1]!, before, token);
      if (found?.members && found.members.length) {
        return found.members.map((m) => {
          const c = new vscode.CompletionItem(
            m.name,
            m.kind === "field" ? vscode.CompletionItemKind.Field : vscode.CompletionItemKind.Method
          );
          c.detail = m.kind === "field" ? `${m.name}: ${m.detail}` : m.detail;
          c.sortText = (m.kind === "field" ? "0" : "1") + m.name;
          if (m.def) {
            const comment = docComment(m.document, m.def.defLine);
            if (comment) c.documentation = comment;
          }
          return c;
        });
      }
    }
    return stringMethodCompletions();
  }

  items.push(...keywordCompletions());
  items.push(...typeCompletions());
  items.push(...predefinedMacroCompletions());

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

  // p.x and p.move(): the field or method of p's struct.
  const lineBefore = line.slice(0, wordRange.start.character);
  const recv = /([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*$/.exec(lineBefore);
  if (recv && !/\.\s*\.\s*$/.test(lineBefore)) {
    const before = document.offsetAt(wordRange.start) - recv[0].length;
    const found = await membersOfReceiver(document, recv[1]!, before, token);
    const member = found?.members?.find((m) => m.name === word);
    if (found && member) {
      return hoverFromMember(found.type, member, wordRange);
    }
    if (HOVER_DOCS[word]) {
      return new vscode.Hover(HOVER_DOCS[word]!, wordRange);
    }
    return null;
  }

  const directive = /^\s*#\s*([a-z]+)/.exec(line);
  if (directive && directive[1] === word && HOVER_DOCS[`#${word}`]) {
    return new vscode.Hover(HOVER_DOCS[`#${word}`]!, wordRange);
  }

  // Keyword and built-in docs (match, step, Some, ...), unless the file defines the name.
  if (HOVER_DOCS[word]) {
    const own = indexNexaDefinitions(document).some((d) => d.name === word);
    if (!own) {
      return new vscode.Hover(HOVER_DOCS[word]!, wordRange);
    }
  }

  const local = indexNexaDefinitions(document).filter((d) => d.name === word);
  if (local.length) {
    // The nearest definition above the cursor, for a name declared more than once.
    const here = document.offsetAt(position);
    const above = local.filter((d) => document.offsetAt(d.nameRange.start) <= here);
    return hoverFromDef(document, (above.length ? above[above.length - 1] : local[0])!, wordRange);
  }
  const included = await collectIncludedDefinitions(document, token);
  const hit = included.find((h) => h.def.name === word);
  if (hit) {
    return hoverFromDef(hit.document, hit.def, wordRange);
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
        return documentSymbols(document);
      },
    })
  );

  const diagnostics = registerDiagnostics(context);
  registerRename(context, docFilter);
  registerFolding(context, docFilter);

  context.subscriptions.push(
    vscode.languages.registerSignatureHelpProvider(
      docFilter,
      {
        provideSignatureHelp(document, position, token) {
          return provideSignatureHelp(document, position, token);
        },
      },
      { triggerCharacters: ["(", ","], retriggerCharacters: [",", ")"] }
    ),
    vscode.commands.registerCommand("nexa.showCpp", () => {
      const editor = vscode.window.activeTextEditor;
      if (editor?.document.languageId === "nexa") {
        void showGeneratedCpp(editor.document, diagnostics.check);
      }
    }),
    vscode.commands.registerCommand("nexa.checkFile", async () => {
      const editor = vscode.window.activeTextEditor;
      if (editor?.document.languageId === "nexa") {
        if (editor.document.isDirty) {
          await editor.document.save();
        }
        diagnostics.check(editor.document);
      }
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
