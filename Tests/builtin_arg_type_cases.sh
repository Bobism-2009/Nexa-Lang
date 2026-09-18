#!/bin/sh
# Argument types for module builtins (BOB-54).
#
# BOB-53 taught NexaC to refuse a void module builtin in value position, using
# the runtime signatures as the authority. The argument *types* in those same
# signatures were still nobody's business at Nexa level, so
# `gfx.open(100, 100, "t")` transpiled and clang was left to explain it -- an
# error about __nexa_gfx_open(const std::string&, int, int, int), generated
# code the user never wrote. The tables in Transpiler.hpp now read a letter per
# parameter off each runtime signature, and this suite is their cover.
#
# Three layers, cheapest first, the same shape as the other Tests/*_cases.sh:
#
#   accept   Every builtin that takes an argument, called correctly. This is
#            the half that catches over-rejection: a row whose letters drift
#            away from its runtime signature fails here, loudly, rather than
#            quietly refusing working programs.
#
#   allow    The conversions the language does allow and this check must not
#            take away: a float where an int parameter is, char and bool as
#            numbers, a number to any os.* text parameter (they go through
#            std::to_string), and gfx.blit/gfx.icon taking either an image id
#            or a path.
#
#   reject   A wrong-type call per parameter kind per namespace. Each must
#            fail, name the signature and the parameter, and leak no C++.
#
# Transpile only (--source): nothing here reaches the C++ compiler, so the
# whole suite runs anywhere NexaC does.
#
# Usage: Tests/builtin_arg_type_cases.sh [path-to-NexaC]  (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
n=0

# accept_prog <label> <program>
# The call is right, so NexaC must transpile it.
accept_prog() {
    n=$((n + 1))
    printf '%s\n' "$2" > "$WORK/$1.nxa"
    if ! out=$("$NEXAC" "$WORK/$1.nxa" --source "$WORK/$1.cpp" 2>&1); then
        echo "FAIL $1: NexaC refused a correctly typed call"
        printf '%s\n' "$out" | tail -n 2 | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $1"
}

# accept <label> <include> <body> -- accept_prog around one main()
accept() {
    accept_prog "$1" "$2
fn main() {
$3
}"
}

# reject_prog <label> <program> <want>
# The program has an argument of the wrong type, so NexaC must fail, and the
# diagnostic must contain <want> -- which names the signature and the
# parameter, not a generated C++ symbol.
reject_prog() {
    n=$((n + 1))
    printf '%s\n' "$2" > "$WORK/$1.nxa"
    if out=$("$NEXAC" "$WORK/$1.nxa" --source "$WORK/$1.cpp" 2>&1); then
        echo "FAIL $1: NexaC accepted an argument of the wrong type"
        fails=$((fails + 1))
        return
    fi
    case $out in
        *"$3"*) ;;
        *)
            echo "FAIL $1: refused, but not with the argument-type diagnostic"
            echo "  wanted: $3"
            printf '%s\n' "$out" | tail -n 2 | sed 's/^/  /'
            fails=$((fails + 1))
            return
            ;;
    esac
    # The whole point is that the user never hears about generated C++.
    case $out in
        *__nexa_*)
            echo "FAIL $1: the diagnostic leaked a generated C++ name"
            printf '%s\n' "$out" | tail -n 2 | sed 's/^/  /'
            fails=$((fails + 1))
            return
            ;;
    esac
    echo "ok $1"
}

# reject <label> <include> <body> <want> -- reject_prog around one main()
reject() {
    reject_prog "$1" "$2
fn main() {
$3
}" "$4"
}

GFX='#include <std/gfx>'
OS='#include <std/os>'
MATH='#include <std/math>'
FILE='#include <std/file>'
NET='#include <std/network>'
CRYPTO='#include <std/crypto>'
TIME='#include <std/time>'
RANDOM='#include <std/random>'

echo "-- accept: every builtin that takes an argument, called correctly"

accept gfx_ok_open        "$GFX" '    gfx.open("t", 320, 240, 2);'
accept gfx_ok_resize      "$GFX" '    gfx.resize(320, 240, 2);'
accept gfx_ok_maxfps      "$GFX" '    gfx.maxfps(60);'
accept gfx_ok_keys        "$GFX" '    let a = gfx.key("space");
    let b = gfx.pressed("a");
    let c = gfx.released("b");
    let d = gfx.mouse("left");'
