#pragma once

// NexaC --help.
//
// The help NexaC used to print was nine fixed pages, numbered 1..9, each a
// hand-written wall of std::cout. That shape stopped fitting the language some
// releases ago: six of the thirteen standard modules had no page at all
// (std/gfx alone is 65 calls), and the "core" page still described a language
// whose only types were int and string.
//
// What replaced it is a table of named topics. A topic is a SUMMARY, not the
// reference -- it names every call a module has and groups them by what they
// are for, shows one example, and points at SYNTAX/ for the semantics. That
// division is the whole design: SYNTAX/*.txt says what a call MEANS and is
// free to take 450 lines doing it, while this says what EXISTS and has to fit
// on a screen. Tests/help_cases.sh pins the first half of that -- every call
// documented in SYNTAX/Modules.txt must appear on its module's page -- so a
// module that grows a call fails the suite until help learns the name.
//
// Adding a topic is adding a row to kTopics. Nothing else knows how many
// there are.

#include "nexapkg.hpp"

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace nexa {
namespace help {

// Where a topic sits in the index on the landing page.
enum class Group { Language, Module, Tool };

struct Topic {
    const char* name;     // canonical spelling, e.g. "std/gfx"
    const char* aliases;  // space-separated alternates, e.g. "gfx graphics"
    Group group;
    const char* blurb;    // one line, for the index
    const char* body;     // the page itself; null = rendered by a callback
};

// A module include that was retired, and what took its place. Printed by name
// rather than as "unknown topic", the same way the parser answers an
// #include <std/http> today.
struct Retired {
    const char* name;
    const char* replacement;
    const char* why;
};

inline const Retired kRetired[] = {
    { "std/http", "std/network", "the whole wire is one include now" },
    { "std/tcp",  "std/network", "the whole wire is one include now" },
};

inline const Topic kTopics[] = {

// ---------------------------------------------------------------- language --

{ "core", "lang language syntax types", Group::Language,
  "the language at a glance: types, declarations, control flow",
  R"HELP(core - the Nexa language at a glance
  Full reference: SYNTAX/Core.txt and SYNTAX/ControlFlow.txt

ENTRY POINT
  fn main() { ... }                    exactly one, in an executable
  fn main(args: []string): void { }    args[0] is the program path, as in C

TYPES
  int  unsigned int  short  unsigned short  long  unsigned long  size_t
  float  bool  char  unsigned char  string
  *T pointers   []T slices   map[K]V maps   Result[T]   structs   enums
  fn(T): Ret    a function value (closure)

DECLARATIONS
  let x = 42;              let s = "hi";        let ok = true;
  let n: int = -10;        let pi = 3.14;       let c = 'A';
  let const name: string = "Bob";               must have an initializer
  let buf: unsigned char[4080];                 fixed array, not a slice
  Globals are the same syntax at file scope.

STRUCTS AND ENUMS
  struct Point { x: int; y: int;
      fn len2(): int { return self.x * self.x + self.y * self.y; } }
  let p = Point { x: 1, y: 2 };        omitted fields are zeroed
  enum Colour { Red; Green; }          let c: enum Colour = Colour.Red;

CONTROL FLOW
  if (cond) { } else if (cond) { } else { }     braces optional for one stmt
  while (cond) { }                              break;  continue;
  for (i, n) { }          counts 0..n-1
  for (x in xs) { }       slice elements
  for (k, v in m) { }     map keys and values
  switch (e) { case 0: ... break; default: ... }   int, string or enum cases
  cond ? a : b            goto label;   label:

OPERATORS
  + - * / %      & | ^ ~ << >>      ! && ||      == != < <= > >=
  Tightest first: * / % , + - , << >> , & , ^ , | , comparisons, && , ||
  x & 2 == 2 is (x & 2) == 2 -- unlike C.
  (int)x (float)x (string)x ...     sizeof(T)   sizeof expr

POINTERS AND MEMORY
  let p: *int = &x;   *p = 20;   ptr->field;   null
  let a: *int = new int[4];   delete[] a;      new T / delete
  There is no garbage collector. Dangling pointers are undefined, as in C.

MORE
  strings slices maps       s.upper() xs.push(v) m.has(k)  -- SYNTAX/Core.txt
  Result[T]                 ok(v) err("...") r.ok() r.value() r.error()
  #include                  modules, .nxa files, C/C++ headers, packages
  NexaC --help std/io       any standard module
  NexaC --help options      every command-line option)HELP" },

// ------------------------------------------------------------------- tools --

{ "options", "cli flags opts usage", Group::Tool,
  "every command-line option, in full",
  R"HELP(options - the NexaC command line
  Full reference: SYNTAX/CLI.txt

COMMANDS
  NexaC <file.nxa> [-o <out>]        compile one file
  NexaC init [dir]                   scaffold a project
  NexaC build [dir]                  build the entry .nxa in a project
  NexaC upgrade [--check] [--yes] [--user | --all-users]
                                     install a newer GitHub release if one
                                     exists; --check only reports versions
  NexaC nexapkg <cmd>                packages (also installed as `nexapkg`)

OUTPUT
  -o <name>          name the output. A .cpp/.cc/.cxx name means --source.
                     Windows adds .exe, and a library gets its platform
                     extension, when you leave one off.
  --source <f.cpp>   emit the generated C++ and stop; never runs the compiler
  --dll              Windows .dll
  --shared           .so (Linux) / .dylib (macOS)
  --static-lib       static archive: .a (Linux) / .lib (Windows)
                     aliases: --staticlib --lib
  --link <file>      link an archive/object/library into the executable.
                     Repeatable. .a and .o are baked in; .so and .dll link
                     dynamically.
  --no-console       Windows GUI .exe with no console window

TARGETS
  --win              Windows .exe (mingw-w64 when cross-compiling from Linux)
  --wasm             WebAssembly. em++ writes a <out>.js loader with the
                     .wasm baked into it, and a <out>.html that loads it --
                     a canvas page for a std/gfx program, a text console
                     otherwise. Embedding is the default because a browser
                     will not fetch a separate .wasm over file://, so the
                     page opens by double-clicking. WASI-SDK writes a bare
                     .wasm with no loader and no page. If the toolchain is
                     missing, NexaC offers to install Emscripten into
                     ~/emsdk and reuses anything already there.
                     Set NEXA_WASM_CXX to force a compiler.
  --wasm-split       Write .html, .js and .wasm as three files instead of
                     baking the .wasm in. Serve them together over http://;
                     file:// will not load the .wasm. Alias: --split. Needs
                     em++ and --wasm. (Useful when you are serving the page
                     anyway and want the .wasm cached on its own.)

BUILD
  -r, --run          run it after building. Without --debug the binary goes to
                     a temp file and is deleted afterwards.
  -g, --debug        a binary built to be inspected rather than shipped:
                       -g -O0, frame pointers, nothing stripped
                       implies --preserve-names, so `break my_fn` works
                       -fsanitize=address,undefined when the compiler can
                         link it, else UBSan, else UBSan trap, else plain -g,
                         printing which one it used
                       the debugger steps through your .nxa: every statement
                         carries a #line back to the file and line you wrote
                       the generated C++ is kept as <output>.debug.cpp, which
                         line info still names for runtime helpers
                       with --run the binary is kept, not deleted
                       libraries get -g -O0 but no sanitizer
                     Rejected with --small, and with --wasm (debug builds
                     target native gdb/lldb, not the browser DWARF workflow).
  -p, --preserve-names   keep your function names in the generated C++.
                     Library builds do this anyway, so dll.call finds them.
  --small            optimize for size (-Os)

OTHER
  -h, --help [topic]     this help; NexaC --help core, --help std/gfx
  -v, --version          print the version and exit

ENVIRONMENT
  NEXA_CXX           C++ compiler to use instead of clang++ / g++
  NEXA_WASM_CXX      compiler for --wasm builds
  NEXA_CXXFLAGS      extra flags passed through to the C++ compiler)HELP" },

{ "nexapkg", "pkg package packages deps dependencies", Group::Tool,
  "the package manager: add, install, update, remove",
  nullptr },

// ----------------------------------------------------------------- modules --

{ "std/io", "io print console stdin stdout", Group::Module,
  "console: print, println, readln, getline, to_int",
  R"HELP(std/io - console input and output
  #include <std/io>
  Full reference: SYNTAX/Modules.txt (std/io)

  out     io.print(a[, ...])       write, no newline
          io.println(a[, ...])     write, then a newline
          io.flush()               flush stdout
  in      io.readln()              one line from stdin, as a string
          io.read_int()            one line from stdin, as an int
  text    io.to_int(s)             parse a decimal int (failure is 0)
          io.getline(text[, n])    line n of a buffer, 1-based
          io.getline(text, "key")  the line starting "key:", from the key on
          io.trim(s[, prefix])     strip whitespace, then the prefix once
                                   also spelled trim(s) -- it is a builtin

Several arguments print back to back with nothing between them. Inside a `+`
chain, one string operand makes the whole chain text, so int, float, bool and
char convert: io.println("n=" + n). Pure numeric `+` stays arithmetic.

  fn main() {
      io.print("Name: ");
      let name = io.readln();
      io.println("hello, " + name.trim());
  }

len(s) and the core string methods -- s.upper() s.split(",") s.contains(x) --
need no include at all. NexaC --help core.)HELP" },

{ "std/os", "os system process environment machine", Group::Module,
  "the machine: processes, environment, clipboard, audio, power",
  R"HELP(std/os - the machine around the program
  #include <std/os>
  Full reference: SYNTAX/Modules.txt (std/os)

  run       os.system(cmd)             shell; as an expression, its stdout
            os.spawn(prog[, arg...])   run directly, no shell; returns a pid
            os.spawn_wait(...)         the same, waiting; returns the exit code
            os.spawn_at(cwd, ...)      the same, in a working directory
            os.wait(pid)  os.kill(pid)  os.exit(code)
  env       os.getenv(name)  os.setenv(name, v)  os.unsetenv(name)
            os.environ() / os.env()    []string of KEY=value
  identity  os.hostname()  os.username() / os.user()  os.home()  os.lang()
            os.platform()  os.arch()  os.endian()  os.isatty()
            os.executable()  os.exe_dir()  os.getprocessid() / os.getpid()
  machine   os.cpu_count()  os.total_mem()  os.avail_mem()  os.page_size()
            os.uptime()  os.shell()  os.newline()  os.path_sep()
  places    os.cwd()  os.chdir(p)  os.tempdir()  os.which(name)
            os.config_dir()  os.cache_dir()  os.desktop()
  files     os.load(path)              read a whole file, binary-safe
            os.save(path, data)        write one, binary-safe; 1/0
  desktop   os.open(target)  os.notify(title, msg)  os.messagebox(text, title)
            os.clip_get()  os.clip_set(text)  os.type(text)  os.play(path)
  audio     os.set_volume(pct)  os.get_volume()
            os.mute()  os.unmute()  os.toggle_mute()
  screen    os.set_brightness(pct)  os.get_brightness()
  session   os.lock()  os.logout()  os.suspend()  os.shutdown()  os.reboot()
  console   os.hideconsolewindow()  os.showconsolewindow()
            os.minimizeconsolewindow()  os.maximizeconsolewindow()
            os.grepkeys() / os.getkey()  os.keypressed()     (Windows)

The calls that DO something rather than report something are statements only:
lock shutdown reboot suspend logout mute unmute toggle_mute set_volume
set_brightness clip_set type notify open messagebox exit setenv unsetenv and
the console four. A `let x = os.exit(1);` is an error, not a 0.

Every text parameter here also takes a number and converts it on the way in --
os.setenv("PORT", 8080) -- with os.getprocessid(name) the one exception.

  fn main() {
      io.println(os.platform() + " " + os.arch());
      os.notify("build", "done");
  })HELP" },

{ "std/file", "file fs path directory", Group::Module,
  "files and paths: read, write, list, copy, the path helpers",
  R"HELP(std/file - files, directories and paths
  #include <std/file>
  Full reference: SYNTAX/Modules.txt (std/file)

  bytes   file.read(path)            the whole file, as a string
          file.write(path, content)  overwrite; statement, or 1/0
          file.append(path, content) append; statement, or 1/0
  ask     file.exists(path)  file.isdir(path)  file.isfile(path)
          file.size(path)            bytes, or -1
  manage  file.mkdir(path)           creates parents too
          file.remove(path) / file.delete(path)      a file or empty directory
          file.remove_all(path)      recursive
          file.rename(from, to) / file.move(from, to)
          file.copy(from, to)        file or directory tree
          file.list(path) / file.listdir(path)       names, as []string
  paths   file.join(a, b)  file.abspath(p)  file.extension(p)
          file.dirname(p) / file.parent(p)
          file.basename(p) / file.name(p)
          file.cwd()  file.chdir(path)

A string holding a path also takes .Write(c) and .Append(c) directly:
SaveData.Write("Test").

I/O is C stdio in binary mode -- no CR/LF translation, and bytes from \xHH,
os.load and os.save survive a write/read round trip. write and append answer 1
on success and 0 on failure, including a buffered write that only fails at
close time; ignoring the answer stays legal.

  fn main() {
      file.mkdir("out");
      file.write(file.join("out", "log.txt"), "hello\n");
      io.println(file.read("out/log.txt"));
  }

Only read/write/append are emitted unless you call a path or listing API, so a
write-only program stays hello-world sized.)HELP" },

{ "std/math", "math arithmetic trig", Group::Module,
  "abs, min, max, pow, sqrt, rounding, trig, logs, pi and e",
  R"HELP(std/math - floating-point maths
  #include <std/math>
  Full reference: SYNTAX/Modules.txt (std/math)

  compare   math.abs(x)  math.min(a, b)  math.max(a, b)
  powers    math.pow(base, exp)  math.sqrt(x)  math.exp(x)
  logs      math.log(x)          natural
            math.log10(x)        base 10
  rounding  math.floor(x)  math.ceil(x)  math.round(x)
  trig      math.sin(x)  math.cos(x)  math.tan(x)        radians
  constants math.pi  math.e                              no parentheses

Every math.* call returns float. Assign to an int to get one:

  fn main() {
      let h = math.sqrt(math.pow(3.0, 2.0) + math.pow(4.0, 2.0));
      let n: int = math.floor(h);
      io.println(n);
  })HELP" },

{ "std/random", "random rand rng seed", Group::Module,
  "random.int and random.seed",
  R"HELP(std/random - random numbers
  #include <std/random>
  Full reference: SYNTAX/Modules.txt (std/random)

  random.int(min, max)   a random int in [min, max], both ends included
  random.seed(n)         seed the generator, for a reproducible sequence

  fn main() {
      random.seed(42);
      io.println(random.int(1, 6));
  })HELP" },

{ "std/time", "time sleep clock timer", Group::Module,
  "sleep, monotonic now_ms, and the duration helpers",
  R"HELP(std/time - sleeping and measuring
  #include <std/time>
  Full reference: SYNTAX/Modules.txt (std/time)

  time.sleep(duration)      sleep; the duration is milliseconds
  time.seconds(n)           n * 1000 milliseconds
  time.milliseconds(n)      n milliseconds
  time.now_ms()             monotonic milliseconds, as a float

now_ms is QueryPerformanceCounter on Windows and CLOCK_MONOTONIC elsewhere, so
it measures elapsed time and never jumps when the wall clock is set. sleep is
Sleep / nanosleep natively -- no <thread> -- and emscripten_sleep on wasm, so
the browser can paint.

  fn main() {
      let t0 = time.now_ms();
      time.sleep(time.seconds(1));
      io.println(time.now_ms() - t0);
  })HELP" },

{ "std/thread", "thread threads worker concurrency spawn", Group::Module,
  "OS threads: spawn, join, and reusable workers",
  R"HELP(std/thread - threads
  #include <std/thread>
  Full reference: SYNTAX/Modules.txt (std/thread)

  one-shot   thread.spawn(fn_name)        run fn_name() on a new OS thread
             thread.spawn(fn(a, b))       a call, with its arguments
             thread.spawn(os.notify(t,m)) an os.* call
             thread.join(handle)          wait for it
  reusable   thread.worker()              a worker thread; returns a handle
             thread.run(worker, job)      queue a job on it, same job forms
             thread.worker_join(worker)   stop it and wait

spawn(fn_name) takes zero-argument functions only. spawn(call) captures the
arguments by value.

  fn worker() { io.println("working"); }

  fn main() {
      let t = thread.spawn(worker);
      thread.join(t);
  }

A thread is also how a program serves and keeps going: NexaC --help std/network)HELP" },

{ "std/crypto", "crypto hash sha256 hmac base64 hex xor", Group::Module,
  "hashes, HMAC, hex and base64, XOR, random bytes",
  R"HELP(std/crypto - digests and encodings
  #include <std/crypto>
  Full reference: SYNTAX/Modules.txt (std/crypto)

  digest    crypto.sha256(data)             lowercase hex
            crypto.sha1(data)               legacy; prefer sha256
            crypto.hmac_sha256(key, data)   lowercase hex
  encode    crypto.hex_encode(data)     crypto.hex_decode(hex)
            crypto.base64_encode(data)  crypto.base64_decode(b64)
  obscure   crypto.xor(data, key_string)      rotating XOR, symmetric
            crypto.xor(data, k1, k2, ...)     the same with int keys
  entropy   crypto.random_bytes(n)      n bytes, as a string

XOR is not encryption -- call it again with the same key to undo it. Only the
helpers you call are emitted, and hex_encode/hex_decode of a literal fold at
compile time.

  fn main() {
      io.println(crypto.sha256("hello"));
      let enc = crypto.xor("secret text", "key");
      io.println(crypto.xor(enc, "key"));
  })HELP" },

{ "std/network", "network net http tcp udp socket server client web", Group::Module,
  "the whole wire: http.* client and server, raw tcp.*, udp.*",
  R"HELP(std/network - HTTP, TCP and UDP
  #include <std/network>
  Full reference: SYNTAX/Modules.txt (std/network)

One include for the whole wire. The prefix on a call says which protocol the
line speaks, and a program carries only the protocols it calls -- sending a
datagram links no HTTP transport at all.

  http client   http.get(url[, headers])
                http.post(url, body[, headers])
                http.put / http.patch, the same shape
                http.delete(url[, headers])
                    These five return Result[string] carrying the body. A
                    non-2xx status is a failure: .error() is "HTTP 404".
                http.request(method, url, body[, headers])
                    Result[HttpResponse] { status, body, headers }. Here only
                    a transport failure is an error, so 404 and 500 succeed.
  http server   http.localhost([port])    Result[HttpServer] { port, socket }
                http.accept(server)       Result[HttpRequest]; blocks
                http.reply(request, status, body[, headers])
                http.raw(request, text)   exactly these bytes, nothing added
                http.close(server)
                    One request per connection: reply and raw both answer and
                    close. Loopback only. Not available on wasm.
  tcp           tcp.connect(host, port)   tcp.listen(port)   tcp.accept(l)
                tcp.send(h, data)         tcp.recv(h[, max])
                tcp.port(h)               tcp.close(h)
                    Int handles, 0 means no socket. tcp.recv is ONE read, not
                    a whole message -- read until you have one. "" means the
                    peer hung up. tcp.listen binds every interface.
  udp           udp.open(port)            udp.port(h)
                udp.send(h, host, port, data)
                udp.recv(h[, max])        udp.sender(h)  udp.sender_port(h)
                udp.close(h)
                    One datagram in, one datagram out -- no stream to reframe.
                    Delivery is not promised. Not available on wasm at all.

Transport is the OS's own: WinHTTP and Winsock on Windows, CFNetwork on macOS,
POSIX sockets plus a dlopen'd libssl on Linux. Nothing is bundled.

  fn main() {
      let page = http.get("https://example.com/");
      if (page.ok()) { io.println(page.value()); }
      else { io.println(page.error()); }
  }

HttpResponse, HttpServer and HttpRequest are http.*'s own types; a program
that includes std/network cannot declare another by those names.
std/thread runs an accept loop beside the rest of a program.)HELP" },

{ "std/json", "json parse stringify", Group::Module,
  "nested JSON: parse, stringify, and the Json value type",
  R"HELP(std/json - JSON values
  #include <std/json>
  Full reference: SYNTAX/Modules.txt (std/json)

The type is Json (json is accepted too). It nests -- objects and arrays all
the way down, not a flat string map.

  text     json.parse(s)              on failure .ok() is false and
                                      .as_string() is the error
           json.stringify(v)          compact
           json.stringify(v, indent)  pretty, when indent > 0
  build    json.of(x)              int/float/bool/string/Json/[]T/map to Json
           json.null()  json.bool(b)  json.int(n)  json.float(x)
           json.string(s)  json.array()  json.object()
  ask      v.ok()  v.kind()           "null" "bool" "number" "string"
                                      "array" "object" "error"
           v.is_null() is_bool() is_number() is_string() is_array()
           v.is_object() is_error()
  read     v.as_bool()  v.as_int()  v.as_float()  v.as_string()
           v.len()  v.has(key)  v.get(key)  v.get(i)  v.keys()
  write    v.set(key, val)  v.push(val)  v.remove(key)
  index    v["name"]   v[0]   v["name"] = 1

A JSON null parses to kind null; a parse error is kind error, which is the one
whose ok() is false.

  fn main() {
      let v: Json = json.parse("{\"a\":1,\"b\":[true,\"x\"]}");
      io.println(v.get("a").as_int());
      io.println(json.stringify(v, 2));
  })HELP" },

{ "std/gfx", "gfx graphics window pixel draw audio sound image sprite input keyboard mouse", Group::Module,
  "pixel window: shapes, text, images, sound, keyboard and mouse",
  R"HELP(std/gfx - a pixel window
  #include <std/gfx>
  Full reference: SYNTAX/Modules.txt (std/gfx)

A framebuffer you plot pixels into, scaled up into a window. Win32, Cocoa, X11
or an Emscripten canvas; no extra library to install on any of them.

  window    gfx.open(title, w, h[, scale])     gfx.close()   gfx.poll()
            gfx.closed()      gfx.present()    gfx.resize(w, h[, scale])
            gfx.maxfps(n)     gfx.save(path)   gfx.icon(src)
            gfx.width()  gfx.height()  gfx.scale()
            gfx.title()  gfx.title(s)  gfx.cursor([on])
  style     gfx.fullscreen([on])   gfx.borderless([on])
            gfx.ontop([on])        gfx.transparent([on])
  draw      gfx.clear(r,g,b)   gfx.plot(x,y,r,g,b)   gfx.get(x,y)
            gfx.fill(...)      gfx.rect(...)
            gfx.round_rect(...)     gfx.fill_round_rect(...)
            gfx.line(x1,y1,x2,y2,r,g,b[,thickness])
            gfx.circle(...)    gfx.fill_circle(...)
            gfx.ellipse(...)   gfx.fill_ellipse(...)
            gfx.arc(...)       gfx.pie(...)
            gfx.tri(...)       gfx.fill_tri(...)
            gfx.poly(xs,ys,r,g,b)   gfx.fill_poly(...)
            gfx.alpha([a])     global draw alpha, 0..255
  text      gfx.text(x,y,s,r,g,b[,scale])      5x7 ASCII, 32..126
            gfx.text_size([n])  gfx.text_width(s)  gfx.text_height(s)
  images    gfx.image(path)    gfx.decode(bytes)
            gfx.image_w(id)    gfx.image_h(id)
            gfx.blit(x, y, src[, w, h])
            gfx.blit(x, y, src, sx, sy, sw, sh[, dw, dh])
            gfx.blit_rot(x, y, src, angle[, w, h])
  sound     gfx.sound(path)    gfx.play(id[, vol])   gfx.loop(id[, vol])
            gfx.stop([voice])  gfx.volume([v])
            gfx.audio([rate])  gfx.sample(s)
            gfx.audio_queued()  gfx.audio_flush()
  input     gfx.key(name)  gfx.pressed(name)  gfx.released(name)
            gfx.typed()    gfx.wheel()        gfx.wheel_x()
            gfx.mouse(button)  gfx.mouse_x()  gfx.mouse_y()
            gfx.drop()     gfx.opendialog([filter]) / gfx.openfile(...)

  fn main() {
      gfx.open("hello", 160, 100, 6);
      while (gfx.closed() != 1) {
          gfx.poll();
          gfx.clear(16, 16, 24);
          gfx.fill_circle(80, 50, 30, 200, 70, 70);
          gfx.present();
      }
  }

Colours are r, g, b in 0..255. Angles are degrees, 0 at 12 o'clock, running
clockwise. With no window open nothing is an error: drawing does nothing and
the readers answer 0. The drawing calls hand nothing back and are statements
only, so `let R = gfx.rect(...);` is an error naming the call.)HELP" },

{ "std/gfx3d", "gfx3d 3d three opengl vulkan gl cube mesh model obj wavefront camera render sound audio wav play", Group::Module,
  "3D window: camera, triangles and cubes, on OpenGL",
  R"HELP(std/gfx3d - a 3D window
  #include <std/gfx3d>
  Full reference: SYNTAX/Modules.txt (std/gfx3d)

std/gfx's loop with a depth buffer under it. Triangles go to the GPU and the
depth buffer decides what is in front, so there is no get and no save here --
no array of pixels to read back.

  window    gfx3d.open(title, w, h)   gfx3d.close()   gfx3d.poll()
            gfx3d.closed()            gfx3d.present() gfx3d.maxfps(n)
            gfx3d.width()             gfx3d.height()
  camera    gfx3d.camera(ex, ey, ez, tx, ty, tz)     eye, then what it looks at
            gfx3d.perspective(fov, near, far)        fov in degrees
  draw      gfx3d.clear(r, g, b)
            gfx3d.tri(x1,y1,z1, x2,y2,z2, x3,y3,z3, r,g,b)
            gfx3d.cube(x, y, z, size, r, g, b)       centred on x,y,z
            gfx3d.box(x, y, z, w, h, d, r, g, b)
            gfx3d.sphere(x, y, z, radius, r, g, b)
            gfx3d.capsule(x1,y1,z1, x2,y2,z2, radius, r,g,b)
            gfx3d.cylinder(...)  same axis, flat ends
            gfx3d.cone(...)      radius at the first point, a tip at the second
            gfx3d.line3(x1,y1,z1, x2,y2,z2, r,g,b)
            gfx3d.grid(size, step, r, g, b)          the y = 0 plane
  light     gfx3d.light(x, y, z[, r, g, b])   direction it shines from
            gfx3d.ambient([level])            0..255 facing away; default 87
  place     gfx3d.translate(x, y, z)   moves what is drawn next
            gfx3d.rotate(rx, ry, rz)   degrees, right-hand rule, X then Y then Z
            gfx3d.scale(s)             uniform
            gfx3d.reset()              back to world space; gfx3d.clear does too
  input     gfx3d.key(name)  gfx3d.pressed(name)  gfx3d.released(name)
            gfx3d.typed()    gfx3d.wheel()        gfx3d.wheel_x()
            gfx3d.mouse(button)  gfx3d.mouse_x()  gfx3d.mouse_y()
                                      the names std/gfx uses, read while focused
  renderer  gfx3d.renderer(name)      ask, before open; "opengl" or "vulkan"
            gfx3d.backend()           what the window actually opened with
  model     gfx3d.model(path)         load a .obj; handle, or 0
            gfx3d.draw(id, x, y, z, scale, r, g, b)
            gfx3d.model_tris(id)      triangles it parsed to
                                      scale is world units whatever the file
                                      was authored at
  sound     gfx3d.sound(path)     gfx3d.play(id[, vol])  gfx3d.loop(id[, vol])
            gfx3d.stop([voice])   gfx3d.volume([v])      0..255
            gfx3d.audio([rate])   gfx3d.sample(s)
            gfx3d.audio_queued()  gfx3d.audio_flush()
                                      std/gfx's mixer, and only ever one of it

  fn main() {
      gfx3d.open("cube", 800, 600);
      gfx3d.camera(4.0, 3.0, 5.0, 0.0, 0.0, 0.0);
      while (gfx3d.closed() != 1) {
          gfx3d.poll();
          gfx3d.clear(18, 18, 28);
          gfx3d.cube(0.0, 0.0, 0.0, 2.0, 220, 90, 70);
          gfx3d.present();
      }
      gfx3d.close();
  }

Two renderers are named and one is built: asking for vulkan quietly gives you
opengl, and backend() reports the truth. Depth test and back-face culling are
on from the start, though a lone gfx3d.tri is drawn from both sides. A cube's
flat sides take the direction of the face and stay sharp-edged; curved ones
take the direction the surface really points and come out smooth. One
directional light, in world space, with no shadows: nothing here knows that
one shape is between another and the light.

Models are Wavefront .obj, read here rather than linked: v, vn and f, with
everything else skipped. A model is centred and divided by its longest side as
it is drawn, so scale means the same thing it does on a cube however the file
was authored, and it is drawn from both sides because exporters disagree about
which way a face is wound.

Sound is std/gfx's sound, unchanged -- same names, same arguments, same WAV
files. A program that includes both modules gets one mixer between them, not
two: one master volume, one set of sixteen voices, one open device. It is not
positioned by the camera; the mixer sums to mono, so a voice has nowhere to be.

Windows, macOS, Linux and the browser. The first three are OpenGL 1.1 with
three different windows under it; --wasm is a second renderer, because WebGL
has no fixed-function pipeline -- same picture, shaders underneath. Nothing
to install for any of them.

Colours are 0..255. The draws hand nothing back and are statements only, so
`let C = gfx3d.cube(...);` is an error naming the call -- as in std/gfx.)HELP" },

{ "std/dll", "dll so dylib shared library plugin", Group::Module,
  "load a .dll / .so / .dylib and call into it",
  R"HELP(std/dll - dynamic libraries
  #include <std/dll>
  Full reference: SYNTAX/Modules.txt (std/dll)

  dll.load(path)                 load a .so / .dll / .dylib; returns a handle
  dll.call(h, "sym")             call an exported function
  dll.call(h, "sym", args...)    the same, passing arguments

  fn main() {
      let h = dll.load("./plugin.so");
      dll.call(h, "plugin_init", 42);
  }

Build the other side with NexaC lib.nxa --dll (or --shared). Library builds
preserve names, so dll.call finds the symbol you wrote. A `fn __init__()` in a
library runs when the library loads.

A #include copy of a global is not shared with a library -- pass values in
through dll.call. For linking an archive into the executable instead, see
--static-lib and --link: NexaC --help options.)HELP" },

{ "std/inline", "inline inline_cpp cpp raw escape", Group::Module,
  "inline_cpp! { ... } -- raw C++ where Nexa has no API",
  R"HELP(std/inline - raw C++
  #include <std/inline>
  Full reference: SYNTAX/Inline.txt

Without the include, inline_cpp! is rejected at parse time.

  inline_cpp! {
      ... C++ statements ...
  }

Inside a function the body is emitted into that function, wrapped in a
compound statement. At file scope it is emitted at translation-unit scope with
no extra braces, so you can paste functions, globals, or an int main().

Any line starting with #include is hoisted to the top of the generated .cpp
and deduplicated, because C++ does not allow an include inside a function.
There is no automatic <iostream>; add what you need.

  #include <std/inline>

  fn main() {
      inline_cpp! {
  #include <iostream>
          std::cout << "Hello\n";
      }
  }

The body is not parsed by Nexa -- brace matching only, so a } inside a C++
string can end the block early. Clang has to accept whatever comes out.
Reach for this last; a program that uses it keeps RTTI and exceptions, and
loses the undefined-name check.)HELP" },

};

inline const size_t kTopicCount = sizeof(kTopics) / sizeof(kTopics[0]);

// ---------------------------------------------------------------- matching --

inline std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// What the user typed, reduced to the spelling the table uses. Leading dashes,
// angle brackets and a trailing slash are all ways people write a topic that
// mean the same one: --core, <std/io>, std/io/.
inline std::string normalize(const std::string& raw) {
    std::string s = lower(raw);
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == '-' || s[b] == '<')) b++;
    while (e > b && (s[e - 1] == '>' || s[e - 1] == '/')) e--;
    s = s.substr(b, e - b);
    if (s.size() > 8 && s.compare(0, 8, "#include") == 0) {
        size_t p = s.find_first_not_of(" \t", 8);
        if (p != std::string::npos) return normalize(s.substr(p));
    }
    return s;
}

