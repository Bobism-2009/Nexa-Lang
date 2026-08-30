import * as vscode from "vscode";

export const NEXA_KEYWORDS = [
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

export const NEXA_TYPES = [
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
];

export const STD_INCLUDES = [
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
export const MODULE_MEMBERS: Record<string, { name: string; detail: string }[]> = {
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
    { name: "lock", detail: "os.lock() — lock the session" },
    { name: "set_volume", detail: "os.set_volume(percent)" },
    { name: "get_volume", detail: "os.get_volume()" },
    { name: "set_brightness", detail: "os.set_brightness(percent)" },
    { name: "get_brightness", detail: "os.get_brightness()" },
    { name: "type", detail: "os.type(text) — type into focused window" },
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
    { name: "call", detail: "dll.call(handle, name, args...)" },
  ],
};

export const STRING_METHODS = [
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

export const HOVER_DOCS: Record<string, string> = {
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

export function keywordCompletions(): vscode.CompletionItem[] {
  return NEXA_KEYWORDS.map((k) => {
    const c = new vscode.CompletionItem(k, vscode.CompletionItemKind.Keyword);
    c.sortText = "0" + k;
    return c;
  });
}

export function typeCompletions(): vscode.CompletionItem[] {
  return NEXA_TYPES.map((t) => {
    const c = new vscode.CompletionItem(t, vscode.CompletionItemKind.TypeParameter);
    c.sortText = "1" + t;
    return c;
  });
}

export function moduleMemberCompletions(
  moduleName: string
): vscode.CompletionItem[] {
  const members = MODULE_MEMBERS[moduleName];
  if (!members) {
    return [];
  }
  return members.map((m) => {
    const c = new vscode.CompletionItem(
      m.name,
      vscode.CompletionItemKind.Method
    );
    c.detail = m.detail;
    c.documentation = HOVER_DOCS[`${moduleName}.${m.name}`] ?? m.detail;
    c.insertText = m.name;
    return c;
  });
}

export function stringMethodCompletions(): vscode.CompletionItem[] {
  return STRING_METHODS.map((m) => {
    const c = new vscode.CompletionItem(m, vscode.CompletionItemKind.Method);
    c.detail = `s.${m}(...)`;
    return c;
  });
}

/** Text before cursor on current line, trimmed. */
export function linePrefix(document: vscode.TextDocument, position: vscode.Position): string {
  return document.lineAt(position.line).text.slice(0, position.character);
}
