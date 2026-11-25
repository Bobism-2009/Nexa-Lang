@echo off
rem Builds NexaC.exe from this folder, from cmd.exe:  build.cmd [--installer] [compiler]
rem The compiler is the argument, else clang++, else g++. A static NexaC.exe with the
rem Nexa icon in it: NexaC.rc, compiled with llvm-rc for an MSVC-target clang++ or with
rem windres for a MinGW one. With neither tool it says so and builds without the icon.
rem --installer: then Installer.exe too, built by that NexaC from Installer/main.nxa, with
rem the Nexa icon through --link (programs NexaC builds get no icon of their own).
rem From Git Bash, MSYS2, Linux or macOS, use build.sh.
rem (No goto labels: cmd mis-jumps to labels in a file with LF line endings.)
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "INSTALLER="
if /i "%~1"=="--installer" (
    set "INSTALLER=1"
    shift
)
set "CXX=%~1"
if "!CXX!"=="" for %%c in (clang++ g++) do if "!CXX!"=="" where %%c >nul 2>&1 && set "CXX=%%c"
if "!CXX!"=="" (
    echo build.cmd: no clang++ or g++ on PATH 1>&2
    exit /b 1
)

set "TARGET="
for /f "delims=" %%t in ('"!CXX!" -dumpmachine') do set "TARGET=%%t"
set "ICON="
echo !TARGET! | findstr /i "msvc" >nul
if !errorlevel! equ 0 (
    set "FLAGS=-std=c++17 -O2 -Wno-deprecated-declarations"
    where llvm-rc >nul 2>&1 && llvm-rc -fo NexaC.res NexaC.rc && set "ICON=NexaC.res"
) else (
    set "FLAGS=-std=c++17 -O2 -static -static-libgcc -static-libstdc++"
    for %%w in (windres llvm-windres) do if not defined ICON where %%w >nul 2>&1 && %%w NexaC.rc -O coff -o NexaC-icon.o && set "ICON=NexaC-icon.o"
)

if not defined ICON echo build.cmd: no llvm-rc or windres found, so NexaC.exe gets no icon 1>&2
rem call: the compiler may be a .cmd shim (WinGet and WindowsApps make them), and
rem a batch file run without call never comes back to this one.
call "!CXX!" !FLAGS! NexaC.cpp !ICON! -o NexaC.exe -ladvapi32
if errorlevel 1 exit /b 1
if defined ICON (echo build.cmd: built NexaC.exe with !CXX!, with the Nexa icon) else (echo build.cmd: built NexaC.exe with !CXX!)

rem "%~dp0NexaC.exe": the one just built, by its path -- cmd may not look in the current
rem folder, and would run an installed NexaC instead.
rem NexaC builds Windows programs with MinGW, so the installer's icon is a COFF object
rem from windres: NexaC's own when it was made that way, else a fresh one.
if not defined INSTALLER exit /b 0
set "OBJ="
if "!ICON!"=="NexaC-icon.o" set "OBJ=NexaC-icon.o"
if not defined OBJ for %%w in (windres llvm-windres) do if not defined OBJ where %%w >nul 2>&1 && %%w NexaC.rc -O coff -o NexaC-icon.o && set "OBJ=NexaC-icon.o"
if defined OBJ (
    "%~dp0NexaC.exe" Installer\main.nxa --link NexaC-icon.o -o Installer.exe >nul
) else (
    echo build.cmd: no windres found, so Installer.exe gets no icon 1>&2
    "%~dp0NexaC.exe" Installer\main.nxa -o Installer.exe >nul
)
if errorlevel 1 exit /b 1
if defined OBJ (echo build.cmd: built Installer.exe, with the Nexa icon) else (echo build.cmd: built Installer.exe)
