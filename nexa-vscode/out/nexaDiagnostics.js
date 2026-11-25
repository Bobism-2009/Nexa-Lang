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
exports.parseNexaOutput = parseNexaOutput;
exports.runNexacSource = runNexacSource;
exports.registerDiagnostics = registerDiagnostics;
exports.showGeneratedCpp = showGeneratedCpp;
const cp = __importStar(require("child_process"));
const fs = __importStar(require("fs"));
const os = __importStar(require("os"));
const path = __importStar(require("path"));
const vscode = __importStar(require("vscode"));
const nexaRun_1 = require("./nexaRun");
/**
 * NexaC's problems from its output:
 *   [Nexa] Error: C:\p\main.nxa: Expected ';' at line 5
 *   [Nexa] Warning: C:\p\main.nxa:2: #warning careful
 * Lines that follow a problem without a [Nexa] prefix continue its message.
 */
function parseNexaOutput(out) {
    const problems = [];
    let last = null;
    for (const raw of out.split(/\r?\n/)) {
        const lineText = raw.trimEnd();
        const m = /^\[Nexa\] (Error|Warning): (.*)$/.exec(lineText.trim());
        if (!m) {
            if (/^\[Nexa\]/.test(lineText.trim())) {
                last = null;
            }
            else if (last && lineText.trim() !== "") {
                last.message += "\n" + lineText.trim();
            }
            continue;
        }
        let rest = m[2];
        let file = null;
        let line = null;
        const at = /^(.*?\.nxa)(?::(\d+))?: (.*)$/i.exec(rest);
        if (at) {
            file = at[1];
            if (at[2]) {
                line = Number(at[2]);
            }
            rest = at[3];
        }
        const all = [...rest.matchAll(/ at line (\d+)(?:, column \d+)?/g)];
        const lineAt = all[all.length - 1];
        if (lineAt) {
            if (line === null) {
                line = Number(lineAt[1]);
            }
            rest = rest.slice(0, lineAt.index) + rest.slice(lineAt.index + lineAt[0].length);
        }
        last = { severity: m[1] === "Error" ? "error" : "warning", file, line, message: rest.trim() };
        problems.push(last);
    }
    return problems;
}
/** Runs NexaC with --source into a temp file: the Nexa checks, no C++ compiler. */
function runNexacSource(filePath, outCpp, onDone) {
    return cp.execFile((0, nexaRun_1.getNexacPath)(), [filePath, "--source", outCpp], { cwd: path.dirname(filePath), timeout: 20000, windowsHide: true, maxBuffer: 4 * 1024 * 1024 }, (err, stdout, stderr) => {
        const code = err?.code;
        const missing = code === "ENOENT";
        const killed = !!(err && err.killed);
        onDone({ problems: parseNexaOutput(`${stdout}\n${stderr}`), ok: !err, missing, killed });
    });
}
function tempCpp(filePath) {
    const base = path.basename(filePath, path.extname(filePath)).replace(/[^A-Za-z0-9_-]/g, "_");
    return path.join(os.tmpdir(), "nexa-vscode", `${base}-${process.pid}.cpp`);
}
function rangeFor(doc, line) {
    const l = Math.max(0, (line ?? 1) - 1);
    if (!doc || l >= doc.lineCount) {
        return new vscode.Range(l, 0, l, 1000);
    }
    const t = doc.lineAt(l);
    return new vscode.Range(l, t.firstNonWhitespaceCharacterIndex, l, t.text.length);
}
function registerDiagnostics(context) {
    const collection = vscode.languages.createDiagnosticCollection("nexa");
    context.subscriptions.push(collection);
    const running = new Map();
    /** The files each checked file put problems on, to clear them on the next check. */
    const owned = new Map();
    let warnedMissing = false;
    const clear = (key) => {
        for (const uri of owned.get(key) ?? []) {
            collection.delete(vscode.Uri.parse(uri));
        }
        owned.delete(key);
    };
    const check = (doc) => {
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
                    void vscode.window.showWarningMessage(`NexaC was not found ("${(0, nexaRun_1.getNexacPath)()}"), so Nexa files are not checked. Set nexa.nexacPath to it.`);
                }
                return;
            }
            if (killed) {
                return;
            }
            clear(key);
            const byFile = new Map();
            for (const p of problems) {
                const file = p.file && fs.existsSync(p.file) ? path.resolve(p.file) : key;
                const uri = vscode.Uri.file(file);
                const open = vscode.workspace.textDocuments.find((d) => d.uri.fsPath === uri.fsPath);
                const d = new vscode.Diagnostic(rangeFor(open, p.line), p.message, p.severity === "error" ? vscode.DiagnosticSeverity.Error : vscode.DiagnosticSeverity.Warning);
                d.source = "NexaC";
                const k = uri.toString();
                byFile.set(k, [...(byFile.get(k) ?? []), d]);
            }
            const mine = new Set();
            for (const [uri, diags] of byFile) {
                collection.set(vscode.Uri.parse(uri), diags);
                mine.add(uri);
            }
            owned.set(key, mine);
        });
        running.set(key, child);
    };
    const enabled = () => vscode.workspace.getConfiguration("nexa").get("checkOnSave", true);
    // Checked once when it first shows in an editor, then on each save. Not on open:
    // references and rename open every .nxa in the workspace without showing them.
    const shown = new Set();
    const checkShown = () => {
        if (!enabled())
            return;
        for (const editor of vscode.window.visibleTextEditors) {
            const doc = editor.document;
            if (doc.languageId === "nexa" && doc.uri.scheme === "file" && !shown.has(doc.uri.fsPath)) {
                shown.add(doc.uri.fsPath);
                check(doc);
            }
        }
    };
    context.subscriptions.push(vscode.workspace.onDidSaveTextDocument((doc) => {
        if (enabled())
            check(doc);
    }), vscode.window.onDidChangeVisibleTextEditors(checkShown), vscode.workspace.onDidCloseTextDocument((doc) => {
        running.get(doc.uri.fsPath)?.kill();
        running.delete(doc.uri.fsPath);
        shown.delete(doc.uri.fsPath);
        clear(doc.uri.fsPath);
    }), vscode.workspace.onDidChangeConfiguration((e) => {
        if (e.affectsConfiguration("nexa.checkOnSave") && !enabled()) {
            collection.clear();
            owned.clear();
        }
    }));
    checkShown();
    return { check };
}
/** Nexa: Show Generated C++ — the file's C++ beside it, or its problems. */
async function showGeneratedCpp(doc, check) {
    if (doc.isDirty) {
        await doc.save();
    }
    const out = path.join(os.tmpdir(), "nexa-vscode", `${path.basename(doc.uri.fsPath, ".nxa")}.cpp`);
    fs.mkdirSync(path.dirname(out), { recursive: true });
    await new Promise((resolve) => {
        runNexacSource(doc.uri.fsPath, out, async ({ ok, missing, problems }) => {
            if (missing) {
                void vscode.window.showErrorMessage(`NexaC was not found ("${(0, nexaRun_1.getNexacPath)()}"). Set nexa.nexacPath to it.`);
            }
            else if (!ok || !fs.existsSync(out)) {
                check(doc);
                const first = problems.find((p) => p.severity === "error");
                void vscode.window.showErrorMessage(first ? `NexaC: ${first.message.split("\n")[0]}` : "NexaC could not make C++ from this file.");
            }
            else {
                const cpp = await vscode.workspace.openTextDocument(vscode.Uri.file(out));
                await vscode.window.showTextDocument(cpp, { viewColumn: vscode.ViewColumn.Beside, preview: true, preserveFocus: true });
            }
            resolve();
        });
    });
}
