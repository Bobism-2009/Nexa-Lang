import * as vscode from "vscode";
import { MODULE_MEMBERS, HOVER_DOCS } from "./nexaCompletions";
import {
  collectIncludedDefinitions,
  definitionHead,
  docComment,
  indexNexaDefinitions,
  maskCode,
  type IndexedDef,
} from "./nexaIndex";
import { membersOfReceiver } from "./nexaTypes";

const NOT_CALLS = new Set([
  "if", "while", "for", "switch", "match", "catch", "fn", "return", "sizeof",
  "and", "or", "not", "in", "step",
]);

/** Built-ins that are not module members. */
const BUILTINS: Record<string, string> = {
  len: "len(x) — the length of a string, slice or map",
  trim: "trim(s) — s without the spaces at its ends",
  bits: "bits(x) — the raw bits of a float",
  from_bits32: "from_bits32(n) — the float32 with these bits",
  from_bits64: "from_bits64(n) — the float64 with these bits",
  Some: "Some(value) — an Option that holds value",
};

export interface CallSite {
  /** Offset of the call's `(`. */
  paren: number;
  /** Which argument the cursor is in, from 0. */
  argIndex: number;
  qualifier: string | null;
  name: string;
  /** Offset where the qualifier (or the name) starts. */
  calleeStart: number;
}

/** The call whose argument list holds `offset`, read from code with strings and comments blanked. */
export function callAt(masked: string, offset: number): CallSite | null {
  let depth = 0;
  let args = 0;
  for (let i = offset - 1; i >= 0 && offset - i < 6000; i--) {
    const c = masked[i];
    if (c === ")" || c === "]" || c === "}") {
      depth++;
    } else if (c === "(" || c === "[" || c === "{") {
      if (depth > 0) {
        depth--;
        continue;
      }
      if (c !== "(") {
        return null;
      }
      const before = masked.slice(Math.max(0, i - 200), i);
      const m = /(?:([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]\n]*\]\s*)?$/.exec(before);
      if (!m || NOT_CALLS.has(m[2]!)) {
        return null;
      }
      return {
        paren: i,
        argIndex: args,
        qualifier: m[1] ?? null,
        name: m[2]!,
        calleeStart: i - before.length + m.index,
      };
    } else if (c === "," && depth === 0) {
      args++;
    } else if (c === ";" && depth === 0) {
      return null;
    }
  }
  return null;
}

/** Each parameter's [start, end) in `label`, from the first `(` to its match. */
export function parameterSpans(label: string): [number, number][] {
  const open = label.indexOf("(");
  if (open < 0) {
    return [];
  }
  const spans: [number, number][] = [];
  let depth = 0;
  let start = open + 1;
  for (let i = open + 1; i < label.length; i++) {
    const c = label[i];
    if (c === "(" || c === "[" || c === "{") {
      depth++;
    } else if ((c === ")" || c === "]" || c === "}") && depth > 0) {
      depth--;
    } else if ((c === "," || c === ")") && depth === 0) {
      let a = start;
      let b = i;
      while (a < b && label[a] === " ") a++;
      while (b > a && label[b - 1] === " ") b--;
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

function signatureFrom(label: string, docs: string | undefined, argIndex: number): vscode.SignatureHelp {
  const sig = new vscode.SignatureInformation(label, docs ? new vscode.MarkdownString(docs) : undefined);
  const spans = parameterSpans(label);
  sig.parameters = spans.map((s) => new vscode.ParameterInformation(s));
  const help = new vscode.SignatureHelp();
  help.signatures = [sig];
  help.activeSignature = 0;
  const variadic = spans.length > 0 && /\.\.\./.test(label.slice(spans[spans.length - 1]![0], spans[spans.length - 1]![1]));
  help.activeParameter = spans.length === 0 ? 0 : variadic ? Math.min(argIndex, spans.length - 1) : argIndex;
  return help;
}

function fromDef(doc: vscode.TextDocument, d: IndexedDef, argIndex: number): vscode.SignatureHelp {
  let label = definitionHead(doc, d).replace(/^(?:priv\s+)?(?:extern\s+)?fn\s+/, "");
  if (d.kind === "macro") {
    label = label.replace(/^#\s*define\s+/, "").replace(/\)\s.*$/, ")");
  }
  return signatureFrom(label, docComment(doc, d.defLine) || undefined, argIndex);
}

export async function provideSignatureHelp(
  document: vscode.TextDocument,
  position: vscode.Position,
  token: vscode.CancellationToken
): Promise<vscode.SignatureHelp | null> {
  const masked = maskCode(document.getText());
  const call = callAt(masked, document.offsetAt(position));
  if (!call) {
    return null;
  }
  const { qualifier, name, argIndex } = call;

  if (qualifier && MODULE_MEMBERS[qualifier]) {
    const member = MODULE_MEMBERS[qualifier]!.find((m) => m.name === name);
    if (!member) {
      return null;
    }
    const [label, ...rest] = member.detail.split(" — ");
    const docs = HOVER_DOCS[`${qualifier}.${name}`] ?? (rest.length ? rest.join(" — ") : undefined);
    return signatureFrom(label!.trim(), docs, argIndex);
  }

  if (qualifier) {
    const recv = await membersOfReceiver(document, qualifier, call.calleeStart, token);
    const method = recv?.members?.find((m) => m.kind === "method" && m.name === name);
    if (method?.def) {
      return fromDef(method.document, method.def, argIndex);
    }
    return null;
  }

  const isCallable = (d: IndexedDef) => d.name === name && (d.kind === "function" || d.kind === "macro");
  const local = indexNexaDefinitions(document).find(isCallable);
  if (local) {
    return fromDef(document, local, argIndex);
  }
  const included = (await collectIncludedDefinitions(document, token)).find((h) => isCallable(h.def));
  if (included) {
    return fromDef(included.document, included.def, argIndex);
  }
  if (BUILTINS[name]) {
    const [label, docs] = BUILTINS[name]!.split(" — ");
    return signatureFrom(label!, docs, argIndex);
  }
  return null;
}