accept gfx_ok_clear       "$GFX" '    gfx.clear(1, 2, 3);'
accept gfx_ok_plot        "$GFX" '    gfx.plot(1, 2, 3, 4, 5);'
accept gfx_ok_get         "$GFX" '    let p = gfx.get(1, 2);'
accept gfx_ok_fill        "$GFX" '    gfx.fill(1, 2, 3, 4, 5, 6, 7);'
accept gfx_ok_rect        "$GFX" '    gfx.rect(1, 2, 3, 4, 5, 6, 7);'
accept gfx_ok_line        "$GFX" '    gfx.line(1, 2, 3, 4, 5, 6, 7);
    gfx.line(1, 2, 3, 4, 5, 6, 7, 3);'
accept gfx_ok_circle      "$GFX" '    gfx.circle(1, 2, 3, 4, 5, 6);
    gfx.fill_circle(1, 2, 3, 4, 5, 6);'
accept gfx_ok_ellipse     "$GFX" '    gfx.ellipse(1, 2, 3, 4, 5, 6, 7);
    gfx.fill_ellipse(1, 2, 3, 4, 5, 6, 7);'
accept gfx_ok_arc         "$GFX" '    gfx.arc(1, 2, 3, 0, 90, 5, 6, 7);
    gfx.pie(1, 2, 3, 0, 90, 5, 6, 7);'
accept gfx_ok_round_rect  "$GFX" '    gfx.round_rect(1, 2, 8, 6, 2, 5, 6, 7);
    gfx.fill_round_rect(1, 2, 8, 6, 2, 5, 6, 7);'
accept gfx_ok_tri         "$GFX" '    gfx.tri(0, 0, 4, 0, 0, 4, 1, 2, 3);
    gfx.fill_tri(0, 0, 4, 0, 0, 4, 1, 2, 3);'
accept gfx_ok_poly        "$GFX" '    let xs: []int = [0, 4, 0];
    let ys: []int = [0, 0, 4];
    gfx.poly(xs, ys, 1, 2, 3);
    gfx.fill_poly(xs, ys, 1, 2, 3);'
accept gfx_ok_text        "$GFX" '    gfx.text(1, 2, "hi", 4, 5, 6);
    gfx.text(1, 2, "hi", 4, 5, 6, 2);'
accept gfx_ok_text_size   "$GFX" '    let g = gfx.text_size();
    let s = gfx.text_size(2);'
accept gfx_ok_text_dims   "$GFX" '    let w = gfx.text_width("hi");
    let h = gfx.text_height("hi", 2);'
accept gfx_ok_title       "$GFX" '    gfx.title("hi");'
accept gfx_ok_opendialog  "$GFX" '    let f = gfx.opendialog("*.png");'
accept gfx_ok_images      "$GFX" '    let i = gfx.image("a.png");
    let d = gfx.decode("bytes");
    let w = gfx.image_w(i);
    let h = gfx.image_h(i);'
accept gfx_ok_save        "$GFX" '    let ok = gfx.save("shot.bmp");'
accept gfx_ok_window      "$GFX" '    let a = gfx.fullscreen(1);
    let b = gfx.borderless(1);
    let c = gfx.ontop(1);
    let d = gfx.transparent(1);
    let e = gfx.cursor(0);
    let f = gfx.alpha(128);'
accept gfx_ok_audio       "$GFX" '    let a = gfx.audio(44100);
    let s = gfx.sample(0);
    let n = gfx.sound("a.wav");
    let p = gfx.play(n);
    let l = gfx.loop(n, 128);
    let t = gfx.stop(p);
    let v = gfx.volume(128);'
accept gfx_ok_blit        "$GFX" '    let i = gfx.image("a.png");
    gfx.blit(0, 0, i);
    gfx.blit(0, 0, i, 8, 8);
    gfx.blit(0, 0, i, 1, 2, 3, 4);
    gfx.blit(0, 0, i, 1, 2, 3, 4, 8, 8);
    gfx.blit_rot(0, 0, i, 45);
    gfx.blit_rot(0, 0, i, 45, 8, 8);
    gfx.icon(i);'

accept os_ok_text         "$OS" '    os.clip_set("x");
    os.type("x");
    os.notify("t", "m");
    os.open("x");
    os.messagebox("t", "m");
    os.setenv("K", "V");
    os.unsetenv("K");'
accept os_ok_numbers      "$OS" '    os.set_volume(50);
    os.set_brightness(50);
    os.exit(0);'
accept os_ok_process      "$OS" '    let a = os.spawn("echo", "hi");
    let b = os.spawn_at("/tmp", "echo", "hi");
    let c = os.wait(a);
    let d = os.kill(a);
    let e = os.getprocessid("init");'
