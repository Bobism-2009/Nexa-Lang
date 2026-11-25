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
const nexaDiagnostics_1 = require("./nexaDiagnostics");
const nexaSignature_1 = require("./nexaSignature");
const nexaRename_1 = require("./nexaRename");
const nexaFolding_1 = require("./nexaFolding");
const nexaSymbols_1 = require("./nexaSymbols");
const nexaIndex_2 = require("./nexaIndex");
const nexaTypes_1 = require("./nexaTypes");
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
        case "macro":
            return vscode.CompletionItemKind.Constant;
        case "type":
            return vscode.CompletionItemKind.TypeParameter;
        case "shape":
            return vscode.CompletionItemKind.Interface;
        default: {
            const _never = kind;
            return _never;
        }
    }
}
function sourceLabel(doc) {
    return path.basename(doc.uri.fsPath);
}
/** The definition's head (a function's whole signature), its doc comment, and where it is. */
function hoverFromDef(doc, d, at) {
    const md = new vscode.MarkdownString();
    md.appendCodeblock((0, nexaIndex_2.definitionHead)(doc, d), "nexa");
    const comment = (0, nexaIndex_2.docComment)(doc, d.defLine);
    if (comment) {
        md.appendMarkdown("\n\n");
        md.appendText(comment);
    }
    md.appendMarkdown(`\n\n*${d.kind}* — ${sourceLabel(doc)}:${d.defLine + 1}`);
    return new vscode.Hover(md, at);
}
function hoverFromMember(type, m, at) {
    if (m.def) {
        const h = hoverFromDef(m.document, m.def, at);
        return h;
    }
    const md = new vscode.MarkdownString();
    md.appendCodeblock(`${m.name}: ${m.detail}`, "nexa");
    const comment = (0, nexaIndex_2.docComment)(m.document, m.range.start.line);
    if (comment) {
        md.appendMarkdown("\n\n");
        md.appendText(comment);
    }
    md.appendMarkdown(`\n\n*field of ${type}* — ${sourceLabel(m.document)}:${m.range.start.line + 1}`);
    return new vscode.Hover(md, at);
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
    if (/^\s*#\s*[a-z]*$/.test(prefix)) {
        return (0, nexaCompletions_1.directiveCompletions)();
    }
    // io., os., ... list the module; any other name. (a string, a slice, a map) falls
    // through to the methods.
    const modDot = prefix.match(/(?:^|[^A-Za-z0-9_.])([a-z][a-z0-9]*)\.\s*([A-Za-z_]*)$/);
    if (modDot && nexaCompletions_1.MODULE_MEMBERS[modDot[1]]) {
        return (0, nexaCompletions_1.moduleMemberCompletions)(modDot[1]);
    }
    const strDot = prefix.match(/\.([A-Za-z_]*)$/);
    if (strDot) {
        // p. on a struct or shape: its fields and methods.
        const recv = prefix.match(/([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*[A-Za-z_]*$/);
        if (recv && !/\.\s*\.\s*[A-Za-z_]*$/.test(prefix)) {
            const before = document.offsetAt(position) - recv[0].length;
            const found = await (0, nexaTypes_1.membersOfReceiver)(document, recv[1], before, token);
            if (found?.members && found.members.length) {
                return found.members.map((m) => {
                    const c = new vscode.CompletionItem(m.name, m.kind === "field" ? vscode.CompletionItemKind.Field : vscode.CompletionItemKind.Method);
                    c.detail = m.kind === "field" ? `${m.name}: ${m.detail}` : m.detail;
                    c.sortText = (m.kind === "field" ? "0" : "1") + m.name;
                    if (m.def) {
                        const comment = (0, nexaIndex_2.docComment)(m.document, m.def.defLine);
                        if (comment)
                            c.documentation = comment;
                    }
                    return c;
                });
            }
        }
        return (0, nexaCompletions_1.stringMethodCompletions)();
    }
    items.push(...(0, nexaCompletions_1.keywordCompletions)());
    items.push(...(0, nexaCompletions_1.typeCompletions)());
    items.push(...(0, nexaCompletions_1.predefinedMacroCompletions)());
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
    // p.x and p.move(): the field or method of p's struct.
    const lineBefore = line.slice(0, wordRange.start.character);
    const recv = /([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*$/.exec(lineBefore);
    if (recv && !/\.\s*\.\s*$/.test(lineBefore)) {
        const before = document.offsetAt(wordRange.start) - recv[0].length;
        const found = await (0, nexaTypes_1.membersOfReceiver)(document, recv[1], before, token);
        const member = found?.members?.find((m) => m.name === word);
        if (found && member) {
            return hoverFromMember(found.type, member, wordRange);
        }
        if (nexaCompletions_1.HOVER_DOCS[word]) {
            return new vscode.Hover(nexaCompletions_1.HOVER_DOCS[word], wordRange);
        }
        return null;
    }
    const directive = /^\s*#\s*([a-z]+)/.exec(line);
    if (directive && directive[1] === word && nexaCompletions_1.HOVER_DOCS[`#${word}`]) {
        return new vscode.Hover(nexaCompletions_1.HOVER_DOCS[`#${word}`], wordRange);
    }
    // Keyword and built-in docs (match, step, Some, ...), unless the file defines the name.
    if (nexaCompletions_1.HOVER_DOCS[word]) {
        const own = (0, nexaIndex_1.indexNexaDefinitions)(document).some((d) => d.name === word);
        if (!own) {
            return new vscode.Hover(nexaCompletions_1.HOVER_DOCS[word], wordRange);
        }
    }
    const local = (0, nexaIndex_1.indexNexaDefinitions)(document).filter((d) => d.name === word);
    if (local.length) {
        // The nearest definition above the cursor, for a name declared more than once.
        const here = document.offsetAt(position);
        const above = local.filter((d) => document.offsetAt(d.nameRange.start) <= here);
        return hoverFromDef(document, (above.length ? above[above.length - 1] : local[0]), wordRange);
    }
    const included = await (0, nexaIndex_1.collectIncludedDefinitions)(document, token);
    const hit = included.find((h) => h.def.name === word);
    if (hit) {
        return hoverFromDef(hit.document, hit.def, wordRange);
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
            return (0, nexaSymbols_1.documentSymbols)(document);
        },
    }));
    const diagnostics = (0, nexaDiagnostics_1.registerDiagnostics)(context);
    (0, nexaRename_1.registerRename)(context, docFilter);
    (0, nexaFolding_1.registerFolding)(context, docFilter);
    context.subscriptions.push(vscode.languages.registerSignatureHelpProvider(docFilter, {
        provideSignatureHelp(document, position, token) {
            return (0, nexaSignature_1.provideSignatureHelp)(document, position, token);
        },
    }, { triggerCharacters: ["(", ","], retriggerCharacters: [",", ")"] }), vscode.commands.registerCommand("nexa.showCpp", () => {
        const editor = vscode.window.activeTextEditor;
        if (editor?.document.languageId === "nexa") {
            void (0, nexaDiagnostics_1.showGeneratedCpp)(editor.document, diagnostics.check);
        }
    }), vscode.commands.registerCommand("nexa.checkFile", async () => {
        const editor = vscode.window.activeTextEditor;
        if (editor?.document.languageId === "nexa") {
            if (editor.document.isDirty) {
                await editor.document.save();
            }
            diagnostics.check(editor.document);
        }
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
