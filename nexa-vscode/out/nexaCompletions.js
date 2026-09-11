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
    "self",
];
exports.NEXA_TYPES = [
    "int",
    "unsigned int",
    "unsigned char",
    "short",
    "unsigned short",
    "long",
    "unsigned long",
    "size_t",
    "string",
    "bool",
    "float",
    "char",
    "void",
    "*int",
    "*char",
    "*void",
    "*size_t",
    "[]int",
    "[]string",
    "map",
    "fn",
    "Json",
    "Result",
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
    "std/json",
    "std/time",
    "std/thread",
    "std/gfx",
    "std/inline",
];
/** module prefix -> member completions */
exports.MODULE_MEMBERS = {
    io: [
        { name: "print", detail: "io.print(arg[, ...]) — print without newline" },
        { name: "println", detail: "io.println(arg[, ...]) — print args, then newline" },
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
        { name: "spawn_wait", detail: "os.spawn_wait(prog [, arg...]) — spawn and wait for exit code" },
        { name: "spawn_at", detail: "os.spawn_at(cwd, prog [, arg...]) — spawn with working directory" },
        { name: "wait", detail: "os.wait(pid) — wait for child, return exit code" },
        { name: "kill", detail: "os.kill(pid) — terminate process" },
        { name: "platform", detail: "os.platform() — windows|linux|darwin" },
        { name: "arch", detail: "os.arch() — x86_64|x86|arm64|arm" },
        { name: "cpu_count", detail: "os.cpu_count() — logical CPUs" },
        { name: "getenv", detail: "os.getenv(name)" },
        { name: "setenv", detail: "os.setenv(name, value)" },
        { name: "unsetenv", detail: "os.unsetenv(name)" },
        { name: "hostname", detail: "os.hostname()" },
        { name: "username", detail: "os.username() / os.user()" },
        { name: "home", detail: "os.home()" },
        { name: "tempdir", detail: "os.tempdir() — system temp directory" },
        { name: "cwd", detail: "os.cwd() — current working directory" },
        { name: "chdir", detail: "os.chdir(path) — change working directory" },
        { name: "executable", detail: "os.executable() — full path of this exe" },
        { name: "which", detail: "os.which(name) — find executable on PATH" },
        { name: "total_mem", detail: "os.total_mem() — physical RAM in bytes" },
        { name: "avail_mem", detail: "os.avail_mem() — available RAM in bytes" },
        { name: "page_size", detail: "os.page_size() — memory page size" },
        { name: "uptime", detail: "os.uptime() — seconds since boot" },
        { name: "shell", detail: "os.shell() — default shell path" },
        { name: "newline", detail: "os.newline() — native line ending" },
        { name: "path_sep", detail: "os.path_sep() — directory separator" },
        { name: "lang", detail: "os.lang() — user locale / LANG" },
        { name: "isatty", detail: "os.isatty() — 1 if stdout is a terminal" },
        { name: "environ", detail: "os.environ() — KEY=value environment list" },
        { name: "env", detail: "os.env() — alias for os.environ()" },
        { name: "config_dir", detail: "os.config_dir() — per-user config directory" },
        { name: "cache_dir", detail: "os.cache_dir() — per-user cache directory" },
        { name: "desktop", detail: "os.desktop() — desktop folder" },
        { name: "endian", detail: "os.endian() — little or big" },
        { name: "exit", detail: "os.exit(code)" },
        { name: "getpid", detail: "os.getpid() / os.getprocessid()" },
        { name: "exe_dir", detail: "os.exe_dir()" },
        { name: "clip_get", detail: "os.clip_get()" },
        { name: "clip_set", detail: "os.clip_set(text)" },
        { name: "notify", detail: "os.notify(title, message)" },
        { name: "open", detail: "os.open(target)" },
        { name: "load", detail: "os.load(path) — read entire file as binary string" },
        { name: "save", detail: "os.save(path, data) — write a string as binary, returns 1/0" },
        { name: "play", detail: "os.play(path) — play an audio file (blocks until done)" },
        { name: "lock", detail: "os.lock() — lock the session" },
        { name: "set_volume", detail: "os.set_volume(percent)" },
        { name: "get_volume", detail: "os.get_volume()" },
        { name: "set_brightness", detail: "os.set_brightness(percent)" },
        { name: "get_brightness", detail: "os.get_brightness()" },
        { name: "type", detail: "os.type(text) — type into focused window" },
    ],
    file: [
        { name: "read", detail: "file.read(path)" },
        { name: "write", detail: "file.write(path, content) — binary write" },
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
        { name: "get", detail: "http.get(url) — Result[string]; .ok() / .value() / .error()" },
        { name: "post", detail: "http.post(url, body) — Result[string]; .ok() / .value() / .error()" },
    ],
    json: [
        { name: "parse", detail: "json.parse(s) — nested JSON; .ok() is false on error" },
        { name: "stringify", detail: "json.stringify(v[, indent]) — compact or pretty" },
        { name: "of", detail: "json.of(x) — int/float/bool/string/[]T/map[string]T to Json" },
        { name: "null", detail: "json.null()" },
        { name: "bool", detail: "json.bool(b)" },
        { name: "int", detail: "json.int(n)" },
        { name: "float", detail: "json.float(x)" },
        { name: "string", detail: "json.string(s)" },
        { name: "array", detail: "json.array()" },
        { name: "object", detail: "json.object()" },
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
        { name: "call", detail: "dll.call(handle, name, args...)" },
    ],
    gfx: [
        { name: "open", detail: "gfx.open(title, w, h[, scale]) — pixel window" },
        { name: "resize", detail: "gfx.resize(w, h[, scale]) — change framebuffer size" },
        { name: "width", detail: "gfx.width() — framebuffer width" },
        { name: "height", detail: "gfx.height() — framebuffer height" },
        { name: "scale", detail: "gfx.scale() — integer window scale" },
        { name: "title", detail: "gfx.title() / gfx.title(s) — get or set window title" },
        { name: "close", detail: "gfx.close()" },
        { name: "poll", detail: "gfx.poll() — process window events" },
        { name: "closed", detail: "gfx.closed() — 1 if the window was closed" },
        { name: "clear", detail: "gfx.clear(r, g, b)" },
        { name: "plot", detail: "gfx.plot(x, y, r, g, b)" },
        { name: "get", detail: "gfx.get(x, y) — 0xRRGGBB or -1" },
        { name: "fill", detail: "gfx.fill(x, y, w, h, r, g, b)" },
        { name: "rect", detail: "gfx.rect(x, y, w, h, r, g, b) — rectangle outline" },
        { name: "line", detail: "gfx.line(x1, y1, x2, y2, r, g, b[, t]) — optional thickness" },
        { name: "circle", detail: "gfx.circle(cx, cy, rad, r, g, b) — circle outline" },
        { name: "fill_circle", detail: "gfx.fill_circle(cx, cy, rad, r, g, b)" },
        { name: "ellipse", detail: "gfx.ellipse(cx, cy, rx, ry, r, g, b) — ellipse outline" },
        { name: "fill_ellipse", detail: "gfx.fill_ellipse(cx, cy, rx, ry, r, g, b)" },
        { name: "tri", detail: "gfx.tri(x1, y1, x2, y2, x3, y3, r, g, b) — triangle outline" },
        { name: "fill_tri", detail: "gfx.fill_tri(x1, y1, x2, y2, x3, y3, r, g, b)" },
        { name: "poly", detail: "gfx.poly(xs, ys, r, g, b) — closed []int polygon outline; 1/0" },
        { name: "fill_poly", detail: "gfx.fill_poly(xs, ys, r, g, b) — even-odd filled polygon; 1/0" },
        { name: "text", detail: "gfx.text(x, y, s, r, g, b[, scale]) — 5x7 ASCII" },
        { name: "text_size", detail: "gfx.text_size() / gfx.text_size(n) — default text scale" },
        { name: "text_width", detail: "gfx.text_width(s[, scale]) — measure width, no draw" },
        { name: "text_height", detail: "gfx.text_height(s[, scale]) — measure height, no draw" },
        { name: "present", detail: "gfx.present() — blit framebuffer (letterboxed, double-buffered on Windows)" },
        { name: "fullscreen", detail: "gfx.fullscreen() / gfx.fullscreen(on) — get or set fullscreen" },
        { name: "audio", detail: "gfx.audio([rate]) — open 16-bit mono PCM (default 44100)" },
        { name: "sample", detail: "gfx.sample(s) — queue one s16 sample" },
        { name: "audio_queued", detail: "gfx.audio_queued() — samples still buffered" },
        { name: "audio_flush", detail: "gfx.audio_flush() — submit a partial PCM buffer" },
        { name: "image", detail: "gfx.image(path) — load PNG/JPEG/BMP/GIF, returns handle or 0" },
        { name: "decode", detail: "gfx.decode(bytes) — decode image bytes, returns handle or 0" },
        { name: "image_w", detail: "gfx.image_w(id) — image width" },
        { name: "image_h", detail: "gfx.image_h(id) — image height" },
        { name: "blit", detail: "gfx.blit(x, y, src[, w, h] | src, sx, sy, sw, sh[, dw, dh]) — image or sprite" },
        { name: "alpha", detail: "gfx.alpha() / gfx.alpha(a) — global draw alpha 0..255 (255 = opaque)" },
        { name: "save", detail: "gfx.save(path) — write the framebuffer to a 24-bit BMP; 1/0" },
        { name: "key", detail: "gfx.key(name) — 1 if that key is down (window focused; includes f11)" },
        { name: "pressed", detail: "gfx.pressed(name) — 1 if that key went down this poll (window focused)" },
        { name: "mouse_x", detail: "gfx.mouse_x() — cursor X in framebuffer pixels" },
        { name: "mouse_y", detail: "gfx.mouse_y() — cursor Y in framebuffer pixels" },
        { name: "mouse", detail: "gfx.mouse(button) — 1 if left/right/middle is down" },
        { name: "drop", detail: "gfx.drop() — path of a file dropped on the window" },
        { name: "opendialog", detail: "gfx.opendialog([filter]) — file explorer picker" },
        { name: "openfile", detail: "gfx.openfile([filter]) — alias for gfx.opendialog" },
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
    "push",
    "pop",
    "clear",
    "has",
    "remove",
    "insert",
    "keys",
    "values",
    "ok",
    "value",
    "error",
];
exports.HOVER_DOCS = {
    "http.get": "GET a URL. Returns Result[string]: .ok() / .value() for the body, .error() on failure.",
    "http.post": "POST a URL. Returns Result[string]: .ok() / .value() for the body, .error() on failure.",
    "json.parse": "Parse JSON text into a Json value. On failure .ok() is false and .as_string() is the error.",
    "json.stringify": "Serialize Json (or a native value via json.of) to text. Optional indent pretty-prints.",
    "json.of": "Convert int, float, bool, string, Json, []T, or map[string]T to Json.",
    "io.println": "Print to stdout with a trailing newline.",
    "io.readln": "Read one line from stdin; returns string.",
    "os.spawn": "Start a program directly (no shell). Returns process id or 0.",
    "os.spawn_wait": "Start a program and wait. Returns the exit code, or -1 on failure.",
    "os.spawn_at": "Start a program with a working directory. Returns process id or 0.",
    "os.wait": "Wait for a spawned process. Returns the exit code, or -1 on failure.",
    "os.kill": "Terminate a process by id. Returns 1 on success, 0 on failure.",
    "os.tempdir": "System temporary directory.",
    "os.arch": "CPU architecture (x86_64, x86, arm64, arm).",
    "os.cpu_count": "Number of logical CPUs.",
    "os.which": "Find an executable on PATH. Returns the full path or empty.",
    "os.total_mem": "Physical RAM in bytes (size_t).",
    "os.avail_mem": "Available physical RAM in bytes (size_t).",
    "os.page_size": "OS memory page size in bytes.",
    "os.uptime": "Seconds since boot.",
    "os.shell": "Default shell path (COMSPEC / SHELL).",
    "os.newline": "Native line ending.",
    "os.path_sep": "Directory separator.",
    "os.lang": "User locale or LANG.",
    "os.isatty": "1 if stdout is a terminal, else 0.",
    "os.environ": "Environment as KEY=value strings.",
    "os.env": "Alias for os.environ().",
    "os.config_dir": "Per-user config directory.",
    "os.cache_dir": "Per-user cache directory.",
    "os.desktop": "Desktop folder path.",
    "os.endian": "Native endianness: little or big.",
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