inline bool isWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// `haystack` contains `word` bounded by non-identifier characters on both
// sides. Used against topic bodies, so "int" does not match "print".
inline bool hasWord(const std::string& haystack, const std::string& word) {
    if (word.empty()) return false;
    for (size_t p = haystack.find(word); p != std::string::npos;
         p = haystack.find(word, p + 1)) {
        const bool leftOk = (p == 0) || !isWordChar(haystack[p - 1]);
        const size_t after = p + word.size();
        const bool rightOk = (after >= haystack.size()) || !isWordChar(haystack[after]);
        if (leftOk && rightOk) return true;
    }
    return false;
}

// The topic whose canonical name or alias list is exactly `q`, or -1.
inline int findExact(const std::string& q) {
    if (q.empty()) return -1;
    for (size_t i = 0; i < kTopicCount; i++) {
        if (q == lower(kTopics[i].name)) return static_cast<int>(i);
    }
    for (size_t i = 0; i < kTopicCount; i++) {
        if (hasWord(kTopics[i].aliases, q)) return static_cast<int>(i);
    }
    return -1;
}

// Topics whose page lists `.word` as a call: `blit` finds std/gfx, `push`
// finds core. This is what makes a name you half-remember enough to search on.
inline std::vector<int> findByCallName(const std::string& q) {
    std::vector<int> hits;
    if (q.empty()) return hits;
    for (size_t i = 0; i < kTopicCount; i++) {
        if (!kTopics[i].body) continue;
        if (hasWord(kTopics[i].body, "." + q)) hits.push_back(static_cast<int>(i));
    }
    return hits;
}

