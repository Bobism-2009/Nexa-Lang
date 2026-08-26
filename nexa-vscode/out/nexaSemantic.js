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
exports.SEMANTIC_LEGEND = void 0;
exports.buildSemanticTokens = buildSemanticTokens;
exports.registerSemanticTokens = registerSemanticTokens;
const vscode = __importStar(require("vscode"));
const nexaIndex_1 = require("./nexaIndex");
const MODULES = "io|os|dll|file|random|math|crypto|http|time|thread";
const MODULE_CALL = new RegExp(`\\b(${MODULES})\\.([A-Za-z_][A-Za-z0-9_]*)`, "g");
const INCLUDE_ANGLE = /^\s*(#include)\s*<([^>]+)>/;
const INCLUDE_QUOTE = /^\s*(#include)\s*"([^"]+)"/;
const STD_MODULES = "io|os|dll|file|random|math|crypto|http|time|thread|inline";
exports.SEMANTIC_LEGEND = new vscode.SemanticTokensLegend(["module", "member", "importKeyword", "importModule", "importPrefix"], []);
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
    const masked = (0, nexaIndex_1.maskComments)(document.getText());
    const lines = masked.split(/\r?\n/);
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
        MODULE_CALL.lastIndex = 0;
        let call;
        while ((call = MODULE_CALL.exec(lineText)) !== null) {
            builder.push(line, call.index, call[1].length, 0, 0);
            builder.push(line, call.index + call[1].length + 1, call[2].length, 1, 0);
        }
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
