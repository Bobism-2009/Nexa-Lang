import * as cp from "child_process";
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import * as vscode from "vscode";
import { maskComments } from "./nexaIndex";
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
    // An error in an included file names the include chain, outermost first:
    //   main.nxa: sub\Lib.nxa: Expected ';' at line 3
    // The innermost file is the one it is in.
    let at: RegExpExecArray | null;
    while ((at = /^(.*?\.nxa)(?::(\d+))?: (.*)$/i.exec(rest)) !== null) {
      file = at[1]!;
      line = at[2] ? Number(at[2]) : null;
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

const QUOTE_INCLUDE_RE = /^[ \t]*#[ \t]*include[ \t]*"([^"\n]+)"/gm;
/** Each file's quoted includes, resolved, kept while its mtime holds. */
const includeCache = new Map<string, { mtime: number; includes: string[] }>();

function samePath(p: string): string {
  const n = path.resolve(p);
  return process.platform === "win32" ? n.toLowerCase() : n;
}

function includesOf(file: string): string[] {
  let mtime: number;
  try {
    mtime = fs.statSync(file).mtimeMs;
  } catch {
    return [];
  }
  const hit = includeCache.get(file);
  if (hit && hit.mtime === mtime) {
    return hit.includes;
  }
  let text = "";
  try {
    text = fs.readFileSync(file, "utf8");
  } catch {
    return [];
  }
  const includes: string[] = [];
  const masked = maskComments(text);
  QUOTE_INCLUDE_RE.lastIndex = 0;
  let m: RegExpExecArray | null;
  while ((m = QUOTE_INCLUDE_RE.exec(masked)) !== null) {
    const rel = m[1]!;
    for (const p of /\.[A-Za-z0-9]+$/.test(rel) ? [rel] : [rel, `${rel}.nxa`]) {
      const abs = path.isAbsolute(p) ? p : path.join(path.dirname(file), p);
      if (/\.nxa$/i.test(abs) && fs.existsSync(abs)) {
        includes.push(samePath(abs));
        break;
      }
    }
  }
  includeCache.set(file, { mtime, includes });
  return includes;
}

/**
 * The workspace files to check for `file`: the top files that include it (directly or
 * through other files), since an included file leans on what they include before it.
 * Just `file` when nothing includes it.
 */
export async function checkRootsFor(file: string): Promise<string[]> {
  const me = samePath(file);
  let uris: vscode.Uri[] = [];
  try {
    uris = await vscode.workspace.findFiles("**/*.nxa", "{**/node_modules/**,**/.git/**}", 3000);
  } catch {
    return [file];
  }
  const includedBy = new Map<string, string[]>();
  const real = new Map<string, string>();
  for (const uri of uris) {
    const from = uri.fsPath;
    real.set(samePath(from), from);
    for (const inc of includesOf(from)) {
      includedBy.set(inc, [...(includedBy.get(inc) ?? []), samePath(from)]);
    }
  }
  if (!includedBy.has(me)) {
    return [file];
  }
  const roots: string[] = [];
  const seen = new Set<string>([me]);
  const queue = [me];
  while (queue.length > 0) {
    const cur = queue.shift()!;
    const parents = includedBy.get(cur) ?? [];
    if (parents.length === 0 && cur !== me) {
      roots.push(real.get(cur) ?? cur);
    }
    for (const p of parents) {
      if (!seen.has(p)) {
        seen.add(p);
        queue.push(p);
      }
    }
  }
  // Files that only include each other in a ring have no top: check the direct includers.
  return roots.length > 0 ? roots : (includedBy.get(me) ?? []).map((p) => real.get(p) ?? p);
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
  /** Per checked file, the problems it found, by the file they are in. */
  const results = new Map<string, Map<string, vscode.Diagnostic[]>>();
  let warnedMissing = false;

  /** A file's problems: those every check found in it, once each. */
  const publish = (uri: string) => {
    const all: vscode.Diagnostic[] = [];
    const seen = new Set<string>();
    for (const byFile of results.values()) {
      for (const d of byFile.get(uri) ?? []) {
        const k = `${d.range.start.line}\0${d.severity}\0${d.message}`;
        if (!seen.has(k)) {
          seen.add(k);
          all.push(d);
        }
      }
    }
    if (all.length > 0) {
      collection.set(vscode.Uri.parse(uri), all);
    } else {
      collection.delete(vscode.Uri.parse(uri));
    }
  };

  const setResults = (key: string, byFile: Map<string, vscode.Diagnostic[]> | null) => {
    const touched = new Set<string>([...(results.get(key)?.keys() ?? []), ...(byFile?.keys() ?? [])]);
    if (byFile) {
      results.set(key, byFile);
    } else {
      results.delete(key);
    }
    for (const uri of touched) {
      publish(uri);
    }
  };
  const clear = (key: string) => setResults(key, null);

  const check = (doc: vscode.TextDocument) => {
    if (doc.languageId !== "nexa" || doc.uri.scheme !== "file") {
      return;
    }
    const file = doc.uri.fsPath;
    void checkRootsFor(file).then((roots) => {
      if (!roots.some((r) => samePath(r) === samePath(file))) {
        // Checked through the files that include it; its own lone check would be wrong.
        running.get(file)?.kill();
        running.delete(file);
        clear(file);
      }
      for (const root of roots) {
        checkFile(root);
      }
    });
  };

  const checkFile = (key: string) => {
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
      setResults(key, byFile);
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
        results.clear();
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
  // A file other files include is made into C++ as part of the first of them.
  const roots = await checkRootsFor(doc.uri.fsPath);
  const source = roots.find((r) => samePath(r) === samePath(doc.uri.fsPath)) ?? roots[0] ?? doc.uri.fsPath;
  const out = path.join(os.tmpdir(), "nexa-vscode", `${path.basename(source, ".nxa")}.cpp`);
  fs.mkdirSync(path.dirname(out), { recursive: true });
  await new Promise<void>((resolve) => {
    runNexacSource(source, out, async ({ ok, missing, problems }) => {
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
