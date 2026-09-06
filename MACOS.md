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

`std/http` links against the macOS CoreFoundation and CFNetwork frameworks and
supports HTTP and HTTPS through the operating system networking stack.

## Graphics (`std/gfx`)

`#include <std/gfx>` opens a Cocoa window. NexaC compiles the generated code as
Objective-C++ (`-x objective-c++ -fobjc-arc`) and links `-framework Cocoa`
and `-framework ApplicationServices`. `gfx.key` uses the HID key state
(`CGEventSourceKeyState`).