accept os_ok_files        "$OS" '    let a = os.load("a.txt");
    let b = os.save("a.txt", "hi");
    let c = os.play("a.wav");
    let d = os.which("sh");
    let e = os.chdir("/tmp");
    let f = os.system("true");'

accept math_ok_one        "$MATH" '    let a = math.abs(-1);
    let b = math.sqrt(9);
    let c = math.floor(1.5);
    let d = math.ceil(1.5);
    let e = math.round(1.5);
    let f = math.sin(0);
    let g = math.cos(0);
    let h = math.tan(0);
    let i = math.log(1);
    let j = math.log10(10);
    let k = math.exp(1);'
accept math_ok_two        "$MATH" '    let a = math.min(1, 2);
    let b = math.max(1, 2);
    let c = math.pow(2, 8);'

accept file_ok_paths      "$FILE" '    let a = file.read("a.txt");
    let b = file.exists("a.txt");
    let c = file.mkdir("d");
    let d = file.size("a.txt");
    let e = file.isdir("d");
    let f = file.isfile("a.txt");
    let g = file.list("d");
    let h = file.abspath("a.txt");
    let i = file.dirname("a.txt");
    let j = file.basename("a.txt");
    let k = file.extension("a.txt");'
accept file_ok_pairs      "$FILE" '    let a = file.write("a.txt", "hi");
    let b = file.append("a.txt", "hi");
    let c = file.rename("a.txt", "b.txt");
    let d = file.copy("a.txt", "b.txt");
    let e = file.join("d", "a.txt");'

accept tcp_ok             "$NET" '    let c = tcp.connect("127.0.0.1", 80);
    let l = tcp.listen(5000);
    let p = tcp.accept(l);
    let s = tcp.send(c, "hi");
    let r = tcp.recv(c);
    let m = tcp.recv(c, 64);
    let q = tcp.port(c);
    let z = tcp.close(c);'
accept udp_ok             "$NET" '    let h = udp.open(9000);
    let s = udp.send(h, "127.0.0.1", 9001, "hi");
    let r = udp.recv(h);
    let m = udp.recv(h, 64);
    let a = udp.sender(h);
    let p = udp.sender_port(h);
    let z = udp.close(h);'
accept http_ok_client     "$NET" '    let a = http.get("http://x/");
    let b = http.delete("http://x/", ["X: y"]);
    let c = http.post("http://x/", "body");
    let d = http.put("http://x/", "body", ["X: y"]);
    let e = http.patch("http://x/", "body");
    let f = http.request("HEAD", "http://x/", "", ["X: y"]);'
accept http_ok_server     "$NET" '    let s = http.localhost(8080);
    if (s.ok()) {
        let srv = s.value();
        let r = http.accept(srv);
        if (r.ok()) {
            let req = r.value();
            let a = http.reply(req, 200, "hi", ["X: y"]);
            let b = http.raw(req, "HTTP/1.1 200 OK");
        }
        let c = http.close(srv);
    }'

accept crypto_ok          "$CRYPTO" '    let a = crypto.sha256("x");
    let b = crypto.sha1("x");
    let c = crypto.hex_encode("x");
    let d = crypto.hex_decode("78");
    let e = crypto.base64_encode("x");
    let f = crypto.base64_decode("eA==");
    let g = crypto.hmac_sha256("k", "x");
    let h = crypto.random_bytes(16);
    let i = crypto.xor("x", "k");
    let j = crypto.xor("x", 1, 2, 3);'

accept time_ok            "$TIME" '    time.sleep(10);'
accept random_ok          "$RANDOM" '    random.seed(7);
    let r = random.int(1, 6);'

echo "-- allow: the conversions this check must not take away"

accept allow_gfx_float    "$GFX" '    let f: float = 1.5;
    gfx.rect(f, f, 10, 10, 255, 255, 255);
    gfx.arc(10, 10, 5, 0.0, 90.0, 1, 2, 3);'
accept allow_gfx_charbool "$GFX" '    let c: char = 65;
    let b: bool = true;
    gfx.plot(c, c, 1, 2, 3);
    let v = gfx.fullscreen(b);'
accept allow_gfx_wide     "$GFX" '    let w: long = 320;
    gfx.open("t", w, w);'
accept allow_gfx_blit_path "$GFX" '    gfx.blit(0, 0, "a.png");
    gfx.blit_rot(0, 0, "a.png", 45);
    gfx.icon("a.png");'
