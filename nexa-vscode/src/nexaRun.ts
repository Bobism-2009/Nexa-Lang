import * as vscode from "vscode";
import * as path from "path";
export function getNexacPath(): string {
  const cfg = vscode.workspace.getConfiguration("nexa");
  return cfg.get<string>("nexacPath", "NexaC");
}

export async function runNexaFile(
  document: vscode.TextDocument,
  runAfterBuild: boolean
): Promise<void> {
  if (document.languageId !== "nexa") {
    return;
  }
  const filePath = document.uri.fsPath;
  const nexac = getNexacPath();
  const args = runAfterBuild ? [filePath, "--run"] : [filePath];
  const term = vscode.window.createTerminal({
    name: runAfterBuild ? "Nexa Run" : "Nexa Build",
    cwd: path.dirname(filePath),
  });
  term.show();
  const quoted = (s: string) => (s.includes(" ") ? `"${s}"` : s);
  term.sendText(`${quoted(nexac)} ${args.map(quoted).join(" ")}`);
}
