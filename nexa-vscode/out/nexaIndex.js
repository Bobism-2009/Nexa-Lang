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
exports.extractIncludes = extractIncludes;
exports.resolveIncludeUri = resolveIncludeUri;
exports.collectIncludedDefinitions = collectIncludedDefinitions;
exports.includeSpecAtPosition = includeSpecAtPosition;
const path = __importStar(require("path"));
const vscode = __importStar(require("vscode"));
const FN_RE = /\b(?:extern\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(/g;
const STRUCT_RE = /\bstruct\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const ENUM_RE = /\benum\s+([A-Za-z_][A-Za-z0-9_]*)\b/g;
const LET_RE = /^\s*let\s+(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:[:=;]|$)/gm;
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
        if (kind === "function" && !/\b(?:extern\s+)?fn\s/.test(line.text)) {
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
        ...indexLetBindings(doc, masked),
    ];
    return uniqueByLine(defs);
}
function indexLetBindings(doc, masked) {
    const out = [];
    LET_RE.lastIndex = 0;
    let m;
    while ((m = LET_RE.exec(masked)) !== null) {
        const name = m[1];
        const nameOffset = m.index + m[0].indexOf(name);
        const defStartPos = doc.positionAt(m.index);
        const line = doc.lineAt(defStartPos.line);
        const nameStart = doc.positionAt(nameOffset);
        const nameEnd = doc.positionAt(nameOffset + name.length);
        out.push({
            name,
            kind: "variable",
            nameRange: new vscode.Range(nameStart, nameEnd),
            lineRange: new vscode.Range(defStartPos.line, 0, defStartPos.line, line.text.length),
            defLine: defStartPos.line,
        });
    }
    return out;
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
const INCLUDE_LINE_RE = /^\s*#include\s*(?:<([^>]+)>|"([^"]+)")/gm;
function extractIncludes(masked) {
    const out = [];
    INCLUDE_LINE_RE.lastIndex = 0;
    let m;
    while ((m = INCLUDE_LINE_RE.exec(masked)) !== null) {
        if (m[2]) {
            out.push({ kind: "quote", path: m[2] });
        }
        else if (m[1]) {
            out.push({ kind: "angle", path: m[1] });
        }
    }
    return out;
}
function shouldFollowInclude(p) {
    const norm = p.replace(/\\/g, "/");
    if (norm.startsWith("std/")) {
        return false;
    }
    if (/\.(h|hh|hpp|hxx|c|cc|cpp|cxx)$/i.test(norm)) {
        return false;
    }
    return true;
}
async function uriExists(uri) {
    try {
        await vscode.workspace.fs.stat(uri);
        return true;
    }
    catch {
        return false;
    }
}
function withNxaIfNeeded(p) {
    const norm = p.replace(/\\/g, "/");
    if (/\.[A-Za-z0-9]+$/.test(norm)) {
        return [p];
    }
    return [p, `${p}.nxa`];
}
function packageSearchRoots(from) {
    const roots = [];
    const folder = vscode.workspace.getWorkspaceFolder(from);
    if (folder) {
        roots.push(folder.uri);
        roots.push(vscode.Uri.joinPath(folder.uri, ".nexa", "packages"));
    }
    const home = process.env.USERPROFILE || process.env.HOME;
    if (home) {
        roots.push(vscode.Uri.file(path.join(home, ".nexa", "packages")));
    }
    return roots;
}
async function resolveIncludeUri(from, spec) {
    if (!shouldFollowInclude(spec.path)) {
        return null;
    }
    const parts = spec.path.replace(/\\/g, "/").split("/").filter((p) => p.length > 0);
    if (parts.length === 0) {
        return null;
    }
    if (spec.kind === "quote") {
        const baseDir = path.dirname(from.uri.fsPath);
        for (const rel of withNxaIfNeeded(spec.path)) {
            const abs = path.isAbsolute(rel)
                ? path.normalize(rel)
                : path.normalize(path.join(baseDir, rel));
            const uri = vscode.Uri.file(abs);
            if (await uriExists(uri)) {
                return uri;
            }
        }
        return null;
    }
    for (const root of packageSearchRoots(from.uri)) {
        for (const rel of withNxaIfNeeded(spec.path)) {
            const relParts = rel.replace(/\\/g, "/").split("/").filter((p) => p.length > 0);
            const uri = vscode.Uri.joinPath(root, ...relParts);
            if (await uriExists(uri)) {
                return uri;
            }
        }
    }
    return null;
}
async function collectIncludedDefinitions(document, token) {
    const out = [];
    const visited = new Set([document.uri.toString()]);
    const queue = [document];
    let scanned = 0;
    while (queue.length > 0) {
        if (token?.isCancellationRequested || scanned > 100) {
            break;
        }
        const current = queue.shift();
        scanned += 1;
        const specs = extractIncludes(maskComments(current.getText()));
        for (const spec of specs) {
            if (token?.isCancellationRequested) {
                break;
            }
            const uri = await resolveIncludeUri(current, spec);
            if (!uri) {
                continue;
            }
            const key = uri.toString();
            if (visited.has(key)) {
                continue;
            }
            visited.add(key);
            let included;
            try {
                included = await vscode.workspace.openTextDocument(uri);
            }
            catch {
                continue;
            }
            for (const def of indexNexaDefinitions(included)) {
                out.push({ def, document: included });
            }
            queue.push(included);
        }
    }
    return out;
}
function includeSpecAtPosition(document, position) {
    const line = document.lineAt(position.line).text;
    const quote = /^\s*#include\s*"([^"]+)"/.exec(line);
    if (quote) {
        const start = line.indexOf('"') + 1;
        const end = start + quote[1].length;
        if (position.character >= start && position.character <= end) {
            return { kind: "quote", path: quote[1] };
        }
    }
    const angle = /^\s*#include\s*<([^>]+)>/.exec(line);
    if (angle) {
        const start = line.indexOf("<") + 1;
        const end = start + angle[1].length;
        if (position.character >= start && position.character <= end) {
            return { kind: "angle", path: angle[1] };
        }
    }
    return null;
}