inline std::vector<int> findByText(const std::string& q) {
    std::vector<int> hits;
    if (q.size() < 3) return hits;
    for (size_t i = 0; i < kTopicCount; i++) {
        if (!kTopics[i].body) continue;
        if (hasWord(lower(kTopics[i].body), q)) hits.push_back(static_cast<int>(i));
    }
    return hits;
}

inline size_t editDistance(const std::string& a, const std::string& b) {
    std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++) prev[j] = j;
    for (size_t i = 1; i <= a.size(); i++) {
        cur[0] = i;
        for (size_t j = 1; j <= b.size(); j++) {
            const size_t sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
            const size_t del = prev[j] + 1;
            const size_t ins = cur[j - 1] + 1;
            cur[j] = sub < del ? (sub < ins ? sub : ins) : (del < ins ? del : ins);
        }
        prev = cur;
    }
    return prev[b.size()];
}

// Near misses over every name and alias: std/gxf -> std/gfx, thred -> thread.
inline std::vector<std::string> suggestions(const std::string& q) {
    std::vector<std::string> out;
    if (q.empty()) return out;
    for (size_t i = 0; i < kTopicCount; i++) {
        const std::string name = lower(kTopics[i].name);
        bool isNear = editDistance(q, name) <= 2;
        if (!isNear) {
            std::string word;
            std::string aliases = kTopics[i].aliases;
            aliases.push_back(' ');
            for (char c : aliases) {
                if (c == ' ') {
                    if (!word.empty() && editDistance(q, word) <= 2) isNear = true;
                    word.clear();
                } else {
                    word.push_back(c);
                }
            }
        }
        if (isNear) out.push_back(kTopics[i].name);
    }
    return out;
}

