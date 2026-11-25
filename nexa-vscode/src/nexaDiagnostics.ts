import * as cp from "child_process";
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import * as vscode from "vscode";
import { getNexacPath } from "./nexaRun";

export interface NexaProblem {
  severity: "error" | "warning";
  /** The file NexaC names, or null when it names none. */
  file: string | null;
  /** 1-based, or null when NexaC gives no line. */
  line: number | null;
  message: string;
}

/**
 * NexaC's problems from its output:
 *   [Nexa] Error: C:\p\main.nxa: Expected ';' at line 5
 *   [Nexa] Warning: C:\p\main.nxa:2: #warning careful
 * Lines that follow a problem without a [Nexa] prefix continue its message.
 */
export function parseNexaOutput(out: string): NexaProblem[] {
  const problems: NexaProblem[] = [];
  let last: NexaProblem | null = null;
  for (const raw of out.split(/\r?\n/)) {
    const lineText = raw.trimEnd();
    const m = /^\[Nexa\] (Error|Warning): (.*)$/.exec(lineText.trim());
    if (!m) {
      if (/^\[Nexa\]/.test(lineText.trim())) {
        last = null;
      } else if (last && lineText.trim() !== "") {
        last.message += "\n" + lineText.trim();
      }
      continue;
    }
    let rest = m[2]!;
    let file: string | null = null;
    let line: number | null = null;
    const at = /^(.*?\.nxa)(?::(\d+))?: (.*)$/i.exec(rest);
    if (at) {
      file = at[1]!;
      if (at[2]) {
        line = Number(at[2]);
      }
      rest = at[3]!;
    }
    const all = [...rest.matchAll(/ at line (\d+)(?:, column \d+)?/g)];
    const lineAt = all[all.length - 1];
    if (lineAt) {
      if (line === null) {
        line = Number(lineAt[1]);
      }
      rest = rest.slice(0, lineAt.index!) + rest.slice(lineAt.index! + lineAt[0].length);
    }
    last = { severity: m[1] === "Error" ? "error" : "warning", file, line, message: rest.trim() };
    problems.push(last);
  }
  return problems;
}

/** Runs NexaC with --source into a temp file: the Nexa checks, no C++ compiler. */
export function runNexacSource(
  filePath: string,
  outCpp: string,
  onDone: (result: { problems: NexaProblem[]; ok: boolean; missing: boolean; killed: boolean }) => void
): cp.ChildProcess {
  return cp.execFile(
    getNexacPath(),
    [filePath, "--source", outCpp],
    { cwd: path.dirname(filePath), timeout: 20000, windowsHide: true, maxBuffer: 4 * 1024 * 1024 },
    (err, stdout, stderr) => {
      const code = (err as NodeJS.ErrnoException | null)?.code;
      const missing = code === "ENOENT";
      const killed = !!(err && (err as cp.ExecException).killed);
      onDone({ problems: parseNexaOutput(`${stdout}\n${stderr}`), ok: !err, missing, killed });
    }
  );
}

function tempCpp(filePath: string): string {
  const base = path.basename(filePath, path.extname(filePath)).replace(/[^A-Za-z0-9_-]/g, "_");
  return path.join(os.tmpdir(), "nexa-vscode", `${base}-${process.pid}.cpp`);
}

function rangeFor(doc: vscode.TextDocument | undefined, line: number | null): vscode.Range {
  const l = Math.max(0, (line ?? 1) - 1);
  if (!doc || l >= doc.lineCount) {
    return new vscode.Range(l, 0, l, 1000);
  }
  const t = doc.lineAt(l);
  return new vscode.Range(l, t.firstNonWhitespaceCharacterIndex, l, t.text.length);
}

