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
exports.typeOfName = typeOfName;
exports.bareTypeName = bareTypeName;
exports.membersOf = membersOf;
exports.membersOfReceiver = membersOfReceiver;
const vscode = __importStar(require("vscode"));
const nexaIndex_1 = require("./nexaIndex");
const ID = "[A-Za-z_][A-Za-z0-9_]*";
const TYPE = `(?:enum\\s+)?((?:\\[\\]\\s*)*\\*?\\s*${ID}(?:\\[[^\\]\\n]*\\])?)`;
function escapeRe(s) {
    return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
/**
 * The type the code before `before` gives `name`: from `let name: T`, `let name = T{`,
 * a parameter `name: T`, or `for (name in xs)` over a `[]T`. `self` is the struct or
 * shape around it. The nearest declaration wins. Null when nothing says.
 */
function typeOfName(masked, name, before, depth = 0) {
    if (name === "self") {
        const re = /\b(?:struct|shape)\s+([A-Za-z_][A-Za-z0-9_]*)[^{;]*\{/g;
        let best = null;
        let m;
        while ((m = re.exec(masked)) !== null && m.index < before) {
            const close = (0, nexaIndex_1.matchBrace)(masked, m.index + m[0].length - 1);
            if (close < 0 || close > before) {
                best = m[1];
            }
        }
        return best;
    }
    const n = escapeRe(name);
    const patterns = [
        new RegExp(`\\b(?:let|const)\\s+(?:const\\s+)?${n}\\s*:\\s*${TYPE}`, "g"),
        new RegExp(`\\b(?:let|const)\\s+(?:const\\s+)?${n}\\s*=\\s*(?:new\\s+)?(${ID})\\s*(?:\\[[^\\]\\n]*\\]\\s*)?\\{`, "g"),
        new RegExp(`[(,]\\s*${n}\\s*:\\s*${TYPE}`, "g"),
    ];
    let bestAt = -1;
    let type = null;
    for (const re of patterns) {
        let m;
        while ((m = re.exec(masked)) !== null && m.index < before) {
            if (m.index > bestAt) {
                bestAt = m.index;
                type = m[1].replace(/\s+/g, "");
            }
        }
    }
    const loop = new RegExp(`\\bfor\\s*\\(\\s*${n}\\s+in\\s+(${ID})\\s*\\)`, "g");
    let m;
    while ((m = loop.exec(masked)) !== null && m.index < before) {
        if (m.index > bestAt && depth < 3) {
            const of = typeOfName(masked, m[1], m.index, depth + 1);
            if (of && of.startsWith("[]")) {
                bestAt = m.index;
                type = of.slice(2);
            }
        }
    }
    return type;
}
/** `*Point` and `Box[int]` name the struct Point and Box. */
function bareTypeName(type) {
    return type.replace(/^\*+/, "").replace(/\[.*$/, "");
}
/** The fields and methods of the struct or shape `typeName`, here or in an include. */
async function membersOf(document, typeName, token) {
    const bare = bareTypeName(typeName);
    if (!bare || typeName.startsWith("[]")) {
        return null;
    }
    const isContainer = (d) => d.name === bare && (d.kind === "struct" || d.kind === "shape");
    let doc = document;
    let def = (0, nexaIndex_1.indexNexaDefinitions)(document).find(isContainer);
    if (!def) {
        const hit = (await (0, nexaIndex_1.collectIncludedDefinitions)(document, token)).find((h) => isContainer(h.def));
        if (!hit) {
            return null;
        }
        doc = hit.document;
        def = hit.def;
    }
    const masked = (0, nexaIndex_1.maskCode)(doc.getText());
    const body = (0, nexaIndex_1.scanHead)(masked, doc.offsetAt(def.nameRange.end));
    if (body.open < 0 || body.close < 0) {
        return [];
    }
    const out = [];
    for (const f of (0, nexaIndex_1.structFields)(masked, body.open, body.close)) {
        const start = doc.positionAt(f.offset);
        out.push({
            name: f.name,
            kind: "field",
            detail: f.detail,
            document: doc,
            range: new vscode.Range(start, start.translate(0, f.name.length)),
        });
    }
    for (const d of (0, nexaIndex_1.indexNexaDefinitions)(doc)) {
        const at = doc.offsetAt(d.nameRange.start);
        if (d.kind === "function" && at > body.open && at < body.close) {
            out.push({
                name: d.name,
                kind: "method",
                detail: (0, nexaIndex_1.definitionHead)(doc, d, masked),
                document: doc,
                range: d.nameRange,
                def: d,
            });
        }
    }
    return out;
}
/** The members of whatever `recv` is at `before` in `document`, or null. */
async function membersOfReceiver(document, recv, before, token) {
    const type = typeOfName((0, nexaIndex_1.maskCode)(document.getText()), recv, before);
    if (!type) {
        return null;
    }
    return { type, members: await membersOf(document, type, token) };
}
