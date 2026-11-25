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
exports.NOT_NAMES = exports.SEMANTIC_LEGEND = void 0;
exports.buildSemanticTokens = buildSemanticTokens;
exports.registerSemanticTokens = registerSemanticTokens;
const vscode = __importStar(require("vscode"));
const nexaIndex_1 = require("./nexaIndex");
const nexaCompletions_1 = require("./nexaCompletions");
const MODULES = "io|os|dll|file|random|math|crypto|http|tcp|udp|json|time|thread|term|gfx3d|gfx|ui|physics";
const MODULE_CALL = new RegExp(`\\b(${MODULES})\\.([A-Za-z_][A-Za-z0-9_]*)`, "g");
const INCLUDE_ANGLE = /^\s*(#include)\s*<([^>]+)>/;
const INCLUDE_QUOTE = /^\s*(#include)\s*"([^"]+)"/;
const STD_MODULES = "io|os|dll|file|random|math|crypto|network|json|time|thread|term|gfx3d|gfx|ui|physics|inline";
exports.SEMANTIC_LEGEND = new vscode.SemanticTokensLegend(["module", "member", "importKeyword", "importModule", "importPrefix", "nexaVariable",
    "nexaFunctionDef", "nexaFunctionCall", "nexaMacro"], []);
const VARIABLE = 5;
const FUNCTION_DEF = 6;
const FUNCTION_CALL = 7;
const MACRO = 8;
// Every name #define gives a meaning to in this file, plus the ones NexaC predefines.
function macroNames(text) {
    const names = new Set(nexaCompletions_1.NEXA_PREDEFINED_MACROS.map((m) => m.name).filter((n) => n !== "__has_include"));
    const def = /^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)/gm;
    let m;
    while ((m = def.exec(text)) !== null)
        names.add(m[1]);
    return names;
}
exports.NOT_NAMES = new Set(("fn extern new delete sizeof let const struct enum type shape priv if else while for " +
    "switch match case default return break continue defer goto try catch throw in step " +
    "and or not self null true false int short long size_t string bool float float64 " +
    "float32 char void unsigned map Result result").split(" "));
function addParams(list, names) {
    for (const part of list.split(",")) {
        const m = /^\s*(?:\.\.\.\s*)?([A-Za-z_][A-Za-z0-9_]*)\s*(?::|$)/.exec(part);
        if (m && !exports.NOT_NAMES.has(m[1]))
            names.add(m[1]);
    }
}
// The names this file declares as variables: let/const, function and lambda
// parameters, for-loop and catch variables. Scopes are not tracked: a name
// declared anywhere is a variable everywhere it is used, which is what the eye wants.
function declaredNames(text) {
    const names = new Set();
    const add = (n) => { if (n && !exports.NOT_NAMES.has(n))
        names.add(n); };
    let m;
    const decl = /\b(?:let|const)\s+([A-Za-z_][A-Za-z0-9_]*)/g;
    while ((m = decl.exec(text)) !== null)
        add(m[1]);
    const fn = /\bfn\b(?:\s+[A-Za-z_][A-Za-z0-9_]*)?\s*(?:\[[^\]]*\]\s*)?\(([^)]*)\)/g;
    while ((m = fn.exec(text)) !== null)
        addParams(m[1], names);
    const loop = /\bfor\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)(?:\s*,\s*([A-Za-z_][A-Za-z0-9_]*))?\s+in\b/g;
    while ((m = loop.exec(text)) !== null) {
        add(m[1]);
        add(m[2]);
    }
    const caught = /\bcatch\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)/g;
    while ((m = caught.exec(text)) !== null)
        add(m[1]);
    return names;
}
const WORD = /[A-Za-z_][A-Za-z0-9_]*/g;
function pushIncludeAngle(builder, line, match) {
    const base = match.index ?? 0;
    const includeAt = match[0].indexOf("#include");
    builder.push(line, base + includeAt, 8, 2, 0);
    const path = match[2] ?? "";
    const lt = match[0].indexOf("<");
    const pathStart = base + lt + 1;
    const std = new RegExp(`^(std/)(${STD_MODULES})$`).exec(path);
    if (std) {
        builder.push(line, pathStart, std[1].length, 4, 0);
        builder.push(line, pathStart + std[1].length, std[2].length, 3, 0);
        return;
    }
    builder.push(line, pathStart, path.length, 3, 0);
}
function pushIncludeQuote(builder, line, match) {
    const base = match.index ?? 0;
    const includeAt = match[0].indexOf("#include");
    builder.push(line, base + includeAt, 8, 2, 0);
    const path = match[2] ?? "";
    const q = match[0].indexOf('"');
    builder.push(line, base + q + 1, path.length, 3, 0);
}
function buildSemanticTokens(document) {
    const builder = new vscode.SemanticTokensBuilder(exports.SEMANTIC_LEGEND);
    const masked = (0, nexaIndex_1.maskStrings)((0, nexaIndex_1.maskComments)(document.getText()));
    const lines = masked.split(/\r?\n/);
    const vars = declaredNames(masked);
    const macros = macroNames(masked);
    for (let line = 0; line < lines.length; line++) {
        const lineText = lines[line] ?? "";
        const angle = INCLUDE_ANGLE.exec(lineText);
        if (angle) {
            pushIncludeAngle(builder, line, angle);
            continue;
        }
        const quoted = INCLUDE_QUOTE.exec(lineText);
        if (quoted) {
            pushIncludeQuote(builder, line, quoted);
            continue;
        }
        const directive = /^\s*#/.test(lineText);
        const tokens = [];
        MODULE_CALL.lastIndex = 0;
        let call;
        while ((call = MODULE_CALL.exec(lineText)) !== null) {
            tokens.push([call.index, call[1].length, 0]);
            tokens.push([call.index + call[1].length + 1, call[2].length, 1]);
        }
        WORD.lastIndex = 0;
        let w;
        while ((w = WORD.exec(lineText)) !== null) {
            const at = w.index;
            const name = w[0];
            if (exports.NOT_NAMES.has(name) || /[0-9]/.test(lineText[at - 1] ?? ""))
                continue;
            if (tokens.some(([s, l]) => at >= s && at < s + l))
                continue;
            if (macros.has(name) && !/\.\s*$/.test(lineText.slice(0, at).replace(/\.\.\s*$/, ""))) {
                tokens.push([at, name.length, MACRO]);
                continue;
            }
            if (directive)
                continue;
            const before = lineText.slice(0, at);
            if (/\bfn\s+$/.test(before)) {
                tokens.push([at, name.length, FUNCTION_DEF]);
                continue;
            }
            if (/^\s*\(/.test(lineText.slice(at + name.length))) {
                tokens.push([at, name.length, FUNCTION_CALL]);
                continue;
            }
            if (!vars.has(name))
                continue;
            if (/[0-9]/.test(lineText[at - 1] ?? ""))
                continue;
            if (/\.\s*$/.test(lineText.slice(0, at)) && !/\.\.\s*$/.test(lineText.slice(0, at)))
                continue;
            if (/^\s*\(/.test(lineText.slice(at + name.length)))
                continue;
            if (tokens.some(([s, l]) => at >= s && at < s + l))
                continue;
            tokens.push([at, name.length, VARIABLE]);
        }
        tokens.sort((x, y) => x[0] - y[0]);
        for (const [start, length, type] of tokens)
            builder.push(line, start, length, type, 0);
    }
    return builder.build();
}
function registerSemanticTokens(context) {
    const selector = [
        { language: "nexa", scheme: "file" },
    ];
    context.subscriptions.push(vscode.languages.registerDocumentSemanticTokensProvider(selector, {
        provideDocumentSemanticTokens(document) {
            return buildSemanticTokens(document);
        },
    }, exports.SEMANTIC_LEGEND));
}
