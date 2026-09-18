# NexaC on macOS

NexaC supports native Apple silicon and Intel Macs through Apple Clang.

## Requirements

- macOS 11 or newer
- Apple Command Line Tools (`xcode-select --install`)

## Build and install

```sh
make
make install
```

The default install prefix is `~/.local`. Add `~/.local/bin` to `PATH` if it is
not already present:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

## Compile Nexa programs

```sh
NexaC hello.nxa -o hello
./hello
```

Dynamic libraries use the native `.dylib` extension:

```sh
NexaC library.nxa --shared -o library
```

NexaC uses `-dynamiclib` and the Apple linker’s `-dead_strip` option on macOS.
Linux-only static runtime and GNU linker flags are not passed to Apple Clang.

## HTTP support

`std/network`'s http.* links against the macOS CoreFoundation and CFNetwork
frameworks and supports HTTP and HTTPS through the operating system networking
stack. Its tcp.* and udp.* are POSIX sockets and link nothing extra; a program
that calls only those does not pull the frameworks in.

## Graphics (`std/gfx`)

`#include <std/gfx>` opens a Cocoa window. NexaC compiles the generated code as
Objective-C++ (`-x objective-c++ -fobjc-arc`) and links `-framework Cocoa`
and `-framework ApplicationServices`. `gfx.key` uses the HID key state
(`CGEventSourceKeyState`) only while the gfx window is the key window.
`gfx.mouse_x` / `gfx.mouse_y` / `gfx.mouse` use the cursor location in the
content view and `pressedMouseButtons`. `gfx.fullscreen(1)` / `gfx.fullscreen(0)`
call `toggleFullScreen`. `gfx.borderless(1)` swaps the window's style mask for
`NSWindowStyleMaskBorderless | NSWindowStyleMaskResizable` — the resize bar is
kept because a mask of `Borderless` alone answers NO to `canBecomeKeyWindow`,
which would cost the program its keyboard — and puts the content rect back
where it was, since `setStyleMask:` keeps the frame rather than the content.
`gfx.ontop(1)` / `gfx.ontop(0)` set the window's `level` to
`NSFloatingWindowLevel` / `NSNormalWindowLevel` — Floating is the level AppKit
keeps palettes at, above every normal window including other applications' and
below the ones the system reserves for menus and alerts.
`gfx.transparent(1)` sets `setOpaque:NO` and a `clearColor` background, and the
content view's `isOpaque` answers from the same flag — a view that says it is
opaque is a promise AppKit takes at its word and stops drawing anything behind.
The frame goes out as `kCGImageAlphaPremultipliedLast` rather than
`kCGImageAlphaNoneSkipLast`, premultiplied into a scratch copy on the way
because `CGBitmapContextCreate` has no non-premultiplied form to offer; the
framebuffer itself stays straight, which is what `gfx.get` and every blend read
back. `invalidateShadow` goes with the toggle, so the old shape's drop shadow
is not left drawn around a window that no longer has that shape.
`gfx.audio` / `gfx.sample` compile but return 0 on
macOS (PCM output is implemented on Windows and wasm).