export function registerDiagnostics(context: vscode.ExtensionContext): {
  check: (doc: vscode.TextDocument) => void;
} {
  const collection = vscode.languages.createDiagnosticCollection("nexa");
  context.subscriptions.push(collection);
  const running = new Map<string, cp.ChildProcess>();
  /** The files each checked file put problems on, to clear them on the next check. */
  const owned = new Map<string, Set<string>>();
  let warnedMissing = false;

  const clear = (key: string) => {
    for (const uri of owned.get(key) ?? []) {
      collection.delete(vscode.Uri.parse(uri));
    }
    owned.delete(key);
  };

  const check = (doc: vscode.TextDocument) => {
    if (doc.languageId !== "nexa" || doc.uri.scheme !== "file") {
      return;
    }
    const key = doc.uri.fsPath;
    running.get(key)?.kill();
    const out = tempCpp(key);
    fs.mkdirSync(path.dirname(out), { recursive: true });
    const child = runNexacSource(key, out, ({ problems, missing, killed }) => {
      if (running.get(key) !== child) {
        return; // a newer check replaced this one
      }
      running.delete(key);
      fs.unlink(out, () => undefined);
      if (missing) {
        if (!warnedMissing) {
          warnedMissing = true;
          void vscode.window.showWarningMessage(
            `NexaC was not found ("${getNexacPath()}"), so Nexa files are not checked. Set nexa.nexacPath to it.`
          );
        }
        return;
      }
      if (killed) {
        return;
      }
      clear(key);
      const byFile = new Map<string, vscode.Diagnostic[]>();
      for (const p of problems) {
        const file = p.file && fs.existsSync(p.file) ? path.resolve(p.file) : key;
        const uri = vscode.Uri.file(file);
        const open = vscode.workspace.textDocuments.find((d) => d.uri.fsPath === uri.fsPath);
        const d = new vscode.Diagnostic(
          rangeFor(open, p.line),
          p.message,
          p.severity === "error" ? vscode.DiagnosticSeverity.Error : vscode.DiagnosticSeverity.Warning
        );
        d.source = "NexaC";
        const k = uri.toString();
        byFile.set(k, [...(byFile.get(k) ?? []), d]);
      }
      const mine = new Set<string>();
      for (const [uri, diags] of byFile) {
        collection.set(vscode.Uri.parse(uri), diags);
        mine.add(uri);
      }
      owned.set(key, mine);
    });
    running.set(key, child);
  };

  const enabled = () => vscode.workspace.getConfiguration("nexa").get<boolean>("checkOnSave", true);
  // Checked once when it first shows in an editor, then on each save. Not on open:
  // references and rename open every .nxa in the workspace without showing them.
  const shown = new Set<string>();
  const checkShown = () => {
    if (!enabled()) return;
    for (const editor of vscode.window.visibleTextEditors) {
      const doc = editor.document;
      if (doc.languageId === "nexa" && doc.uri.scheme === "file" && !shown.has(doc.uri.fsPath)) {
        shown.add(doc.uri.fsPath);
        check(doc);
      }
    }
  };

  context.subscriptions.push(
    vscode.workspace.onDidSaveTextDocument((doc) => {
      if (enabled()) check(doc);
    }),
    vscode.window.onDidChangeVisibleTextEditors(checkShown),
    vscode.workspace.onDidCloseTextDocument((doc) => {
      running.get(doc.uri.fsPath)?.kill();
      running.delete(doc.uri.fsPath);
      shown.delete(doc.uri.fsPath);
      clear(doc.uri.fsPath);
    }),
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("nexa.checkOnSave") && !enabled()) {
        collection.clear();
        owned.clear();
      }
    })
  );
  checkShown();
  return { check };
}

/** Nexa: Show Generated C++ — the file's C++ beside it, or its problems. */
export async function showGeneratedCpp(doc: vscode.TextDocument, check: (doc: vscode.TextDocument) => void): Promise<void> {
  if (doc.isDirty) {
    await doc.save();
  }
  const out = path.join(os.tmpdir(), "nexa-vscode", `${path.basename(doc.uri.fsPath, ".nxa")}.cpp`);
  fs.mkdirSync(path.dirname(out), { recursive: true });
  await new Promise<void>((resolve) => {
    runNexacSource(doc.uri.fsPath, out, async ({ ok, missing, problems }) => {
      if (missing) {
        void vscode.window.showErrorMessage(`NexaC was not found ("${getNexacPath()}"). Set nexa.nexacPath to it.`);
      } else if (!ok || !fs.existsSync(out)) {
        check(doc);
        const first = problems.find((p) => p.severity === "error");
        void vscode.window.showErrorMessage(
          first ? `NexaC: ${first.message.split("\n")[0]}` : "NexaC could not make C++ from this file."
        );
      } else {
        const cpp = await vscode.workspace.openTextDocument(vscode.Uri.file(out));
        await vscode.window.showTextDocument(cpp, { viewColumn: vscode.ViewColumn.Beside, preview: true, preserveFocus: true });
      }
      resolve();
    });
  });
}