// ----------------------------------------------------------------- output --

inline void printTopic(int i) {
    if (kTopics[i].body) {
        std::cout << kTopics[i].body << "\n";
    } else {
        // nexapkg keeps its own help, which is already the page this would be.
        pkg::printHelp();
    }
}

inline void printGroup(const char* heading, const char* note, Group g) {
    std::cout << "\n" << heading;
    if (note && *note) {
        const size_t pad = 47 > std::string(heading).size()
                               ? 47 - std::string(heading).size() : 1;
        std::cout << std::string(pad, ' ') << note;
    }
    std::cout << "\n";
    for (size_t i = 0; i < kTopicCount; i++) {
        if (kTopics[i].group != g) continue;
        std::string name = kTopics[i].name;
        if (name.size() < 13) name += std::string(13 - name.size(), ' ');
        std::cout << "  " << name << kTopics[i].blurb << "\n";
    }
}

inline void printIndex(const char* version) {
    std::cout <<
        "NexaC " << version << " - the Nexa compiler\n"
        "Nexa source (.nxa) becomes one C++ translation unit, then a native\n"
        "binary. Needs clang++ or g++ on PATH; --wasm needs Emscripten.\n"
        "\n"
        "Usage\n"
        "  NexaC <file.nxa> [-o <out>]     compile\n"
        "  NexaC <file.nxa> -r             compile to a temp binary and run it\n"
        "  NexaC <file.nxa> --source <f>   emit the generated C++ and stop\n"
        "  NexaC init [dir]                scaffold a project\n"
        "  NexaC build [dir]               build the project in dir\n"
        "  NexaC upgrade                   install a newer release if one exists\n"
        "  NexaC nexapkg <cmd>             packages (also installed as nexapkg)\n"
        "\n"
        "Options                                 in full: NexaC --help options\n"
        "  -o <name>    name the output      -r, --run     run after building\n"
        "  -p           keep your fn names   -g, --debug   steps through .nxa\n"
        "  --small      optimize for size    --source <f>  emit C++ only\n"
        "  --dll  --shared  --static-lib  --link <file>    libraries\n"
        "  --win  --wasm  --no-console                     other targets\n";

    printGroup("Language", "", Group::Language);
    printGroup("Modules", "#include <std/NAME>", Group::Module);
    printGroup("Tooling", "", Group::Tool);

    std::cout <<
        "\n"
        "  NexaC --help <topic>      e.g. NexaC --help std/gfx\n"
        "  NexaC --help <call>       e.g. NexaC --help gfx.blit, --help push\n"
        "  NexaC --help all          every topic, for piping into a pager\n"
        "\n"
        "Every topic here is a summary. The reference is SYNTAX/ in the repo:\n"
        "https://github.com/Bobism-2009/Nexa-Lang\n";
}