accept allow_os_stringify "$OS" '    os.open(8080);
    os.setenv("PORT", 8080);
    os.notify("up", 8080);
    os.spawn("echo", 1, 2, 3);'
accept allow_file_content "$FILE" '    let a = file.write("a.txt", 7);
    let b = file.append("a.txt", 7);'
accept allow_crypto_num   "$CRYPTO" '    let a = crypto.sha256(1234);
    let b = crypto.hmac_sha256(1, 2);'
accept allow_math_mixed   "$MATH" '    let a = math.min(1, 2.5);
    let b = math.pow(2, 0.5);'
# A struct field's type is known to the walk that runs this check only through
# the declaration stack: varStructScopes_ is filled as codegen goes, and is
# still empty here. Without that fallback `l.caption` inferred as int and this
# working program was refused.
accept_prog allow_struct_field '#include <std/gfx>
struct Label {
    caption: string;
    size: int;
}
fn main() {
    let l = Label { caption: "hi", size: 2 };
    gfx.title(l.caption);
    gfx.text_size(l.size);
}'
# With inline C++ or a C++ header in the program a name can be declared
# somewhere this walk cannot read, so the check stands down entirely -- the
# same condition, for the same reason, as the undefined-name check.
accept allow_inline_cpp   '#include <std/inline>
#include <std/gfx>' '    inline_cpp! {
        int __q = 1; (void)__q;
    }
    gfx.title(7);'

echo "-- reject: a wrong-type argument per kind per namespace"

# The founder's report, and the reason for the issue: an int where the title
# goes and a string where the width goes, both in one call.
reject gfx_bad_open       "$GFX" '    gfx.open(100, 100, "t");' \
    'gfx.open(title, w, h[, scale]) expects text for title, but got a number'
reject gfx_bad_clear      "$GFX" '    gfx.clear(255, "255", 0);' \
    'gfx.clear(r, g, b) expects a number for g, but got text'
reject gfx_bad_text       "$GFX" '    gfx.text(1, 2, 3, 4, 5, 6);' \
    'gfx.text(x, y, s, r, g, b[, scale]) expects text for s, but got a number'
reject gfx_bad_key        "$GFX" '    let k = gfx.key(32);' \
    'gfx.key(name) expects text for name, but got a number'
reject gfx_bad_title      "$GFX" '    gfx.title(7);' \
    'gfx.title([s]) expects text for s, but got a number'
reject gfx_bad_maxfps     "$GFX" '    gfx.maxfps("60");' \
    'gfx.maxfps(fps) expects a number for fps, but got text'
reject gfx_bad_save       "$GFX" '    let ok = gfx.save(1);' \
    'gfx.save(path) expects text for path, but got a number'
reject gfx_bad_sound      "$GFX" '    let s = gfx.sound(1);' \
    'gfx.sound(path) expects text for path, but got a number'
# An aggregate where a scalar goes: neither text nor a number, and named by
# its own type rather than by whatever C++ would have said.
reject gfx_bad_poly_rgb   "$GFX" '    let xs: []int = [0, 4, 0];
    gfx.poly(xs, xs, xs, 1, 2);' \
    "gfx.poly(xs, ys, r, g, b) expects a number for r, but got '[]int'"
reject gfx_bad_blit_src   "$GFX" '    let xs: []int = [0, 1];
    gfx.blit(0, 0, xs);' \
    "gfx.blit(x, y, src[, w, h]) expects text or a number for src, but got '[]int'"
# The source-rect form names its own arguments, so the seven-argument call
# must be told about sx and not about w.
reject gfx_bad_blit_rect  "$GFX" '    let i = gfx.image("a.png");
    gfx.blit(0, 0, i, "1", 2, 3, 4);' \
    'gfx.blit(x, y, src, sx, sy, sw, sh[, dw, dh]) expects a number for sx, but got text'
# A Nexa enum is `enum class : int` in C++, so it does not reach an int
# parameter on its own -- and Colour.Red names a type and a variant rather
# than a variable and a field, so the type has to be recognised there too.
reject_prog gfx_bad_enum '#include <std/gfx>
enum Colour {
    Red;
    Green;
}
fn main() {
    gfx.clear(Colour.Red, 0, 0);
}' \
    "gfx.clear(r, g, b) expects a number for r, but got 'Colour'"

reject os_bad_exit        "$OS" '    os.exit("bye");' \
    'os.exit(code) expects a number for code, but got text'
reject os_bad_volume      "$OS" '    os.set_volume("loud");' \
    'os.set_volume(v) expects a number for v, but got text'
