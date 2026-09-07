# Prefer clang++, fallback to g++ if clang not available
CXX := $(shell which clang++ 2>/dev/null || which g++ 2>/dev/null || echo "g++")
CXXFLAGS = -std=c++17 -O2
PREFIX ?= $(HOME)/.local

# Install *build* deps only (compile Nexa programs). Not wasm. Not runtime libs we ship.
# gfx statically embeds X11 — the -dev packages are required on this machine.
install-deps:
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		xcode-select -p >/dev/null 2>&1 || xcode-select --install; \
		echo "macOS: Apple Command Line Tools requested/available"; \
	elif command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y clang g++ build-essential git \
			libx11-dev libxcb1-dev libxau-dev libxdmcp-dev; \
		echo "Optional Windows cross-compile: sudo apt-get install -y mingw-w64"; \
	elif command -v dnf >/dev/null 2>&1; then \
		sudo dnf install -y clang gcc-c++ make git \
			libX11-devel libxcb-devel libXau-devel libXdmcp-devel; \
	elif command -v pacman >/dev/null 2>&1; then \
		sudo pacman -S --noconfirm --needed base-devel clang gcc git libx11 libxcb libxau libxdmcp; \
	elif command -v zypper >/dev/null 2>&1; then \
		sudo zypper --non-interactive install clang gcc-c++ make git \
			libX11-devel libxcb-devel libXau-devel libXdmcp-devel; \
	elif command -v apk >/dev/null 2>&1; then \
		sudo apk add clang g++ make git libx11-dev libxcb-dev libxau-dev libxdmcp-dev; \
	else \
		echo "NexaC: no supported package manager (apt, dnf, pacman, zypper, apk)"; \
		exit 1; \
	fi

NexaC: NexaC.cpp include/Lexer.hpp include/Parser.hpp include/Transpiler.hpp include/Modules.hpp include/nexapkg.hpp include/PlatformEmit.hpp
	$(CXX) $(CXXFLAGS) NexaC.cpp -o NexaC

install: NexaC
	install -d $(PREFIX)/bin
	install -m 755 NexaC $(PREFIX)/bin/NexaC
	ln -sf NexaC $(PREFIX)/bin/nexapkg
	ln -sf NexaC $(PREFIX)/bin/nexac

win: NexaC.cpp include/Lexer.hpp include/Parser.hpp include/Transpiler.hpp include/Modules.hpp
	$(MAKE) -C WIN

installer: NexaC
	./NexaC Installer/main.nxa -o installer

# Build Tests/dll_call_args_lib.nxa as Windows DLL (requires mingw-w64)
dll: NexaC
	./NexaC Tests/dll_call_args_lib.nxa --dll -o Tests/plugin.dll

# Build Tests/dll_call_args_lib.nxa as Linux .so
so: NexaC
	./NexaC Tests/dll_call_args_lib.nxa --shared -o Tests/plugin.so

# Build Tests/dll_call_args_lib.nxa as a macOS dynamic library
dylib: NexaC
	./NexaC Tests/dll_call_args_lib.nxa --shared -o Tests/plugin.dylib

# Build Tests/wasm_hello_test.nxa as WebAssembly (requires em++ or WASI-SDK)
wasm: NexaC
	./NexaC Tests/wasm_hello_test.nxa --wasm -o Tests/wasm_hello

# Build Examples/Number Guessing Game.nxa as Windows .exe (requires mingw-w64)
win-exe: NexaC
	./NexaC "Examples/Number Guessing Game.nxa" --win -o Tests/NumberGuessingGame.exe

# Build Tests/PkgTest (nexapkg package test)
pkgtest: NexaC
	cd Tests/PkgTest && ../../NexaC nexapkg install && ../../NexaC main.nxa -o pkgtest && ./pkgtest

clean:
	rm -f NexaC nexapkg
	$(MAKE) -C WIN clean

.PHONY: install install-deps win installer dll so dylib wasm win-exe clean pkgtest
