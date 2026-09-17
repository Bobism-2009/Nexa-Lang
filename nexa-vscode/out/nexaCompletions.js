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
    "HttpResponse",
    "HttpServer",
    "HttpRequest",
];
exports.STD_INCLUDES = [
    "std/io",
    "std/os",
    "std/file",
    "std/dll",
    "std/random",
    "std/math",
    "std/crypto",
    "std/network",
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
        { name: "get", detail: "http.get(url[, headers]) — Result[string]; .ok() / .value() / .error()" },
        { name: "post", detail: "http.post(url, body[, headers]) — Result[string]" },
        { name: "put", detail: "http.put(url, body[, headers]) — Result[string]" },
        { name: "patch", detail: "http.patch(url, body[, headers]) — Result[string]" },
        { name: "delete", detail: "http.delete(url[, headers]) — Result[string]" },
        { name: "request", detail: "http.request(method, url, body[, headers]) — Result[HttpResponse]" },
        { name: "localhost", detail: "http.localhost([port]) — Result[HttpServer]; binds 127.0.0.1, port picked by the OS when omitted" },
        { name: "accept", detail: "http.accept(server) — Result[HttpRequest]; waits for one request and reads all of it" },
        { name: "reply", detail: "http.reply(request, status, body[, headers]) — answer it and close; 1 on success" },
        { name: "raw", detail: "http.raw(request, text) — answer with exactly these bytes and close; 1 on success" },
        { name: "close", detail: "http.close(server) — stop listening; 1 on success" },
    ],
    tcp: [
        { name: "connect", detail: "tcp.connect(host, port) — handle, 0 on failure" },
        { name: "listen", detail: "tcp.listen(port) — listening handle on every interface, 0 on failure" },
        { name: "accept", detail: "tcp.accept(listener) — handle for one connection; blocks" },
        { name: "send", detail: "tcp.send(handle, data) — writes all of data; returns bytes written" },
        { name: "recv", detail: "tcp.recv(handle[, max]) — one read, up to max (64K); \"\" when the peer hung up" },
        { name: "port", detail: "tcp.port(handle) — the port this end is on; 0 on failure" },
        { name: "close", detail: "tcp.close(handle) — close it; returns 1" },
    ],
    udp: [
        { name: "open", detail: "udp.open(port) — bind a port (0 = one the OS picks); handle, 0 on failure" },
        { name: "port", detail: "udp.port(handle) — the port this socket is on; 0 on failure" },
        { name: "send", detail: "udp.send(handle, host, port, data) — one datagram; returns the bytes sent" },
        { name: "recv", detail: "udp.recv(handle[, max]) — one whole datagram, up to max (64K); \"\" on error" },
        { name: "sender", detail: "udp.sender(handle) — address the last packet read came from" },
        { name: "sender_port", detail: "udp.sender_port(handle) — the port it came from" },
        { name: "close", detail: "udp.close(handle) — close it; returns 1" },
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
        { name: "round_rect", detail: "gfx.round_rect(x, y, w, h, rad, r, g, b) — rounded outline" },
        { name: "fill_round_rect", detail: "gfx.fill_round_rect(x, y, w, h, rad, r, g, b)" },
        { name: "line", detail: "gfx.line(x1, y1, x2, y2, r, g, b[, t]) — optional thickness" },
        { name: "circle", detail: "gfx.circle(cx, cy, rad, r, g, b) — circle outline" },
        { name: "fill_circle", detail: "gfx.fill_circle(cx, cy, rad, r, g, b)" },
        { name: "ellipse", detail: "gfx.ellipse(cx, cy, rx, ry, r, g, b) — ellipse outline" },
        { name: "fill_ellipse", detail: "gfx.fill_ellipse(cx, cy, rx, ry, r, g, b)" },
        { name: "arc", detail: "gfx.arc(cx, cy, rad, a0, a1, r, g, b) — arc outline; degrees, 0 = up, clockwise" },
        { name: "pie", detail: "gfx.pie(cx, cy, rad, a0, a1, r, g, b) — filled wedge; same angles" },
        { name: "tri", detail: "gfx.tri(x1, y1, x2, y2, x3, y3, r, g, b) — triangle outline" },
        { name: "fill_tri", detail: "gfx.fill_tri(x1, y1, x2, y2, x3, y3, r, g, b)" },
        { name: "poly", detail: "gfx.poly(xs, ys, r, g, b) — closed []int polygon outline; 1/0" },
        { name: "fill_poly", detail: "gfx.fill_poly(xs, ys, r, g, b) — even-odd filled polygon; 1/0" },
        { name: "text", detail: "gfx.text(x, y, s, r, g, b[, scale]) — 5x7 ASCII" },
        { name: "text_size", detail: "gfx.text_size() / gfx.text_size(n) — default text scale" },
        { name: "text_width", detail: "gfx.text_width(s[, scale]) — measure width, no draw" },
        { name: "text_height", detail: "gfx.text_height(s[, scale]) — measure height, no draw" },
        { name: "present", detail: "gfx.present() — blit framebuffer (letterboxed, double-buffered on Windows)" },
        { name: "maxfps", detail: "gfx.maxfps(n) — cap the loop at n frames a second; 0 removes the cap" },
        { name: "fullscreen", detail: "gfx.fullscreen() / gfx.fullscreen(on) — get or set fullscreen" },
        { name: "audio", detail: "gfx.audio([rate]) — open 16-bit mono PCM (default 44100)" },
        { name: "sample", detail: "gfx.sample(s) — queue one s16 sample" },
        { name: "audio_queued", detail: "gfx.audio_queued() — samples still buffered" },
        { name: "audio_flush", detail: "gfx.audio_flush() — submit a partial PCM buffer" },
        { name: "sound", detail: "gfx.sound(path) — load a WAV, returns handle or 0" },
        { name: "play", detail: "gfx.play(id[, volume]) — play once, returns a voice or 0" },
        { name: "loop", detail: "gfx.loop(id[, volume]) — play repeating, returns a voice or 0" },
        { name: "stop", detail: "gfx.stop() / gfx.stop(voice) — stop every voice, or one" },
        { name: "volume", detail: "gfx.volume() / gfx.volume(v) — get or set master volume 0..255" },
        { name: "image", detail: "gfx.image(path) — load PNG/JPEG/BMP/GIF, returns handle or 0" },
        { name: "decode", detail: "gfx.decode(bytes) — decode image bytes, returns handle or 0" },
        { name: "image_w", detail: "gfx.image_w(id) — image width" },
        { name: "image_h", detail: "gfx.image_h(id) — image height" },
        { name: "blit", detail: "gfx.blit(x, y, src[, w, h] | src, sx, sy, sw, sh[, dw, dh]) — image or sprite" },
        { name: "blit_rot", detail: "gfx.blit_rot(x, y, src, angle[, w, h]) — draw turned clockwise; 1/0" },
        { name: "icon", detail: "gfx.icon(src) — set the window icon from a handle or path; 1/0" },
        { name: "cursor", detail: "gfx.cursor() / gfx.cursor(on) — show or hide the OS cursor" },
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
    "http.get": "GET a URL. Returns Result[string]: .ok() / .value() for the body. A non-2xx status is a failure with .error() = HTTP <status>. Takes an optional trailing []string of raw request header lines.",
    "http.post": "POST a URL. Returns Result[string]: .ok() / .value() for the body. A non-2xx status is a failure with .error() = HTTP <status>. Takes an optional trailing []string of raw request header lines.",
    "http.put": "PUT a URL. Returns Result[string]: .ok() / .value() for the body. A non-2xx status is a failure with .error() = HTTP <status>. Takes an optional trailing []string of raw request header lines.",
    "http.patch": "PATCH a URL. Returns Result[string]: .ok() / .value() for the body. A non-2xx status is a failure with .error() = HTTP <status>. Takes an optional trailing []string of raw request header lines.",
    "http.delete": "DELETE a URL. Returns Result[string]: .ok() / .value() for the body. A non-2xx status is a failure with .error() = HTTP <status>. Takes an optional trailing []string of raw request header lines.",
    "http.request": "Any method, with the full response. Returns Result[HttpResponse] — .status, .body, .headers. Only a transport failure is an error; any status the server answered with is a success.",
    "http.localhost": "Listen on 127.0.0.1. Returns Result[HttpServer] — .port is the port the OS picked (pass one to choose it yourself), .socket is the listening socket. Raw OS sockets: Winsock on Windows, POSIX everywhere else. Not available on wasm.",
    "http.accept": "Wait for one request on a server and read all of it. Returns Result[HttpRequest] — .method, .path, .body, .headers, .socket. Blocks until a client connects.",
    "http.reply": "Answer a request with a status and a body, then close the connection. Writes the status line, your header lines, Content-Length and Connection: close; a Content-Length or Connection you pass is dropped. Returns 1 on success, 0 on failure.",
    "http.raw": "Answer a request with exactly these bytes — status line, headers and body as written, nothing added — then close. This is how a program frames a response itself. Returns 1 on success, 0 on failure.",
    "http.close": "Stop listening: closes the server's listening socket. Returns 1.",
    "tcp.connect": "Dial a TCP host. Returns a handle (the socket as the OS numbers it), or 0 on failure. Resolves IPv4 and IPv6.",
    "tcp.listen": "Listen on every interface (0.0.0.0) at that port; 0 asks the OS for a free one. Returns a listening handle, or 0 on failure. Unlike http.localhost, this is reachable from the network.",
    "tcp.accept": "Wait for one connection on a listening handle and return its handle, or 0 on failure. Blocks.",
    "tcp.send": "Write all of data to a handle. Returns the bytes written — the whole string on success, less when the connection broke.",
    "tcp.recv": "One read, up to max bytes (64K when omitted). TCP is a stream, so this is whatever had arrived, not a whole message. Binary-safe. Returns \"\" when the peer hung up.",
    "tcp.port": "The port this end of the socket is on — how a program that asked for port 0 finds out which one the OS picked. 0 on failure.",
    "tcp.close": "Close a handle. Returns 1.",
    "udp.open": "Bind a UDP port and get a handle back, or 0 on failure. Port 0 asks the OS for a free one — udp.port then says which. Binds every interface (0.0.0.0). There is no listen and no accept: the one socket both sends and reads.",
    "udp.port": "The port this socket is on — how a program that opened port 0 finds out where its replies will arrive. 0 on failure.",
    "udp.send": "Send one datagram to host:port. All-or-nothing (a datagram has no short write), so the count is the whole string or 0. The host is resolved per call, which is what lets a server answer a different peer each time round its loop.",
    "udp.recv": "Read one whole datagram, up to max bytes (64K when omitted). UDP is not a stream — what was sent as one packet arrives as one packet, so there is no reading until a message is complete. Binary-safe. \"\" is a failed read or an empty datagram.",
    "udp.sender": "The address the last packet THIS THREAD read on that handle came from. \"\" before the first read.",
    "udp.sender_port": "The port the last packet this thread read on that handle came from. 0 before the first read.",
    "udp.close": "Close a handle. Returns 1.",
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
