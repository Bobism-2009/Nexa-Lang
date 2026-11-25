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
exports.documentSymbols = documentSymbols;
const vscode = __importStar(require("vscode"));
const nexaIndex_1 = require("./nexaIndex");
function symbolKind(d, inStruct) {
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
/**
 * The outline as a tree: a struct or shape holds its fields and methods, an enum its
 * variants, a function its locals. Each symbol spans its whole body, so the
 * breadcrumbs follow the cursor into it.
 */
function documentSymbols(document) {
    const masked = (0, nexaIndex_1.maskCode)(document.getText());
    const defs = (0, nexaIndex_1.indexNexaDefinitions)(document).sort((a, b) => document.offsetAt(a.nameRange.start) - document.offsetAt(b.nameRange.start));
    const nodes = [];
    for (const d of defs) {
        const nameStart = document.offsetAt(d.nameRange.start);
        const lineStart = document.offsetAt(d.lineRange.start);
        let end = document.offsetAt(d.lineRange.end);
        let body = null;
        if (d.kind === "function" || d.kind === "struct" || d.kind === "enum" || d.kind === "shape") {
            const head = (0, nexaIndex_1.scanHead)(masked, document.offsetAt(d.nameRange.end));
            if (head.open >= 0 && head.close >= 0) {
                body = { open: head.open, close: head.close };
                end = head.close + 1;
            }
        }
        const detail = d.kind === "function"
            ? (0, nexaIndex_1.definitionHead)(document, d, masked).replace(/^.*?\bfn\s+[A-Za-z_][A-Za-z0-9_]*\s*/, "")
            : d.kind === "type"
                ? (0, nexaIndex_1.definitionHead)(document, d, masked).replace(/^.*?=\s*/, "= ")
                : "";
        const sym = new vscode.DocumentSymbol(d.name, detail, symbolKind(d, false), new vscode.Range(document.positionAt(lineStart), document.positionAt(end)), d.nameRange);
        if (body && (d.kind === "struct" || d.kind === "shape")) {
            for (const f of (0, nexaIndex_1.structFields)(masked, body.open, body.close)) {
                const at = document.positionAt(f.offset);
                const r = new vscode.Range(at, at.translate(0, f.name.length));
                sym.children.push(new vscode.DocumentSymbol(f.name, f.detail, vscode.SymbolKind.Field, r, r));
            }
        }
        if (body && d.kind === "enum") {
            for (const v of (0, nexaIndex_1.enumVariants)(masked, body.open, body.close)) {
                const at = document.positionAt(v.offset);
                const r = new vscode.Range(at, at.translate(0, v.name.length));
                sym.children.push(new vscode.DocumentSymbol(v.name, v.detail, vscode.SymbolKind.EnumMember, r, r));
            }
        }
        nodes.push({ sym, start: nameStart, end, def: d });
    }
    // Nest each symbol in the innermost function, struct or shape whose body holds it.
    const containers = nodes.filter((n) => n.sym.range.end.line > n.sym.range.start.line);
    const top = [];
    for (const n of nodes) {
        let parent = null;
        for (const c of containers) {
            if (c === n || n.start <= c.start || n.start >= c.end)
                continue;
            if (!parent || c.end - c.start < parent.end - parent.start)
                parent = c;
        }
        if (parent) {
            const inStruct = parent.def.kind === "struct" || parent.def.kind === "shape";
            n.sym.kind = symbolKind(n.def, inStruct);
            parent.sym.children.push(n.sym);
        }
        else {
            top.push(n.sym);
        }
    }
    for (const n of nodes) {
        n.sym.children.sort((a, b) => a.range.start.compareTo(b.range.start));
    }
    return top;
}
