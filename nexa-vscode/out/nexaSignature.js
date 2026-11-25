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
exports.callAt = callAt;
exports.parameterSpans = parameterSpans;
exports.provideSignatureHelp = provideSignatureHelp;
const vscode = __importStar(require("vscode"));
const nexaCompletions_1 = require("./nexaCompletions");
const nexaIndex_1 = require("./nexaIndex");
const nexaTypes_1 = require("./nexaTypes");
const NOT_CALLS = new Set([
    "if", "while", "for", "switch", "match", "catch", "fn", "return", "sizeof",
    "and", "or", "not", "in", "step",
]);
/** Built-ins that are not module members. */
const BUILTINS = {
    len: "len(x) — the length of a string, slice or map",
    trim: "trim(s) — s without the spaces at its ends",
    bits: "bits(x) — the raw bits of a float",
    from_bits32: "from_bits32(n) — the float32 with these bits",
    from_bits64: "from_bits64(n) — the float64 with these bits",
    Some: "Some(value) — an Option that holds value",
};
/** The call whose argument list holds `offset`, read from code with strings and comments blanked. */
function callAt(masked, offset) {
    let depth = 0;
    let args = 0;
    for (let i = offset - 1; i >= 0 && offset - i < 6000; i--) {
        const c = masked[i];
        if (c === ")" || c === "]" || c === "}") {
            depth++;
        }
        else if (c === "(" || c === "[" || c === "{") {
            if (depth > 0) {
                depth--;
                continue;
            }
            if (c !== "(") {
                return null;
            }
            const before = masked.slice(Math.max(0, i - 200), i);
            const m = /(?:([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]\n]*\]\s*)?$/.exec(before);
            if (!m || NOT_CALLS.has(m[2])) {
                return null;
            }
            return {
                paren: i,
                argIndex: args,
                qualifier: m[1] ?? null,
                name: m[2],
                calleeStart: i - before.length + m.index,
            };
        }
        else if (c === "," && depth === 0) {
            args++;
        }
        else if (c === ";" && depth === 0) {
            return null;
        }
    }
    return null;
}
/** Each parameter's [start, end) in `label`, from the first `(` to its match. */
function parameterSpans(label) {
    const open = label.indexOf("(");
    if (open < 0) {
        return [];
    }
    const spans = [];
    let depth = 0;
    let start = open + 1;
    for (let i = open + 1; i < label.length; i++) {
        const c = label[i];
        if (c === "(" || c === "[" || c === "{") {
            depth++;
        }
        else if ((c === ")" || c === "]" || c === "}") && depth > 0) {
            depth--;
        }
        else if ((c === "," || c === ")") && depth === 0) {
            let a = start;
            let b = i;
            while (a < b && label[a] === " ")
                a++;
            while (b > a && label[b - 1] === " ")
                b--;
            if (b > a) {
                spans.push([a, b]);
            }
            if (c === ")") {
                break;
            }
            start = i + 1;
        }
    }
    return spans;
}
function signatureFrom(label, docs, argIndex) {
    const sig = new vscode.SignatureInformation(label, docs ? new vscode.MarkdownString(docs) : undefined);
    const spans = parameterSpans(label);
    sig.parameters = spans.map((s) => new vscode.ParameterInformation(s));
    const help = new vscode.SignatureHelp();
    help.signatures = [sig];
    help.activeSignature = 0;
    const variadic = spans.length > 0 && /\.\.\./.test(label.slice(spans[spans.length - 1][0], spans[spans.length - 1][1]));
    help.activeParameter = spans.length === 0 ? 0 : variadic ? Math.min(argIndex, spans.length - 1) : argIndex;
    return help;
}
function fromDef(doc, d, argIndex) {
    let label = (0, nexaIndex_1.definitionHead)(doc, d).replace(/^(?:priv\s+)?(?:extern\s+)?fn\s+/, "");
    if (d.kind === "macro") {
        label = label.replace(/^#\s*define\s+/, "").replace(/\)\s.*$/, ")");
    }
    return signatureFrom(label, (0, nexaIndex_1.docComment)(doc, d.defLine) || undefined, argIndex);
}
async function provideSignatureHelp(document, position, token) {
    const masked = (0, nexaIndex_1.maskCode)(document.getText());
    const call = callAt(masked, document.offsetAt(position));
    if (!call) {
        return null;
    }
    const { qualifier, name, argIndex } = call;
    if (qualifier && nexaCompletions_1.MODULE_MEMBERS[qualifier]) {
        const member = nexaCompletions_1.MODULE_MEMBERS[qualifier].find((m) => m.name === name);
        if (!member) {
            return null;
        }
        const [label, ...rest] = member.detail.split(" — ");
        const docs = nexaCompletions_1.HOVER_DOCS[`${qualifier}.${name}`] ?? (rest.length ? rest.join(" — ") : undefined);
        return signatureFrom(label.trim(), docs, argIndex);
    }
    if (qualifier) {
        const recv = await (0, nexaTypes_1.membersOfReceiver)(document, qualifier, call.calleeStart, token);
        const method = recv?.members?.find((m) => m.kind === "method" && m.name === name);
        if (method?.def) {
            return fromDef(method.document, method.def, argIndex);
        }
        return null;
    }
    const isCallable = (d) => d.name === name && (d.kind === "function" || d.kind === "macro");
    const local = (0, nexaIndex_1.indexNexaDefinitions)(document).find(isCallable);
    if (local) {
        return fromDef(document, local, argIndex);
    }
    const included = (await (0, nexaIndex_1.collectIncludedDefinitions)(document, token)).find((h) => isCallable(h.def));
    if (included) {
        return fromDef(included.document, included.def, argIndex);
    }
    if (BUILTINS[name]) {
        const [label, docs] = BUILTINS[name].split(" — ");
        return signatureFrom(label, docs, argIndex);
    }
    return null;
}
