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
exports.foldingRanges = foldingRanges;
exports.registerFolding = registerFolding;
const vscode = __importStar(require("vscode"));
const nexaIndex_1 = require("./nexaIndex");
/**
 * Folds for `{ }`, `[ ]` and `( )` that span lines, `#if` ... `#else` ... `#endif`
 * branches, `// region` ... `// endregion`, block comments, runs of `//` comments and
 * runs of `#include`. With a provider, VS Code folds only what it gives, so it gives all of it.
 */
function foldingRanges(document) {
    const text = document.getText();
    const masked = (0, nexaIndex_1.maskCode)(text);
    const out = [];
    const stack = [];
    let line = 0;
    for (let i = 0; i < masked.length; i++) {
        const c = masked[i];
        if (c === "\n") {
            line++;
        }
        else if (c === "{" || c === "[" || c === "(") {
            stack.push({ line });
        }
        else if (c === "}" || c === "]" || c === ")") {
            const open = stack.pop();
            if (open && line > open.line) {
                // Keep the closing line visible when it starts with the bracket.
                const closingFirst = /^\s*[}\])]/.test(document.lineAt(line).text);
                const end = closingFirst ? line - 1 : line;
                if (end > open.line) {
                    out.push(new vscode.FoldingRange(open.line, end));
                }
            }
        }
    }
    const lines = text.split(/\r?\n/);
    const ifs = [];
    const regions = [];
    let commentRun = -1;
    let includeRun = -1;
    const endRun = (start, last, kind) => {
        if (start >= 0 && last > start) {
            out.push(new vscode.FoldingRange(start, last, kind));
        }
    };
    for (let l = 0; l < lines.length; l++) {
        const t = lines[l];
        const directive = /^\s*#\s*([a-z]+)/.exec(t)?.[1];
        if (directive && /^if(n?def)?$/.test(directive)) {
            ifs.push(l);
        }
        else if (directive && /^(else|elif|elifn?def)$/.test(directive) && ifs.length) {
            const start = ifs.pop();
            if (l - 1 > start) {
                out.push(new vscode.FoldingRange(start, l - 1, vscode.FoldingRangeKind.Region));
            }
            ifs.push(l);
        }
        else if (directive === "endif" && ifs.length) {
            const start = ifs.pop();
            if (l - 1 > start) {
                out.push(new vscode.FoldingRange(start, l - 1, vscode.FoldingRangeKind.Region));
            }
        }
        if (/^\s*\/\/\s*#?region\b/.test(t)) {
            regions.push(l);
        }
        else if (/^\s*\/\/\s*#?endregion\b/.test(t) && regions.length) {
            out.push(new vscode.FoldingRange(regions.pop(), l, vscode.FoldingRangeKind.Region));
        }
        const lineComment = /^\s*\/\//.test(t) && !/^\s*\/\/\s*#?(end)?region\b/.test(t);
        if (lineComment) {
            if (commentRun < 0)
                commentRun = l;
        }
        else {
            endRun(commentRun, l - 1, vscode.FoldingRangeKind.Comment);
            commentRun = -1;
        }
        if (directive === "include") {
            if (includeRun < 0)
                includeRun = l;
        }
        else if (t.trim() !== "" || includeRun < 0) {
            endRun(includeRun, lastNonBlank(lines, l - 1), vscode.FoldingRangeKind.Imports);
            includeRun = -1;
        }
    }
    endRun(commentRun, lines.length - 1, vscode.FoldingRangeKind.Comment);
    endRun(includeRun, lastNonBlank(lines, lines.length - 1), vscode.FoldingRangeKind.Imports);
    // Block comments: a `/*` that is not inside a string or after a `//`.
    const noStrings = (0, nexaIndex_1.maskStrings)(text);
    const block = /\/\*[\s\S]*?\*\//g;
    let m;
    while ((m = block.exec(noStrings)) !== null) {
        const lineStart = noStrings.lastIndexOf("\n", m.index) + 1;
        if (noStrings.slice(lineStart, m.index).includes("//")) {
            block.lastIndex = m.index + 2;
            continue;
        }
        const a = document.positionAt(m.index).line;
        const b = document.positionAt(m.index + m[0].length).line;
        if (b > a) {
            out.push(new vscode.FoldingRange(a, b, vscode.FoldingRangeKind.Comment));
        }
    }
    return out;
}
function lastNonBlank(lines, from) {
    let l = from;
    while (l >= 0 && lines[l].trim() === "")
        l--;
    return l;
}
function registerFolding(context, selector) {
    context.subscriptions.push(vscode.languages.registerFoldingRangeProvider(selector, {
        provideFoldingRanges(document) {
            return foldingRanges(document);
        },
    }));
}
