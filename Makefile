# Prefer clang++, fallback to g++ if clang not available
CXX := $(shell which clang++ 2>/dev/null || which g++ 2>/dev/null || echo "g++")
CXXFLAGS = -std=c++17 -O2
PREFIX ?= $(HOME)/.local

# Install build deps (macOS or Debian/Ubuntu/Raspberry Pi OS)
install-deps:
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		xcode-select -p >/dev/null 2>&1 || xcode-select --install; \
		echo "macOS: Apple Command Line Tools requested/available"; \
	else \
		sudo apt update && sudo apt install -y clang g++; \
		echo "Optional for Windows cross-compile: sudo apt install -y mingw-w64"; \
	fi

NexaC: NexaC.cpp include/Lexer.hpp include/Parser.hpp include/Transpiler.hpp include/Modules.hpp include/nexapkg.hpp
	$(CXX) $(CXXFLAGS) NexaC.cpp -o NexaC

install: NexaC
	install -d $(PREFIX)/bin
	install -m 755 NexaC $(PREFIX)/bin/NexaC
	ln -sf NexaC $(PREFIX)/bin/nexapkg
	ln -sf NexaC $(PREFIX)/bin/nexac

win: NexaC.cpp include/Lexer.hpp include/Parser.hpp include/Transpiler.hpp include/Modules.hpp
	$(MAKE) -C WIN

installer: NexaC
	./NexaC Installer/Installer.nxa -o installer

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
