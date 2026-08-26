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
exports.getNexacPath = getNexacPath;
exports.runNexaFile = runNexaFile;
const vscode = __importStar(require("vscode"));
const path = __importStar(require("path"));
function getNexacPath() {
    const cfg = vscode.workspace.getConfiguration("nexa");
    return cfg.get("nexacPath", "NexaC");
}
async function runNexaFile(document, runAfterBuild) {
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
    const quoted = (s) => (s.includes(" ") ? `"${s}"` : s);
    term.sendText(`${quoted(nexac)} ${args.map(quoted).join(" ")}`);
}
