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
const vscode = __importStar(require("vscode"));
const nexaIndex_1 = require("./nexaIndex");
const WORD = (name) => new RegExp(`\\b${escapeRe(name)}\\b`, "g");
function escapeRe(s) {
    return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
/** Only keywords / declarations that are never a user-defined identifier for lookup. */
const RESERVED = new Set("if,else,while,for,switch,case,default,return,break,continue,let,const,fn"
    .split(",")
    .map((s) => s.trim()));
function isReservedWordForLookup(name) {
    if (RESERVED.has(name)) {
        return true;
    }
    if (name === "struct" || name === "enum" || name === "false" || name === "true") {
        return true;
    }
    return false;
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
    if (d.kind === "struct") {
        return vscode.SymbolKind.Struct;
    }
    if (d.kind === "enum") {
        return vscode.SymbolKind.Enum;
    }
    return vscode.SymbolKind.Function;
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
function activate(context) {
    const docFilter = [
        { language: "nexa", scheme: "file" },
    ];
    context.subscriptions.push(vscode.languages.registerDefinitionProvider(docFilter, {
        provideDefinition(document, position, token) {
            return (async () => {
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
                return findDefinitionInOtherFiles(name, document.uri, token);
            })();
        },
    }));
    context.subscriptions.push(vscode.languages.registerReferenceProvider(docFilter, {
        provideReferences(document, position, context, token) {
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
                if (context.includeDeclaration) {
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
            return (0, nexaIndex_1.indexNexaDefinitions)(document).map((d) => {
                return new vscode.DocumentSymbol(d.name, d.kind, defKindToSymbol(d), d.lineRange, d.nameRange);
            });
        },
    }));
    context.subscriptions.push(vscode.languages.registerCompletionItemProvider(docFilter, {
        provideCompletionItems(document) {
            const items = [];
            for (const d of (0, nexaIndex_1.indexNexaDefinitions)(document)) {
                if (d.kind === "function") {
                    const c = new vscode.CompletionItem(d.name, vscode.CompletionItemKind.Function);
                    c.detail = "Nexa function in this file";
                    items.push(c);
                }
            }
            return items;
        },
    }));
}
function deactivate() { }
