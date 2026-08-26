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
exports.HOVER_DOCS = exports.STRING_METHODS = exports.MODULE_MEMBERS = exports.STD_INCLUDES = exports.NEXA_TYPES = exports.NEXA_KEYWORDS = void 0;
exports.keywordCompletions = keywordCompletions;
exports.typeCompletions = typeCompletions;
exports.moduleMemberCompletions = moduleMemberCompletions;
exports.stringMethodCompletions = stringMethodCompletions;
exports.linePrefix = linePrefix;
const vscode = __importStar(require("vscode"));
exports.NEXA_KEYWORDS = [
    "fn",
    "extern",
    "let",
    "const",
    "struct",
    "enum",
    "if",
    "else",
    "while",
    "for",
    "switch",
    "case",
    "default",
    "return",
    "break",
    "continue",
    "goto",
    "try",
    "catch",
    "throw",
    "new",
    "delete",
    "sizeof",
    "true",
    "false",
    "null",
];
exports.NEXA_TYPES = [
    "int",
    "unsigned int",
    "unsigned char",
    "string",
    "bool",
    "float",
    "char",
    "void",
    "*int",
    "*char",
    "*void",
];
exports.STD_INCLUDES = [
    "std/io",
    "std/os",
    "std/file",
    "std/dll",
    "std/random",
    "std/math",
    "std/crypto",
    "std/http",
    "std/time",
    "std/thread",
    "std/inline",
];
/** module prefix -> member completions */
exports.MODULE_MEMBERS = {
    io: [
        { name: "print", detail: "io.print(arg) — print without newline" },
        { name: "println", detail: "io.println(arg) — print with newline" },
        { name: "flush", detail: "io.flush() — flush stdout" },
        { name: "readln", detail: "io.readln() — read line from stdin" },
        { name: "read_int", detail: "io.read_int() — read int from stdin" },
        { name: "to_int", detail: "io.to_int(s) — parse string as int" },
        { name: "getline", detail: "io.getline(text[, lineNo])" },
        { name: "trim", detail: "io.trim(s[, prefix])" },
    ],
    os: [
        { name: "system", detail: "os.system(cmd)" },
        { name: "spawn", detail: "os.spawn(prog [, arg...]) — start process, no shell" },
        { name: "platform", detail: "os.platform() — windows|linux|darwin" },
        { name: "getenv", detail: "os.getenv(name)" },
        { name: "setenv", detail: "os.setenv(name, value)" },
        { name: "hostname", detail: "os.hostname()" },
        { name: "username", detail: "os.username() / os.user()" },
        { name: "home", detail: "os.home()" },
        { name: "exit", detail: "os.exit(code)" },
        { name: "getpid", detail: "os.getpid() / os.getprocessid()" },
        { name: "exe_dir", detail: "os.exe_dir()" },
        { name: "clip_get", detail: "os.clip_get()" },
        { name: "clip_set", detail: "os.clip_set(text)" },
        { name: "notify", detail: "os.notify(title, message)" },
        { name: "open", detail: "os.open(target)" },
    ],
    file: [
        { name: "read", detail: "file.read(path)" },
        { name: "write", detail: "file.write(path, content)" },
        { name: "append", detail: "file.append(path, content)" },
        { name: "exists", detail: "file.exists(path)" },
        { name: "mkdir", detail: "file.mkdir(path)" },
        { name: "list", detail: "file.list(path) — []string" },
        { name: "cwd", detail: "file.cwd()" },
        { name: "join", detail: "file.join(a, b)" },
        { name: "abspath", detail: "file.abspath(path)" },
    ],
    random: [
        { name: "int", detail: "random.int(min, max)" },
        { name: "seed", detail: "random.seed(n)" },
    ],
    math: [
        { name: "abs", detail: "math.abs(x)" },
        { name: "min", detail: "math.min(a, b)" },
        { name: "max", detail: "math.max(a, b)" },
        { name: "pow", detail: "math.pow(base, exp)" },
        { name: "sqrt", detail: "math.sqrt(x)" },
        { name: "floor", detail: "math.floor(x)" },
        { name: "ceil", detail: "math.ceil(x)" },
        { name: "round", detail: "math.round(x)" },
        { name: "sin", detail: "math.sin(x)" },
        { name: "cos", detail: "math.cos(x)" },
        { name: "pi", detail: "math.pi — constant" },
        { name: "e", detail: "math.e — constant" },
    ],
    crypto: [
        { name: "sha256", detail: "crypto.sha256(data)" },
        { name: "hmac_sha256", detail: "crypto.hmac_sha256(key, data)" },
        { name: "xor", detail: "crypto.xor(data, key...)" },
        { name: "hex_encode", detail: "crypto.hex_encode(data)" },
        { name: "base64_encode", detail: "crypto.base64_encode(data)" },
    ],
    http: [
        { name: "get", detail: "http.get(url)" },
        { name: "post", detail: "http.post(url, body)" },
    ],
    time: [
        { name: "sleep", detail: "time.sleep(ms)" },
        { name: "seconds", detail: "time.seconds(n)" },
        { name: "milliseconds", detail: "time.milliseconds(n)" },
        { name: "now_ms", detail: "time.now_ms() — monotonic float" },
    ],
    thread: [
        { name: "spawn", detail: "thread.spawn(fn_or_call)" },
        { name: "join", detail: "thread.join(handle)" },
        { name: "worker", detail: "thread.worker()" },
        { name: "run", detail: "thread.run(worker, job)" },
        { name: "worker_join", detail: "thread.worker_join(worker)" },
    ],
    dll: [
        { name: "load", detail: "dll.load(path)" },
        { name: "call", detail: "dll.call(handle, name)" },
    ],
};
exports.STRING_METHODS = [
    "upper",
    "lower",
    "trim",
    "len",
    "contains",
    "starts_with",
    "ends_with",
    "index_of",
    "replace",
    "substring",
    "repeat",
    "split",
];
exports.HOVER_DOCS = {
    "io.println": "Print to stdout with a trailing newline.",
    "io.readln": "Read one line from stdin; returns string.",
    "os.spawn": "Start a program directly (no shell). Returns process id or 0.",
    "os.system": "Run a shell command; blocks until complete.",
    "extern fn": "Declare a C library function (extern \"C\"). No body.",
    null: "Null pointer literal (emits nullptr).",
};
function keywordCompletions() {
    return exports.NEXA_KEYWORDS.map((k) => {
        const c = new vscode.CompletionItem(k, vscode.CompletionItemKind.Keyword);
        c.sortText = "0" + k;
        return c;
    });
}
function typeCompletions() {
    return exports.NEXA_TYPES.map((t) => {
        const c = new vscode.CompletionItem(t, vscode.CompletionItemKind.TypeParameter);
        c.sortText = "1" + t;
        return c;
    });
}
function moduleMemberCompletions(moduleName) {
    const members = exports.MODULE_MEMBERS[moduleName];
    if (!members) {
        return [];
    }
    return members.map((m) => {
        const c = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
        c.detail = m.detail;
        c.documentation = exports.HOVER_DOCS[`${moduleName}.${m.name}`] ?? m.detail;
        c.insertText = m.name;
        return c;
    });
}
function stringMethodCompletions() {
    return exports.STRING_METHODS.map((m) => {
        const c = new vscode.CompletionItem(m, vscode.CompletionItemKind.Method);
        c.detail = `s.${m}(...)`;
        return c;
    });
}
/** Text before cursor on current line, trimmed. */
function linePrefix(document, position) {
    return document.lineAt(position.line).text.slice(0, position.character);
}
