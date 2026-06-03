NexaC Windows Build
===================

Cross-compile from Linux:
  cd WIN && make
  (Requires: apt install mingw-w64)

Or from project root:
  make win

Output: NexaC.exe in WIN/

On Windows (native):
  cd WIN && make
  Requires ONE of: clang (LLVM), g++ (MinGW), or gcc (MinGW)
  - LLVM/Clang: https://releases.llvm.org/ (add to PATH)
  - MinGW-w64: https://www.mingw-w64.org/ or MSYS2: https://www.msys2.org/
  NexaC auto-detects clang, g++, gcc and falls back if one fails.
