import * as vscode from "vscode";
import { maskComments, maskStrings } from "./nexaIndex";
import { NEXA_PREDEFINED_MACROS } from "./nexaCompletions";

const MODULES =
  "io|os|dll|file|random|math|crypto|http|tcp|udp|json|time|thread|term|gfx3d|gfx|ui|physics";
const MODULE_CALL = new RegExp(
  `\\b(${MODULES})\\.([A-Za-z_][A-Za-z0-9_]*)`,
  "g"
);
const INCLUDE_ANGLE = /^\s*(#include)\s*<([^>]+)>/;
const INCLUDE_QUOTE = /^\s*(#include)\s*"([^"]+)"/;
const STD_MODULES =
  "io|os|dll|file|random|math|crypto|network|json|time|thread|term|gfx3d|gfx|ui|physics|inline";

export const SEMANTIC_LEGEND = new vscode.SemanticTokensLegend(
  ["module", "member", "importKeyword", "importModule", "importPrefix", "nexaVariable",
    "nexaFunctionDef", "nexaFunctionCall", "nexaMacro"],
  []
);
const VARIABLE = 5;
const FUNCTION_DEF = 6;
const FUNCTION_CALL = 7;
const MACRO = 8;

// Every name #define gives a meaning to in this file, plus the ones NexaC predefines.
function macroNames(text: string): Set<string> {
  const names = new Set<string>(
    NEXA_PREDEFINED_MACROS.map((m) => m.name).filter((n) => n !== "__has_include")
  );
  const def = /^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)/gm;
  let m: RegExpExecArray | null;
  while ((m = def.exec(text)) !== null) names.add(m[1]!);
  return names;
}

export const NOT_NAMES = new Set(
  ("fn extern new delete sizeof let const struct enum type shape priv if else while for " +
    "switch match case default return break continue defer goto try catch throw in step " +
    "and or not self null true false int short long size_t string bool float float64 " +
    "float32 char void unsigned map Result result").split(" ")
);

function addParams(list: string, names: Set<string>): void {
  for (const part of list.split(",")) {
    const m = /^\s*(?:\.\.\.\s*)?([A-Za-z_][A-Za-z0-9_]*)\s*(?::|$)/.exec(part);
    if (m && !NOT_NAMES.has(m[1]!)) names.add(m[1]!);
  }
}

// The names this file declares as variables: let/const, function and lambda
// parameters, for-loop and catch variables. Scopes are not tracked: a name
// declared anywhere is a variable everywhere it is used, which is what the eye wants.
function declaredNames(text: string): Set<string> {
  const names = new Set<string>();
  const add = (n: string | undefined) => { if (n && !NOT_NAMES.has(n)) names.add(n); };
  let m: RegExpExecArray | null;
  const decl = /\b(?:let|const)\s+([A-Za-z_][A-Za-z0-9_]*)/g;
  while ((m = decl.exec(text)) !== null) add(m[1]);
  const fn = /\bfn\b(?:\s+[A-Za-z_][A-Za-z0-9_]*)?\s*(?:\[[^\]]*\]\s*)?\(([^)]*)\)/g;
  while ((m = fn.exec(text)) !== null) addParams(m[1]!, names);
  const loop = /\bfor\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)(?:\s*,\s*([A-Za-z_][A-Za-z0-9_]*))?\s+in\b/g;
  while ((m = loop.exec(text)) !== null) { add(m[1]); add(m[2]); }
  const caught = /\bcatch\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)/g;
  while ((m = caught.exec(text)) !== null) add(m[1]);
  return names;
}

const WORD = /[A-Za-z_][A-Za-z0-9_]*/g;

function pushIncludeAngle(
  builder: vscode.SemanticTokensBuilder,
  line: number,
  match: RegExpExecArray
): void {
  const base = match.index ?? 0;
  const includeAt = match[0].indexOf("#include");
  builder.push(line, base + includeAt, 8, 2, 0);

  const path = match[2] ?? "";
  const lt = match[0].indexOf("<");
  const pathStart = base + lt + 1;
  const std = new RegExp(`^(std/)(${STD_MODULES})$`).exec(path);
  if (std) {
    builder.push(line, pathStart, std[1]!.length, 4, 0);
    builder.push(line, pathStart + std[1]!.length, std[2]!.length, 3, 0);
    return;
  }
  builder.push(line, pathStart, path.length, 3, 0);
}

function pushIncludeQuote(
  builder: vscode.SemanticTokensBuilder,
  line: number,
  match: RegExpExecArray
): void {
  const base = match.index ?? 0;
  const includeAt = match[0].indexOf("#include");
  builder.push(line, base + includeAt, 8, 2, 0);

  const path = match[2] ?? "";
  const q = match[0].indexOf('"');
  builder.push(line, base + q + 1, path.length, 3, 0);
}

export function buildSemanticTokens(
  document: vscode.TextDocument
): vscode.SemanticTokens {
  const builder = new vscode.SemanticTokensBuilder(SEMANTIC_LEGEND);
  const masked = maskStrings(maskComments(document.getText()));
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

    const tokens: [number, number, number][] = [];
    MODULE_CALL.lastIndex = 0;
    let call: RegExpExecArray | null;
    while ((call = MODULE_CALL.exec(lineText)) !== null) {
      tokens.push([call.index, call[1]!.length, 0]);
      tokens.push([call.index + call[1]!.length + 1, call[2]!.length, 1]);
    }

    WORD.lastIndex = 0;
    let w: RegExpExecArray | null;
    while ((w = WORD.exec(lineText)) !== null) {
      const at = w.index;
      const name = w[0];
      if (NOT_NAMES.has(name) || /[0-9]/.test(lineText[at - 1] ?? "")) continue;
      if (tokens.some(([s, l]) => at >= s && at < s + l)) continue;
      if (macros.has(name) && !/\.\s*$/.test(lineText.slice(0, at).replace(/\.\.\s*$/, ""))) {
        tokens.push([at, name.length, MACRO]);
        continue;
      }
      if (directive) continue;
      const before = lineText.slice(0, at);
      if (/\bfn\s+$/.test(before)) {
        tokens.push([at, name.length, FUNCTION_DEF]);
        continue;
      }
      if (/^\s*\(/.test(lineText.slice(at + name.length))) {
        tokens.push([at, name.length, FUNCTION_CALL]);
        continue;
      }
      if (!vars.has(name)) continue;
      if (/[0-9]/.test(lineText[at - 1] ?? "")) continue;
      if (/\.\s*$/.test(lineText.slice(0, at)) && !/\.\.\s*$/.test(lineText.slice(0, at))) continue;
      if (/^\s*\(/.test(lineText.slice(at + name.length))) continue;
      if (tokens.some(([s, l]) => at >= s && at < s + l)) continue;
      tokens.push([at, name.length, VARIABLE]);
    }

    tokens.sort((x, y) => x[0] - y[0]);
    for (const [start, length, type] of tokens) builder.push(line, start, length, type, 0);
  }

  return builder.build();
}

export function registerSemanticTokens(
  context: vscode.ExtensionContext
): void {
  const selector: vscode.DocumentSelector = [
    { language: "nexa", scheme: "file" },
  ];
  context.subscriptions.push(
    vscode.languages.registerDocumentSemanticTokensProvider(
      selector,
      {
        provideDocumentSemanticTokens(document) {
          return buildSemanticTokens(document);
        },
      },
      SEMANTIC_LEGEND
    )
  );
}
