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
exports.maskComments = maskComments;
exports.indexNexaDefinitions = indexNexaDefinitions;
const vscode = __importStar(require("vscode"));
const FN_RE = /\bfn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(/g;
const STRUCT_RE = /\bstruct\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const ENUM_RE = /\benum\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
/** Replace // and / ** / with spaces. Same length as input for offset mapping. */
function maskComments(s) {
    const a = s.split("");
    for (let i = 0; i < a.length; i++) {
        if (i < a.length - 1 && a[i] === "/" && a[i + 1] === "/") {
            a[i] = " ";
            a[i + 1] = " ";
            i += 2;
            while (i < a.length && a[i] !== "\n" && a[i] !== "\r") {
                a[i] = " ";
                i++;
            }
            i--;
            continue;
        }
        if (i < a.length - 1 && a[i] === "/" && a[i + 1] === "*") {
            a[i] = " ";
            a[i + 1] = " ";
            i += 2;
            while (i < a.length - 1) {
                if (a[i] === "*" && a[i + 1] === "/") {
                    a[i] = " ";
                    a[i + 1] = " ";
                    i += 1;
                    break;
                }
                a[i] = " ";
                i++;
            }
            continue;
        }
    }
    return a.join("");
}
function runPattern(doc, masked, re, kind) {
    const out = [];
    re.lastIndex = 0;
    let m;
    while ((m = re.exec(masked)) !== null) {
        const name = m[1];
        if (!name) {
            continue;
        }
        const nameOffset = m.index + m[0].indexOf(name);
        const defStart = m.index;
        const defStartPos = doc.positionAt(defStart);
        const line = doc.lineAt(defStartPos.line);
        if (kind === "function" && !/\bfn\s/.test(line.text)) {
            continue;
        }
        if (kind === "struct" && !/\bstruct\s/.test(line.text)) {
            continue;
        }
        if (kind === "enum" && !/\benum\s/.test(line.text)) {
            continue;
        }
        const nameStart = doc.positionAt(nameOffset);
        const nameEnd = doc.positionAt(nameOffset + name.length);
        out.push({
            name,
            kind,
            nameRange: new vscode.Range(nameStart, nameEnd),
            lineRange: new vscode.Range(defStartPos.line, 0, defStartPos.line, line.text.length),
            defLine: defStartPos.line,
        });
    }
    return out;
}
function indexNexaDefinitions(doc) {
    const masked = maskComments(doc.getText());
    const defs = [
        ...runPattern(doc, masked, FN_RE, "function"),
        ...runPattern(doc, masked, STRUCT_RE, "struct"),
        ...runPattern(doc, masked, ENUM_RE, "enum"),
    ];
    return uniqueByLine(defs);
}
function uniqueByLine(defs) {
    const seen = new Set();
    return defs.filter((d) => {
        const k = `${d.defLine}\0${d.name}\0${d.kind}`;
        if (seen.has(k)) {
            return false;
        }
        seen.add(k);
        return true;
    });
}