reject os_bad_wait        "$OS" '    let w = os.wait("1");' \
    'os.wait(pid) expects a number for pid, but got text'
# os.* text parameters take a number too, but nothing can stringify a slice.
reject os_bad_open        "$OS" '    let xs: []int = [1];
    os.open(xs);' \
    "os.open(target) expects text or a number for target, but got '[]int'"
reject os_bad_spawn       "$OS" '    let xs: []int = [1];
    let p = os.spawn("echo", xs);' \
    "os.spawn(prog[, arg...]) expects text or a number for arg, but got '[]int'"
# os.getprocessid is the one os.* text parameter that is not stringified.
reject os_bad_pid         "$OS" '    let p = os.getprocessid(42);' \
    'os.getprocessid([name]) expects text for name, but got a number'

reject math_bad_sqrt      "$MATH" '    let r = math.sqrt("9");' \
    'math.sqrt(x) expects a number for x, but got text'
reject math_bad_pow       "$MATH" '    let r = math.pow(2, "8");' \
    'math.pow(base, exp) expects a number for exp, but got text'

reject file_bad_read      "$FILE" '    let s = file.read(1);' \
    'file.read(path) expects text for path, but got a number'
reject file_bad_write     "$FILE" '    let ok = file.write(1, "hi");' \
    'file.write(path, content) expects text for path, but got a number'
reject file_bad_size      "$FILE" '    let n = file.size(3);' \
    'file.size(path) expects text for path, but got a number'
reject file_bad_join      "$FILE" '    let p = file.join("d", 2);' \
    'file.join(a, b) expects text for b, but got a number'

reject tcp_bad_connect    "$NET" '    let c = tcp.connect(80, "127.0.0.1");' \
    'tcp.connect(host, port) expects text for host, but got a number'
reject tcp_bad_send       "$NET" '    let c = tcp.connect("127.0.0.1", 80);
    let s = tcp.send(c, 42);' \
    'tcp.send(handle, data) expects text for data, but got a number'
reject tcp_bad_close      "$NET" '    let z = tcp.close("c");' \
    'tcp.close(handle) expects a number for handle, but got text'
reject udp_bad_send       "$NET" '    let h = udp.open(9000);
    let s = udp.send(h, 127, 9001, "hi");' \
    'udp.send(handle, host, port, data) expects text for host, but got a number'
reject udp_bad_open       "$NET" '    let h = udp.open("9000");' \
    'udp.open(port) expects a number for port, but got text'

reject http_bad_url       "$NET" '    let r = http.get(80);' \
    'http.get(url[, headers]) expects text for url, but got a number'
reject http_bad_headers   "$NET" '    let r = http.get("http://x/", "X: y");' \
    'http.get(url[, headers]) expects a []string of header lines for headers, but got text'
reject http_bad_body      "$NET" '    let r = http.post("http://x/", 7);' \
    'http.post(url, body[, headers]) expects text for body, but got a number'
# The server and the request are http.*'s own types, so a handle-looking int
# is as wrong there as text would be.
reject http_bad_accept    "$NET" '    let r = http.accept(3);' \
    'http.accept(server) expects an http.localhost() server for server, but got a number'
reject http_bad_reply     "$NET" '    let s = http.localhost(0);
    let a = http.reply(s, 200, "hi");' \
    "http.reply(request, status, body[, headers]) expects an http.accept() request for request, but got 'Result[HttpServer]'"

reject crypto_bad_bytes   "$CRYPTO" '    let b = crypto.random_bytes("16");' \
    'crypto.random_bytes(n) expects a number for n, but got text'
# One key may be text (a repeating key) or a number; more than one goes into a
# vector<int>, so every one of them has to be a number.
reject crypto_bad_keys    "$CRYPTO" '    let x = crypto.xor("x", 1, "k");' \
    'crypto.xor(data, key...) expects a number for key, but got text'
reject crypto_bad_data    "$CRYPTO" '    let xs: []int = [1];
    let h = crypto.sha256(xs);' \
    "crypto.sha256(data) expects text or a number for data, but got '[]int'"

reject time_bad_sleep     "$TIME" '    time.sleep("1000");' \
    'time.sleep(ms) expects a number for ms, but got text'
reject random_bad_int     "$RANDOM" '    let r = random.int("1", 6);' \
    'random.int(min, max) expects a number for min, but got text'

if [ "$fails" -eq 0 ]; then
    echo "ok builtin argument types: $n cases"
    exit 0
fi
echo "builtin_arg_type: $fails failure(s) of $n"
exit 1