// The nine numbered pages NexaC used to have. Kept resolving, because the old
// spellings are in muscle memory and in older notes, but answered by name so
// the new one is the thing you learn.
inline const char* retiredPageTopic(const std::string& q) {
    std::string n = q;
    if (n.size() > 4 && n.compare(0, 4, "page") == 0) n = n.substr(4);
    if (n.size() != 1 || !std::isdigit(static_cast<unsigned char>(n[0]))) return nullptr;
    switch (n[0]) {
        case '1': return "";            // the old usage page is the index now
        case '2': return "core";
        case '3': return "std/io";
        case '4': return "std/os";
        case '5': return "std/dll";
        case '6': return "std/file";
        case '7': return "std/random";
        case '8': return "std/thread";
        case '9': return "std/inline";
        default: return nullptr;
    }
}

// ------------------------------------------------------------------- entry --

inline int run(const std::string& rawQuery, const char* version) {
    const std::string q = normalize(rawQuery);

    if (q.empty()) {
        printIndex(version);
        return 0;
    }

    if (q == "all") {
        for (size_t i = 0; i < kTopicCount; i++) {
            if (i) std::cout << "\n" << std::string(72, '-') << "\n\n";
            printTopic(static_cast<int>(i));
        }
        return 0;
    }

    if (q == "topics" || q == "list" || q == "index") {
        printIndex(version);
        return 0;
    }

    int i = findExact(q);
    if (i >= 0) {
        printTopic(i);
        return 0;
    }

    for (const Retired& r : kRetired) {
        if (q != r.name) continue;
        std::cout << "[Nexa] " << r.name << " was retired -- " << r.why << ".\n"
                  << "[Nexa] It is " << r.replacement << " now:\n\n";
        printTopic(findExact(r.replacement));
        return 0;
    }

    if (const char* page = retiredPageTopic(q)) {
        std::cout << "[Nexa] Help pages are named, not numbered.\n";
        if (!*page) {
            std::cout << "[Nexa] The old page 1 is what NexaC --help prints:\n\n";
            printIndex(version);
        } else {
            std::cout << "[Nexa] Try: NexaC --help " << page << "\n\n";
            printTopic(findExact(page));
        }
        return 0;
    }

    // gfx.blit -> the std/gfx page, saying where the call lives.
    const size_t dot = q.find('.');
    if (dot != std::string::npos && dot > 0) {
        const int owner = findExact(q.substr(0, dot));
        if (owner >= 0) {
            std::cout << "[Nexa] " << q << " is in " << kTopics[owner].name << ":\n\n";
            printTopic(owner);
            return 0;
        }
    }

    std::vector<int> hits = findByCallName(q);
    if (hits.empty()) hits = findByText(q);
    if (hits.size() == 1) {
        std::cout << "[Nexa] " << rawQuery << " is in " << kTopics[hits[0]].name << ":\n\n";
        printTopic(hits[0]);
        return 0;
    }
    if (hits.size() > 1) {
        std::cout << "[Nexa] " << rawQuery << " appears in several topics:\n";
        for (int h : hits) std::cout << "         NexaC --help " << kTopics[h].name << "\n";
        return 0;
    }

    std::cerr << "[Nexa] No help topic '" << rawQuery << "'.\n";
    const std::vector<std::string> nearby = suggestions(q);
    if (!nearby.empty()) {
        std::cerr << "[Nexa] Did you mean:";
        for (const std::string& s : nearby) std::cerr << " " << s;
        std::cerr << "\n";
    }
    std::cerr << "[Nexa] NexaC --help          lists every topic\n";
    return 1;
}

}  // namespace help
}  // namespace nexa
