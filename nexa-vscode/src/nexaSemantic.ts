import * as vscode from "vscode";
import { maskComments } from "./nexaIndex";

const MODULES =
  "io|os|dll|file|random|math|crypto|http|time|thread";
const MODULE_CALL = new RegExp(
  `\\b(${MODULES})\\.([A-Za-z_][A-Za-z0-9_]*)`,
  "g"
);
const INCLUDE_ANGLE = /^\s*(#include)\s*<([^>]+)>/;
const INCLUDE_QUOTE = /^\s*(#include)\s*"([^"]+)"/;
const STD_MODULES =
  "io|os|dll|file|random|math|crypto|http|time|thread|inline";

export const SEMANTIC_LEGEND = new vscode.SemanticTokensLegend(
  ["module", "member", "importKeyword", "importModule", "importPrefix"],
  []
);

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
  const masked = maskComments(document.getText());
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
    let call: RegExpExecArray | null;
    while ((call = MODULE_CALL.exec(lineText)) !== null) {
      builder.push(line, call.index, call[1]!.length, 0, 0);
      builder.push(
        line,
        call.index + call[1]!.length + 1,
        call[2]!.length,
        1,
        0
      );
    }
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
