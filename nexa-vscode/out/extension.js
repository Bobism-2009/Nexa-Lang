"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const path = __importStar(require("path"));
const vscode = __importStar(require("vscode"));
const nexaIndex_1 = require("./nexaIndex");
const nexaCompletions_1 = require("./nexaCompletions");
const nexaRun_1 = require("./nexaRun");
const nexaSemantic_1 = require("./nexaSemantic");
const WORD = (name) => new RegExp(`\\b${escapeRe(name)}\\b`, "g");
function escapeRe(s) {
    return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
const RESERVED = new Set([
    "if", "else", "while", "for", "switch", "case", "default", "return",
    "break", "continue", "let", "const", "fn", "extern", "struct", "enum",
    "try", "catch", "throw", "true", "false", "null", "new", "delete", "sizeof",
]);
function isReservedWordForLookup(name) {
    return RESERVED.has(name);
}
function allRefsInDocument(doc, name) {
    if (isReservedWordForLookup(name)) {
        return [];
    }
    const masked = (0, nexaIndex_1.maskComments)(doc.getText());
    const re = WORD(name);
    re.lastIndex = 0;
    const out = [];
    let m;
    while ((m = re.exec(masked)) !== null) {
        const start = doc.positionAt(m.index);
        const end = doc.positionAt(m.index + name.length);
        out.push(new vscode.Location(doc.uri, new vscode.Range(start, end)));
    }
    return out;
}
async function allRefsInWorkspace(document, name, token) {
    if (isReservedWordForLookup(name)) {
        return [];
    }
    if (!vscode.workspace.getWorkspaceFolder(document.uri)) {
        return allRefsInDocument(document, name);
    }
    const uris = await vscode.workspace.findFiles("**/*.nxa", "{**/node_modules/**,**/.git/**}", 2000);
    const out = [];
    for (const uri of uris) {
        if (token.isCancellationRequested) {
            return out;
        }
        let doc;
        try {
            doc = await vscode.workspace.openTextDocument(uri);
        }
        catch {
            continue;
        }
        const re = new RegExp(`\\b${escapeRe(name)}\\b`, "g");
        const masked = (0, nexaIndex_1.maskComments)(doc.getText());
        let m;
        while ((m = re.exec(masked)) !== null) {
            const start = doc.positionAt(m.index);
            const end = doc.positionAt(m.index + name.length);
            out.push(new vscode.Location(doc.uri, new vscode.Range(start, end)));
        }
    }
    return out;
}
function defKindToSymbol(d) {
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
            const _never = d.kind;
            return _never;
        }
    }
}
function defKindToCompletion(kind) {
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
            const _never = kind;
            return _never;
        }
    }
}
function sourceLabel(doc) {
    return path.basename(doc.uri.fsPath);
}
function hoverFromDef(doc, d) {
    const line = doc.lineAt(d.defLine).text.trim();
    const md = new vscode.MarkdownString();
    md.appendMarkdown(`**${d.kind}** \`${d.name}\` — ${sourceLabel(doc)}\n\n`);
    md.appendCodeblock(line, "nexa");
    return new vscode.Hover(md, d.nameRange);
}
function locationsFromLocated(hits) {
    if (hits.length === 1) {
        return new vscode.Location(hits[0].document.uri, hits[0].def.nameRange);
    }
    return hits.map((h) => new vscode.Location(h.document.uri, h.def.nameRange));
}
function isDefNameRange(doc, name, range) {
    for (const d of (0, nexaIndex_1.indexNexaDefinitions)(doc)) {
        if (d.name === name && d.nameRange.isEqual(range)) {
            return true;
        }
    }
    return false;
}
function locationsFromIndexedDefs(document, defs) {
    if (defs.length === 1) {
        return new vscode.Location(document.uri, defs[0].nameRange);
    }
    return defs.map((d) => new vscode.Location(document.uri, d.nameRange));
}
async function findDefinitionInOtherFiles(name, ownUri, token) {
    if (isReservedWordForLookup(name)) {
        return null;
    }
    if (!vscode.workspace.getWorkspaceFolder(ownUri)) {
        return null;
    }
    const uris = await vscode.workspace.findFiles("**/*.nxa", "{**/node_modules/**,**/.git/**}", 500);
    const found = [];
    for (const uri of uris) {
        if (token.isCancellationRequested) {
            return null;
        }
        if (uri.toString() === ownUri.toString()) {
            continue;
        }
        let doc;
        try {
            doc = await vscode.workspace.openTextDocument(uri);
        }
        catch {
            continue;
        }
        for (const d of (0, nexaIndex_1.indexNexaDefinitions)(doc)) {
            if (d.name === name) {
                found.push(new vscode.Location(doc.uri, d.nameRange));
            }
        }
    }
    if (found.length === 0) {
        return null;
    }
    if (found.length === 1) {
        return found[0];
    }
    return found;
}
async function provideCompletions(document, position, token) {
    const prefix = (0, nexaCompletions_1.linePrefix)(document, position);
    const items = [];
    const modDot = prefix.match(/(?:^|\s)([a-z]+)\.\s*([A-Za-z_]*)$/);
    if (modDot) {
        return (0, nexaCompletions_1.moduleMemberCompletions)(modDot[1]);
    }
    const strDot = prefix.match(/\.([A-Za-z_]*)$/);
    if (strDot) {
        return (0, nexaCompletions_1.stringMethodCompletions)();
    }
    items.push(...(0, nexaCompletions_1.keywordCompletions)());
    items.push(...(0, nexaCompletions_1.typeCompletions)());
    for (const d of (0, nexaIndex_1.indexNexaDefinitions)(document)) {
        const c = new vscode.CompletionItem(d.name, defKindToCompletion(d.kind));
        c.detail = `Nexa ${d.kind} in this file`;
        c.sortText = "2" + d.name;
        items.push(c);
    }
    const included = await (0, nexaIndex_1.collectIncludedDefinitions)(document, token);
    const seen = new Set((0, nexaIndex_1.indexNexaDefinitions)(document).map((d) => `${d.kind}:${d.name}`));
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
    for (const mod of Object.keys(nexaCompletions_1.MODULE_MEMBERS)) {
        const c = new vscode.CompletionItem(mod, vscode.CompletionItemKind.Module);
        c.detail = `${mod}.* standard module`;
        c.insertText = new vscode.SnippetString(`${mod}.$0`);
        items.push(c);
    }
    return items;
}
async function provideHover(document, position, token) {
    const includeSpec = (0, nexaIndex_1.includeSpecAtPosition)(document, position);
    if (includeSpec) {
        const uri = await (0, nexaIndex_1.resolveIncludeUri)(document, includeSpec);
        if (uri) {
            return new vscode.Hover(`Included file \`${path.basename(uri.fsPath)}\``, document.lineAt(position.line).range);
        }
    }
    const wordRange = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
    if (!wordRange) {
        return null;
    }
    const word = document.getText(wordRange);
    const line = document.lineAt(position.line).text;
    const modCall = line.match(new RegExp(`\\b(${Object.keys(nexaCompletions_1.MODULE_MEMBERS).join("|")})\\.${escapeRe(word)}\\b`));
    if (modCall) {
        const key = `${modCall[1]}.${word}`;
        const members = nexaCompletions_1.MODULE_MEMBERS[modCall[1]];
        const member = members?.find((m) => m.name === word);
        const docText = nexaCompletions_1.HOVER_DOCS[key] ?? member?.detail ?? key;
        return new vscode.Hover(docText, wordRange);
    }
    if (word === "null" && nexaCompletions_1.HOVER_DOCS.null) {
        return new vscode.Hover(nexaCompletions_1.HOVER_DOCS.null, wordRange);
    }
    if (line.includes("extern fn") && word !== "extern" && word !== "fn") {
        const defs = (0, nexaIndex_1.indexNexaDefinitions)(document).filter((d) => d.name === word && d.kind === "function");
        if (defs.length) {
            return new vscode.Hover("extern fn — C FFI declaration", defs[0].nameRange);
        }
    }
    const local = (0, nexaIndex_1.indexNexaDefinitions)(document).filter((d) => d.name === word);
    if (local.length) {
        return hoverFromDef(document, local[0]);
    }
    const included = await (0, nexaIndex_1.collectIncludedDefinitions)(document, token);
    const hit = included.find((h) => h.def.name === word);
    if (hit) {
        return hoverFromDef(hit.document, hit.def);
    }
    return null;
}
function activate(context) {
    (0, nexaSemantic_1.registerSemanticTokens)(context);
    const docFilter = [
        { language: "nexa", scheme: "file" },
    ];
    context.subscriptions.push(vscode.commands.registerCommand("nexa.runFile", () => {
        const editor = vscode.window.activeTextEditor;
        if (editor?.document.languageId === "nexa") {
            void (0, nexaRun_1.runNexaFile)(editor.document, true);
        }
    }), vscode.commands.registerCommand("nexa.buildFile", () => {
        const editor = vscode.window.activeTextEditor;
        if (editor?.document.languageId === "nexa") {
            void (0, nexaRun_1.runNexaFile)(editor.document, false);
        }
    }));
    context.subscriptions.push(vscode.languages.registerDefinitionProvider(docFilter, {
        provideDefinition(document, position, token) {
            return (async () => {
                const includeSpec = (0, nexaIndex_1.includeSpecAtPosition)(document, position);
                if (includeSpec) {
                    const uri = await (0, nexaIndex_1.resolveIncludeUri)(document, includeSpec);
                    if (uri) {
                        return new vscode.Location(uri, new vscode.Position(0, 0));
                    }
                }
                const idRange = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
                if (!idRange) {
                    return null;
                }
                const name = document.getText(idRange);
                if (isReservedWordForLookup(name)) {
                    return null;
                }
                const local = (0, nexaIndex_1.indexNexaDefinitions)(document).filter((d) => d.name === name);
                if (local.length) {
                    return locationsFromIndexedDefs(document, local);
                }
                const included = await (0, nexaIndex_1.collectIncludedDefinitions)(document, token);
                const hits = included.filter((h) => h.def.name === name);
                if (hits.length) {
                    return locationsFromLocated(hits);
                }
                return findDefinitionInOtherFiles(name, document.uri, token);
            })();
        },
    }));
    context.subscriptions.push(vscode.languages.registerReferenceProvider(docFilter, {
        provideReferences(document, position, refContext, token) {
            return (async () => {
                const idRange = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
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
                const out = [];
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
    }));
    context.subscriptions.push(vscode.languages.registerDocumentSymbolProvider(docFilter, {
        provideDocumentSymbols(document) {
            return (0, nexaIndex_1.indexNexaDefinitions)(document).map((d) => new vscode.DocumentSymbol(d.name, d.kind, defKindToSymbol(d), d.lineRange, d.nameRange));
        },
    }));
    context.subscriptions.push(vscode.languages.registerCompletionItemProvider(docFilter, {
        provideCompletionItems(document, position, token) {
            return provideCompletions(document, position, token);
        },
    }, ".", " "));
    context.subscriptions.push(vscode.languages.registerHoverProvider(docFilter, {
        provideHover(document, position, token) {
            return provideHover(document, position, token);
        },
    }));
}
function deactivate() { }
