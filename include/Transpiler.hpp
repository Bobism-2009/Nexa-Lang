#pragma once

#include "Parser.hpp"
#include "Modules.hpp"
#include "PlatformEmit.hpp"
#include "StbImageRuntime.hpp"
#include <string>
#include <sstream>
#include <cstdio>
#include <map>
#include <set>
#include <functional>
#include <vector>
#include <cctype>
#include <limits>
#include <optional>

namespace nexa {

// Lines starting with #include in inline_cpp bodies are hoisted to file scope (C++ requires includes outside functions).
inline void collectInlineCppIncludeLines(const std::string& body, std::vector<std::string>& order, std::set<std::string>& seen) {
    std::istringstream ss(body);
    std::string line;
    while (std::getline(ss, line)) {
        size_t start = 0;
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) start++;
        if (start + 8 <= line.size() && line.compare(start, 8, "#include") == 0) {
            std::string inc = line.substr(start);
            if (seen.insert(inc).second) order.push_back(inc);
        }
    }
}

inline std::string stripInlineCppIncludeLines(const std::string& body) {
    std::ostringstream rest;
    std::istringstream ss(body);
    std::string line;
    while (std::getline(ss, line)) {
        size_t start = 0;
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) start++;
        if (start + 8 <= line.size() && line.compare(start, 8, "#include") == 0) continue;
        rest << line << "\n";
    }
    std::string r = rest.str();
    while (!r.empty() && (r.back() == '\n' || r.back() == '\r')) r.pop_back();
    return r;
}

inline void walkAstForInlineCppIncludes(const AstNode& n, std::vector<std::string>& order, std::set<std::string>& seen) {
    if (n.type == AstNode::Type::InlineCpp) collectInlineCppIncludeLines(n.value, order, seen);
    for (const AstNode& c : n.children) walkAstForInlineCppIncludes(c, order, seen);
}

// Decomposition of an integer literal's token text, as the lexer produced it.
// `base` is 16 for 0x/0X, 8 for a leading zero, 10 otherwise; `magnitude` is the
// digits' value with the sign stripped. `valid` is false for text that is not a
// plain integer literal, in which case nothing else has been filled in.
struct IntLiteralText {
    bool valid = false;
    bool negative = false;
    int base = 10;
    unsigned long long magnitude = 0;
};

inline IntLiteralText parseIntLiteralText(const std::string& text) {
    IntLiteralText lit;
    size_t i = 0;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
        lit.negative = (text[i] == '-');
        i++;
    }
    if (i >= text.size()) return lit;
    if (text[i] == '0' && i + 1 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
        lit.base = 16;
        i += 2;
        if (i >= text.size()) return lit;
    } else if (text[i] == '0' && i + 1 < text.size()) {
        lit.base = 8;
        i++;
    }
    const unsigned long long uMax = std::numeric_limits<unsigned long long>::max();
    for (size_t j = i; j < text.size(); ++j) {
        unsigned char c = static_cast<unsigned char>(text[j]);
        unsigned long long d;
        if (c >= '0' && c <= '9') d = static_cast<unsigned long long>(c - '0');
        else if (c >= 'a' && c <= 'f') d = static_cast<unsigned long long>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = static_cast<unsigned long long>(c - 'A' + 10);
        else return lit;  // not an integer literal (a float, a suffix, something else)
        if (d >= static_cast<unsigned long long>(lit.base)) return lit;
        const unsigned long long b = static_cast<unsigned long long>(lit.base);
        if (lit.magnitude > (uMax - d) / b) return lit;  // the lexer rejects these; leave the text alone
        lit.magnitude = lit.magnitude * b + d;
    }
    lit.valid = true;
    return lit;
}

// The C++ spelling of an integer literal the Nexa lexer accepted.
//
// C++ gives an unsuffixed *decimal* literal the first type that fits from
// int -> long -> long long, and that list has no unsigned fallback. A decimal
// above i64 max therefore has no type the standard offers: clang takes
// `unsigned long long` under protest and warns -Wimplicitly-unsigned-literal,
// pointing at machine-written C++ the user never typed. A ULL suffix names the
// type the compiler was already choosing, so the value and the type are
// unchanged and the warning goes away. Hex and octal need nothing — their
// candidate type lists already include the unsigned types.
//
// `-9223372036854775808` is the one negative that needs care. It is a negation
// applied to 9223372036854775808, which is itself past i64 max, so the literal
// went unsigned and the negation wrapped back to the same magnitude: emitted
// verbatim, `-9223372036854775808 < 0` was *false*. Building i64 min by
// subtraction keeps the expression `long long` and the comparison honest.
inline std::string emitIntLiteral(const std::string& text) {
    const IntLiteralText lit = parseIntLiteralText(text);
    if (!lit.valid || lit.base != 10) return text;
    const unsigned long long i64Max = 9223372036854775807ULL;
    if (lit.magnitude <= i64Max) return text;
    if (!lit.negative) return text + "ULL";
    if (lit.magnitude == i64Max + 1) return "(-9223372036854775807LL - 1)";
    return text;  // out of range for i64 either way; the lexer already refused it
}

// Sentinel comment a debug build emits around every statement; a final pass rewrites each one
// into a real `#line` directive so the debugger maps generated C++ back to the .nxa source.
//
// Why a marker instead of writing `#line` directly: the snap-back directive has to name the line
// number it sits on, and two later passes (duplicate-#include removal, inactive platform guards)
// delete whole lines. Any number computed during emission would be stale by the time the text is
// final. Emitting a placeholder and numbering it last is the only order that can be correct.
//
// A marker is only honoured when it is the whole line (indentation aside), because `#line` is only
// legal as the first token on a line. That also makes the failure mode safe: a marker that somehow
// ends up spliced mid-line stays an ordinary comment instead of becoming a syntax error.
inline const std::string& nexaLineMark() {
    static const std::string mark = "//__nexa_line_mark__";
    return mark;
}

// Quote a path for a `#line` directive. Only backslash and double quote need escaping, which is
// what makes Windows paths safe here.
inline std::string nexaLineFileLiteral(const std::string& path) {
    std::string q = "\"";
    for (char c : path) {
        if (c == '\\' || c == '"') q += '\\';
        q += c;
    }
    q += '"';
    return q;
}

// Converts Nexa AST to C++ source code
class Transpiler {
public:
    Transpiler(const std::vector<AstNode>& ast, const Modules& modules, bool preserveNames = false, bool buildDll = false,
              CppTarget target = hostCppTarget(), bool lineDirectives = false, const std::string& generatedCppPath = "")
        : ast_(ast), modules_(modules), preserveNames_(preserveNames), buildDll_(buildDll), target_(target),
          lineDirectives_(lineDirectives), generatedCppPath_(generatedCppPath) {}

    // Valid after transpile(): which C++ features the generated code actually uses.
    // Lets the build step drop exception/RTTI machinery when nothing needs it.
    const Modules::CppUsage& cppUsage() const { return cppUsage_; }

    std::string transpile() {
        std::ostringstream out;

        if (target_ == CppTarget::Wasm) {
            out << "#define NEXA_WASM 1\n";
        }

        Modules::CppUsage& cppUsage = cppUsage_;
        cppUsage = Modules::CppUsage{};
        std::function<void(const AstNode&)> detectCppUsage = [&](const AstNode& n) {
            if (n.initFromDllLoad) cppUsage.dll = true;
            if (n.initFromReadln) cppUsage.ioReadln = true;
            switch (n.type) {
                case AstNode::Type::IoPrint:
                case AstNode::Type::IoPrintln: cppUsage.ioPrint = true; break;
                case AstNode::Type::IoFlush: cppUsage.ioFlush = true; break;
                case AstNode::Type::IoReadln: cppUsage.ioReadln = true; break;
                case AstNode::Type::IoGetline: cppUsage.ioGetline = true; break;
                case AstNode::Type::IoToInt: cppUsage.ioToInt = true; break;
                case AstNode::Type::OsSystem: cppUsage.osSystem = true; break;
                case AstNode::Type::OsExec: cppUsage.osExec = true; break;
                case AstNode::Type::OsGetenv: cppUsage.osGetenv = true; break;
                case AstNode::Type::OsPlatform: cppUsage.osPlatform = true; break;
                case AstNode::Type::OsExeDir: cppUsage.osExeDir = true; break;
                case AstNode::Type::OsGetProcessId: cppUsage.osGetProcessId = true; break;
                case AstNode::Type::OsHideConsoleWindow:
                case AstNode::Type::OsShowConsoleWindow:
                case AstNode::Type::OsMinimizeConsoleWindow:
                case AstNode::Type::OsMaximizeConsoleWindow: cppUsage.osWindowControl = true; break;
                case AstNode::Type::OsMessageBox: cppUsage.osMessageBox = true; break;
                case AstNode::Type::OsLock: cppUsage.osLock = true; break;
                case AstNode::Type::OsShutdown: cppUsage.osShutdown = true; break;
                case AstNode::Type::OsReboot: cppUsage.osReboot = true; break;
                case AstNode::Type::OsSuspend: cppUsage.osSuspend = true; break;
                case AstNode::Type::OsLogout: cppUsage.osLogout = true; break;
                case AstNode::Type::OsSetVolume:
                case AstNode::Type::OsGetVolume:
                case AstNode::Type::OsMute:
                case AstNode::Type::OsUnmute:
                case AstNode::Type::OsToggleMute: cppUsage.osAudio = true; break;
                case AstNode::Type::OsSetBrightness:
                case AstNode::Type::OsGetBrightness: cppUsage.osBrightness = true; break;
                case AstNode::Type::OsClipSet:
                case AstNode::Type::OsClipGet: cppUsage.osClipboard = true; break;
                case AstNode::Type::OsType: cppUsage.osType = true; break;
                case AstNode::Type::OsNotify:
                case AstNode::Type::OsOpen: cppUsage.osDesktop = true; break;
                case AstNode::Type::OsLoad: cppUsage.osLoad = true; break;
                case AstNode::Type::OsSave: cppUsage.osSave = true; break;
                case AstNode::Type::OsPlay: cppUsage.osPlay = true; break;
                case AstNode::Type::OsSpawn:
                case AstNode::Type::OsWait:
                case AstNode::Type::OsKill: cppUsage.osSpawn = true; break;
                case AstNode::Type::OsTempDir: cppUsage.osTempDir = true; break;
                case AstNode::Type::OsArch: cppUsage.osArch = true; break;
                case AstNode::Type::OsCpuCount: cppUsage.osCpuCount = true; break;
                case AstNode::Type::OsWhich: cppUsage.osWhich = true; break;
                case AstNode::Type::OsUnsetenv: cppUsage.osSetenv = true; break;
                case AstNode::Type::OsExecutable: cppUsage.osExeDir = true; break;
                case AstNode::Type::OsCwd:
                case AstNode::Type::OsChdir: cppUsage.osCwd = true; break;
                case AstNode::Type::OsInfo: cppUsage.osInfo = true; break;
                case AstNode::Type::OsExit: cppUsage.osExit = true; break;
                case AstNode::Type::OsHostname: cppUsage.osHostname = true; break;
                case AstNode::Type::OsUsername: cppUsage.osUsername = true; break;
                case AstNode::Type::OsHome: cppUsage.osHome = true; break;
                case AstNode::Type::OsSetenv: cppUsage.osSetenv = true; break;
                case AstNode::Type::OsGrepKeys: cppUsage.osGrepKeys = true; break;
                case AstNode::Type::OsKeyPressed: cppUsage.osKeyPressed = true; break;
                case AstNode::Type::FileRead:
                    cppUsage.fileRead = true;
                    break;
                case AstNode::Type::FileWrite:
                case AstNode::Type::FileAppend:
                    cppUsage.fileWrite = true;
                    break;
                case AstNode::Type::FileExists:
                case AstNode::Type::FileMkdir:
                case AstNode::Type::FileCall:
                    cppUsage.fileFs = true;
                    break;
                case AstNode::Type::RandomInt:
                case AstNode::Type::RandomSeed: cppUsage.random = true; break;
                case AstNode::Type::MathCall: cppUsage.math = true; break;
                case AstNode::Type::CryptoCall: {
                    const std::string& fn = n.value;
                    const bool hexLit = (fn == "hex_encode" || fn == "hex_decode") &&
                        !n.children.empty() && cryptoArgIsLiteral(n.children[0]);
                    if (!hexLit) cppUsage.crypto = true;
                    if ((fn == "hex_encode" || fn == "hex_decode") && !hexLit) cppUsage.cryptoHex = true;
                    else if (fn == "xor") cppUsage.cryptoXor = true;
                    else if (fn == "base64_encode" || fn == "base64_decode") cppUsage.cryptoBase64 = true;
                    else if (fn == "sha256") {
                        cppUsage.cryptoSha256 = true;
                        cppUsage.cryptoHex = true;
                    } else if (fn == "sha1") {
                        cppUsage.cryptoSha1 = true;
                        cppUsage.cryptoHex = true;
                    } else if (fn == "hmac_sha256") {
                        cppUsage.cryptoHmac = true;
                        cppUsage.cryptoSha256 = true;
                        cppUsage.cryptoHex = true;
                    } else if (fn == "random_bytes") cppUsage.cryptoRandom = true;
                    break;
                }
                case AstNode::Type::HttpCall:
                    cppUsage.http = true;
                    cppUsage.result = true;
                    if (httpVerbIsServer(n.value)) cppUsage.httpServer = true;
                    else if (n.value == "request") cppUsage.httpResponse = true;
                    else cppUsage.httpSimple = true;
                    break;
                case AstNode::Type::TcpCall:
                    cppUsage.tcp = true;
                    if (n.value == "connect") cppUsage.tcpConnect = true;
                    else if (n.value == "listen" || n.value == "accept") cppUsage.tcpListen = true;
                    break;
                case AstNode::Type::UdpCall:
                    cppUsage.udp = true;
                    if (n.value == "sender" || n.value == "sender_port") cppUsage.udpSender = true;
                    break;
                case AstNode::Type::GfxCall:
                    cppUsage.gfx = true;
                    noteGfxUsage(n, cppUsage);
                    break;
                case AstNode::Type::Gfx3dCall: cppUsage.gfx3d = true; break;
                case AstNode::Type::JsonCall: cppUsage.json = true; break;
                case AstNode::Type::ResultMake: cppUsage.result = true; break;
                case AstNode::Type::StrMethod:
                    if (!tryFoldStrMethodToExpr(n, nullptr)) cppUsage.str = true;
                    break;
                case AstNode::Type::TimeSleep: cppUsage.timeSleep = true; break;
                case AstNode::Type::TimeNowMs: cppUsage.timeChrono = true; break;
                case AstNode::Type::ThreadSpawn:
                    cppUsage.thread = true;
                    if (!n.children.empty()) cppUsage.threadLambda = true;
                    break;
                case AstNode::Type::ThreadJoin: cppUsage.thread = true; break;
                case AstNode::Type::ThreadWorker:
                case AstNode::Type::ThreadWorkerJoin:
                    cppUsage.thread = true;
                    cppUsage.threadWorker = true;
                    break;
                case AstNode::Type::ThreadRun:
                    cppUsage.thread = true;
                    cppUsage.threadWorker = true;
                    if (n.children.size() > 1 && !n.children[1].children.empty()) cppUsage.threadLambda = true;
                    break;
                case AstNode::Type::DllLoad:
                case AstNode::Type::DllCall: cppUsage.dll = true; break;
                case AstNode::Type::FnCall:
                    if (n.initValue == "." && n.value == "value") cppUsage.exceptions = true;
                    break;
                case AstNode::Type::TryCatch:
                case AstNode::Type::Throw: cppUsage.exceptions = true; break;
                default: break;
            }
            if (n.declType == "json" || n.fnReturnType == "json" ||
                n.declType.find("json") != std::string::npos ||
                n.fnReturnType.find("json") != std::string::npos) {
                cppUsage.json = true;
            }
            if (nexaIsResultType(n.declType) || nexaIsResultType(n.fnReturnType) ||
                n.declType.find("Result[") != std::string::npos ||
                n.fnReturnType.find("Result[") != std::string::npos) {
                cppUsage.result = true;
            }
            for (const std::string& pt : n.paramTypes) {
                if (pt == "json" || pt.find("json") != std::string::npos) cppUsage.json = true;
                if (nexaIsResultType(pt) || pt.find("Result[") != std::string::npos) cppUsage.result = true;
            }
            for (const AstNode& c : n.children) detectCppUsage(c);
        };
        for (const AstNode& node : ast_) detectCppUsage(node);

        // C++ includes from enabled modules
        if (buildDll_) {
            out << "#ifdef _WIN32\n";
            out << "#define NEXA_EXPORT __declspec(dllexport)\n";
            out << "#else\n";
            out << "#define NEXA_EXPORT __attribute__((visibility(\"default\")))\n";
            out << "#endif\n\n";
        }
        std::string moduleCppIncludes = modules_.getCppIncludes(cppUsage);
        out << moduleCppIncludes;
        std::vector<std::string> inlineCppHoisted;
        std::set<std::string> inlineCppSeen;
        for (const AstNode& node : ast_) walkAstForInlineCppIncludes(node, inlineCppHoisted, inlineCppSeen);
        for (const std::string& inc : inlineCppHoisted) out << inc << "\n";
        structFields_.clear();
        structFieldOrder_.clear();
        structCppNames_.clear();
        enumCppNames_.clear();
        enumVariants_.clear();
        enumFirstVariant_.clear();
        std::set<std::string> typeNames;
        int structId = 0;
        int enumId = 0;
        if (modules_.hasHttp()) {
            // std/http's own structs: known like any struct, but defined by the
            // runtime, so they are seeded here and never emitted below. The
            // names belong to the module -- a program that redefines one lands
            // in the duplicate-type-name check with everything else.
            structFields_["HttpResponse"]["status"] = "int";
            structFields_["HttpResponse"]["body"] = "string";
            structFields_["HttpResponse"]["headers"] = "[]string";
            structFieldOrder_["HttpResponse"] = { "status", "body", "headers" };
            structCppNames_["HttpResponse"] = "__nexa_http_response";
            typeNames.insert("HttpResponse");
            structFields_["HttpServer"]["port"] = "int";
            structFields_["HttpServer"]["socket"] = "int";
            structFieldOrder_["HttpServer"] = { "port", "socket" };
            structCppNames_["HttpServer"] = "__nexa_http_server";
            typeNames.insert("HttpServer");
            structFields_["HttpRequest"]["method"] = "string";
            structFields_["HttpRequest"]["path"] = "string";
            structFields_["HttpRequest"]["body"] = "string";
            structFields_["HttpRequest"]["headers"] = "[]string";
            structFields_["HttpRequest"]["socket"] = "int";
            structFieldOrder_["HttpRequest"] = { "method", "path", "body", "headers", "socket" };
            // __nexa_http_request is already the client call http.request, so
            // the struct a server accepts is __nexa_http_incoming: in C++ a
            // function of that name would hide a struct of that name.
            structCppNames_["HttpRequest"] = "__nexa_http_incoming";
            typeNames.insert("HttpRequest");
        }
        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::StructDef) {
                if (!typeNames.insert(node.value).second) {
                    throw std::runtime_error("Duplicate type name '" + node.value + "'");
                }
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    structFields_[node.value][node.paramNames[i]] = node.paramTypes[i];
                }
                structFieldOrder_[node.value] = node.paramNames;
                for (const AstNode& meth : node.children) {
                    if (meth.type == AstNode::Type::Function) {
                        structMethods_[node.value][meth.value] = &meth;
                    }
                }
                structCppNames_[node.value] = preserveNames_ ? node.value : ("__nexa_S" + std::to_string(structId++));
            } else if (node.type == AstNode::Type::EnumDef) {
                if (!typeNames.insert(node.value).second) {
                    throw std::runtime_error("Duplicate type name '" + node.value + "'");
                }
                for (const std::string& v : node.paramNames) {
                    enumVariants_[node.value].insert(v);
                }
                enumFirstVariant_[node.value] = node.paramNames[0];
                enumCppNames_[node.value] = preserveNames_ ? node.value : ("__nexa_E" + std::to_string(enumId++));
            }
        }
        bool needsString = false;
        bool needsCstr = false;
        bool needsCstdlib = false;
        bool needsCstddef = false;
        for (const auto& sn : structFields_) {
            for (const auto& fn : sn.second) {
                if (fn.second == "string") {
                    needsString = true;
                    break;
                }
            }
            if (needsString) break;
        }
        std::function<void(const AstNode&)> checkNeedsString = [&](const AstNode& n) {
            if (n.type == AstNode::Type::ExprCast) {
                if (n.value == "string") needsString = true;
                if (n.value == "float" || nexaIsNumericIntType(n.value)) {
                    needsString = true;
                    needsCstdlib = true;
                }
            }
            if (n.type == AstNode::Type::Variable && (n.initUninitialized || n.initFromReadln || n.initFromFileRead || (!n.initIsInt && !n.initFromDllLoad && n.children.empty()))) needsString = true;
            if (n.type == AstNode::Type::Variable && !n.children.empty() && exprProducesString(n.children[0])) needsString = true;
            if (n.type == AstNode::Type::FnCall && modules_.hasCppHeader() && libcHeaderReturnHint(n.value) == "string") {
                needsString = true;
                needsCstr = true;
            }
            if (n.type == AstNode::Type::Variable) {
                for (const auto& c : n.children) checkNeedsString(c);
            }
            if (n.type == AstNode::Type::Variable && !n.children.empty() && n.children[0].type == AstNode::Type::ExprArrayLiteral) {
                for (const auto& c : n.children[0].children) { if (exprProducesString(c)) { needsString = true; break; } }
            }
            if (n.type == AstNode::Type::IoPrintln || n.type == AstNode::Type::IoPrint) {
                for (const AstNode& a : n.children) {
                    if (exprProducesString(a)) { needsString = true; }
                    checkNeedsString(a);
                }
            }
            if (n.type == AstNode::Type::OsSystem && !n.children.empty() && exprProducesString(n.children[0])) needsString = true;
            if (n.type == AstNode::Type::Throw && !n.children.empty() && exprProducesString(n.children[0])) needsString = true;
            if (n.type == AstNode::Type::While && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsString(c); }
            if (n.type == AstNode::Type::For && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsString(c); }
            if (n.type == AstNode::Type::ForIn) {
                needsString = true;
                if (n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsString(c); }
            }
            if (n.type == AstNode::Type::IfElse) { for (size_t i = 1; i < n.children.size(); i++) { for (const auto& c : n.children[i].children) checkNeedsString(c); } }
            if (n.type == AstNode::Type::TryCatch && n.children.size() >= 2) {
                for (const auto& c : n.children[0].children) checkNeedsString(c);
                for (const auto& c : n.children[1].children) checkNeedsString(c);
            }
            if (n.type == AstNode::Type::Block) { for (const auto& c : n.children) checkNeedsString(c); }
        };
        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::MainFunction || node.type == AstNode::Type::Function) {
                for (const AstNode& c : node.children) checkNeedsString(c);
                for (const std::string& pt : node.paramTypes) {
                    if (pt == "string") needsString = true;
                }
                if (node.type == AstNode::Type::Function && node.fnReturnType == "string") needsString = true;
            }
            if (node.type == AstNode::Type::Variable && (node.declType == "string" || (node.initUninitialized && !nexaIsIntegerType(node.declType) && node.declType != "bool" && node.declType != "float" && !isPointerType(node.declType) && !isStructDeclType(node.declType) && !isEnumDeclType(node.declType)) || node.initFromFileRead || (!node.initIsInt && !node.initFromDllLoad && node.children.empty()))) needsString = true;
            if (node.type == AstNode::Type::Variable && !node.children.empty() && exprProducesString(node.children[0])) needsString = true;
            if (node.type == AstNode::Type::Variable && !node.children.empty() && node.children[0].type == AstNode::Type::ExprArrayLiteral) {
                for (const auto& c : node.children[0].children) { if (exprProducesString(c)) { needsString = true; break; } }
            }
        }
        bool needsVector = false;
        std::function<void(const AstNode&)> checkNeedsVector = [&](const AstNode& n) {
            if (n.type == AstNode::Type::OsInfo && n.value == "environ") {
                needsVector = true;
                needsString = true;
            }
            if (n.type == AstNode::Type::Variable && n.initFromArray) needsVector = true;
            if (n.type == AstNode::Type::Variable && !n.children.empty() && n.children[0].type == AstNode::Type::ExprArrayLiteral) needsVector = true;
            if (n.type == AstNode::Type::ExprArrayLiteral || n.type == AstNode::Type::ExprArrayIndex || n.type == AstNode::Type::AssnIndex) needsVector = true;
            if (n.type == AstNode::Type::While && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsVector(c); }
            if (n.type == AstNode::Type::For && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsVector(c); }
            if (n.type == AstNode::Type::ForIn) needsVector = true;
            if (n.type == AstNode::Type::ForIn && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsVector(c); }
            if (n.type == AstNode::Type::IfElse) { for (size_t i = 1; i < n.children.size(); i++) { for (const auto& c : n.children[i].children) checkNeedsVector(c); } }
            if (n.type == AstNode::Type::TryCatch && n.children.size() >= 2) {
                for (const auto& c : n.children[0].children) checkNeedsVector(c);
                for (const auto& c : n.children[1].children) checkNeedsVector(c);
            }
            if (n.type == AstNode::Type::Block) { for (const auto& c : n.children) checkNeedsVector(c); }
        };
        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::MainFunction || node.type == AstNode::Type::Function) {
                for (const auto& c : node.children) checkNeedsVector(c);
            }
            if (node.type == AstNode::Type::Variable && node.initFromArray) needsVector = true;
            if (node.type == AstNode::Type::Variable && !node.children.empty() && node.children[0].type == AstNode::Type::ExprArrayLiteral) needsVector = true;
            if (node.type == AstNode::Type::MainFunction && node.paramNames.size() == 1 && node.paramTypes.size() == 1 &&
                node.paramTypes[0] == "[]string") {
                needsVector = true;
                needsString = true;
            }
        }
        auto typeMentionsSizeT = [](const std::string& t) {
            size_t i = 0;
            while (i < t.size() && t[i] == '*') i++;
            return t.compare(i, std::string::npos, "size_t") == 0;
        };
        std::function<void(const AstNode&)> checkNeedsCstddef = [&](const AstNode& n) {
            if (typeMentionsSizeT(n.declType) || typeMentionsSizeT(n.fnReturnType) || typeMentionsSizeT(n.value)) {
                needsCstddef = true;
            }
            if (n.type == AstNode::Type::OsInfo && (n.value == "total_mem" || n.value == "avail_mem")) {
                needsCstddef = true;
            }
            for (const std::string& pt : n.paramTypes) {
                if (typeMentionsSizeT(pt)) needsCstddef = true;
            }
            if (needsCstddef) return;
            for (const AstNode& c : n.children) checkNeedsCstddef(c);
        };
        for (const AstNode& node : ast_) {
            checkNeedsCstddef(node);
            if (needsCstddef) break;
        }
        bool needsMap = false;
        bool needsFunctional = false;
        std::function<void(const std::string&)> noteContainerType = [&](const std::string& t) {
            if (nexaIsSliceType(t)) {
                needsVector = true;
                noteContainerType(nexaSliceElem(t));
            } else if (nexaIsMapType(t)) {
                needsMap = true;
                std::string k, v;
                if (nexaSplitMapType(t, k, v)) {
                    noteContainerType(k);
                    noteContainerType(v);
                }
            } else if (nexaIsFnType(t)) {
                needsFunctional = true;
                std::vector<std::string> params;
                std::string ret;
                if (nexaSplitFnType(t, params, ret)) {
                    for (const std::string& p : params) noteContainerType(p);
                    noteContainerType(ret);
                }
            } else if (nexaIsResultType(t)) {
                noteContainerType(nexaResultInner(t));
            } else if (t == "string" || (t.size() >= 7 && t.compare(0, 7, "[]strin") == 0)) {
                needsString = true;
            }
        };
        std::function<void(const AstNode&)> checkNeedsContainers = [&](const AstNode& n) {
            noteContainerType(n.declType);
            noteContainerType(n.fnReturnType);
            for (const std::string& pt : n.paramTypes) noteContainerType(pt);
            if (n.type == AstNode::Type::ExprLambda) needsFunctional = true;
            if (n.type == AstNode::Type::FnCall && (n.value == "push" || n.value == "pop" || n.value == "clear" ||
                    n.value == "insert" || n.value == "keys" || n.value == "values")) {
                needsVector = true;
            }
            // The slice algorithms, gated on `.` because unlike push/pop these share
            // their names with plausible user functions -- a program with its own
            // fn min(a, b) has no slice in it and should not gain <vector>.
            if (n.type == AstNode::Type::FnCall && n.initValue == "." &&
                    (n.value == "sort" || n.value == "sort_desc" || n.value == "reverse" ||
                     n.value == "join" || n.value == "min" || n.value == "max" || n.value == "sum")) {
                needsVector = true;
                // join builds a std::string out of a []string; every other one hands
                // back an element or nothing, so it is the only one of these that can
                // introduce a string to a program that had none.
                if (n.value == "join") needsString = true;
            }
            if (n.type == AstNode::Type::ExprSlice) needsVector = true;
            // checkNeedsVector above only walks statement positions, so container uses
            // nested inside an expression (io.println(s.split(v)[0])) land here instead.
            if (n.type == AstNode::Type::ExprArrayLiteral || n.type == AstNode::Type::ExprArrayIndex ||
                n.type == AstNode::Type::AssnIndex || n.type == AstNode::Type::ForIn) {
                needsVector = true;
            }
            if (n.type == AstNode::Type::StrMethod && n.value == "split") {
                needsVector = true;
                needsString = true;
            }
            // len()/slicing over a folded string literal promotes it with std::string(...).
            if (n.type == AstNode::Type::ExprLen || n.type == AstNode::Type::ExprSlice ||
                (n.type == AstNode::Type::ExprAdd && exprProducesString(n))) {
                needsString = true;
            }
            // So does a comparison whose two operands both emit as bare C++ string literals:
            // emitComparison promotes the left one rather than compare two const char*
            // addresses. This runs before function signatures are registered, so it stays
            // purely syntactic -- inferring a type here would throw on a forward call.
            if (isComparisonNodeType(n.type) && n.children.size() >= 2 &&
                mayEmitBareCppStringLiteral(n.children[0]) && mayEmitBareCppStringLiteral(n.children[1]) &&
                !(isPlainStringLiteralChain(n.children[0]) && isPlainStringLiteralChain(n.children[1]))) {
                needsString = true;
            }
            if (n.type == AstNode::Type::FnCall && (n.value == "has" || n.value == "remove" ||
                    n.value == "keys" || n.value == "values")) {
                needsMap = true;
            }
            for (const AstNode& c : n.children) checkNeedsContainers(c);
        };
        for (const AstNode& node : ast_) checkNeedsContainers(node);
        if (needsString && moduleCppIncludes.find("#include <string>\n") == std::string::npos) out << "#include <string>\n";
        if (needsVector && moduleCppIncludes.find("#include <vector>\n") == std::string::npos) out << "#include <vector>\n";
        if (needsMap && moduleCppIncludes.find("#include <map>\n") == std::string::npos) out << "#include <map>\n";
        if (needsFunctional && moduleCppIncludes.find("#include <functional>\n") == std::string::npos) out << "#include <functional>\n";
        // Float->string helper that matches io.print's "%g" formatting (e.g. 12.0 -> "12", not "12.000000").
        if (needsString && moduleCppIncludes.find("#include <cstdio>\n") == std::string::npos) out << "#include <cstdio>\n";
        if (needsCstdlib && moduleCppIncludes.find("#include <cstdlib>\n") == std::string::npos) out << "#include <cstdlib>\n";
        if (needsCstddef && moduleCppIncludes.find("#include <cstddef>\n") == std::string::npos) out << "#include <cstddef>\n";
        if (!moduleCppIncludes.empty() || !inlineCppHoisted.empty() || needsString || needsVector || needsMap || needsFunctional || needsCstdlib || needsCstddef) out << "\n";
        if (needsString) out << "[[maybe_unused]] static std::string __nexa_f2s(double __v) { char __b[32]; std::snprintf(__b, sizeof(__b), \"%g\", __v); return std::string(__b); }\n\n";
        if (needsCstr) out << "[[maybe_unused]] static std::string __nexa_cstr(const char* __p) { return __p ? std::string(__p) : std::string(); }\n\n";

        bool wroteUserCppHeaders = false;
        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::CppHeaderInclude) {
                out << node.value << "\n";
                wroteUserCppHeaders = true;
            }
        }
        if (wroteUserCppHeaders) out << "\n";

        // Decide, once, which parameters can be bound by const reference instead of copied.
        // This has to happen before the first signature goes out, which is a struct's
        // in-body method declaration: a declaration and its out-of-line definition have to
        // agree, and C++ says so with "does not match any declaration".
        analyzeParamPassing();

        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::StructDef) {
                const std::string& nexaName = node.value;
                std::string cppName = structCppNames_.at(nexaName);
                out << "struct " << cppName << " {\n";
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    out << "    " << nexaTypeToCpp(node.paramTypes[i]) << " " << node.paramNames[i] << ";\n";
                }
                out << "    bool operator==(const " << cppName << "& __o) const {\n";
                if (node.paramNames.empty()) {
                    out << "        return true;\n";
                } else {
                    out << "        return ";
                    for (size_t i = 0; i < node.paramNames.size(); i++) {
                        if (i) out << " && ";
                        out << node.paramNames[i] << " == __o." << node.paramNames[i];
                    }
                    out << ";\n";
                }
                out << "    }\n";
                out << "    bool operator!=(const " << cppName << "& __o) const { return !(*this == __o); }\n";
                for (const AstNode& meth : node.children) {
                    if (meth.type == AstNode::Type::Function) emitStructMethodDecl(out, meth);
                }
                out << "};\n\n";
            } else if (node.type == AstNode::Type::EnumDef) {
                const std::string& nexaName = node.value;
                std::string cppName = enumCppNames_.at(nexaName);
                out << "enum class " << cppName << " : int {\n";
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    out << "    " << node.paramNames[i];
                    if (i + 1 < node.paramNames.size()) out << ",";
                    out << "\n";
                }
                out << "};\n\n";
            }
        }

        varStructScopes_.clear();
        varStructScopes_.push_back({});
        buildFnOverloadTableAndInitGlobalNexaDecl();
        // Give every empty `[]` the element type of the place it was written, so
        // that the checks below and codegen both see []string rather than []int.
        stampEmptySliceLiterals();
        // Diagnose undefined names, redeclarations, bad initializers, unknown fields and
        // stray break/continue here, while a Nexa file and line are still attached to the
        // AST. Anything that survives this is the C++ backend's problem.
        checkSemantics();
        for (const auto& kv : globalNexaDecl_) {
            if (isStructDeclType(kv.second)) {
                varStructScopes_[0][kv.first] = structNameFromDecl(kv.second);
            }
        }

        auto fnNameInitOnly = [&](const std::string& name) -> std::string {
            if (preserveNames_) return name;
            return "__nexa_fn_" + std::to_string(slotForZeroArgFunctionNamed(name));
        };

        // Emit top-level items in source order: globals, file-scope inline_cpp!, functions, main
        std::map<std::string, std::string> globalVarMap;
        std::map<std::string, bool> globalVarIsString;
        std::map<std::string, bool> globalVarIsFloat;
        std::map<std::string, bool> globalVarIsChar;
        std::map<std::string, bool> globalVarIsBool;
        std::map<std::string, bool> globalVarIsEnum;
        std::map<std::string, bool> globalVarIsArray;
        std::map<std::string, bool> globalVarIsConst;
        int globalIdx = 0;
        bool justEmittedGlobal = false;
        bool wroteMain = false;

        // Emit forward declarations for every function so call sites can appear before
        // the function body. Without this, NexaC emits functions in source order with
        // no prototypes, which forces users to topologically order their definitions.
        {
            bool wroteAnyProto = false;
            for (size_t astIdx = 0; astIdx < ast_.size(); ++astIdx) {
                const AstNode& node = ast_[astIdx];
                if (node.type != AstNode::Type::Function) continue;
                if (node.isExtern) {
                    emitExternDecl(out, node);
                    wroteAnyProto = true;
                    continue;
                }
                std::string cppName = cppFnNameForAstIndex(astIdx);
                bool hasValRet = false, hasVoidRet = false;
                stmtsClassifyReturns(node.children, hasValRet, hasVoidRet);
                std::string retCpp;
                if (!node.fnReturnType.empty()) {
                    if (node.fnReturnType == "void") retCpp = "void";
                    else                              retCpp = nexaTypeToCpp(node.fnReturnType);
                } else {
                    retCpp = hasValRet ? "int" : "void";
                }
                out << (buildDll_ ? "extern \"C\" NEXA_EXPORT " : "static ") << retCpp << " " << cppName << "(";
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    if (i > 0) out << ", ";
                    std::string nexaT = "int";
                    if (i < node.paramTypes.size() && !node.paramTypes[i].empty()) {
                        nexaT = canonicalParamType(node, i);
                    }
                    out << (buildDll_ ? dllExportParamCpp(nexaT) : paramSigCpp(node, i));
                }
                out << ");\n";
                wroteAnyProto = true;
            }
            if (wroteAnyProto) out << "\n";
        }
        for (const AstNode& node : ast_) {
            if (node.type != AstNode::Type::StructDef) continue;
            std::string cppName = structCppNames_.at(node.value);
            for (const AstNode& meth : node.children) {
                if (meth.type == AstNode::Type::Function) emitStructMethodBody(out, meth, cppName);
            }
        }

        for (size_t astIdx = 0; astIdx < ast_.size(); ++astIdx) {
            const AstNode& node = ast_[astIdx];
            if (node.type == AstNode::Type::Include || node.type == AstNode::Type::CppHeaderInclude) {
                continue;
            }
            if (node.type != AstNode::Type::Variable) {
                if (justEmittedGlobal) {
                    out << "\n";
                    justEmittedGlobal = false;
                }
            }
            if (node.type != AstNode::Type::Variable && node.type != AstNode::Type::InlineCpp &&
                node.type != AstNode::Type::Function && node.type != AstNode::Type::MainFunction &&
                node.type != AstNode::Type::StructDef && node.type != AstNode::Type::EnumDef) {
                continue;
            }
            if (node.type == AstNode::Type::StructDef || node.type == AstNode::Type::EnumDef) {
                continue;
            }
            if (node.type == AstNode::Type::InlineCpp) {
                emitInlineCppFileScope(out, stripInlineCppIncludeLines(node.value));
                out << "\n";
                continue;
            }
            if (node.type == AstNode::Type::Function) {
                if (node.isExtern) continue;
                std::string cppName = cppFnNameForAstIndex(astIdx);
                bool hasValRet = false, hasVoidRet = false;
                stmtsClassifyReturns(node.children, hasValRet, hasVoidRet);
                if (hasValRet && hasVoidRet) {
                    throw std::runtime_error("function '" + node.value + "' mixes 'return;' and 'return expr;'");
                }
                bool voidFn = false;
                std::string retCpp;
                if (!node.fnReturnType.empty()) {
                    if (node.fnReturnType == "void") {
                        if (hasValRet) {
                            throw std::runtime_error("cannot return a value from void function '" + node.value + "'");
                        }
                        voidFn = true;
                        retCpp = "void";
                    } else {
                        if (hasVoidRet) {
                            throw std::runtime_error(
                                "return with no value in function '" + node.value + "' that returns " + node.fnReturnType);
                        }
                        voidFn = false;
                        retCpp = nexaTypeToCpp(node.fnReturnType);
                    }
                } else {
                    voidFn = !hasValRet;
                    retCpp = hasValRet ? "int" : "void";
                }
                out << (buildDll_ ? "extern \"C\" NEXA_EXPORT " : "static ") << retCpp << " " << cppName << "(";
                std::map<std::string, std::string> varMap = globalVarMap;
                int varIdx = 0;
                std::vector<std::pair<std::string, std::string>> dllStringParams;
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    if (i > 0) out << ", ";
                    std::string pname = preserveNames_ ? node.paramNames[i] : ("__nexa_param_" + std::to_string(i));
                    std::string nexaT = "int";
                    if (i < node.paramTypes.size() && !node.paramTypes[i].empty()) {
                        nexaT = canonicalParamType(node, i);
                    }
                    if (buildDll_ && nexaT == "string") {
                        std::string cname = "__nexa_c_" + pname;
                        out << "const char* " << cname;
                        dllStringParams.push_back({pname, cname});
                    } else {
                        out << paramSigCpp(node, i) << " " << pname;
                    }
                    varMap[node.paramNames[i]] = pname;
                }
                out << ") {\n";
                for (const auto& sp : dllStringParams) {
                    out << "    std::string " << sp.first << " = " << sp.second << " ? " << sp.second << " : \"\";\n";
                }
                varIdx = static_cast<int>(node.paramNames.size());
                std::map<std::string, bool> varIsString = globalVarIsString;
                std::map<std::string, bool> varIsConst = globalVarIsConst;
                std::map<std::string, bool> varIsFloat = globalVarIsFloat;
                std::map<std::string, bool> varIsChar = globalVarIsChar;
                std::map<std::string, bool> varIsBool = globalVarIsBool;
                std::map<std::string, bool> varIsEnum = globalVarIsEnum;
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    bool isStr = (i < node.paramTypes.size() && node.paramTypes[i] == "string");
                    varIsString[node.paramNames[i]] = isStr;
                    varIsFloat[node.paramNames[i]] = (i < node.paramTypes.size() && node.paramTypes[i] == "float");
                    varIsChar[node.paramNames[i]] = (i < node.paramTypes.size() && node.paramTypes[i] == "char");
                    varIsBool[node.paramNames[i]] = (i < node.paramTypes.size() && node.paramTypes[i] == "bool");
                    varIsEnum[node.paramNames[i]] = (i < node.paramTypes.size() && isEnumDeclType(node.paramTypes[i]));
                }
                varStructPush();
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    if (i < node.paramTypes.size() && isStructDeclType(node.paramTypes[i])) {
                        varStructDeclare(node.paramNames[i], structNameFromDecl(node.paramTypes[i]));
                    }
                }
                nexaDeclStack_.push_back(globalNexaDecl_);
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    nexaDeclStack_.back()[node.paramNames[i]] = canonicalParamType(node, i);
                }
                emitFnRet_ = voidFn ? EmitFnRet::VoidFn : EmitFnRet::IntFn;
                emitBlockStatements(out, node.children, varMap, varIdx, varIsString, varIsConst, varIsFloat,
                                    varIsChar, varIsBool, varIsEnum);
                emitFnRet_ = EmitFnRet::Main;
                nexaDeclStack_.pop_back();
                varStructPop();
                emitImplicitFnTail(out, node, hasValRet);
                out << "}\n\n";
                continue;
            }
            if (node.type == AstNode::Type::MainFunction) {
                if (!buildDll_) {
                    wroteMain = true;
                    const bool sliceMain = node.paramNames.size() == 1 && node.paramTypes.size() == 1 &&
                                           node.paramTypes[0] == "[]string";
                    if (!node.paramNames.empty() && !sliceMain) {
                        throw std::runtime_error(
                            "main(...) only supports an optional single parameter (args: []string)");
                    }
                    if (sliceMain) {
                        out << "int main(int argc, char** argv) {\n";
                    } else {
                        out << "int main() {\n";
                    }
                    std::map<std::string, std::string> varMap = globalVarMap;
                    int varIdx = 0;
                    std::map<std::string, bool> varIsString = globalVarIsString;
                    std::map<std::string, bool> varIsConst = globalVarIsConst;
                    std::map<std::string, bool> varIsFloat = globalVarIsFloat;
                    std::map<std::string, bool> varIsChar = globalVarIsChar;
                    std::map<std::string, bool> varIsBool = globalVarIsBool;
                    std::map<std::string, bool> varIsEnum = globalVarIsEnum;
                    if (sliceMain) {
                        std::string aname = preserveNames_ ? node.paramNames[0] : "__nexa_var_0";
                        varMap[node.paramNames[0]] = aname;
                        varIsString[node.paramNames[0]] = true;
                        varIdx = 1;
                        out << "    std::vector<std::string> " << aname << ";\n";
                        out << "    " << aname << ".reserve(static_cast<size_t>(argc));\n";
                        out << "    for (int __nexa_ai = 0; __nexa_ai < argc; ++__nexa_ai) {\n";
                        out << "        " << aname << ".emplace_back(argv[__nexa_ai] ? argv[__nexa_ai] : \"\");\n";
                        out << "    }\n";
                    }
                    bool mainValRet = false, mainVoidRet = false;
                    stmtsClassifyReturns(node.children, mainValRet, mainVoidRet);
                    if (mainValRet && mainVoidRet) {
                        throw std::runtime_error("main mixes 'return;' and 'return expr;'");
                    }
                    if (!node.fnReturnType.empty()) {
                        if (node.fnReturnType != "void") {
                            throw std::runtime_error(
                                "main may only use `: void` as an explicit return type (or omit it for int main)");
                        }
                        if (mainValRet) {
                            throw std::runtime_error("cannot return a value from void main()");
                        }
                    }
                    varStructPush();
                    nexaDeclStack_.push_back(globalNexaDecl_);
                    if (sliceMain) {
                        nexaDeclStack_.back()[node.paramNames[0]] = "[]string";
                    }
                    emitFnRet_ = EmitFnRet::Main;
                    emitBlockStatements(out, node.children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum);
                    emitFnRet_ = EmitFnRet::Main;
                    nexaDeclStack_.pop_back();
                    varStructPop();
                    if (!stmtsEndWithReturn(node.children)) {
                        out << "    return 0;\n";
                    }
                    out << "}\n";
                }
                continue;
            }
            if (node.initFromReadln) {
                throw std::runtime_error("Global variable cannot use io.readln()");
            }
            if (node.initFromFileRead) {
                throw std::runtime_error("Global variable cannot use file.read()");
            }
            if (node.initFromDllLoad) {
                throw std::runtime_error("Global variable cannot use dll.load()");
            }
            std::string vname = preserveNames_ ? node.value : ("__nexa_g_" + std::to_string(globalIdx++));
            if (!node.children.empty() && node.children[0].type == AstNode::Type::OsGetenv) {
                const std::string& envName = node.children[0].value;
                out << "const char* __nexa_ge_" << globalIdx << " = getenv(\"" << escapeString(envName) << "\");\n";
                out << "std::string " << vname << " = __nexa_ge_" << globalIdx << " ? __nexa_ge_" << globalIdx << " : \"\";\n";
                globalVarMap[node.value] = vname;
                globalVarIsString[node.value] = true;
                globalVarIsArray[node.value] = false;
                globalIdx++;
                justEmittedGlobal = true;
                continue;
            }
            if (!node.children.empty() && node.children[0].type == AstNode::Type::OsPlatform) {
                out << "std::string " << vname << " = __nexa_os_platform();\n";
                globalVarMap[node.value] = vname;
                globalVarIsString[node.value] = true;
                globalVarIsArray[node.value] = false;
                justEmittedGlobal = true;
                continue;
            }
            if (!node.children.empty() && node.children[0].type == AstNode::Type::OsExeDir) {
                out << "std::string " << vname << " = __nexa_exe_dir();\n";
                globalVarMap[node.value] = vname;
                globalVarIsString[node.value] = true;
                globalVarIsArray[node.value] = false;
                justEmittedGlobal = true;
                continue;
            }
            if (!node.children.empty() && node.children[0].type == AstNode::Type::OsGetProcessId) {
                std::string rhs = emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat,
                                           &globalVarIsChar, &globalVarIsBool);
                if (!node.declType.empty() && node.declType == "string") {
                    out << "std::string " << vname << " = std::to_string(" << rhs << ");\n";
                    globalVarIsString[node.value] = true;
                } else {
                    out << "int " << vname << " = " << rhs << ";\n";
                    globalVarIsString[node.value] = false;
                }
                globalVarMap[node.value] = vname;
                globalVarIsArray[node.value] = false;
                justEmittedGlobal = true;
                continue;
            }
            globalVarMap[node.value] = vname;
            globalVarIsConst[node.value] = node.isConst;
            globalVarIsFloat[node.value] = (!node.declType.empty() && node.declType == "float") || node.initIsFloat;
            globalVarIsChar[node.value] = (!node.declType.empty() && node.declType == "char") || node.initIsChar;
            globalVarIsBool[node.value] = (!node.declType.empty() && node.declType == "bool") || node.initIsBool;
            globalVarIsEnum[node.value] = !node.declType.empty() && isEnumDeclType(node.declType);
            bool isArray = node.initFromArray || (!node.children.empty() && node.children[0].type == AstNode::Type::ExprArrayLiteral);
            bool isStrArr = isArray && !node.children.empty() && arrayInitProducesString(node.children[0], globalVarIsString);
            bool isStr = !isPointerType(node.declType) && !isStructDeclType(node.declType) && !isEnumDeclType(node.declType) && (!node.declType.empty() ? (node.declType == "string") : (node.initUninitialized || (!node.initIsInt && !node.initIsBool && !node.initIsFloat && !node.initIsChar && !isArray && node.children.empty()) || (!node.children.empty() && (exprProducesString(node.children[0]) || isStrArr))));
            if (node.declType.empty() && !isArray && !node.children.empty()) {
                std::string it = inferExprNexaType(node.children[0]);
                if (it == "string") isStr = true;
                else if (nexaIsNumericIntType(it) || isPointerType(it) || it == "char" || it == "float" || it == "bool") isStr = false;
            }
            globalVarIsString[node.value] = isStr;
            globalVarIsArray[node.value] = isArray || node.isFixedArray;
            if (node.initUninitialized) {
                std::string c = node.isConst ? "const " : "";
                if (node.isFixedArray) {
                    if (isStructDeclType(node.declType) || isCppDeclType(node.declType)) {
                        out << c << nexaTypeToCpp(node.declType) << " " << vname << "[" << node.arraySize << "]{};\n";
                    } else {
                        std::string cppType = nexaTypeToCpp(node.declType);
                        out << c << cppType << " " << vname << "[" << node.arraySize << "];\n";
                    }
                } else if (!node.declType.empty() && isPointerType(node.declType)) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << " = nullptr;\n";
                } else if (!node.declType.empty() && nexaIsNumericIntType(node.declType)) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << " = 0;\n";
                } else if (!node.declType.empty() && node.declType == "bool") {
                    out << c << "bool " << vname << " = false;\n";
                } else if (!node.declType.empty() && node.declType == "float") {
                    out << c << "double " << vname << " = 0.0;\n";
                } else if (!node.declType.empty() && node.declType == "char") {
                    out << c << "char " << vname << " = '\\0';\n";
                } else if (!node.declType.empty() && isStructDeclType(node.declType)) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << "{};\n";
                } else if (!node.declType.empty() && isCppDeclType(node.declType)) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << "{};\n";
                } else if (!node.declType.empty() && isEnumDeclType(node.declType)) {
                    std::string en = enumNameFromDecl(node.declType);
                    std::string cpp = enumCppNames_.at(en);
                    out << c << cpp << " " << vname << " = " << cpp << "::" << enumFirstVariant_.at(en) << ";\n";
                } else if (!node.declType.empty() && (nexaIsSliceType(node.declType) || nexaIsMapType(node.declType) || nexaIsFnType(node.declType) || nexaIsResultType(node.declType) || node.declType == "json")) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << ";\n";
                } else {
                    out << c << "std::string " << vname << ";\n";
                }
            } else if (isArray && !node.children.empty()) {
                std::string c = node.isConst ? "const " : "";
                std::string decl = nexaDeclFromVariableAst(node);
                if (!nexaIsSliceType(decl)) decl = arrayInitProducesString(node.children[0], globalVarIsString) ? "[]string" : "[]int";
                out << c << nexaTypeToCpp(decl) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
            } else if (!node.children.empty()) {
                std::string c = node.isConst ? "const " : "";
                if (!node.declType.empty() && isPointerType(node.declType)) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else if (!node.declType.empty() && nexaIsNumericIntType(node.declType)) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else if (!node.declType.empty() && isEnumDeclType(node.declType)) {
                    std::string en = enumNameFromDecl(node.declType);
                    out << c << enumCppNames_.at(en) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else if (!node.declType.empty() && isStructDeclType(node.declType)) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else if (!node.declType.empty() && isCppDeclType(node.declType)) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else if (!node.declType.empty() && (nexaIsSliceType(node.declType) || nexaIsMapType(node.declType) || nexaIsFnType(node.declType) || nexaIsResultType(node.declType) || node.declType == "json")) {
                    out << c << nexaTypeToCpp(node.declType) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else {
                    bool useBool = !node.declType.empty() ? (node.declType == "bool") : node.initIsBool;
                    bool useInt = !node.declType.empty() ? nexaIsNumericIntType(node.declType) : node.initIsInt;
                    bool useFloat = !node.declType.empty() ? (node.declType == "float") : node.initIsFloat;
                    bool useChar = !node.declType.empty() ? (node.declType == "char") : node.initIsChar;
                    std::string inferred;
                    if (node.declType.empty() && !node.children.empty()) {
                        inferred = inferExprNexaType(node.children[0]);
                        if (inferred == "string") {
                            useInt = false;
                            useFloat = false;
                            useChar = false;
                            useBool = false;
                        } else if (inferred == "float") {
                            useFloat = true;
                            useInt = false;
                        } else if (inferred == "bool") {
                            useBool = true;
                            useInt = false;
                        } else if (inferred == "char") {
                            useChar = true;
                            useInt = false;
                        }
                    }
                    if (!inferred.empty() && isPointerType(inferred)) {
                        out << c << nexaTypeToCpp(inferred) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                    } else if (!inferred.empty() && nexaIsNumericIntType(inferred)) {
                        out << c << nexaTypeToCpp(inferred) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                    } else if (!inferred.empty() && (nexaIsFnType(inferred) || nexaIsSliceType(inferred) || nexaIsMapType(inferred) || nexaIsResultType(inferred) || isStructDeclType(inferred) || inferred == "json")) {
                        out << c << nexaTypeToCpp(inferred) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                    } else {
                        std::string cppType = c + (useBool ? "bool " : useFloat ? "double " : useChar ? "char " : (useInt ? "int " : "std::string "));
                        out << cppType << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                    }
                }
            } else if (node.initIsBool || (!node.declType.empty() && node.declType == "bool")) {
                std::string c = node.isConst ? "const " : "";
                out << c << "bool " << vname << " = " << (node.initValue == "true" ? "true" : "false") << ";\n";
            } else if (node.initIsInt || (!node.declType.empty() && nexaIsNumericIntType(node.declType))) {
                std::string c = node.isConst ? "const " : "";
                std::string cppT = (!node.declType.empty() && nexaIsNumericIntType(node.declType)) ? nexaTypeToCpp(node.declType) : "int";
                out << c << cppT << " " << vname << " = " << emitIntLiteral(node.initValue) << ";\n";
            } else {
                std::string c = node.isConst ? "const " : "";
                out << c << "std::string " << vname << " = " << emitCppStringValue(node.initValue) << ";\n";
            }
            justEmittedGlobal = true;
        }

        // Emit DLL/SO loader hook: auto-call __init__ when library is loaded
        if (buildDll_) {
            bool hasInit = false;
            for (const AstNode& node : ast_) {
                if (node.type == AstNode::Type::Function && node.value == "__init__") {
                    hasInit = true;
                    break;
                }
            }
            if (hasInit) {
                for (const AstNode& node : ast_) {
                    if (node.type == AstNode::Type::Function && node.value == "__init__" && !node.paramNames.empty()) {
                        throw std::runtime_error("fn __init__() must have no parameters for DLL/SO auto-init");
                    }
                }
                std::string initName = fnNameInitOnly("__init__");
                out << "#ifdef _WIN32\n";
                out << "#include <windows.h>\n";
                out << "BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {\n";
                out << "    (void)hinstDLL;\n";
                out << "    (void)lpvReserved;\n";
                out << "    if (fdwReason == DLL_PROCESS_ATTACH) {\n";
                out << "        " << initName << "();\n";
                out << "    }\n";
                out << "    return TRUE;\n";
                out << "}\n";
                out << "#else\n";
                out << "__attribute__((constructor))\n";
                out << "static void __nexa_auto_init(void) {\n";
                out << "    " << initName << "();\n";
                out << "}\n";
                out << "#endif\n\n";
            }
        }

        // If no fn main() and no C++ main from file-scope inline, but fn __init__() exists (exe entry), call __init__ from main.
        if (!buildDll_) {
            if (!wroteMain) {
                bool hasInitOnly = false;
                for (const AstNode& node : ast_) {
                    if (node.type == AstNode::Type::Function && node.value == "__init__") {
                        hasInitOnly = true;
                        break;
                    }
                }
                if (hasInitOnly) {
                    out << "int main() {\n";
                    out << "    " << fnNameInitOnly("__init__") << "();\n";
                    out << "    return 0;\n";
                    out << "}\n";
                }
            }
        }

        // Strip first, then dedup. A header that two runtime blocks both need
        // is written by both -- std/gfx and std/time each want <windows.h> --
        // and whether the two copies are duplicates is a question that can only
        // be answered once the platform ladders are gone: before the strip the
        // gfx copy sits inside `#elif defined(_WIN32)` and std/time's inside its
        // own, so neither counts as unconditional and neither is dropped. After
        // it, on a Windows build, both are plain lines in a flat file and the
        // second goes. On a build where only one branch survives, only one copy
        // was ever there to keep.
        std::string src = dedupUnconditionalIncludes(stripInactivePlatformGuards(out.str(), target_));
        if (cppUsage_.gfx && cppUsage_.gfxImage &&
            (target_ == CppTarget::Linux || target_ == CppTarget::Wasm)) {
            // ~8,000 lines of stb, so only a program that can reach the decoder gets it.
            // Nothing needs a stub in its place: __nexa_gfx_decode_rgba is the only caller
            // and it is emitted by the same flag, so a program without the decoder has no
            // reference to satisfy. See the GfxCall usage scan.
            src += gfxStbImageRuntimeCpp();
        }
        // Last, once nothing else will add or drop a line: turn the statement markers into
        // `#line` directives. The appended gfx runtime carries no markers, and the final
        // snap-back before it already points line info back at the generated file.
        if (lineDirectives_) src = rewriteLineMarks(src);
        return src;
    }

private:
    // Brackets one statement with a begin/end marker pair. RAII because the statement loop uses
    // `continue` in several places, and a begin marker without its end would leave every following
    // line of generated glue claiming to be .nxa source.
    struct LineMarkScope {
        std::ostringstream* out = nullptr;
        LineMarkScope(bool enabled, std::ostringstream& o, const AstNode& n, const std::string& indent) {
            if (!enabled || n.line == 0 || n.srcFile.empty()) return;
            // A path containing a newline would split the marker across lines and break parsing.
            if (n.srcFile.find('\n') != std::string::npos || n.srcFile.find('\r') != std::string::npos) return;
            out = &o;
            o << indent << nexaLineMark() << " B " << n.line << " " << n.srcFile << "\n";
        }
        ~LineMarkScope() { if (out) *out << nexaLineMark() << " E\n"; }
        LineMarkScope(const LineMarkScope&) = delete;
        LineMarkScope& operator=(const LineMarkScope&) = delete;
    };

    // Recognise a marker line. `payload` receives what follows the sentinel: "E" for a snap-back,
    // or "B <line> <path>" for a statement start. Anything else is left alone as a plain comment.
    static bool parseLineMark(const std::string& line, std::string& payload) {
        const std::string& mark = nexaLineMark();
        size_t i = line.find_first_not_of(" \t");
        if (i == std::string::npos) return false;
        if (line.compare(i, mark.size(), mark) != 0) return false;
        size_t p = i + mark.size();
        if (p >= line.size() || line[p] != ' ') return false;
        payload = line.substr(p + 1);
        while (!payload.empty() && payload.back() == '\r') payload.pop_back();
        if (payload == "E") return true;
        if (payload.rfind("B ", 0) != 0) return false;
        size_t sp = payload.find(' ', 2);
        if (sp == std::string::npos || sp == 2 || sp + 1 >= payload.size()) return false;
        for (size_t k = 2; k < sp; ++k) {
            if (!std::isdigit(static_cast<unsigned char>(payload[k]))) return false;
        }
        return true;
    }

    // Rewrite markers into `#line` directives. Must run last, on the final text: each marker line
    // becomes exactly one line, so numbering stays 1:1 with the input and a snap-back is simply
    // "the line after this one" — which is only true once no later pass can move lines.
    std::string rewriteLineMarks(const std::string& src) const {
        const std::string genFile = nexaLineFileLiteral(generatedCppPath_);

        std::vector<std::string> lines;
        std::vector<std::string> marks;  // parsed payload per line, empty when not a marker
        for (size_t pos = 0;;) {
            size_t nl = src.find('\n', pos);
            lines.push_back(src.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
            marks.emplace_back();
            if (!parseLineMark(lines.back(), marks.back())) marks.back().clear();
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }

        std::ostringstream res;
        for (size_t i = 0; i < lines.size(); ++i) {
            const bool isLast = (i + 1 == lines.size());
            if (marks[i].empty()) {
                res << lines[i];
                if (!isLast) res << "\n";
                continue;
            }
            // Only the last marker of a consecutive run governs a real line of code; the ones
            // before it would be superseded on the very next line. Drop them to a blank line,
            // which keeps the 1:1 numbering the snap-backs depend on. Without this, straight-line
            // code carries a dead snap-back between every pair of statements.
            if (!isLast && !marks[i + 1].empty()) {
                res << "\n";
                continue;
            }
            if (marks[i] == "E") {
                // i is 0-based, so the next line is number i + 2.
                res << "#line " << (i + 2) << " " << genFile << "\n";
            } else {
                size_t sp = marks[i].find(' ', 2);
                res << "#line " << marks[i].substr(2, sp - 2) << " "
                    << nexaLineFileLiteral(marks[i].substr(sp + 1)) << "\n";
            }
        }
        return res.str();
    }

    const std::vector<AstNode>& ast_;
    const Modules& modules_;
    bool preserveNames_;
    bool buildDll_;
    CppTarget target_;
    // Debug builds map generated C++ back to the .nxa source with `#line` directives.
    // generatedCppPath_ is the name the snap-back directives use to leave that mapping again.
    bool lineDirectives_;
    std::string generatedCppPath_;
    Modules::CppUsage cppUsage_;
    // While emitting a function or main body: how bare `return;` / value returns are interpreted
    enum class EmitFnRet { Main, IntFn, VoidFn };
    mutable EmitFnRet emitFnRet_ = EmitFnRet::Main;
    std::map<std::string, std::map<std::string, std::string>> structFields_;
    std::map<std::string, std::vector<std::string>> structFieldOrder_;
    std::map<std::string, std::string> structCppNames_;
    std::map<std::string, std::string> enumCppNames_;
    std::map<std::string, std::set<std::string>> enumVariants_;
    std::map<std::string, std::string> enumFirstVariant_;
    mutable std::vector<std::map<std::string, std::string>> varStructScopes_;
    std::map<std::string, std::map<std::string, const AstNode*>> structMethods_;
    mutable std::string methodSelfType_;

    struct FnOverloadSlot {
        size_t astIndex = 0;
        std::string name;
        std::vector<std::string> paramTypes;
        size_t minArgs = 0;  // required args (params before the first default)
    };
    std::vector<FnOverloadSlot> fnOverloadSlots_;
    std::map<std::string, std::string> globalNexaDecl_;
    std::vector<std::map<std::string, std::string>> nexaDeclStack_;

    std::string canonicalParamType(const AstNode& fn, size_t i) const {
        if (i >= fn.paramTypes.size()) return "int";
        const std::string& pt = fn.paramTypes[i];
        return pt.empty() ? "int" : pt;
    }

    // =====================================================================
    // Pass heavy parameters by const reference
    //
    // `fn total(xs: []int)` used to emit `int total(std::vector<int> xs)`, so every call
    // deep-copied the whole slice — and the same for string, map, struct, Result and json
    // parameters. C passes a pointer; we emit `const std::vector<int>&` wherever that is
    // observationally identical to the copy.
    //
    // The copy is only observable one way: the callee changes the caller's object during
    // the call and then reads the parameter, where a copy would still hold the old value.
    // So a parameter is bound by reference only when all three hold:
    //
    //   1. the callee never modifies it — no assignment, element or field store, mutating
    //      method, `&p`, or hand-off to a function whose signature we cannot see;
    //   2. the callee, transitively, writes nothing the caller's argument could alias: no
    //      global, no `self` field (a method can be called as `p.merge(p)`), nothing
    //      through a pointer;
    //   3. the program never takes a global's address, never splices in inline_cpp!, and
    //      never starts a thread — the three ways a write can happen out of this walk's
    //      sight.
    //
    // Anything that fails a test keeps its by-value signature. This removes copies; it
    // does not change what a program prints.
    // =====================================================================

    // Root variable of an lvalue chain: xs, xs[i], p.a.b, *p, xs[1:2] all root at a name.
    static std::string exprRootVarName(const AstNode& e) {
        switch (e.type) {
            case AstNode::Type::ExprVarRef:
                return e.value;
            case AstNode::Type::ExprArrayIndex:
            case AstNode::Type::ExprMember:
            case AstNode::Type::ExprSlice:
            case AstNode::Type::ExprDeref:
                return e.children.empty() ? std::string() : exprRootVarName(e.children[0]);
            default:
                return std::string();
        }
    }

    static void walkAstNode(const AstNode& n, const std::function<void(const AstNode&)>& f) {
        f(n);
        for (const AstNode& c : n.children) walkAstNode(c, f);
        for (const AstNode& d : n.paramDefaults) walkAstNode(d, f);
    }

    static bool isAssignStmtType(AstNode::Type t) {
        switch (t) {
            case AstNode::Type::Assignment:
            case AstNode::Type::AssnAdd:
            case AstNode::Type::AssnSub:
            case AstNode::Type::AssnMul:
            case AstNode::Type::AssnDiv:
            case AstNode::Type::AssnMod:
            case AstNode::Type::AssnBitAnd:
            case AstNode::Type::AssnBitOr:
            case AstNode::Type::AssnBitXor:
            case AstNode::Type::AssnShl:
            case AstNode::Type::AssnShr:
            case AstNode::Type::AssnIndex:
            case AstNode::Type::IncPost:
            case AstNode::Type::DecPost:
                return true;
            default:
                return false;
        }
    }

    // Built-in container methods known to leave the receiver alone.
    static bool isReadOnlyBuiltinMethod(const std::string& m) {
        return m == "len" || m == "size" || m == "has" || m == "contains" || m == "index_of" ||
               m == "get" || m == "get_at" || m == "keys" || m == "values" || m == "count" ||
               m == "empty" || m == "ok" || m == "value" || m == "error" || m == "stringify" ||
               m == "join" || m == "min" || m == "max" || m == "sum";
    }

    // xs.sort() / xs.sort_desc(), as an in-place heapsort over the emitted receiver.
    //
    // Heapsort rather than std::sort because <algorithm> is not one of the headers a
    // Nexa program pulls in, and a slice sort is not worth making it one -- the whole
    // point of the emit is that a program includes only what it reaches. Heapsort is
    // the one O(n log n) in-place sort that needs no recursion, no scratch buffer and
    // no median-picking, so it fits in a call-site lambda; insertion sort would have
    // fitted too, but a quadratic sort behind a one-word method call is a trap.
    //
    // The direction is the heap's comparison: a max-heap drains ascending, so `<`
    // sorts up and `>` sorts down. Heapsort is unstable, which is unobservable here
    // because a slice of int / float / string holds no payload beside the key.
    static std::string emitSliceSort(const std::string& recv, bool descending) {
        const std::string op = descending ? " > " : " < ";
        return "([&](){ auto& __nexa_v = " + recv + "; size_t __nexa_n = __nexa_v.size();"
               " auto __nexa_sift = [&](size_t __nexa_r, size_t __nexa_m){"
               " while (true) {"
               " size_t __nexa_c = __nexa_r * 2 + 1;"
               " if (__nexa_c >= __nexa_m) break;"
               " if (__nexa_c + 1 < __nexa_m && __nexa_v[__nexa_c]" + op + "__nexa_v[__nexa_c + 1]) __nexa_c++;"
               " if (!(__nexa_v[__nexa_r]" + op + "__nexa_v[__nexa_c])) break;"
               " auto __nexa_t = __nexa_v[__nexa_r]; __nexa_v[__nexa_r] = __nexa_v[__nexa_c]; __nexa_v[__nexa_c] = __nexa_t;"
               " __nexa_r = __nexa_c; } };"
               " for (size_t __nexa_i = __nexa_n / 2; __nexa_i-- > 0; ) __nexa_sift(__nexa_i, __nexa_n);"
               " for (size_t __nexa_i = __nexa_n; __nexa_i-- > 1; ) {"
               " auto __nexa_t = __nexa_v[0]; __nexa_v[0] = __nexa_v[__nexa_i]; __nexa_v[__nexa_i] = __nexa_t;"
               " __nexa_sift(0, __nexa_i); } })()";
    }

    // Type of an lvalue chain (xs, p.a.b, xs[i], *p) rooted at `rootName`, given that
    // name's Nexa type. Empty when the chain leaves what the tables know.
    std::string chainTypeFrom(const AstNode& e, const std::string& rootName,
                              const std::string& rootType) const {
        switch (e.type) {
            case AstNode::Type::ExprVarRef:
                return e.value == rootName ? rootType : std::string();
            case AstNode::Type::ExprDeref: {
                if (e.children.empty()) return std::string();
                std::string t = chainTypeFrom(e.children[0], rootName, rootType);
                return isPointerType(t) ? pointerPointeeType(t) : std::string();
            }
            case AstNode::Type::ExprSlice:
                return e.children.empty() ? std::string()
                                          : chainTypeFrom(e.children[0], rootName, rootType);
            case AstNode::Type::ExprArrayIndex: {
                if (e.children.empty()) return std::string();
                std::string t = chainTypeFrom(e.children[0], rootName, rootType);
                if (nexaIsSliceType(t)) return nexaSliceElem(t);
                if (nexaIsMapType(t)) {
                    std::string k, v;
                    return nexaSplitMapType(t, k, v) ? v : std::string();
                }
                if (t == "string") return "char";
                return std::string();
            }
            case AstNode::Type::ExprMember: {
                if (e.children.empty()) return std::string();
                std::string t = chainTypeFrom(e.children[0], rootName, rootType);
                if (isPointerType(t)) t = pointerPointeeType(t);
                if (!isStructDeclType(t)) return std::string();
                auto s = structFields_.find(structNameFromDecl(t));
                if (s == structFields_.end()) return std::string();
                auto f = s->second.find(e.value);
                return f == s->second.end() ? std::string() : f->second;
            }
            default:
                return std::string();
        }
    }

    // Does `call` (a `.`/`->` method call) modify the object it is called on? A user's own
    // struct method is emitted as a non-const member function, so calling one counts —
    // both because it may write a field and because it would not compile against a const
    // reference. An unrecognised receiver type is assumed to be modified.
    bool methodCallMutatesReceiver(const AstNode& call, const std::string& rootName,
                                   const std::string& rootType) const {
        if (call.children.empty()) return true;
        std::string recvT = chainTypeFrom(call.children[0], rootName, rootType);
        if (isPointerType(recvT)) recvT = pointerPointeeType(recvT);
        if (isStructDeclType(recvT)) return true;
        if (recvT.empty()) return true;
        return !isReadOnlyBuiltinMethod(call.value);
    }

    // Deliberately reads ast_ rather than fnOverloadSlots_: this analysis runs before the
    // overload table is built, because struct method declarations are emitted before that.
    bool isUserFunctionName(const std::string& name) const {
        for (const AstNode& n : ast_) {
            if (n.type == AstNode::Type::Function && !n.isExtern && n.value == name) return true;
        }
        return false;
    }

    // Parameter types worth a reference. Scalars, enums, pointers and closures are already
    // cheap; `cpp:` header types are left alone because we do not know what they support.
    bool typeIsHeavyParam(const std::string& t) const {
        if (t == "string" || t == "json") return true;
        if (nexaIsSliceType(t) || nexaIsMapType(t) || nexaIsResultType(t)) return true;
        if (isStructDeclType(t)) return structCppNames_.count(structNameFromDecl(t)) > 0;
        return false;
    }

    // std::map::operator[] is non-const and inserts a default element on a miss, so a map
    // reached through a const reference neither compiles nor keeps today's behaviour. A
    // parameter whose type contains a map anywhere therefore may not be indexed.
    bool typeMentionsMap(const std::string& t, std::set<std::string>& seen) const {
        if (nexaIsMapType(t)) return true;
        if (nexaIsSliceType(t)) return typeMentionsMap(nexaSliceElem(t), seen);
        if (nexaIsResultType(t)) return typeMentionsMap(nexaResultInner(t), seen);
        if (isStructDeclType(t)) {
            std::string s = structNameFromDecl(t);
            if (!seen.insert(s).second) return false;
            auto it = structFields_.find(s);
            if (it == structFields_.end()) return false;
            for (const auto& kv : it->second) {
                if (typeMentionsMap(kv.second, seen)) return true;
            }
        }
        return false;
    }

    bool bodyMutatesName(const std::vector<AstNode>& body, const std::string& name,
                         const std::string& nameType, bool indexingIsSafe) const {
        bool hit = false;
        std::function<void(const AstNode&)> visit = [&](const AstNode& n) {
            if (hit) return;
            if (isAssignStmtType(n.type)) {
                if (n.value == name) hit = true;
                return;
            }
            switch (n.type) {
                case AstNode::Type::AssnMember:
                case AstNode::Type::AssnDeref:
                case AstNode::Type::ExprAddrOf:
                case AstNode::Type::StmtDelete:
                    if (!n.children.empty() && exprRootVarName(n.children[0]) == name) hit = true;
                    break;
                case AstNode::Type::ExprArrayIndex:
                    if (!indexingIsSafe && !n.children.empty() &&
                        exprRootVarName(n.children[0]) == name) {
                        hit = true;
                    }
                    break;
                case AstNode::Type::FnCall:
                case AstNode::Type::ExprCall:
                    if (n.initValue == "." || n.initValue == "->") {
                        if (!n.children.empty() && exprRootVarName(n.children[0]) == name &&
                            methodCallMutatesReceiver(n, name, nameType)) {
                            hit = true;
                        }
                    } else if (!isUserFunctionName(n.value)) {
                        // A C header, an extern declaration or a closure variable can take
                        // the argument by non-const reference; we cannot see the signature.
                        for (const AstNode& a : n.children) {
                            if (exprRootVarName(a) == name) hit = true;
                        }
                    }
                    break;
                default:
                    break;
            }
        };
        for (const AstNode& s : body) walkAstNode(s, visit);
        return hit;
    }

    bool bodyWritesNonLocal(const AstNode& fn, const std::set<std::string>& globals) const {
        const bool isMethod = !fn.receiverType.empty();
        bool hit = false;
        auto aliasable = [&](const std::string& root) {
            return !root.empty() && (globals.count(root) > 0 || (isMethod && root == "self"));
        };
        auto rootTypeOf = [&](const std::string& root) -> std::string {
            if (isMethod && root == "self") return fn.receiverType;
            auto it = passGlobalTypes_.find(root);
            return it == passGlobalTypes_.end() ? std::string() : it->second;
        };
        std::function<void(const AstNode&)> visit = [&](const AstNode& n) {
            if (hit) return;
            if (isAssignStmtType(n.type)) {
                if (aliasable(n.value)) hit = true;
                return;
            }
            switch (n.type) {
                case AstNode::Type::AssnDeref:
                case AstNode::Type::InlineCpp:
                    hit = true;  // a store through a pointer can land anywhere
                    break;
                case AstNode::Type::AssnMember:
                    if (!n.children.empty()) {
                        if (n.children[0].isArrowMember) hit = true;
                        else if (aliasable(exprRootVarName(n.children[0]))) hit = true;
                    }
                    break;
                case AstNode::Type::FnCall:
                case AstNode::Type::ExprCall:
                    if (n.initValue == "->") {
                        if (!isReadOnlyBuiltinMethod(n.value)) hit = true;
                    } else if (n.initValue == ".") {
                        std::string root = n.children.empty() ? std::string()
                                                             : exprRootVarName(n.children[0]);
                        if (aliasable(root) && methodCallMutatesReceiver(n, root, rootTypeOf(root))) {
                            hit = true;
                        }
                    } else if (!isUserFunctionName(n.value)) {
                        for (const AstNode& a : n.children) {
                            if (aliasable(exprRootVarName(a))) hit = true;
                        }
                    }
                    break;
                default:
                    break;
            }
        };
        for (const AstNode& s : fn.children) walkAstNode(s, visit);
        return hit;
    }

    void collectCalledNames(const AstNode& fn, std::set<std::string>& out) const {
        std::function<void(const AstNode&)> visit = [&](const AstNode& n) {
            if (n.type != AstNode::Type::FnCall && n.type != AstNode::Type::ExprCall) return;
            out.insert(n.value);  // by name: method and free-function names share one table
        };
        for (const AstNode& s : fn.children) walkAstNode(s, visit);
    }

    void analyzeParamPassing() {
        paramByRef_.clear();
        passGlobalTypes_.clear();
        // Exported DLL functions keep the C ABI their declaration promises.
        if (buildDll_) return;

        std::set<std::string> globals;
        for (const AstNode& n : ast_) {
            if (n.type != AstNode::Type::Variable) continue;
            globals.insert(n.value);
            // Only the written-down type; an inferred one stays unknown, which makes the
            // answer more conservative rather than wrong.
            if (!n.declType.empty()) passGlobalTypes_[n.value] = n.declType;
        }

        bool aliasUnsafeProgram = false;
        std::function<void(const AstNode&)> scanProgram = [&](const AstNode& n) {
            switch (n.type) {
                case AstNode::Type::InlineCpp:
                case AstNode::Type::ThreadSpawn:
                case AstNode::Type::ThreadWorker:
                case AstNode::Type::ThreadRun:
                    aliasUnsafeProgram = true;
                    break;
                case AstNode::Type::ExprAddrOf:
                    if (!n.children.empty() && globals.count(exprRootVarName(n.children[0]))) {
                        aliasUnsafeProgram = true;
                    }
                    break;
                default:
                    break;
            }
        };
        for (const AstNode& n : ast_) walkAstNode(n, scanProgram);
        if (aliasUnsafeProgram) return;

        struct PassFn {
            const AstNode* node = nullptr;
            bool writes = false;
            std::set<std::string> calls;
        };
        std::vector<PassFn> fns;
        for (const AstNode& n : ast_) {
            if (n.type == AstNode::Type::Function && !n.isExtern) {
                fns.push_back({&n, false, {}});
            } else if (n.type == AstNode::Type::StructDef) {
                for (const AstNode& m : n.children) {
                    if (m.type == AstNode::Type::Function) fns.push_back({&m, false, {}});
                }
            }
        }
        for (PassFn& f : fns) {
            f.writes = bodyWritesNonLocal(*f.node, globals);
            collectCalledNames(*f.node, f.calls);
        }
        // Propagate "writes something the caller can see" along the call graph, by name, to
        // a fixed point. Two functions sharing a name (overloads, or a method and a free
        // function) share one entry, which only makes the answer more conservative.
        for (bool changed = true; changed;) {
            changed = false;
            std::set<std::string> writerNames;
            for (const PassFn& f : fns) {
                if (f.writes) writerNames.insert(f.node->value);
            }
            for (PassFn& f : fns) {
                if (f.writes) continue;
                for (const std::string& c : f.calls) {
                    if (writerNames.count(c)) {
                        f.writes = true;
                        changed = true;
                        break;
                    }
                }
            }
        }

        for (const PassFn& f : fns) {
            std::vector<bool> byRef(f.node->paramNames.size(), false);
            if (!f.writes) {
                for (size_t i = 0; i < f.node->paramNames.size(); i++) {
                    std::string t = canonicalParamType(*f.node, i);
                    if (!typeIsHeavyParam(t)) continue;
                    std::set<std::string> seen;
                    bool indexingIsSafe = !typeMentionsMap(t, seen);
                    if (!bodyMutatesName(f.node->children, f.node->paramNames[i], t, indexingIsSafe)) {
                        byRef[i] = true;
                    }
                }
            }
            paramByRef_[f.node] = std::move(byRef);
        }
    }

    bool paramIsByRef(const AstNode& fn, size_t i) const {
        auto it = paramByRef_.find(&fn);
        if (it == paramByRef_.end() || i >= it->second.size()) return false;
        return it->second[i];
    }

    std::string paramSigCpp(const AstNode& fn, size_t i) const {
        std::string cpp = nexaTypeToCpp(canonicalParamType(fn, i));
        return paramIsByRef(fn, i) ? ("const " + cpp + "&") : cpp;
    }

    std::map<const AstNode*, std::vector<bool>> paramByRef_;
    std::map<std::string, std::string> passGlobalTypes_;

    static size_t fnMinArgs(const AstNode& fn) {
        size_t minArgs = fn.paramNames.size();
        for (size_t i = 0; i < fn.paramHasDefault.size(); ++i) {
            if (fn.paramHasDefault[i]) {
                minArgs = i;
                break;
            }
        }
        return minArgs;
    }

    static bool typesMatchForOverload(const std::string& formal, const std::string& actual) {
        if (formal == actual) return true;
        if (formal == "float" && actual == "int") return true;
        if (nexaIsNumericIntType(formal) && nexaIsNumericIntType(actual)) return true;
        // null is compatible with any pointer parameter
        if (isPointerType(formal) && actual == "null") return true;
        // C strings: string literals/values pass to *char extern params
        if (isPointerType(formal) && pointerPointeeType(formal) == "char" && actual == "string") return true;
        return false;
    }

    std::string inferReturnNexaType(const AstNode& fn) const {
        if (!fn.fnReturnType.empty()) return fn.fnReturnType;
        if (fn.isExtern) return "int";
        bool hasValRet = false, hasVoidRet = false;
        stmtsClassifyReturns(fn.children, hasValRet, hasVoidRet);
        if (hasValRet) return "int";
        return "void";
    }

    static std::string libcHeaderReturnHint(const std::string& name) {
        // Used only when a C/C++ header was #included. char* APIs become Nexa string.
        std::string bare = name;
        size_t col = name.rfind("::");
        if (col != std::string::npos) bare = name.substr(col + 2);
        static const char* kString[] = {
            "getenv", "strerror", "strstr", "strchr", "strrchr", "strcpy", "strncpy",
            "strcat", "strncat", "strtok", "strdup", "strpbrk", "asctime", "ctime",
            "tmpnam", "gets", nullptr
        };
        static const char* kVoidPtr[] = {
            "malloc", "calloc", "realloc", "memcpy", "memmove", "memset",
            "fopen", "freopen", "bsearch", nullptr
        };
        static const char* kVoid[] = {
            "free", "exit", "abort", "srand", "perror", "clearerr", "rewind", "qsort", nullptr
        };
        static const char* kFloat[] = {
            "atof", "strtod", "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
            "sqrt", "fabs", "pow", "floor", "ceil", "log", "log10", "exp", nullptr
        };
        static const char* kSizeT[] = {
            "fread", "fwrite", "strftime", nullptr
        };
        auto has = [&](const char* const* tbl) {
            for (int i = 0; tbl[i]; i++) {
                if (bare == tbl[i]) return true;
            }
            return false;
        };
        if (has(kString)) return "string";
        if (has(kVoidPtr)) return "*void";
        if (has(kVoid)) return "void";
        if (has(kFloat)) return "float";
        if (has(kSizeT)) return "size_t";
        return "int";
    }

    static bool isCppDeclType(const std::string& t) {
        return t.size() >= 4 && t.compare(0, 4, "cpp:") == 0;
    }
    static std::string cppNameFromDecl(const std::string& t) {
        return t.substr(4);
    }

    bool hasNexaFnNamed(const std::string& name) const {
        for (const FnOverloadSlot& sl : fnOverloadSlots_) {
            if (sl.name == name) return true;
        }
        return false;
    }

    std::string fnTypeFromFnAst(const AstNode& fn) const {
        std::vector<std::string> params;
        for (size_t i = 0; i < fn.paramNames.size(); i++) {
            params.push_back(canonicalParamType(fn, i));
        }
        return nexaMakeFnType(params, inferReturnNexaType(fn));
    }

    std::string uniqueNamedFnType(const std::string& name) const {
        int n = 0;
        size_t slot = 0;
        for (size_t s = 0; s < fnOverloadSlots_.size(); ++s) {
            if (fnOverloadSlots_[s].name == name) {
                n++;
                slot = s;
            }
        }
        if (n != 1) return "";
        return fnTypeFromFnAst(ast_[fnOverloadSlots_[slot].astIndex]);
    }

    size_t uniqueNamedFnSlot(const std::string& name) const {
        int n = 0;
        size_t slot = 0;
        for (size_t s = 0; s < fnOverloadSlots_.size(); ++s) {
            if (fnOverloadSlots_[s].name == name) {
                n++;
                slot = s;
            }
        }
        if (n != 1) {
            throw std::runtime_error("Cannot use overloaded function '" + name + "' as a value");
        }
        return slot;
    }

    std::string fnTypeFromLambdaAst(const AstNode& e) const {
        std::vector<std::string> params;
        for (size_t i = 0; i < e.paramNames.size(); i++) {
            params.push_back(i < e.paramTypes.size() && !e.paramTypes[i].empty() ? e.paramTypes[i] : "int");
        }
        std::string ret = e.fnReturnType;
        if (ret.empty()) {
            bool hasValRet = false, hasVoidRet = false;
            stmtsClassifyReturns(e.children, hasValRet, hasVoidRet);
            ret = hasValRet ? "int" : "void";
        }
        return nexaMakeFnType(params, ret);
    }

    bool isHeaderImportedCall(const std::string& name) const {
        return modules_.hasCppHeader() && !hasNexaFnNamed(name);
    }

    size_t resolveOverload(const std::string& name,
                           const std::vector<std::string>& actualArgTypes) const {
        const size_t k = actualArgTypes.size();
        std::vector<size_t> compat;
        for (size_t s = 0; s < fnOverloadSlots_.size(); ++s) {
            const FnOverloadSlot& sl = fnOverloadSlots_[s];
            if (sl.name != name) continue;
            const bool variadic = ast_[sl.astIndex].isVariadic;
            if (k < sl.minArgs) continue;
            if (!variadic && k > sl.paramTypes.size()) continue;
            bool ok = true;
            const size_t chk = k < sl.paramTypes.size() ? k : sl.paramTypes.size();
            for (size_t i = 0; i < chk; ++i) {
                if (!typesMatchForOverload(sl.paramTypes[i], actualArgTypes[i])) {
                    ok = false;
                    break;
                }
            }
            if (ok) compat.push_back(s);
        }
        if (compat.empty()) {
            std::string msg = "No matching overload for '" + name + "'(";
            for (size_t i = 0; i < actualArgTypes.size(); ++i) {
                if (i) msg += ", ";
                msg += actualArgTypes[i];
            }
            msg += ")";
            if (!modules_.hasCppHeader()) {
                msg += "; include a C header (#include <foo.h>) or declare: extern fn " + name + "(...): type;";
            }
            throw std::runtime_error(msg);
        }
        // Prefer exact arity match (no defaults filled).
        std::vector<size_t> exactArity;
        for (size_t s : compat) {
            if (fnOverloadSlots_[s].paramTypes.size() == k) exactArity.push_back(s);
        }
        const std::vector<size_t>& pool = exactArity.empty() ? compat : exactArity;

        std::vector<size_t> exact;
        for (size_t s : pool) {
            const FnOverloadSlot& sl = fnOverloadSlots_[s];
            bool isExact = true;
            const size_t chk = k < sl.paramTypes.size() ? k : sl.paramTypes.size();
            for (size_t i = 0; i < chk; ++i) {
                if (sl.paramTypes[i] != actualArgTypes[i]) {
                    isExact = false;
                    break;
                }
            }
            if (isExact) exact.push_back(s);
        }
        if (exact.size() == 1) return exact[0];
        if (exact.size() > 1) {
            throw std::runtime_error("Ambiguous overload resolution for '" + name + "'");
        }
        if (pool.size() == 1) return pool[0];
        throw std::runtime_error("Ambiguous overload resolution for '" + name + "'");
    }

    size_t slotForZeroArgFunctionNamed(const std::string& name) const {
        std::vector<size_t> zs;
        for (size_t s = 0; s < fnOverloadSlots_.size(); ++s) {
            if (fnOverloadSlots_[s].name == name && fnOverloadSlots_[s].paramTypes.empty()) zs.push_back(s);
        }
        if (zs.empty()) {
            throw std::runtime_error("No zero-parameter function '" + name + "' for thread.spawn / init");
        }
        if (zs.size() > 1) {
            throw std::runtime_error("Multiple zero-parameter overloads of '" + name + "' are not allowed for thread.spawn");
        }
        return zs[0];
    }

    std::string cppFnNameForSlot(size_t slotIdx) const {
        const FnOverloadSlot& sl = fnOverloadSlots_.at(slotIdx);
        if (ast_[sl.astIndex].isExtern) return ast_[sl.astIndex].value;
        if (preserveNames_) return ast_[sl.astIndex].value;
        return "__nexa_fn_" + std::to_string(slotIdx);
    }

    std::string emitThreadJobFn(const AstNode& e,
            const std::map<std::string, std::string>& varMap,
            const std::map<std::string, bool>& varIsString,
            const std::map<std::string, bool>& varIsFloat,
            const std::map<std::string, bool>& varIsChar,
            const std::map<std::string, bool>& varIsBool) {
        if (e.children.empty()) {
            size_t z = slotForZeroArgFunctionNamed(e.value);
            return "&" + cppFnNameForSlot(z);
        }
        std::ostringstream body;
        int spawnVarIdx = 0;
        std::map<std::string, std::string> spawnVarMap = varMap;
        std::map<std::string, bool> spawnStr = varIsString;
        std::map<std::string, bool> spawnConst;
        std::map<std::string, bool> spawnFloat = varIsFloat;
        std::map<std::string, bool> spawnChar = varIsChar;
        std::map<std::string, bool> spawnBool = varIsBool;
        std::map<std::string, bool> spawnEnum;
        emitBlockStatements(body, e.children, spawnVarMap, spawnVarIdx, spawnStr, spawnConst,
            spawnFloat, spawnChar, spawnBool, spawnEnum, "", false);
        // This lambda is spliced into the middle of an expression, so the body's first statement
        // would otherwise start on the same line as the `{`. A `#line` marker there could not be
        // rewritten (directives must start a line), so debug builds break the line first.
        return std::string("[=]() {") + (lineDirectives_ ? "\n" : " ") + body.str() + "}";
    }

    std::string cppFnNameForAstIndex(size_t astIndex) const {
        for (size_t s = 0; s < fnOverloadSlots_.size(); ++s) {
            if (fnOverloadSlots_[s].astIndex == astIndex) return cppFnNameForSlot(s);
        }
        throw std::runtime_error("Internal: no overload slot for function at AST index");
    }

    std::string lookupNexaDecl(const std::string& name) const {
        for (auto it = nexaDeclStack_.rbegin(); it != nexaDeclStack_.rend(); ++it) {
            auto j = it->find(name);
            if (j != it->end()) return j->second;
        }
        return "";
    }

    // What a Nexa integer type can hold, as the two numbers a slice literal has to fit inside:
    // the largest value, and the magnitude of the most negative one (0 for an unsigned type).
    // Nexa's integer types *are* the C++ types, so the widths follow the target's data model --
    // `long` is 32-bit on LLP64 Windows and on wasm32, 64-bit on the LP64 Unixes, and `size_t`
    // is 64-bit everywhere but wasm32. `valid` is false for anything that is not one of them.
    struct NexaIntRange {
        bool valid = false;
        unsigned long long maxValue = 0;
        unsigned long long maxNegMagnitude = 0;
    };

    NexaIntRange nexaIntTypeRange(const std::string& t) const {
        const bool long64 = (target_ != CppTarget::Windows && target_ != CppTarget::Wasm);
        const bool sizeT64 = (target_ != CppTarget::Wasm);
        const unsigned long long i32Max = 2147483647ULL;
        const unsigned long long u32Max = 4294967295ULL;
        const unsigned long long i64Max = 9223372036854775807ULL;
        const unsigned long long u64Max = 18446744073709551615ULL;
        const unsigned long long longMax = long64 ? i64Max : i32Max;
        NexaIntRange r;
        r.valid = true;
        if (t == "short")               { r.maxValue = 32767ULL;   r.maxNegMagnitude = 32768ULL; }
        else if (t == "unsigned short") { r.maxValue = 65535ULL; }
        else if (t == "int")            { r.maxValue = i32Max;     r.maxNegMagnitude = i32Max + 1; }
        else if (t == "unsigned int")   { r.maxValue = u32Max; }
        else if (t == "long")           { r.maxValue = longMax;    r.maxNegMagnitude = longMax + 1; }
        else if (t == "unsigned long")  { r.maxValue = long64 ? u64Max : u32Max; }
        else if (t == "size_t")         { r.maxValue = sizeT64 ? u64Max : u32Max; }
        else r.valid = false;
        return r;
    }

    // What one slice literal's elements need from their common type. `maxAt`/`negAt` name the
    // element that set each bound, so a refusal can point at real .nxa source rather than at
    // the generated C++. `usable` is false when the list is not all integers, in which case
    // nothing here is meaningful and the caller keeps its old answer.
    struct SliceLiteralNeed {
        bool usable = false;
        unsigned long long maxValue = 0;
        unsigned long long maxNegMagnitude = 0;
        const AstNode* maxAt = nullptr;
        const AstNode* negAt = nullptr;
    };

    // Noun phrase for one slice element, for use inside a diagnostic.
    std::string sliceElemDesc(const AstNode* e) const {
        if (!e) return "an element";
        if (e->type == AstNode::Type::ExprIntLiteral) return "the element " + e->value;
        std::string t = inferExprNexaType(*e);
        if (t.size() >= 9 && t.compare(0, 9, "arrayelt:") == 0) t = t.substr(9);
        return "the '" + t + "' element";
    }

    // The element type a slice literal needs so that every element keeps its value.
    //
    // Only the first element used to decide this, so `[2147483648, 1]` became a
    // std::vector<int> holding a number no int can represent. List-initialization refuses to
    // narrow, so the user got a clang error about machine-written C++ for a literal
    // SYNTAX/Core.txt calls legal. The type now has to hold *every* element: an integer
    // literal contributes its own value, and any other element contributes the full range of
    // its type (an `int` variable can hold a negative, so it forces a signed common type).
    //
    // The first element's type is still the answer whenever it already fits, so every slice
    // literal that compiled before is emitted exactly as it was. Otherwise the type climbs the
    // int -> long -> unsigned long -> size_t ladder to the first rung that holds the whole
    // list. It stays signed as long as it can because that is what the rest of NexaC does with
    // a wide literal -- emitIntLiteral only reaches for an unsigned type past i64 max -- so
    // `[2147483648][0] - 1` means what the bare literal would mean. A target whose `long` is
    // 32-bit lands on a different rung: Nexa's integer types are the C++ ones, and those are
    // not the same width everywhere. The value is held either way.
    //
    // When no rung holds it -- a list mixing a negative with a value above i64 max, or, on a
    // target whose `long` is 32-bit, anything that needs 64 bits and a sign -- `failing` is set
    // to the element to blame and `why` to the reason. checkSemantics turns that into a
    // [Nexa] Error naming the .nxa line, so codegen never emits the impossible vector.
    std::string arrayLiteralElemNexaType(const AstNode& arr, const AstNode** failing = nullptr,
                                         std::string* why = nullptr) const {
        if (failing) *failing = nullptr;
        if (arr.children.empty()) return "int";
        std::string elemT = inferExprNexaType(arr.children[0]);
        if (elemT.size() >= 9 && elemT.compare(0, 9, "arrayelt:") == 0) elemT = elemT.substr(9);
        // Strings, floats, chars, bools, structs, nested slices: not this function's business.
        if (!nexaIsNumericIntType(elemT)) return elemT;

        SliceLiteralNeed need;
        need.usable = true;
        for (const AstNode& c : arr.children) {
            unsigned long long maxV = 0;
            unsigned long long negV = 0;
            if (c.type == AstNode::Type::ExprIntLiteral) {
                const IntLiteralText lit = parseIntLiteralText(c.value);
                if (!lit.valid) { need.usable = false; break; }
                if (lit.negative) negV = lit.magnitude;
                else maxV = lit.magnitude;
            } else {
                std::string t = inferExprNexaType(c);
                if (t.size() >= 9 && t.compare(0, 9, "arrayelt:") == 0) t = t.substr(9);
                const NexaIntRange r = nexaIntTypeRange(t);
                if (!nexaIsNumericIntType(t) || !r.valid) { need.usable = false; break; }
                maxV = r.maxValue;
                negV = r.maxNegMagnitude;
            }
            if (!need.maxAt || maxV > need.maxValue) { need.maxValue = maxV; need.maxAt = &c; }
            if (negV > need.maxNegMagnitude) { need.maxNegMagnitude = negV; need.negAt = &c; }
        }
        if (!need.usable) return elemT;

        auto holds = [&](const std::string& t) {
            const NexaIntRange r = nexaIntTypeRange(t);
            return r.valid && need.maxValue <= r.maxValue && need.maxNegMagnitude <= r.maxNegMagnitude;
        };
        if (holds(elemT)) return elemT;
        static const char* const ladder[] = {"int", "long", "unsigned long", "size_t"};
        for (const char* t : ladder) {
            if (holds(t)) return t;
        }

        if (failing) {
            const NexaIntRange longR = nexaIntTypeRange("long");
            const std::string hint = (longR.maxValue <= 2147483647ULL)
                ? " (this target's 'long' is 32-bit)" : "";
            if (need.maxNegMagnitude > longR.maxNegMagnitude) {
                *failing = need.negAt;
                if (why) *why = "Slice literal: " + sliceElemDesc(need.negAt) +
                                " does not fit any integer type" + hint;
            } else if (need.negAt) {
                *failing = need.maxAt;
                if (why) *why = "Slice literal: no integer type holds both " +
                                sliceElemDesc(need.negAt) + " and " + sliceElemDesc(need.maxAt) + hint;
            } else {
                *failing = need.maxAt;
                if (why) *why = "Slice literal: " + sliceElemDesc(need.maxAt) +
                                " does not fit any integer type" + hint;
            }
        }
        // Unreachable in a build that ran checkSemantics; the widest rung keeps codegen honest
        // for any caller that did not.
        return nexaIntTypeRange("size_t").maxValue >= nexaIntTypeRange("unsigned long").maxValue
            ? "size_t" : "unsigned long";
    }

    std::string nexaDeclFromVariableAst(const AstNode& v) const {
        if (v.initUninitialized) {
            if (!v.declType.empty()) return v.declType;
            return "int";
        }
        if (!v.declType.empty()) return v.declType;
        if (v.initFromReadln || v.initFromFileRead) return "string";
        if (v.initFromDllLoad) return "int";
        if (v.initFromArray) {
            if (!v.children.empty() && v.children[0].type == AstNode::Type::StrMethod && v.children[0].value == "split") {
                return "[]string";
            }
            if (!v.children.empty() && v.children[0].type == AstNode::Type::FileCall && v.children[0].value == "list") {
                return "[]string";
            }
            if (!v.children.empty() && v.children[0].type == AstNode::Type::OsInfo && v.children[0].value == "environ") {
                return "[]string";
            }
            if (!v.children.empty() && v.children[0].type == AstNode::Type::ExprArrayLiteral) {
                const AstNode& arr = v.children[0];
                if (arr.children.empty()) return "[]int";
                std::string et = arrayLiteralElemNexaType(arr);
                if (nexaIsSliceType(et)) return et;
                return "[]" + et;
            }
            return "[]int";
        }
        if (!v.children.empty()) return inferExprNexaType(v.children[0]);
        if (v.initIsBool) return "bool";
        if (v.initIsFloat) return "float";
        if (v.initIsChar) return "char";
        if (!v.initValue.empty()) return "string";
        return "int";
    }

    std::string inferExprNexaType(const AstNode& e) const {
        switch (e.type) {
            case AstNode::Type::ExprIntLiteral: return "int";
            case AstNode::Type::ExprFloatLiteral: return "float";
            case AstNode::Type::ExprCharLiteral: return "char";
            case AstNode::Type::ExprBoolLiteral: return "bool";
            case AstNode::Type::ExprStringLiteral: return "string";
            case AstNode::Type::ExprVarRef: {
                std::string t = lookupNexaDecl(e.value);
                if (!t.empty()) return t;
                std::string fnT = uniqueNamedFnType(e.value);
                if (!fnT.empty()) return fnT;
                auto enIt = enumCppNames_.find(e.value);
                if (enIt != enumCppNames_.end()) return "enum:" + e.value;
                return "int";
            }
            case AstNode::Type::ExprStructLit:
                return std::string("struct:") + e.value;
            case AstNode::Type::ExprLambda:
                return fnTypeFromLambdaAst(e);
            case AstNode::Type::ExprCall: {
                if (e.children.empty()) return "int";
                std::string ct = inferExprNexaType(e.children[0]);
                if (nexaIsFnType(ct)) {
                    std::vector<std::string> params;
                    std::string ret;
                    if (nexaSplitFnType(ct, params, ret)) return ret;
                }
                return "int";
            }
            case AstNode::Type::ExprMember:
                if (!e.children.empty()) {
                    std::string ft = fieldTypeOfMemberExpr(e);
                    if (!ft.empty()) return ft;
                    // Colour.Red names a type and one of its variants, not a
                    // variable and a field, so there is no field type to find.
                    // It is still an enum, the same as a variable holding it:
                    // reporting int here said gfx.clear(Colour.Red, 0, 0) was
                    // fine and left clang to mention __nexa_E0.
                    if (e.children[0].type == AstNode::Type::ExprVarRef &&
                        lookupNexaDecl(e.children[0].value).empty() &&
                        enumCppNames_.count(e.children[0].value)) {
                        return "enum:" + e.children[0].value;
                    }
                }
                return "int";
            case AstNode::Type::FnCall: {
                if (e.initValue == "." || e.initValue == "->") {
                    if (!e.children.empty()) {
                        std::string recvT = inferExprNexaType(e.children[0]);
                        if (isPointerType(recvT)) recvT = pointerPointeeType(recvT);
                        if (nexaIsResultType(recvT)) {
                            if (e.value == "ok") return "bool";
                            if (e.value == "error") return "string";
                            if (e.value == "value") return nexaResultInner(recvT);
                        }
                        if (recvT == "json") {
                            if (e.value == "ok" || e.value == "is_null" || e.value == "is_bool" ||
                                e.value == "is_number" || e.value == "is_string" || e.value == "is_array" ||
                                e.value == "is_object" || e.value == "is_error" || e.value == "has" ||
                                e.value == "as_bool") {
                                return "bool";
                            }
                            if (e.value == "as_int" || e.value == "len") return "int";
                            if (e.value == "as_float") return "float";
                            if (e.value == "as_string" || e.value == "kind") return "string";
                            if (e.value == "keys") return "[]string";
                            if (e.value == "get") return "json";
                            if (e.value == "set" || e.value == "push" || e.value == "remove") return "void";
                        }
                        if (nexaIsSliceType(recvT)) {
                            if (e.value == "pop" || e.value == "min" || e.value == "max" ||
                                e.value == "sum") {
                                return nexaSliceElem(recvT);
                            }
                            if (e.value == "has") return "bool";
                            if (e.value == "join") return "string";
                            if (e.value == "push" || e.value == "clear" || e.value == "insert" ||
                                e.value == "sort" || e.value == "sort_desc" || e.value == "reverse" ||
                                e.value == "remove") {
                                return "void";
                            }
                        }
                        if (nexaIsMapType(recvT)) {
                            if (e.value == "has") return "bool";
                            if (e.value == "remove" || e.value == "clear") return "void";
                            if (e.value == "keys") {
                                std::string k, v;
                                if (nexaSplitMapType(recvT, k, v)) return "[]" + k;
                            }
                            if (e.value == "values") {
                                std::string k, v;
                                if (nexaSplitMapType(recvT, k, v)) return "[]" + v;
                            }
                        }
                        if (isStructDeclType(recvT)) {
                            std::string sn = structNameFromDecl(recvT);
                            auto sit = structMethods_.find(sn);
                            if (sit != structMethods_.end()) {
                                auto mit = sit->second.find(e.value);
                                if (mit != sit->second.end() && mit->second) {
                                    return inferReturnNexaType(*mit->second);
                                }
                            }
                        }
                    }
                    return libcHeaderReturnHint(e.value);
                }
                if (isHeaderImportedCall(e.value)) {
                    return libcHeaderReturnHint(e.value);
                }
                std::string varT = lookupNexaDecl(e.value);
                if (nexaIsFnType(varT)) {
                    std::vector<std::string> params;
                    std::string ret;
                    if (nexaSplitFnType(varT, params, ret)) return ret;
                }
                std::vector<std::string> argT;
                argT.reserve(e.children.size());
                for (const AstNode& a : e.children) argT.push_back(inferExprNexaType(a));
                size_t slot = resolveOverload(e.value, argT);
                return inferReturnNexaType(ast_[fnOverloadSlots_[slot].astIndex]);
            }
            // Integer operands keep their width through an operation: `long + 1` is a long,
            // not an int. Collapsing it to "int" made every consumer of the inferred type
            // wrong at once - printf picked "%d" for a 64-bit argument, and `let y = big + 1`
            // declared an int and truncated the sum.
            case AstNode::Type::ExprAdd:
                if (e.children.size() >= 2) {
                    std::string t0 = inferExprNexaType(e.children[0]);
                    std::string t1 = inferExprNexaType(e.children[1]);
                    if (t0 == "string" || t1 == "string") return "string";
                    if (t0 == "float" || t1 == "float") return "float";
                    return nexaArithIntResultType(t0, t1);
                }
                if (e.children.size() >= 1) {
                    std::string t0 = inferExprNexaType(e.children[0]);
                    return t0 == "float" ? "float" : nexaArithIntResultType(t0, t0);
                }
                return "int";
            case AstNode::Type::ExprSub:
            case AstNode::Type::ExprMul:
            case AstNode::Type::ExprDiv:
            case AstNode::Type::ExprMod:
            case AstNode::Type::ExprBitAnd:
            case AstNode::Type::ExprBitOr:
            case AstNode::Type::ExprBitXor:
            case AstNode::Type::ExprBitNot:
                if (e.children.size() >= 1) {
                    std::string t0 = inferExprNexaType(e.children[0]);
                    if (e.children.size() >= 2) {
                        std::string t1 = inferExprNexaType(e.children[1]);
                        if (t0 == "float" || t1 == "float") return "float";
                        return nexaArithIntResultType(t0, t1);
                    }
                    return t0 == "float" ? "float" : nexaArithIntResultType(t0, t0);
                }
                return "int";
            case AstNode::Type::ExprShl:
            case AstNode::Type::ExprShr:
                // A shift is not subject to the usual arithmetic conversions: its type is the
                // promoted left operand alone, so `1 << wide` is still an int.
                if (e.children.size() >= 1) {
                    std::string t0 = inferExprNexaType(e.children[0]);
                    return t0 == "float" ? "float" : nexaArithIntResultType(t0, t0);
                }
                return "int";
            case AstNode::Type::ExprLen: return "int";
            case AstNode::Type::ExprTrim: return "string";
            case AstNode::Type::ExprArrayLiteral: {
                if (!e.children.empty()) {
                    std::string et = arrayLiteralElemNexaType(e);
                    if (nexaIsSliceType(et)) return et;
                    return "[]" + et;
                }
                const std::string want = emptySliceTypeOf(e);
                return want.empty() ? std::string("[]int") : want;
            }
            case AstNode::Type::ExprArrayIndex: {
                if (e.children.size() < 2) {
                    std::string baseT = lookupNexaDecl(e.value);
                    return nexaIndexResultType(baseT);
                }
                return nexaIndexResultType(inferExprNexaType(e.children[0]));
            }
            case AstNode::Type::ExprSlice: {
                std::string baseT = e.children.empty() ? lookupNexaDecl(e.value) : inferExprNexaType(e.children[0]);
                if (baseT == "string") return "string";
                if (nexaIsSliceType(baseT)) return baseT;
                return "[]int";
            }
            case AstNode::Type::ExprNew:
                return std::string("*") + (e.value.empty() ? "int" : e.value);
            case AstNode::Type::ExprNull:
                return "null";
            case AstNode::Type::ExprAddrOf: {
                if (e.children.empty()) return "*int";
                return std::string("*") + inferExprNexaType(e.children[0]);
            }
            case AstNode::Type::ExprDeref: {
                if (e.children.empty()) return "int";
                std::string pt = inferExprNexaType(e.children[0]);
                if (isPointerType(pt)) return pointerPointeeType(pt);
                return "int";
            }
            case AstNode::Type::ExprCast:
                return e.value.empty() ? "int" : e.value;
            case AstNode::Type::ExprSizeof:
                return "int";
            case AstNode::Type::IoToInt: return "int";
            case AstNode::Type::RandomInt: return "int";
            case AstNode::Type::MathCall:
                // math.* operates in the floating-point domain and always yields float (double).
                // For an integer result, assign to an int (e.g. let n: int = math.floor(x);).
                return "float";
            case AstNode::Type::CryptoCall:
                return "string";
            case AstNode::Type::HttpCall:
                if (e.value == "request") return nexaMakeResultType("struct:HttpResponse");
                if (e.value == "localhost") return nexaMakeResultType("struct:HttpServer");
                if (e.value == "accept") return nexaMakeResultType("struct:HttpRequest");
                if (httpVerbReturnsInt(e.value)) return "int";
                return nexaMakeResultType("string");
            case AstNode::Type::TcpCall:
                // A handle, a byte count or a 1/0 -- all int; only the bytes
                // tcp.recv read are a string.
                return e.value == "recv" ? "string" : "int";
            case AstNode::Type::UdpCall:
                // The same, plus udp.sender: an address is text, the port
                // beside it is not.
                return (e.value == "recv" || e.value == "sender") ? "string" : "int";
            case AstNode::Type::ResultMake:
                if (e.value == "err") return nexaMakeResultType("void");
                if (e.children.empty()) return nexaMakeResultType("void");
                return nexaMakeResultType(inferExprNexaType(e.children[0]));
            case AstNode::Type::JsonCall:
                if (e.value == "stringify") return "string";
                return "json";
            case AstNode::Type::Gfx3dCall:
                // backend() is the only one that answers with text; the rest
                // are 1/0 or a size, and the draws are statements.
                if (e.value == "backend" || e.value == "typed") return "string";
                return "int";  // ambient reads back a level; the rest are 1/0 or a size
            case AstNode::Type::GfxCall:
                if (e.value == "title") return e.children.empty() ? "string" : "int";
                if (e.value == "drop" || e.value == "opendialog" || e.value == "openfile"
                    || e.value == "typed") return "string";
                if (e.value == "closed" || e.value == "key" || e.value == "pressed"
                    || e.value == "released" || e.value == "wheel" || e.value == "wheel_x"
                    || e.value == "open" || e.value == "resize"
                    || e.value == "mouse_x" || e.value == "mouse_y" || e.value == "mouse"
                    || e.value == "width" || e.value == "height" || e.value == "scale"
                    || e.value == "text_size" || e.value == "text" || e.value == "text_width"
                    || e.value == "text_height" || e.value == "get"
                    || e.value == "image" || e.value == "decode" || e.value == "image_w"
                    || e.value == "image_h" || e.value == "blit" || e.value == "blit_rot"
                    || e.value == "icon" || e.value == "cursor"
                    || e.value == "alpha" || e.value == "save"
                    || e.value == "sound" || e.value == "play" || e.value == "loop"
                    || e.value == "stop" || e.value == "volume"
                    || e.value == "audio" || e.value == "sample"
                    || e.value == "audio_queued" || e.value == "audio_flush"
                    || e.value == "fullscreen" || e.value == "borderless"
                    || e.value == "ontop" || e.value == "transparent"
                    || e.value == "poly" || e.value == "fill_poly") return "int";
                // What is left is the void set the parser refuses in value
                // position: the drawing calls plus poll, present, close and
                // maxfps. Anything value-returning must be named above, or
                // passing it to a function reports its type as void.
                return "void";
            case AstNode::Type::StrMethod:
                if (strMethodReturnsString(e.value)) return "string";
                if (strMethodReturnsBool(e.value)) return "bool";
                if (e.value == "split") return "[]string";
                return "int";  // len, index_of
            case AstNode::Type::OsGetProcessId: return "int";
            case AstNode::Type::OsSpawn: return "int";
            case AstNode::Type::OsWait: return "int";
            case AstNode::Type::OsKill: return "int";
            case AstNode::Type::OsCpuCount: return "int";
            case AstNode::Type::OsChdir: return "int";
            case AstNode::Type::OsGetVolume: return "int";
            case AstNode::Type::OsPlay: return "int";
            case AstNode::Type::OsSave: return "int";
            case AstNode::Type::OsGetBrightness: return "int";
            case AstNode::Type::OsHostname:
            case AstNode::Type::OsUsername:
            case AstNode::Type::OsHome:
            case AstNode::Type::OsPlatform:
            case AstNode::Type::OsExeDir:
            case AstNode::Type::OsExecutable:
            case AstNode::Type::OsTempDir:
            case AstNode::Type::OsArch:
            case AstNode::Type::OsWhich:
            case AstNode::Type::OsCwd:
            case AstNode::Type::OsGetenv:
            case AstNode::Type::OsExec:
            case AstNode::Type::OsLoad:
                return "string";
            case AstNode::Type::OsInfo: {
                const std::string& m = e.value;
                if (m == "total_mem" || m == "avail_mem") return "size_t";
                if (m == "page_size" || m == "uptime" || m == "isatty") return "int";
                if (m == "environ") return "[]string";
                return "string";
            }
            case AstNode::Type::TimeSeconds:
            case AstNode::Type::TimeMilliseconds:
                return "int";
            case AstNode::Type::TimeNowMs:
                return "float";
            case AstNode::Type::IoReadln:
                return "string";
            case AstNode::Type::OsClipGet:
                return "string";
            case AstNode::Type::FileRead:
            case AstNode::Type::IoGetline:
                return "string";
            case AstNode::Type::FileCall:
                if (e.value == "list") return "[]string";
                if (e.value == "cwd" || e.value == "abspath" || e.value == "join" ||
                    e.value == "dirname" || e.value == "basename" || e.value == "extension") {
                    return "string";
                }
                return "int";
            // Comparisons and the logical operators yield bool, not int — overload
            // resolution needs this to match `fn f(x: bool)` against `f(a == b)`.
            case AstNode::Type::CondNot:
            case AstNode::Type::CondEq:
            case AstNode::Type::CondNe:
            case AstNode::Type::CondLt:
            case AstNode::Type::CondGt:
            case AstNode::Type::CondLe:
            case AstNode::Type::CondGe:
            case AstNode::Type::CondAnd:
            case AstNode::Type::CondOr:
                return "bool";
            case AstNode::Type::ExprTernary:
                if (e.children.size() >= 3) {
                    std::string t1 = inferExprNexaType(e.children[1]);
                    std::string t2 = inferExprNexaType(e.children[2]);
                    if (t1 == "string" || t2 == "string") return "string";
                    if (t1 == "float" || t2 == "float") return "float";
                    if (t1 == "bool" && t2 == "bool") return "bool";
                    return t1;
                }
                return "int";
            default:
                return "int";
        }
    }

    std::string emitCCallArg(const AstNode& a,
                             const std::map<std::string, std::string>& varMap,
                             const std::map<std::string, bool>* varIsString,
                             const std::map<std::string, bool>* varIsFloat,
                             const std::map<std::string, bool>* varIsChar,
                             const std::map<std::string, bool>* varIsBool) {
        std::string arg = emitExpr(a, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        if (varIsString && exprIsString(a, *varIsString) && a.type != AstNode::Type::ExprStringLiteral) {
            arg += ".c_str()";
        }
        return arg;
    }

    std::string emitHeaderImportedCall(const AstNode& e,
                                       const std::map<std::string, std::string>& varMap,
                                       const std::map<std::string, bool>* varIsString,
                                       const std::map<std::string, bool>* varIsFloat,
                                       const std::map<std::string, bool>* varIsChar,
                                       const std::map<std::string, bool>* varIsBool) {
        std::string s = e.value + "(";
        for (size_t i = 0; i < e.children.size(); i++) {
            if (i > 0) s += ", ";
            s += emitCCallArg(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        }
        s += ")";
        if (libcHeaderReturnHint(e.value) == "string") {
            s = "__nexa_cstr(" + s + ")";
        }
        return s;
    }

    std::string emitFnCallCpp(const AstNode& e,
                              const std::map<std::string, std::string>& varMap,
                              const std::map<std::string, bool>* varIsString,
                              const std::map<std::string, bool>* varIsFloat,
                              const std::map<std::string, bool>* varIsChar,
                              const std::map<std::string, bool>* varIsBool) {
        if (e.initValue == "." || e.initValue == "->") {
            if (e.children.empty()) {
                throw std::runtime_error("Internal: method call missing receiver");
            }
            std::string recvT = inferExprNexaType(e.children[0]);
            std::string recvBare = recvT;
            if (isPointerType(recvBare)) recvBare = pointerPointeeType(recvBare);
            std::string recv = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            if (nexaIsResultType(recvBare)) {
                if (e.value == "ok" || e.value == "value" || e.value == "error") {
                    if (e.children.size() != 1) throw std::runtime_error(e.value + " expects no arguments");
                    return recv + "." + e.value + "()";
                }
                throw std::runtime_error("Unknown Result method '." + e.value + "()' (use .ok, .value, .error)");
            }
            if (recvBare == "json") {
                auto arg = [&](size_t i) {
                    return emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                };
                auto asJson = [&](size_t i) {
                    std::string s = arg(i);
                    if (inferExprNexaType(e.children[i]) == "json") return s;
                    return std::string("__nexa_json_from(") + s + ")";
                };
                if (e.value == "kind") {
                    if (e.children.size() != 1) throw std::runtime_error("kind expects no arguments");
                    return recv + ".kind_name()";
                }
                if (e.value == "ok" || e.value == "is_null" || e.value == "is_bool" ||
                    e.value == "is_number" || e.value == "is_string" || e.value == "is_array" ||
                    e.value == "is_object" || e.value == "is_error" || e.value == "as_bool" ||
                    e.value == "as_int" || e.value == "as_float" || e.value == "as_string" ||
                    e.value == "len" || e.value == "keys") {
                    if (e.children.size() != 1) throw std::runtime_error(e.value + " expects no arguments");
                    return recv + "." + e.value + "()";
                }
                if (e.value == "has" || e.value == "remove") {
                    if (e.children.size() != 2) throw std::runtime_error(e.value + " expects one argument");
                    return recv + "." + e.value + "(" + arg(1) + ")";
                }
                if (e.value == "get") {
                    if (e.children.size() != 2) throw std::runtime_error("get expects one argument");
                    std::string at = inferExprNexaType(e.children[1]);
                    if (at == "string") return recv + ".get(" + arg(1) + ")";
                    return recv + ".get_at(" + arg(1) + ")";
                }
                if (e.value == "set") {
                    if (e.children.size() != 3) throw std::runtime_error("set expects two arguments");
                    return recv + ".set(" + arg(1) + ", " + asJson(2) + ")";
                }
                if (e.value == "push") {
                    if (e.children.size() != 2) throw std::runtime_error("push expects one argument");
                    return recv + ".push(" + asJson(1) + ")";
                }
                throw std::runtime_error("Unknown Json method '." + e.value + "()'");
            }
            if (nexaIsSliceType(recvBare)) {
                if (e.value == "push") {
                    if (e.children.size() != 2) throw std::runtime_error("push expects one argument");
                    return recv + ".push_back(" +
                        emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
                }
                if (e.value == "pop") {
                    if (e.children.size() != 1) throw std::runtime_error("pop expects no arguments");
                    std::string et = nexaTypeToCpp(nexaSliceElem(recvBare));
                    return "([&](){ auto& __nexa_v = " + recv + "; " + et +
                        " __nexa_x = __nexa_v.back(); __nexa_v.pop_back(); return __nexa_x; })()";
                }
                if (e.value == "clear") {
                    if (e.children.size() != 1) throw std::runtime_error("clear expects no arguments");
                    return recv + ".clear()";
                }
                if (e.value == "insert") {
                    if (e.children.size() != 3) throw std::runtime_error("insert expects two arguments");
                    std::string idx = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    std::string val = emitExpr(e.children[2], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "([&](){ auto& __nexa_v = " + recv + "; int __nexa_i = " + idx +
                        "; int __nexa_n = (int)__nexa_v.size(); if (__nexa_i < 0) __nexa_i = 0; if (__nexa_i > __nexa_n) __nexa_i = __nexa_n; __nexa_v.insert(__nexa_v.begin() + __nexa_i, " +
                        val + "); })()";
                }
                if (e.value == "has") {
                    if (e.children.size() != 2) throw std::runtime_error("has expects one argument");
                    std::string val = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "([&](){ const auto& __nexa_v = " + recv + "; const auto& __nexa_x = " + val +
                        "; for (const auto& __nexa_e : __nexa_v) { if (__nexa_e == __nexa_x) return true; } return false; })()";
                }
                if (e.value == "remove") {
                    if (e.children.size() != 2) throw std::runtime_error("remove expects one argument");
                    // Unlike insert, which clamps, this is the pop rule: an index outside
                    // [0, len) is undefined behaviour, not a silent no-op.
                    std::string idx = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "([&](){ auto& __nexa_v = " + recv + "; int __nexa_i = " + idx +
                        "; __nexa_v.erase(__nexa_v.begin() + __nexa_i); })()";
                }
                if (e.value == "reverse") {
                    if (e.children.size() != 1) throw std::runtime_error("reverse expects no arguments");
                    return "([&](){ auto& __nexa_v = " + recv +
                        "; size_t __nexa_n = __nexa_v.size(); for (size_t __nexa_i = 0; __nexa_i + __nexa_i + 1 < __nexa_n; __nexa_i++) { auto __nexa_t = __nexa_v[__nexa_i]; __nexa_v[__nexa_i] = __nexa_v[__nexa_n - 1 - __nexa_i]; __nexa_v[__nexa_n - 1 - __nexa_i] = __nexa_t; } })()";
                }
                if (e.value == "sort" || e.value == "sort_desc") {
                    if (e.children.size() != 1) throw std::runtime_error(e.value + " expects no arguments");
                    return emitSliceSort(recv, e.value == "sort_desc");
                }
                if (e.value == "join") {
                    if (e.children.size() != 2) throw std::runtime_error("join expects one argument");
                    std::string sep = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "([&](){ const auto& __nexa_v = " + recv + "; std::string __nexa_s = " + sep +
                        "; std::string __nexa_o; for (size_t __nexa_i = 0; __nexa_i < __nexa_v.size(); __nexa_i++) { if (__nexa_i) __nexa_o += __nexa_s; __nexa_o += __nexa_v[__nexa_i]; } return __nexa_o; })()";
                }
                if (e.value == "min" || e.value == "max") {
                    if (e.children.size() != 1) throw std::runtime_error(e.value + " expects no arguments");
                    // Seeding from [0] is the pop rule again: an empty slice has no
                    // minimum, so asking for one is undefined behaviour.
                    std::string op = e.value == "min" ? " < " : " > ";
                    return "([&](){ const auto& __nexa_v = " + recv +
                        "; auto __nexa_m = __nexa_v[0]; for (size_t __nexa_i = 1; __nexa_i < __nexa_v.size(); __nexa_i++) { if (__nexa_v[__nexa_i]" + op +
                        "__nexa_m) __nexa_m = __nexa_v[__nexa_i]; } return __nexa_m; })()";
                }
                if (e.value == "sum") {
                    if (e.children.size() != 1) throw std::runtime_error("sum expects no arguments");
                    // The accumulator is the element type, not auto, so summing []i8
                    // wraps at 8 bits exactly as `a + b` on two i8 values would.
                    std::string et = nexaTypeToCpp(nexaSliceElem(recvBare));
                    return "([&](){ const auto& __nexa_v = " + recv + "; " + et +
                        " __nexa_s = 0; for (const auto& __nexa_e : __nexa_v) __nexa_s += __nexa_e; return __nexa_s; })()";
                }
            }
            if (nexaIsMapType(recvBare)) {
                if (e.value == "has") {
                    if (e.children.size() != 2) throw std::runtime_error("has expects one argument");
                    return "((" + recv + ").find(" +
                        emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) +
                        ") != (" + recv + ").end())";
                }
                if (e.value == "remove") {
                    if (e.children.size() != 2) throw std::runtime_error("remove expects one argument");
                    return recv + ".erase(" +
                        emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
                }
                if (e.value == "clear") {
                    if (e.children.size() != 1) throw std::runtime_error("clear expects no arguments");
                    return recv + ".clear()";
                }
                if (e.value == "keys") {
                    if (e.children.size() != 1) throw std::runtime_error("keys expects no arguments");
                    std::string k, v;
                    if (!nexaSplitMapType(recvBare, k, v)) throw std::runtime_error("Invalid map type");
                    return "([&](){ const auto& __nexa_m = " + recv + "; std::vector<" + nexaTypeToCpp(k) +
                        "> __nexa_ks; __nexa_ks.reserve(__nexa_m.size()); for (const auto& __nexa_kv : __nexa_m) __nexa_ks.push_back(__nexa_kv.first); return __nexa_ks; })()";
                }
                if (e.value == "values") {
                    if (e.children.size() != 1) throw std::runtime_error("values expects no arguments");
                    std::string k, v;
                    if (!nexaSplitMapType(recvBare, k, v)) throw std::runtime_error("Invalid map type");
                    return "([&](){ const auto& __nexa_m = " + recv + "; std::vector<" + nexaTypeToCpp(v) +
                        "> __nexa_vs; __nexa_vs.reserve(__nexa_m.size()); for (const auto& __nexa_kv : __nexa_m) __nexa_vs.push_back(__nexa_kv.second); return __nexa_vs; })()";
                }
            }
            if (isStructDeclType(recvBare)) {
                std::string sn = structNameFromDecl(recvBare);
                auto sit = structMethods_.find(sn);
                if (sit != structMethods_.end() && sit->second.count(e.value)) {
                    std::string s = recv + "." + e.value + "(";
                    for (size_t i = 1; i < e.children.size(); i++) {
                        if (i > 1) s += ", ";
                        s += emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    }
                    s += ")";
                    return s;
                }
            }
            std::string s = recv;
            s += e.initValue;
            s += e.value;
            s += "(";
            for (size_t i = 1; i < e.children.size(); i++) {
                if (i > 1) s += ", ";
                s += emitCCallArg(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            }
            s += ")";
            if (!modules_.hasCppHeader() && !isStructDeclType(recvBare) && !nexaIsSliceType(recvBare) &&
                !nexaIsMapType(recvBare)) {
                throw std::runtime_error("Unknown method '." + e.value + "()'");
            }
            return s;
        }
        if (isHeaderImportedCall(e.value)) {
            return emitHeaderImportedCall(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        }
        std::string calleeDecl = lookupNexaDecl(e.value);
        if (nexaIsFnType(calleeDecl)) {
            auto it = varMap.find(e.value);
            std::string recv = (it != varMap.end()) ? it->second : e.value;
            std::string s = recv + "(";
            for (size_t i = 0; i < e.children.size(); i++) {
                if (i) s += ", ";
                s += emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            }
            s += ")";
            return s;
        }
        std::vector<std::string> argT;
        argT.reserve(e.children.size());
        for (const AstNode& a : e.children) argT.push_back(inferExprNexaType(a));
        size_t slot = resolveOverload(e.value, argT);
        const FnOverloadSlot& sl = fnOverloadSlots_[slot];
        const AstNode& fn = ast_[sl.astIndex];
        std::string name = cppFnNameForSlot(slot);
        std::string s = name + "(";
        size_t emitCount = fn.paramNames.size();
        if (fn.isVariadic && e.children.size() > emitCount) emitCount = e.children.size();
        for (size_t i = 0; i < emitCount; i++) {
            if (i > 0) s += ", ";
            if (i < e.children.size()) {
                std::string arg = emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (fn.isExtern && varIsString) {
                    bool wantCStr = false;
                    if (i < fn.paramTypes.size()) {
                        const std::string& pt = fn.paramTypes[i];
                        wantCStr = isPointerType(pt) && pointerPointeeType(pt) == "char";
                    } else if (fn.isVariadic) {
                        wantCStr = exprIsString(e.children[i], *varIsString);
                    }
                    if (wantCStr && exprIsString(e.children[i], *varIsString) &&
                        e.children[i].type != AstNode::Type::ExprStringLiteral) {
                        arg += ".c_str()";
                    }
                }
                s += arg;
            } else {
                if (i >= fn.paramHasDefault.size() || !fn.paramHasDefault[i] || i >= fn.paramDefaults.size()) {
                    throw std::runtime_error("Internal: missing default for parameter in call to '" + e.value + "'");
                }
                s += emitExpr(fn.paramDefaults[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            }
        }
        s += ")";
        return s;
    }

    void buildFnOverloadTableAndInitGlobalNexaDecl() {
        fnOverloadSlots_.clear();
        globalNexaDecl_.clear();
        // "Every executable must have exactly one fn main()" (SYNTAX/Core.txt). Without
        // this the only complaint came from clang++, as a C++ redefinition error.
        size_t mainCount = 0;
        for (const AstNode& n : ast_) {
            if (n.type == AstNode::Type::MainFunction) mainCount++;
        }
        if (mainCount > 1) {
            throw std::runtime_error("Duplicate fn main(): a program must define exactly one");
        }
        for (size_t ai = 0; ai < ast_.size(); ++ai) {
            const AstNode& n = ast_[ai];
            if (n.type != AstNode::Type::Function) continue;
            FnOverloadSlot slot;
            slot.astIndex = ai;
            slot.name = n.value;
            slot.minArgs = fnMinArgs(n);
            for (size_t i = 0; i < n.paramNames.size(); i++) {
                slot.paramTypes.push_back(canonicalParamType(n, i));
            }
            for (const FnOverloadSlot& ex : fnOverloadSlots_) {
                if (ex.name == slot.name && ex.paramTypes == slot.paramTypes) {
                    throw std::runtime_error("Duplicate function overload '" + slot.name + "'");
                }
            }
            fnOverloadSlots_.push_back(std::move(slot));
        }
        nexaDeclStack_.clear();
        nexaDeclStack_.push_back({});
        for (const AstNode& node : ast_) {
            if (node.type != AstNode::Type::Variable) continue;
            std::string t = nexaDeclFromVariableAst(node);
            nexaDeclStack_.back()[node.value] = t;
            globalNexaDecl_[node.value] = t;
        }
        nexaDeclStack_.clear();
    }

    // =====================================================================
    // Semantic checks
    //
    // A read-only walk over the AST, run after the struct/enum/function tables
    // are built and before any code is emitted. Its job is to turn mistakes that
    // used to reach clang++ (or silently produce invalid C++) into Nexa errors
    // that name a file and a line:
    //
    //   1. use of an undefined name
    //   2. two `let`s of the same name in one scope
    //   3. an initializer whose type is clearly incompatible with `let x: T`
    //   4. a field that the struct does not have
    //   5. `break` / `continue` outside a loop or switch
    //
    // Scoping model: the walk reuses nexaDeclStack_ (name -> Nexa type) so that
    // inferExprNexaType/lookupNexaDecl work unchanged. Scope 0 holds globals; a
    // function pushes one scope for its parameters and the body pushes another,
    // so a local may shadow a parameter or a global but not a sibling `let`.
    // nexaDeclStack_ is left empty afterwards, which is the state emission expects.
    //
    // Where the checker cannot be sure, it stays quiet. Deliberate blind spots:
    //  - undefined-name checking is skipped entirely for programs that do C/C++
    //    interop (a user header or inline_cpp can introduce names Nexa never sees);
    //  - type mismatches are only reported between categories that are definite
    //    from the syntax alone, so implicit numeric conversions still compile.
    // =====================================================================

    // =====================================================================
    // EMPTY SLICE LITERALS
    // =====================================================================
    // `[]` has no element to infer an element type from, so on its own it types
    // as []int. That made the obvious way to start an empty list wrong for every
    // element type but int: `let names: []string = [];` emitted
    // `std::vector<std::string> x = std::vector<int>{}` and handed the user a
    // C++ error about a program they wrote correctly.
    //
    // The element type is always written down at the use site - the variable's
    // declared type, the struct field, the parameter, the return type - so this
    // walk runs once, before checkSemantics, and records it per literal, keyed by
    // node address. ast_ is const for the whole of transpile(), so the addresses
    // are stable; paramByRef_ keys the same way. Recording it before the checks
    // means overload resolution and inferExprNexaType see the real type too, not
    // just codegen.
    //
    // Variable types come from one flat map per function rather than a scope
    // stack: the walk only ever asks "what type was this name declared with",
    // and a name declared twice with two different types is marked unknown
    // instead of guessed. A literal the walk cannot place is absent from the map
    // and keeps the historical []int, so this only ever turns a broken program
    // into a working one.

    std::map<const AstNode*, std::string> emptySliceType_;
    std::map<std::string, std::string> stampVarTypes_;  // name -> declared type; "" = ambiguous

    // The slice type an empty `[]` was written for, or "" when it had no context.
    std::string emptySliceTypeOf(const AstNode& e) const {
        auto it = emptySliceType_.find(&e);
        return it == emptySliceType_.end() ? std::string() : it->second;
    }

    // Its element type, for codegen. An unplaced literal keeps the historical int.
    std::string emptySliceElemNexaType(const AstNode& e) const {
        const std::string want = emptySliceTypeOf(e);
        return nexaIsSliceType(want) ? nexaSliceElem(want) : std::string("int");
    }

    void stampEmptySliceLiterals() {
        for (const AstNode& n : ast_) {
            if (n.type == AstNode::Type::Variable) {
                stampVarTypes_ = globalNexaDecl_;
                for (const AstNode& c : n.children) stampNode(c, n.declType, "");
            } else if (n.type == AstNode::Type::Function || n.type == AstNode::Type::MainFunction) {
                stampFunction(n);
            } else if (n.type == AstNode::Type::StructDef) {
                for (const AstNode& m : n.children) {
                    if (m.type == AstNode::Type::Function) stampFunction(m);
                }
            }
        }
        stampVarTypes_.clear();
    }

    void stampFunction(const AstNode& fn) {
        if (fn.isExtern) return;
        stampVarTypes_ = globalNexaDecl_;
        if (fn.type == AstNode::Type::MainFunction) {
            for (const std::string& p : fn.paramNames) stampVarTypes_[p] = "[]string";
        } else {
            for (size_t i = 0; i < fn.paramNames.size(); i++) {
                stampVarTypes_[fn.paramNames[i]] = canonicalParamType(fn, i);
            }
        }
        if (!fn.receiverType.empty()) stampVarTypes_["self"] = fn.receiverType;
        stampCollectLocals(fn.children);
        const std::string ret = inferReturnNexaType(fn);
        // A default is substituted at the call site, so it is typed by its own parameter.
        for (size_t i = 0; i < fn.paramDefaults.size(); i++) {
            stampNode(fn.paramDefaults[i], canonicalParamType(fn, i), ret);
        }
        for (const AstNode& c : fn.children) stampNode(c, "", ret);
    }

    // Records every `let` in the body. A second `let` of the same name with a
    // different type, or a for-in binding reusing a name, makes it unknown rather
    // than wrong - the flat map has no way to tell the two scopes apart.
    void stampCollectLocals(const std::vector<AstNode>& body) {
        for (const AstNode& s : body) {
            if (s.type == AstNode::Type::Variable) {
                auto it = stampVarTypes_.find(s.value);
                if (it == stampVarTypes_.end()) stampVarTypes_[s.value] = s.declType;
                else if (it->second != s.declType) it->second = "";
            } else if (s.type == AstNode::Type::ForIn) {
                if (!s.value.empty()) stampVarTypes_[s.value] = "";
                if (!s.initValue.empty()) stampVarTypes_[s.initValue] = "";
            }
            stampCollectLocals(s.children);
        }
    }

    std::string stampTypeOfName(const std::string& name) const {
        auto it = stampVarTypes_.find(name);
        return it == stampVarTypes_.end() ? std::string() : it->second;
    }

    // Nexa type of a plain lvalue chain (a name, a field of one, an index of one),
    // resolved from stampVarTypes_ alone. "" means the walk cannot place it.
    std::string stampChainType(const AstNode& e) const {
        switch (e.type) {
            case AstNode::Type::ExprVarRef:
                return stampTypeOfName(e.value);
            case AstNode::Type::ExprDeref: {
                if (e.children.empty()) return std::string();
                const std::string base = stampChainType(e.children[0]);
                return isPointerType(base) ? pointerPointeeType(base) : std::string();
            }
            case AstNode::Type::ExprMember: {
                if (e.children.empty()) return std::string();
                std::string base = stampChainType(e.children[0]);
                if (isPointerType(base)) base = pointerPointeeType(base);
                if (!isStructDeclType(base)) return std::string();
                auto sit = structFields_.find(structNameFromDecl(base));
                if (sit == structFields_.end()) return std::string();
                auto fit = sit->second.find(e.value);
                return fit == sit->second.end() ? std::string() : fit->second;
            }
            case AstNode::Type::ExprArrayIndex: {
                const std::string base = e.children.size() >= 2 ? stampChainType(e.children[0])
                                                                : stampTypeOfName(e.value);
                return base.empty() ? base : nexaIndexResultType(base);
            }
            default:
                return std::string();
        }
    }

    // Declared type of parameter `i` of `name`, when every function with that
    // name agrees on it. Overloads that disagree leave the argument unplaced.
    std::string stampParamType(const std::string& name, size_t i) const {
        std::string found;
        bool any = false;
        for (const FnOverloadSlot& sl : fnOverloadSlots_) {
            if (sl.name != name || i >= sl.paramTypes.size()) continue;
            if (!any) { found = sl.paramTypes[i]; any = true; }
            else if (found != sl.paramTypes[i]) return std::string();
        }
        return any ? found : std::string();
    }

    // Walks one node. `want` is the Nexa type this position is declared to hold
    // ("" when unknown); `fnRet` is the enclosing function's return type.
    void stampNode(const AstNode& n, const std::string& want, const std::string& fnRet) {
        switch (n.type) {
            case AstNode::Type::ExprArrayLiteral: {
                if (n.children.empty()) {
                    if (nexaIsSliceType(want)) emptySliceType_[&n] = want;
                    return;
                }
                const std::string elem = nexaIsSliceType(want) ? nexaSliceElem(want) : std::string();
                for (const AstNode& c : n.children) stampNode(c, elem, fnRet);
                return;
            }
            case AstNode::Type::ExprStructLit: {
                auto sit = structFields_.find(n.value);
                for (size_t i = 0; i < n.children.size(); i++) {
                    std::string field;
                    if (i < n.paramNames.size() && sit != structFields_.end()) {
                        auto fit = sit->second.find(n.paramNames[i]);
                        if (fit != sit->second.end()) field = fit->second;
                    }
                    stampNode(n.children[i], field, fnRet);
                }
                return;
            }
            case AstNode::Type::FnCall: {
                if (n.initValue == "." || n.initValue == "->") {
                    if (n.children.empty()) return;
                    stampNode(n.children[0], "", fnRet);
                    std::string recv = stampChainType(n.children[0]);
                    if (isPointerType(recv)) recv = pointerPointeeType(recv);
                    for (size_t i = 1; i < n.children.size(); i++) {
                        stampNode(n.children[i], stampMethodArgType(recv, n.value, i), fnRet);
                    }
                    return;
                }
                for (size_t i = 0; i < n.children.size(); i++) {
                    stampNode(n.children[i], stampParamType(n.value, i), fnRet);
                }
                return;
            }
            case AstNode::Type::ExprCall: {
                if (n.children.empty()) return;
                stampNode(n.children[0], "", fnRet);
                std::vector<std::string> params;
                std::string ret;
                const bool known = nexaSplitFnType(stampChainType(n.children[0]), params, ret);
                for (size_t i = 1; i < n.children.size(); i++) {
                    const size_t p = i - 1;
                    stampNode(n.children[i], known && p < params.size() ? params[p] : std::string(), fnRet);
                }
                return;
            }
            case AstNode::Type::ExprTernary: {
                if (n.children.size() != 3) break;
                stampNode(n.children[0], "", fnRet);
                stampNode(n.children[1], want, fnRet);
                stampNode(n.children[2], want, fnRet);
                return;
            }
            case AstNode::Type::ExprLambda: {
                const std::string lambdaRet = inferReturnNexaType(n);
                for (const AstNode& c : n.children) stampNode(c, "", lambdaRet);
                return;
            }
            case AstNode::Type::Variable:
                for (const AstNode& c : n.children) stampNode(c, n.declType, fnRet);
                return;
            case AstNode::Type::Return:
                for (const AstNode& c : n.children) stampNode(c, fnRet, fnRet);
                return;
            case AstNode::Type::Assignment:
                for (const AstNode& c : n.children) stampNode(c, stampTypeOfName(n.value), fnRet);
                return;
            case AstNode::Type::AssnIndex: {
                if (n.children.empty()) return;
                std::string t = stampTypeOfName(n.value);
                for (size_t i = 0; i + 1 < n.children.size(); i++) {
                    stampNode(n.children[i], "", fnRet);
                    if (!t.empty()) t = nexaIndexResultType(t);
                }
                // A compound form (`xs[i] += e`) is arithmetic, never a fresh slice.
                const bool plain = n.initValue.empty() || n.initValue == "=";
                stampNode(n.children.back(), plain ? t : std::string(), fnRet);
                return;
            }
            case AstNode::Type::AssnMember: {
                if (n.children.size() < 2) break;
                stampNode(n.children[0], "", fnRet);
                stampNode(n.children[1], n.value == "=" ? stampChainType(n.children[0]) : std::string(), fnRet);
                return;
            }
            case AstNode::Type::AssnDeref: {
                if (n.children.size() < 2) break;
                stampNode(n.children[0], "", fnRet);
                const std::string ptr = stampChainType(n.children[0]);
                const bool plain = n.value == "=";
                stampNode(n.children[1],
                          (plain && isPointerType(ptr)) ? pointerPointeeType(ptr) : std::string(), fnRet);
                return;
            }
            default:
                break;
        }
        for (const AstNode& c : n.children) stampNode(c, "", fnRet);
    }

    // Declared type of argument `argIdx` (1-based past the receiver) of a builtin
    // or user method called on a receiver of type `recv`.
    std::string stampMethodArgType(const std::string& recv, const std::string& method, size_t argIdx) const {
        if (recv.empty()) return std::string();
        if (nexaIsSliceType(recv)) {
            const std::string elem = nexaSliceElem(recv);
            if (argIdx == 1 && (method == "push" || method == "has" || method == "contains" ||
                                method == "index_of")) {
                return elem;
            }
            if (argIdx == 2 && method == "insert") return elem;
            if (argIdx == 1 && method == "join") return "string";
            if (argIdx == 1 && method == "remove") return "int";
            return std::string();
        }
        if (nexaIsMapType(recv)) {
            std::string k, v;
            if (!nexaSplitMapType(recv, k, v)) return std::string();
            if (argIdx == 1 && (method == "has" || method == "remove")) return k;
            return std::string();
        }
        if (isStructDeclType(recv)) {
            auto mit = structMethods_.find(structNameFromDecl(recv));
            if (mit == structMethods_.end()) return std::string();
            auto fit = mit->second.find(method);
            if (fit == mit->second.end() || fit->second == nullptr) return std::string();
            const AstNode& fn = *fit->second;
            const size_t p = argIdx - 1;
            return p < fn.paramNames.size() ? canonicalParamType(fn, p) : std::string();
        }
        return std::string();
    }

    void checkSemantics() {
        semNameChecks_ = !modules_.hasInlineCpp();
        if (semNameChecks_) {
            for (const AstNode& n : ast_) {
                if (n.type == AstNode::Type::CppHeaderInclude || n.type == AstNode::Type::InlineCpp) {
                    semNameChecks_ = false;
                    break;
                }
            }
        }
        semLoc_ = nullptr;
        semLoopDepth_ = 0;
        semSwitchDepth_ = 0;

        std::set<std::string> seenGlobals;
        for (const AstNode& n : ast_) {
            if (n.type != AstNode::Type::Variable) continue;
            if (!seenGlobals.insert(n.value).second) {
                semError(n, "Redeclaration of '" + n.value + "' in the same scope");
            }
        }

        nexaDeclStack_.clear();
        nexaDeclStack_.push_back(globalNexaDecl_);
        for (const AstNode& n : ast_) {
            if (n.type != AstNode::Type::Variable) continue;
            semNoteLoc(n);
            for (const AstNode& c : n.children) semExpr(c);
            semCheckVariableInit(n);
        }
        for (const AstNode& n : ast_) {
            if (n.type == AstNode::Type::Function || n.type == AstNode::Type::MainFunction) {
                semFunction(n);
            } else if (n.type == AstNode::Type::StructDef) {
                for (const AstNode& m : n.children) {
                    if (m.type == AstNode::Type::Function) semFunction(m);
                }
            }
        }
        nexaDeclStack_.clear();
        semLoc_ = nullptr;
    }

    mutable const AstNode* semLoc_ = nullptr;  // nearest node that carries a source location
    int semLoopDepth_ = 0;
    int semSwitchDepth_ = 0;
    bool semNameChecks_ = true;

    void semNoteLoc(const AstNode& n) const {
        if (n.line != 0) semLoc_ = &n;
    }

    [[noreturn]] void semError(const AstNode& at, const std::string& msg) const {
        const AstNode* loc = (at.line != 0) ? &at : semLoc_;
        std::string full;
        if (loc && !loc->srcFile.empty()) full += loc->srcFile + ": ";
        full += msg;
        if (loc && loc->line != 0) full += " at line " + std::to_string(loc->line);
        throw std::runtime_error(full);
    }

    void semDeclare(const std::string& name, const std::string& type) {
        if (name.empty() || nexaDeclStack_.empty()) return;
        nexaDeclStack_.back()[name] = type.empty() ? std::string("int") : type;
    }

    void semFunction(const AstNode& fn) {
        if (fn.isExtern) return;
        nexaDeclStack_.push_back({});
        if (fn.type == AstNode::Type::MainFunction) {
            // fn main(argv) receives the command line as []string.
            for (const std::string& p : fn.paramNames) semDeclare(p, "[]string");
        } else {
            for (size_t i = 0; i < fn.paramNames.size(); i++) {
                semDeclare(fn.paramNames[i], canonicalParamType(fn, i));
            }
        }
        if (!fn.receiverType.empty()) semDeclare("self", fn.receiverType);
        for (const AstNode& d : fn.paramDefaults) semExpr(d);

        const int savedLoop = semLoopDepth_;
        const int savedSwitch = semSwitchDepth_;
        semLoopDepth_ = 0;
        semSwitchDepth_ = 0;
        semBlock(fn.children);
        semLoopDepth_ = savedLoop;
        semSwitchDepth_ = savedSwitch;
        nexaDeclStack_.pop_back();
    }

    void semBlock(const std::vector<AstNode>& stmts) {
        nexaDeclStack_.push_back({});
        for (const AstNode& s : stmts) semStmt(s);
        nexaDeclStack_.pop_back();
    }

    void semStmt(const AstNode& s) {
        semNoteLoc(s);
        switch (s.type) {
            case AstNode::Type::Variable: {
                if (!nexaDeclStack_.empty() && nexaDeclStack_.back().count(s.value)) {
                    semError(s, "Redeclaration of '" + s.value + "' in the same scope");
                }
                // The initializer is checked before the name is bound, so
                // `let x = x;` reports the outer x (or an undefined name).
                for (const AstNode& c : s.children) semExpr(c);
                semCheckVariableInit(s);
                semDeclare(s.value, nexaDeclFromVariableAst(s));
                break;
            }
            case AstNode::Type::Block:
                semBlock(s.children);
                break;
            case AstNode::Type::IfElse: {
                if (!s.children.empty()) semExpr(s.children[0]);
                if (s.children.size() > 1) semBlock(s.children[1].children);
                if (s.children.size() > 2) {
                    const AstNode& tail = s.children[2];
                    if (tail.type == AstNode::Type::IfElse) semStmt(tail);
                    else semBlock(tail.children);
                }
                break;
            }
            case AstNode::Type::While: {
                if (!s.children.empty()) semExpr(s.children[0]);
                semLoopDepth_++;
                if (s.children.size() > 1) semBlock(s.children[1].children);
                semLoopDepth_--;
                break;
            }
            case AstNode::Type::For: {
                if (!s.children.empty()) semExpr(s.children[0]);
                nexaDeclStack_.push_back({});
                semDeclare(s.value, "int");
                semLoopDepth_++;
                if (s.children.size() > 1) semBlock(s.children[1].children);
                semLoopDepth_--;
                nexaDeclStack_.pop_back();
                break;
            }
            case AstNode::Type::ForIn: {
                if (!s.children.empty()) semExpr(s.children[0]);
                std::string collT = s.children.empty() ? std::string() : inferExprNexaType(s.children[0]);
                std::string keyT = "int";
                std::string valT = "int";
                if (nexaIsSliceType(collT)) {
                    keyT = nexaSliceElem(collT);
                } else if (nexaIsMapType(collT)) {
                    nexaSplitMapType(collT, keyT, valT);
                } else if (collT == "string") {
                    keyT = "string";
                }
                nexaDeclStack_.push_back({});
                semDeclare(s.value, keyT);
                if (!s.initValue.empty()) semDeclare(s.initValue, valT);
                semLoopDepth_++;
                if (s.children.size() > 1) semBlock(s.children[1].children);
                semLoopDepth_--;
                nexaDeclStack_.pop_back();
                break;
            }
            case AstNode::Type::Switch: {
                if (!s.children.empty()) semExpr(s.children[0]);
                semSwitchDepth_++;
                for (size_t i = 1; i < s.children.size(); i++) {
                    if (s.children[i].type != AstNode::Type::SwitchCase) continue;
                    semBlock(s.children[i].children);
                }
                semSwitchDepth_--;
                break;
            }
            case AstNode::Type::TryCatch: {
                if (!s.children.empty()) semBlock(s.children[0].children);
                nexaDeclStack_.push_back({});
                if (!s.value.empty()) semDeclare(s.value, "string");  // catch binds the message
                if (s.children.size() > 1) semBlock(s.children[1].children);
                nexaDeclStack_.pop_back();
                break;
            }
            case AstNode::Type::Break:
                if (semLoopDepth_ == 0 && semSwitchDepth_ == 0) {
                    semError(s, "'break' outside of a loop or switch");
                }
                break;
            case AstNode::Type::Continue:
                if (semLoopDepth_ == 0) {
                    semError(s, "'continue' outside of a loop");
                }
                break;
            case AstNode::Type::Label:
            case AstNode::Type::Goto:
            case AstNode::Type::InlineCpp:
                break;
            // Assignment forms name their target in `value` rather than through an
            // ExprVarRef child, so the target has to be checked explicitly.
            case AstNode::Type::Assignment:
            case AstNode::Type::AssnAdd:
            case AstNode::Type::AssnSub:
            case AstNode::Type::AssnMul:
            case AstNode::Type::AssnDiv:
            case AstNode::Type::AssnMod:
            case AstNode::Type::AssnBitAnd:
            case AstNode::Type::AssnBitOr:
            case AstNode::Type::AssnBitXor:
            case AstNode::Type::AssnShl:
            case AstNode::Type::AssnShr:
            case AstNode::Type::AssnIndex:
            case AstNode::Type::IncPost:
            case AstNode::Type::DecPost:
                semCheckNameUse(s, s.value);
                for (const AstNode& c : s.children) semExpr(c);
                break;
            default:
                semExpr(s);
                break;
        }
    }

    void semExpr(const AstNode& e) {
        semNoteLoc(e);
        switch (e.type) {
            case AstNode::Type::ExprVarRef:
                semCheckNameUse(e, e.value);
                break;
            case AstNode::Type::ExprMember:
                semCheckMemberAccess(e);
                break;
            case AstNode::Type::ExprLambda: {
                nexaDeclStack_.push_back({});
                for (size_t i = 0; i < e.paramNames.size(); i++) {
                    semDeclare(e.paramNames[i], i < e.paramTypes.size() ? e.paramTypes[i] : std::string("int"));
                }
                const int savedLoop = semLoopDepth_;
                const int savedSwitch = semSwitchDepth_;
                semLoopDepth_ = 0;
                semSwitchDepth_ = 0;
                for (const AstNode& c : e.children) semStmt(c);
                semLoopDepth_ = savedLoop;
                semSwitchDepth_ = savedSwitch;
                nexaDeclStack_.pop_back();
                break;
            }
            case AstNode::Type::Block:
                semBlock(e.children);
                break;
            case AstNode::Type::InlineCpp:
                break;
            case AstNode::Type::ExprArrayLiteral:
                for (const AstNode& c : e.children) semExpr(c);
                semCheckSliceLiteralWidth(e);
                break;
            case AstNode::Type::GfxCall:
                for (const AstNode& c : e.children) semExpr(c);
                semCheckGfxPoly(e);
                semCheckBuiltinArgTypes(e);
                break;
            case AstNode::Type::Gfx3dCall:
                for (const AstNode& c : e.children) semExpr(c);
                semCheckBuiltinArgTypes(e);
                break;
            case AstNode::Type::FnCall:
            case AstNode::Type::ExprCall:
                for (const AstNode& c : e.children) semExpr(c);
                semCheckSliceAlgo(e);
                break;
            default:
                for (const AstNode& c : e.children) semExpr(c);
                semCheckBuiltinArgTypes(e);
                break;
        }
    }

    // A slice literal whose elements share no integer type is refused here, by line, instead of
    // reaching clang as a narrowing error about generated C++ the user never wrote.
    void semCheckSliceLiteralWidth(const AstNode& e) const {
        const AstNode* bad = nullptr;
        std::string why;
        arrayLiteralElemNexaType(e, &bad, &why);
        if (bad) semError(*bad, why);
    }

    // gfx.poly / gfx.fill_poly are the only gfx calls that take a slice, and the
    // runtime takes the points through a template so any vector of integers
    // works. That makes []string the user's problem to hear about here, by line,
    // rather than as a page of C++ template errors about generated code.
    void semCheckGfxPoly(const AstNode& e) const {
        if (e.value != "poly" && e.value != "fill_poly") return;
        if (e.children.size() < 2) return;
        for (int i = 0; i < 2; i++) {
            std::string t = inferExprNexaType(e.children[(size_t)i]);
            if (nexaIsSliceType(t) && nexaIsNumericIntType(nexaSliceElem(t))) continue;
            semError(e, "gfx." + e.value + "(xs, ys, r, g, b) expects []int point lists, but " +
                std::string(i == 0 ? "xs" : "ys") + " is '" + (t.empty() ? std::string("unknown") : t) + "'");
        }
    }

    // ---- module builtin argument types ------------------------------------
    //
    // BOB-53 refused a void module builtin in value position because the
    // runtime signature said void. The argument types in those same
    // signatures were still nobody's business at Nexa level, so
    // `gfx.open(100, 100, "t")` transpiled and clang was left to explain it:
    // an error about __nexa_gfx_open(const std::string&, int, int, int),
    // generated code the user never wrote.
    //
    // So: one table per namespace, one row per builtin, each row read straight
    // off the runtime signature. A letter per parameter says what the
    // generated call hands it to.
    //
    //   n  a number         an int or double parameter
    //   t  text             const std::string&, reached with no conversion
    //   x  text or a number the transpiler converts on the way in (os.* and
    //                       file.write's content through std::to_string) or
    //                       the call picks its runtime entry point by which
    //                       one it got (gfx.blit's src, gfx.icon's)
    //   h  header lines     []string
    //   s  a server         the struct http.localhost() hands back
    //   r  a request        the struct http.accept() hands back
    //   .  unchecked        checked elsewhere (gfx.poly's point lists), or the
    //                       parameter really does take anything
    //
    // Coarse on purpose. A number is a number whatever its width, and a float
    // where an int is wanted narrows the way C narrows -- a conversion the
    // language allows, so not this check's business. What is caught is the
    // categorical slip: text for a number, a number for text, and any
    // aggregate (slice, map, struct, enum, Result, json, pointer) where a
    // scalar is wanted.
    //
    // Parameter names are read out of the `call` text rather than listed
    // twice, so a row can only ever disagree with itself in count. Where the
    // names change with the argument count (gfx.blit) there is a row per
    // form, narrowest first, and the first row that can hold the call wins.
    struct BuiltinArgRow {
        const char* method;  // "" for the last row of a table
        const char* call;    // shown to the user; the parameter names live here
        const char* kinds;   // a letter per parameter; a trailing '*' repeats the last
    };

    // std/gfx. Zero-argument builtins (close, poll, present, closed, width,
    // height, scale, wheel, wheel_x, typed, mouse_x, mouse_y, drop,
    // audio_queued, audio_flush) need no row.
    // std/gfx3d. Zero-argument builtins (close, poll, present, closed, width,
    // height, backend) need no row.
    static const BuiltinArgRow* gfx3dArgRows() {
        static const BuiltinArgRow rows[] = {
            {"open",        "gfx3d.open(title, w, h)",                                "tnn"},
            {"clear",       "gfx3d.clear(r, g, b)",                                   "nnn"},
            {"camera",      "gfx3d.camera(ex, ey, ez, tx, ty, tz)",                   "nnnnnn"},
            {"perspective", "gfx3d.perspective(fov, near, far)",                      "nnn"},
            {"tri",         "gfx3d.tri(x1, y1, z1, x2, y2, z2, x3, y3, z3, r, g, b)", "nnnnnnnnnnnn"},
            {"cube",        "gfx3d.cube(x, y, z, size, r, g, b)",                     "nnnnnnn"},
            {"sphere",      "gfx3d.sphere(x, y, z, radius, r, g, b)",                 "nnnnnnn"},
            {"box",         "gfx3d.box(x, y, z, w, h, d, r, g, b)",                   "nnnnnnnnn"},
            {"line3",       "gfx3d.line3(x1, y1, z1, x2, y2, z2, r, g, b)",           "nnnnnnnnn"},
            {"grid",        "gfx3d.grid(size, step, r, g, b)",                        "nnnnn"},
            {"translate",   "gfx3d.translate(x, y, z)",                               "nnn"},
            {"rotate",      "gfx3d.rotate(rx, ry, rz)",                               "nnn"},
            {"scale",       "gfx3d.scale(s)",                                         "n"},
            {"ambient",     "gfx3d.ambient([level])",                                 "n"},
            {"light",       "gfx3d.light(x, y, z[, r, g, b])",                        "nnnnnn"},
            {"capsule",     "gfx3d.capsule(x1, y1, z1, x2, y2, z2, radius, r, g, b)", "nnnnnnnnnn"},
            {"cylinder",    "gfx3d.cylinder(x1, y1, z1, x2, y2, z2, radius, r, g, b)","nnnnnnnnnn"},
            {"cone",        "gfx3d.cone(x1, y1, z1, x2, y2, z2, radius, r, g, b)",    "nnnnnnnnnn"},
            {"maxfps",      "gfx3d.maxfps(fps)",                                      "n"},
            {"renderer",    "gfx3d.renderer(name)",                                   "t"},
            {"key",         "gfx3d.key(name)",                                        "t"},
            {"pressed",     "gfx3d.pressed(name)",                                    "t"},
            {"released",    "gfx3d.released(name)",                                   "t"},
            {"mouse",       "gfx3d.mouse(button)",                                    "t"},
            {"", nullptr, nullptr},
        };
        return rows;
    }

    static const BuiltinArgRow* gfxArgRows() {
        static const BuiltinArgRow rows[] = {
            {"open",            "gfx.open(title, w, h[, scale])",                  "tnnn"},
            {"resize",          "gfx.resize(w, h[, scale])",                       "nnn"},
            {"maxfps",          "gfx.maxfps(fps)",                                 "n"},
            {"key",             "gfx.key(name)",                                   "t"},
            {"pressed",         "gfx.pressed(name)",                               "t"},
            {"released",        "gfx.released(name)",                              "t"},
            {"mouse",           "gfx.mouse(name)",                                 "t"},
            {"clear",           "gfx.clear(r, g, b)",                              "nnn"},
            {"plot",            "gfx.plot(x, y, r, g, b)",                         "nnnnn"},
            {"get",             "gfx.get(x, y)",                                   "nn"},
            {"fill",            "gfx.fill(x, y, w, h, r, g, b)",                   "nnnnnnn"},
            {"rect",            "gfx.rect(x, y, w, h, r, g, b)",                   "nnnnnnn"},
            {"line",            "gfx.line(x1, y1, x2, y2, r, g, b[, t])",          "nnnnnnnn"},
            {"circle",          "gfx.circle(cx, cy, rad, r, g, b)",                "nnnnnn"},
            {"fill_circle",     "gfx.fill_circle(cx, cy, rad, r, g, b)",           "nnnnnn"},
            {"ellipse",         "gfx.ellipse(cx, cy, rx, ry, r, g, b)",            "nnnnnnn"},
            {"fill_ellipse",    "gfx.fill_ellipse(cx, cy, rx, ry, r, g, b)",       "nnnnnnn"},
            {"arc",             "gfx.arc(cx, cy, rad, a0, a1, r, g, b)",           "nnnnnnnn"},
            {"pie",             "gfx.pie(cx, cy, rad, a0, a1, r, g, b)",           "nnnnnnnn"},
            {"round_rect",      "gfx.round_rect(x, y, w, h, rad, r, g, b)",        "nnnnnnnn"},
            {"fill_round_rect", "gfx.fill_round_rect(x, y, w, h, rad, r, g, b)",   "nnnnnnnn"},
            {"tri",             "gfx.tri(x1, y1, x2, y2, x3, y3, r, g, b)",        "nnnnnnnnn"},
            {"fill_tri",        "gfx.fill_tri(x1, y1, x2, y2, x3, y3, r, g, b)",   "nnnnnnnnn"},
            // The point lists go through a template; semCheckGfxPoly has them.
            {"poly",            "gfx.poly(xs, ys, r, g, b)",                       "..nnn"},
            {"fill_poly",       "gfx.fill_poly(xs, ys, r, g, b)",                  "..nnn"},
            {"text",            "gfx.text(x, y, s, r, g, b[, scale])",             "nntnnnn"},
            {"text_size",       "gfx.text_size([n])",                              "n"},
            {"text_width",      "gfx.text_width(s[, scale])",                      "tn"},
            {"text_height",     "gfx.text_height(s[, scale])",                     "tn"},
            {"title",           "gfx.title([s])",                                  "t"},
            {"opendialog",      "gfx.opendialog([filter])",                        "t"},
            {"image",           "gfx.image(path)",                                 "t"},
            {"decode",          "gfx.decode(bytes)",                               "t"},
            {"image_w",         "gfx.image_w(id)",                                 "n"},
            {"image_h",         "gfx.image_h(id)",                                 "n"},
            {"save",            "gfx.save(path)",                                  "t"},
            {"icon",            "gfx.icon(src)",                                   "x"},
            {"cursor",          "gfx.cursor([on])",                                "n"},
            {"alpha",           "gfx.alpha([a])",                                  "n"},
            {"fullscreen",      "gfx.fullscreen([on])",                            "n"},
            {"borderless",      "gfx.borderless([on])",                            "n"},
            {"ontop",           "gfx.ontop([on])",                                 "n"},
            {"transparent",     "gfx.transparent([on])",                           "n"},
            {"audio",           "gfx.audio([rate])",                               "n"},
            {"sample",          "gfx.sample(s)",                                   "n"},
            {"sound",           "gfx.sound(path)",                                 "t"},
            {"play",            "gfx.play(sound[, volume])",                       "nn"},
            {"loop",            "gfx.loop(sound[, volume])",                       "nn"},
            {"stop",            "gfx.stop([voice])",                               "n"},
            {"volume",          "gfx.volume([v])",                                 "n"},
            // gfx.blit names its own arguments differently in each form, so
            // the three-and-five form comes first and the source-rect form
            // catches the calls it cannot hold.
            {"blit",            "gfx.blit(x, y, src[, w, h])",                     "nnxnn"},
            {"blit",            "gfx.blit(x, y, src, sx, sy, sw, sh[, dw, dh])",   "nnxnnnnnn"},
            {"blit_rot",        "gfx.blit_rot(x, y, src, angle[, w, h])",          "nnxnnn"},
            {"", nullptr, nullptr},
        };
        return rows;
    }

    // std/math. Every parameter is cast to double on the way in, which is why
    // math.sqrt("9") lands as a static_cast on a std::string.
    static const BuiltinArgRow* mathArgRows() {
        static const BuiltinArgRow rows[] = {
            {"abs",   "math.abs(x)",          "n"},
            {"sqrt",  "math.sqrt(x)",         "n"},
            {"floor", "math.floor(x)",        "n"},
            {"ceil",  "math.ceil(x)",         "n"},
            {"round", "math.round(x)",        "n"},
            {"sin",   "math.sin(x)",          "n"},
            {"cos",   "math.cos(x)",          "n"},
            {"tan",   "math.tan(x)",          "n"},
            {"log",   "math.log(x)",          "n"},
            {"log10", "math.log10(x)",        "n"},
            {"exp",   "math.exp(x)",          "n"},
            {"min",   "math.min(a, b)",       "nn"},
            {"max",   "math.max(a, b)",       "nn"},
            {"pow",   "math.pow(base, exp)",  "nn"},
            {"", nullptr, nullptr},
        };
        return rows;
    }

    // std/crypto. The data parameters go through emitConcatOperand, which
    // stringifies a number, so they are 'x'; crypto.random_bytes takes a count
    // and crypto.xor's keys are a vector<int> unless there is exactly one and
    // it is text, which is why xor has a row per form.
    static const BuiltinArgRow* cryptoArgRows() {
        static const BuiltinArgRow rows[] = {
            {"sha256",        "crypto.sha256(data)",            "x"},
            {"sha1",          "crypto.sha1(data)",              "x"},
            {"hex_encode",    "crypto.hex_encode(data)",        "x"},
            {"hex_decode",    "crypto.hex_decode(hex)",         "x"},
            {"base64_encode", "crypto.base64_encode(data)",     "x"},
            {"base64_decode", "crypto.base64_decode(b64)",      "x"},
            {"hmac_sha256",   "crypto.hmac_sha256(key, data)",  "xx"},
            {"random_bytes",  "crypto.random_bytes(n)",         "n"},
            {"xor",           "crypto.xor(data, key)",          "tx"},
            {"xor",           "crypto.xor(data, key...)",       "tn*"},
            {"", nullptr, nullptr},
        };
        return rows;
    }

    // std/network, the http half. The server and request parameters are the
    // structs http.localhost() and http.accept() hand back, not handles, so a
    // number there is as wrong as text.
    static const BuiltinArgRow* httpArgRows() {
        static const BuiltinArgRow rows[] = {
            {"request",   "http.request(method, url, body[, headers])",   "ttth"},
            {"get",       "http.get(url[, headers])",                     "th"},
            {"delete",    "http.delete(url[, headers])",                  "th"},
            {"post",      "http.post(url, body[, headers])",              "tth"},
            {"put",       "http.put(url, body[, headers])",               "tth"},
            {"patch",     "http.patch(url, body[, headers])",             "tth"},
            {"localhost", "http.localhost([port])",                       "n"},
            {"accept",    "http.accept(server)",                          "s"},
            {"close",     "http.close(server)",                           "s"},
            {"reply",     "http.reply(request, status, body[, headers])",  "rnth"},
            {"raw",       "http.raw(request, bytes)",                      "rt"},
            {"", nullptr, nullptr},
        };
        return rows;
    }

    // std/network, the tcp half: an int handle everywhere, bytes as text.
    static const BuiltinArgRow* tcpArgRows() {
        static const BuiltinArgRow rows[] = {
            {"connect", "tcp.connect(host, port)",   "tn"},
            {"listen",  "tcp.listen(port)",          "n"},
            {"accept",  "tcp.accept(listener)",      "n"},
            {"send",    "tcp.send(handle, data)",    "nt"},
            {"recv",    "tcp.recv(handle[, max])",   "nn"},
            {"port",    "tcp.port(handle)",          "n"},
            {"close",   "tcp.close(handle)",         "n"},
            {"", nullptr, nullptr},
        };
        return rows;
    }

    // std/network, the udp half.
    static const BuiltinArgRow* udpArgRows() {
        static const BuiltinArgRow rows[] = {
            {"open",        "udp.open(port)",                     "n"},
            {"port",        "udp.port(handle)",                   "n"},
            {"send",        "udp.send(handle, host, port, data)", "ntnt"},
            {"recv",        "udp.recv(handle[, max])",            "nn"},
            {"sender",      "udp.sender(handle)",                 "n"},
            {"sender_port", "udp.sender_port(handle)",            "n"},
            {"close",       "udp.close(handle)",                  "n"},
            {"", nullptr, nullptr},
        };
        return rows;
    }

    // std/file, the FileCall half. Every path becomes a `const char*` through
    // .c_str(), so a number there is a member call on an int.
    static const BuiltinArgRow* fileArgRows() {
        static const BuiltinArgRow rows[] = {
            {"remove",     "file.remove(path)",      "t"},
            {"remove_all", "file.remove_all(path)",  "t"},
            {"list",       "file.list(path)",        "t"},
            {"isdir",      "file.isdir(path)",       "t"},
            {"isfile",     "file.isfile(path)",      "t"},
            {"size",       "file.size(path)",        "t"},
            {"chdir",      "file.chdir(path)",       "t"},
            {"abspath",    "file.abspath(path)",     "t"},
            {"dirname",    "file.dirname(path)",     "t"},
            {"basename",   "file.basename(path)",    "t"},
            {"extension",  "file.extension(path)",   "t"},
            {"rename",     "file.rename(from, to)",  "tt"},
            {"copy",       "file.copy(from, to)",    "tt"},
            {"join",       "file.join(a, b)",        "tt"},
            {"", nullptr, nullptr},
        };
        return rows;
    }

    // The builtins that are their own AST node rather than a method name on a
    // namespace node: the whole of os.*, file's original five, time.sleep and
    // random.*. os.* text parameters are 'x' and not 't' because the
    // transpiler runs a non-string through std::to_string on the way in --
    // os.open(8080) is a conversion the language does allow. What it cannot
    // stringify is a slice or a struct. os.getprocessid(name) is the one
    // exception: that argument is passed straight through.
    static const BuiltinArgRow* nodeArgRow(AstNode::Type t, const std::string& tag) {
        static const BuiltinArgRow setVolume     = {"", "os.set_volume(v)",             "n"};
        static const BuiltinArgRow setBrightness = {"", "os.set_brightness(v)",         "n"};
        static const BuiltinArgRow clipSet       = {"", "os.clip_set(s)",               "x"};
        static const BuiltinArgRow osType        = {"", "os.type(s)",                   "x"};
        static const BuiltinArgRow notify        = {"", "os.notify(title, message)",    "xx"};
        static const BuiltinArgRow osOpen        = {"", "os.open(target)",              "x"};
        static const BuiltinArgRow load          = {"", "os.load(path)",                "x"};
        static const BuiltinArgRow save          = {"", "os.save(path, data)",          "xx"};
        static const BuiltinArgRow play          = {"", "os.play(path)",                "x"};
        static const BuiltinArgRow spawn         = {"", "os.spawn(prog[, arg...])",     "x*"};
        static const BuiltinArgRow spawnAt       = {"", "os.spawn_at(cwd, prog[, arg...])", "xx*"};
        static const BuiltinArgRow wait          = {"", "os.wait(pid)",                 "n"};
        static const BuiltinArgRow kill          = {"", "os.kill(pid)",                 "n"};
        static const BuiltinArgRow which         = {"", "os.which(name)",               "x"};
        static const BuiltinArgRow unsetenv      = {"", "os.unsetenv(name)",            "x"};
        static const BuiltinArgRow chdir         = {"", "os.chdir(path)",               "x"};
        static const BuiltinArgRow messagebox    = {"", "os.messagebox(text, title)",   "xx"};
        static const BuiltinArgRow exitRow       = {"", "os.exit(code)",                "n"};
        static const BuiltinArgRow setenv        = {"", "os.setenv(name, value)",       "xx"};
        static const BuiltinArgRow system        = {"", "os.system(command)",           "x"};
        static const BuiltinArgRow pid           = {"", "os.getprocessid([name])",      "t"};
        static const BuiltinArgRow fileRead      = {"", "file.read(path)",              "t"};
        static const BuiltinArgRow fileWrite     = {"", "file.write(path, content)",    "tx"};
        static const BuiltinArgRow fileAppend    = {"", "file.append(path, content)",   "tx"};
        static const BuiltinArgRow fileExists    = {"", "file.exists(path)",            "t"};
        static const BuiltinArgRow fileMkdir     = {"", "file.mkdir(path)",             "t"};
        static const BuiltinArgRow sleepRow      = {"", "time.sleep(ms)",               "n"};
        static const BuiltinArgRow randomInt     = {"", "random.int(min, max)",         "nn"};
        static const BuiltinArgRow randomSeed    = {"", "random.seed(n)",               "n"};
        switch (t) {
            case AstNode::Type::OsSetVolume:     return &setVolume;
            case AstNode::Type::OsSetBrightness: return &setBrightness;
            case AstNode::Type::OsClipSet:       return &clipSet;
            case AstNode::Type::OsType:          return &osType;
            case AstNode::Type::OsNotify:        return &notify;
            case AstNode::Type::OsOpen:          return &osOpen;
            case AstNode::Type::OsLoad:          return &load;
            case AstNode::Type::OsSave:          return &save;
            case AstNode::Type::OsPlay:          return &play;
            case AstNode::Type::OsSpawn:         return tag == "at" ? &spawnAt : &spawn;
            case AstNode::Type::OsWait:          return &wait;
            case AstNode::Type::OsKill:          return &kill;
            case AstNode::Type::OsWhich:         return &which;
            case AstNode::Type::OsUnsetenv:      return &unsetenv;
            case AstNode::Type::OsChdir:         return &chdir;
            case AstNode::Type::OsMessageBox:    return &messagebox;
            case AstNode::Type::OsExit:          return &exitRow;
            case AstNode::Type::OsSetenv:        return &setenv;
            // os.system in statement position and in value position are two
            // node types for one builtin, so they share the one row.
            case AstNode::Type::OsSystem:
            case AstNode::Type::OsExec:          return &system;
            case AstNode::Type::OsGetProcessId:  return &pid;
            case AstNode::Type::FileRead:        return &fileRead;
            case AstNode::Type::FileWrite:       return &fileWrite;
            case AstNode::Type::FileAppend:      return &fileAppend;
            case AstNode::Type::FileExists:      return &fileExists;
            case AstNode::Type::FileMkdir:       return &fileMkdir;
            case AstNode::Type::TimeSleep:       return &sleepRow;
            case AstNode::Type::RandomInt:       return &randomInt;
            case AstNode::Type::RandomSeed:      return &randomSeed;
            default: return nullptr;
        }
    }

    // The number of parameters a row names, and whether it takes more of the
    // last one. A trailing '*' is the repeat: os.spawn(prog[, arg...]).
    static size_t builtinRowArity(const char* kinds, bool* variadic) {
        std::string k(kinds ? kinds : "");
        const bool rep = !k.empty() && k.back() == '*';
        if (variadic) *variadic = rep;
        return rep ? k.size() - 1 : k.size();
    }

    // The first row for `method` that can hold `argc` arguments, or the first
    // row for it at all if none can (the parser's arity rules run earlier, so
    // that means a form this table does not know).
    static const BuiltinArgRow* findBuiltinArgRow(const BuiltinArgRow* rows,
                                                  const std::string& method, size_t argc) {
        const BuiltinArgRow* first = nullptr;
        for (const BuiltinArgRow* r = rows; r->call; ++r) {
            if (method != r->method) continue;
            if (!first) first = r;
            bool variadic = false;
            const size_t arity = builtinRowArity(r->kinds, &variadic);
            if (variadic || argc <= arity) return r;
        }
        return first;
    }

    // The parameter names, read out of the signature text so a row cannot
    // name one thing and check another. "gfx.blit(x, y, src[, w, h])" gives
    // x, y, src, w, h.
    static std::vector<std::string> builtinParamNames(const char* call) {
        std::vector<std::string> names;
        std::string s(call ? call : "");
        const size_t open = s.find('(');
        if (open == std::string::npos) return names;
        std::string cur;
        for (size_t i = open + 1; i < s.size(); i++) {
            const char c = s[i];
            if (c == ',' || c == ')') {
                if (!cur.empty()) names.push_back(cur);
                cur.clear();
                if (c == ')') break;
                continue;
            }
            if (c == ' ' || c == '[' || c == ']' || c == '.') continue;
            cur += c;
        }
        if (!cur.empty()) names.push_back(cur);
        return names;
    }

    static char builtinArgKind(const char* kinds, size_t i) {
        bool variadic = false;
        const size_t arity = builtinRowArity(kinds, &variadic);
        if (arity == 0) return '.';
        if (i < arity) return kinds[i];
        return variadic ? kinds[arity - 1] : '.';
    }

    // Anything that reaches an int or double parameter on its own. char and
    // bool are in because C++ promotes them; float is in because narrowing to
    // an int is a conversion the language allows, the same as C's.
    static bool semArgIsNumber(const std::string& t) {
        return nexaIsNumericIntType(t) || t == "float" || t == "char" || t == "bool";
    }

    static bool semArgKindAccepts(char kind, const std::string& t) {
        switch (kind) {
            case 'n': return semArgIsNumber(t);
            case 't': return t == "string";
            case 'x': return t == "string" || semArgIsNumber(t);
            case 'h': return t == "[]string";
            case 's': return t == "struct:HttpServer";
            case 'r': return t == "struct:HttpRequest";
            default: return true;
        }
    }

    static const char* semArgKindWants(char kind) {
        switch (kind) {
            case 'n': return "a number";
            case 't': return "text";
            case 'x': return "text or a number";
            case 'h': return "a []string of header lines";
            case 's': return "an http.localhost() server";
            case 'r': return "an http.accept() request";
            default: return nullptr;
        }
    }

    // struct:/enum: keep a struct name apart from a plain type inside the
    // transpiler; the user wrote HttpServer, not struct:HttpServer. Stripped
    // wherever they appear, since a tag can be nested: a Result of a struct
    // comes through as Result[struct:HttpServer].
    static std::string semTypeWithoutTags(const std::string& t) {
        std::string out = t;
        for (const char* tag : {"struct:", "enum:"}) {
            const std::string s(tag);
            for (size_t at = out.find(s); at != std::string::npos; at = out.find(s, at)) {
                out.erase(at, s.size());
            }
        }
        return out;
    }

    // What the user wrote, in the words they wrote it in. The two categories
    // they think in get the words; anything else is named by its own type.
    static std::string semArgGot(const std::string& t) {
        if (t == "string") return "text";
        if (semArgIsNumber(t)) return "a number";
        if (t == "void") return "nothing";
        return "'" + semTypeWithoutTags(t) + "'";
    }

    void semCheckBuiltinArgRow(const AstNode& e, const BuiltinArgRow* row) const {
        if (!row) return;
        const std::vector<std::string> names = builtinParamNames(row->call);
        bool variadic = false;
        builtinRowArity(row->kinds, &variadic);
        for (size_t i = 0; i < e.children.size(); i++) {
            const char kind = builtinArgKind(row->kinds, i);
            const char* wants = semArgKindWants(kind);
            if (!wants) continue;
            const std::string t = inferExprNexaType(e.children[i]);
            if (t.empty() || semArgKindAccepts(kind, t)) continue;
            // The trailing name repeats with its letter: the third key of
            // crypto.xor(data, key...) is still a key.
            std::string name = "argument " + std::to_string(i + 1);
            if (i < names.size()) name = names[i];
            else if (variadic && !names.empty()) name = names.back();
            semError(e, std::string(row->call) + " expects " + wants + " for " + name +
                ", but got " + semArgGot(t));
        }
    }

    // Reached for every node in the walk; the namespaces with known
    // signatures answer, everything else falls straight through.
    //
    // Only run when Nexa can see every type in the program. With inline C++ or
    // a C++ header in play a name can be declared somewhere this walk cannot
    // read, and inferExprNexaType answers "int" for a name it does not know --
    // which would read as a number handed to a text parameter. That is the
    // same condition, for the same reason, as the undefined-name check.
    void semCheckBuiltinArgTypes(const AstNode& e) const {
        if (!semNameChecks_) return;
        switch (e.type) {
            case AstNode::Type::GfxCall:
                semCheckBuiltinArgRow(e, findBuiltinArgRow(gfxArgRows(), e.value, e.children.size()));
                break;
            case AstNode::Type::Gfx3dCall:
                semCheckBuiltinArgRow(e, findBuiltinArgRow(gfx3dArgRows(), e.value, e.children.size()));
                break;
            case AstNode::Type::MathCall:
                semCheckBuiltinArgRow(e, findBuiltinArgRow(mathArgRows(), e.value, e.children.size()));
                break;
            case AstNode::Type::HttpCall:
                semCheckBuiltinArgRow(e, findBuiltinArgRow(httpArgRows(), e.value, e.children.size()));
                break;
            case AstNode::Type::TcpCall:
                semCheckBuiltinArgRow(e, findBuiltinArgRow(tcpArgRows(), e.value, e.children.size()));
                break;
            case AstNode::Type::UdpCall:
                semCheckBuiltinArgRow(e, findBuiltinArgRow(udpArgRows(), e.value, e.children.size()));
                break;
            case AstNode::Type::FileCall:
                semCheckBuiltinArgRow(e, findBuiltinArgRow(fileArgRows(), e.value, e.children.size()));
                break;
            case AstNode::Type::CryptoCall:
                semCheckBuiltinArgRow(e, findBuiltinArgRow(cryptoArgRows(), e.value, e.children.size()));
                break;
            default:
                semCheckBuiltinArgRow(e, nodeArgRow(e.type, e.value));
                break;
        }
    }

    // Element types the slice algorithms are documented to work on
    // (SYNTAX/Core.txt, SLICE ALGORITHMS). Ordering needs `<`, summing needs
    // `+=` and a zero; a struct, a map, a nested slice or a Result has none of
    // those, and is caught here rather than inside the emitted lambda.
    //
    // Some rejected types would in fact compile -- a scoped enum compares, and
    // std::vector compares lexicographically, so []Color and [][]int would both
    // build. They are still refused: the set the compiler accepts and the set
    // SYNTAX/ promises are the same set, and widening it later is additive.
    static bool sliceElemIsOrderable(const std::string& elem) {
        return nexaIsNumericIntType(elem) || elem == "float" || elem == "string" ||
               elem == "char" || elem == "bool";
    }

    static bool sliceElemIsSummable(const std::string& elem) {
        return nexaIsNumericIntType(elem) || elem == "float";
    }

    // Can the receiver of an in-place method be written back to? A variable, a
    // field, an element and a pointee all name storage that outlives the call;
    // a slice copy (xs[1:4]), a split, or a function's return value is a
    // temporary, and sorting one sorts something nothing else can see.
    //
    // This is what makes xs[1:4].sort() worth refusing rather than emitting: it
    // reads like "sort that range of xs", and there is no way to make it mean
    // that -- xs[1:4] is a copy by the time any method sees it.
    static bool exprIsStorableReceiver(const AstNode& e) {
        switch (e.type) {
            case AstNode::Type::ExprVarRef:
            case AstNode::Type::ExprDeref:
                return true;
            case AstNode::Type::ExprMember:
            case AstNode::Type::ExprArrayIndex:
                return !e.children.empty() && exprIsStorableReceiver(e.children[0]);
            default:
                return false;
        }
    }

    // A Nexa type as the user wrote it. The tables carry a struct as
    // 'struct:Point' and an enum as 'enum:Color' to keep them apart from a
    // plain name, but a diagnostic quoting '[]struct:Point' is quoting a
    // spelling that appears nowhere in the program.
    static std::string nexaTypeForMessage(const std::string& t) {
        std::string s = t;
        for (const char* tag : {"struct:", "enum:"}) {
            const size_t n = std::string(tag).size();
            size_t p;
            while ((p = s.find(tag)) != std::string::npos) s.erase(p, n);
        }
        return s;
    }

    // sort / sort_desc / reverse / remove / join / min / max / sum on a slice
    // whose elements cannot take the operation. Without this the user's reward
    // for pts.sort() on a []Point is a page of clang errors pointing into
    // generated code they never wrote -- the same reasoning as semCheckGfxPoly.
    void semCheckSliceAlgo(const AstNode& e) const {
        if (e.initValue != "." || e.children.empty()) return;
        const std::string& m = e.value;
        // The in-place ones added with the algorithms. push/pop/insert/clear
        // are deliberately not here: pop and insert on a temporary already
        // fail, and push and clear already compile to a discarded no-op, so
        // either way that is established behaviour and not this check's to
        // change.
        const bool inPlace = (m == "sort" || m == "sort_desc" || m == "reverse" ||
                              m == "remove");
        if (!inPlace && m != "min" && m != "max" && m != "sum" && m != "join") {
            return;
        }
        std::string recvT = inferExprNexaType(e.children[0]);
        if (isPointerType(recvT)) recvT = pointerPointeeType(recvT);
        // Only a known slice reaches the slice emit at all; anything else is a
        // user's own method that happens to share the name, or a receiver whose
        // type inference did not reach, and neither is ours to complain about.
        if (!nexaIsSliceType(recvT)) return;
        const std::string elem = nexaSliceElem(recvT);
        const std::string who = exprRootVarName(e.children[0]);
        const std::string subject = who.empty() ? std::string("the receiver") : who;
        const std::string shown = nexaTypeForMessage(recvT);
        if (inPlace && !exprIsStorableReceiver(e.children[0])) {
            semError(e, "." + m + "() changes the slice in place, so it needs a slice "
                "you can name -- a variable, a field or an element. A slice copy like "
                "xs[1:4], a split, or a function's return value is a temporary, and "
                "reordering one changes nothing");
        }
        // reverse and remove move elements without looking at them, so any
        // element type will do; the rest need `<` or `+=`.
        if (m == "reverse" || m == "remove") return;
        if (m == "join") {
            if (elem == "string") return;
            semError(e, ".join(sep) expects []string, but " + subject + " is '" + shown + "'");
        }
        if (m == "sum") {
            if (sliceElemIsSummable(elem)) return;
            semError(e, ".sum() expects []int or []float, but " + subject +
                " is '" + shown + "'");
        }
        if (sliceElemIsOrderable(elem)) return;
        semError(e, "." + m + "() expects []int, []float or []string, but " + subject +
            " is '" + shown + "'");
    }

    void semCheckNameUse(const AstNode& at, const std::string& name) {
        if (!semNameChecks_ || name.empty()) return;
        if (!lookupNexaDecl(name).empty()) return;
        if (name == "self" || name == "null" || name == "true" || name == "false") return;
        if (hasNexaFnNamed(name)) return;          // a function used as a value
        if (enumCppNames_.count(name)) return;     // bare enum type name
        if (structCppNames_.count(name)) return;   // bare struct type name
        semError(at, "Undefined variable '" + name + "'");
    }

    void semCheckMemberAccess(const AstNode& e) {
        if (e.children.empty()) return;
        const AstNode& base = e.children[0];
        if (base.type == AstNode::Type::ExprVarRef && lookupNexaDecl(base.value).empty()) {
            auto ev = enumVariants_.find(base.value);
            if (ev != enumVariants_.end()) {
                if (!ev->second.count(e.value)) {
                    semError(e, "Unknown enum variant '" + e.value + "' for '" + base.value + "'");
                }
                return;  // Enum.Variant: the base names a type, not a variable
            }
        }
        semExpr(base);
        std::string baseT = inferExprNexaType(base);
        if (isPointerType(baseT)) baseT = pointerPointeeType(baseT);
        if (!isStructDeclType(baseT)) return;  // not a known Nexa struct: no opinion
        const std::string sname = structNameFromDecl(baseT);
        auto fields = structFields_.find(sname);
        if (fields == structFields_.end()) return;
        if (fields->second.count(e.value)) return;
        auto methods = structMethods_.find(sname);
        if (methods != structMethods_.end() && methods->second.count(e.value)) return;
        semError(e, "Struct '" + sname + "' has no field '" + e.value + "'");
    }

    // Coarse type category used only for the "clearly incompatible" initializer check.
    // "" means "cannot tell from the syntax", which suppresses the diagnosis.
    static std::string semCategoryOfType(const std::string& t) {
        if (t == "string") return "string";
        if (t == "float") return "float";
        if (t == "bool") return "bool";
        if (t == "char") return "char";
        if (nexaIsNumericIntType(t)) return "int";
        if (nexaIsSliceType(t)) return "slice";
        if (isStructDeclType(t)) return "struct";
        return "";
    }

    static bool semIsScalarCat(const std::string& c) {
        return c == "int" || c == "float" || c == "char" || c == "bool";
    }

    std::string semCategoryOfExpr(const AstNode& e) const {
        switch (e.type) {
            case AstNode::Type::ExprStringLiteral: return "string";
            case AstNode::Type::ExprIntLiteral: return "int";
            case AstNode::Type::ExprFloatLiteral: return "float";
            case AstNode::Type::ExprCharLiteral: return "char";
            case AstNode::Type::ExprBoolLiteral: return "bool";
            case AstNode::Type::ExprArrayLiteral: return "slice";
            case AstNode::Type::ExprStructLit: return "struct";
            case AstNode::Type::ExprVarRef: return semCategoryOfType(lookupNexaDecl(e.value));
            case AstNode::Type::ExprAdd: {
                if (e.children.size() != 2) return "";
                const std::string a = semCategoryOfExpr(e.children[0]);
                const std::string b = semCategoryOfExpr(e.children[1]);
                if (a == "string" || b == "string") return "string";  // concatenation
                return "";
            }
            default: return "";
        }
    }

    // `let x: T = init` where T and init have definite, incompatible categories.
    // Numeric widening/narrowing (int <-> float <-> char <-> bool) is left alone:
    // those are the implicit conversions Nexa already supports.
    void semCheckVariableInit(const AstNode& v) {
        if (v.declType.empty() || v.initUninitialized || v.isFixedArray) return;
        const std::string declCat = semCategoryOfType(v.declType);
        if (declCat.empty()) return;

        std::string initCat;
        if (!v.children.empty()) {
            initCat = semCategoryOfExpr(v.children[0]);
        } else if (v.initFromReadln || v.initFromFileRead) {
            initCat = "string";
        } else if (v.initFromDllLoad) {
            return;
        }
        if (initCat.empty() || initCat == declCat) return;

        const bool declScalar = semIsScalarCat(declCat);
        const bool initScalar = semIsScalarCat(initCat);
        if (declScalar && initScalar) return;  // implicit numeric conversion

        semError(v, "Type mismatch: '" + v.value + "' is declared " + semTypeLabel(v.declType) +
                    " but the initializer is " + semCatLabel(initCat));
    }

    static std::string semTypeLabel(const std::string& t) {
        if (isStructDeclType(t)) return "struct " + structNameFromDecl(t);
        return "'" + t + "'";
    }

    static std::string semCatLabel(const std::string& c) {
        if (c == "slice") return "an array";
        if (c == "struct") return "a struct value";
        if (c == "int") return "an integer";
        return "a " + c;
    }

    static bool isStructDeclType(const std::string& declType) {
        return declType.size() >= 7 && declType.compare(0, 7, "struct:") == 0;
    }
    static bool isEnumDeclType(const std::string& declType) {
        return declType.size() >= 5 && declType.compare(0, 5, "enum:") == 0;
    }
    static bool isPointerType(const std::string& t) {
        return !t.empty() && t[0] == '*';
    }
    static std::string pointerPointeeType(const std::string& t) {
        if (!isPointerType(t)) return t;
        return t.substr(1);
    }
    static std::string structNameFromDecl(const std::string& declType) {
        return declType.substr(7);
    }
    static std::string enumNameFromDecl(const std::string& declType) {
        return declType.substr(5);
    }
    std::string nexaIndexResultType(const std::string& baseT) const {
        if (isPointerType(baseT)) return pointerPointeeType(baseT);
        if (nexaIsSliceType(baseT)) return nexaSliceElem(baseT);
        if (nexaIsMapType(baseT)) {
            std::string k, v;
            if (nexaSplitMapType(baseT, k, v)) return v;
        }
        if (baseT == "json") return "json";
        if (baseT == "string") return "char";
        if (isStructDeclType(baseT)) return baseT;
        if (nexaIsIntegerType(baseT) || baseT == "bool" || baseT == "float") return baseT;
        return "int";
    }
    std::string nexaTypeToCpp(const std::string& t) const {
        if (isPointerType(t)) {
            return nexaTypeToCpp(pointerPointeeType(t)) + "*";
        }
        if (t == "int") return "int";
        if (t == "string") return "std::string";
        if (t == "bool") return "bool";
        if (t == "float") return "double";
        if (t == "char") return "char";
        if (t == "unsigned char") return "unsigned char";
        if (t == "unsigned int") return "unsigned int";
        if (t == "short") return "short";
        if (t == "unsigned short") return "unsigned short";
        if (t == "long") return "long";
        if (t == "unsigned long") return "unsigned long";
        if (t == "size_t") return "std::size_t";
        if (t == "void") return "void";
        if (t == "null") return "std::nullptr_t";
        if (t == "json") {
            if (!modules_.hasJson()) {
                throw std::runtime_error("Json requires #include <std/json>");
            }
            return "__nexa_json";
        }
        if (nexaIsResultType(t)) {
            return "__nexa_result<" + nexaTypeToCpp(nexaResultInner(t)) + ">";
        }
        if (nexaIsSliceType(t)) {
            return "std::vector<" + nexaTypeToCpp(nexaSliceElem(t)) + ">";
        }
        if (nexaIsMapType(t)) {
            std::string k, v;
            if (!nexaSplitMapType(t, k, v)) throw std::runtime_error("Invalid map type: " + t);
            return "std::map<" + nexaTypeToCpp(k) + ", " + nexaTypeToCpp(v) + ">";
        }
        if (nexaIsFnType(t)) {
            std::vector<std::string> params;
            std::string ret;
            if (!nexaSplitFnType(t, params, ret)) throw std::runtime_error("Invalid fn type: " + t);
            std::string s = "std::function<" + nexaTypeToCpp(ret) + "(";
            for (size_t i = 0; i < params.size(); i++) {
                if (i) s += ", ";
                s += nexaTypeToCpp(params[i]);
            }
            s += ")>";
            return s;
        }
        if (isCppDeclType(t)) return cppNameFromDecl(t);
        if (t.size() >= 7 && t.compare(0, 7, "struct:") == 0) {
            std::string n = t.substr(7);
            auto it = structCppNames_.find(n);
            if (it != structCppNames_.end()) return it->second;
            if (modules_.hasCppHeader()) return n;
            throw std::runtime_error("Unknown struct type: " + n);
        }
        if (t.size() >= 5 && t.compare(0, 5, "enum:") == 0) {
            std::string n = t.substr(5);
            auto it = enumCppNames_.find(n);
            if (it == enumCppNames_.end()) throw std::runtime_error("Unknown enum type: " + n);
            return it->second;
        }
        throw std::runtime_error("Unknown type: " + t);
    }
    // C ABI types for values passed through dll.call / exported DLL functions.
    // std::string cannot cross a statically-linked exe into a shared library.
    std::string dllExportParamCpp(const std::string& t) const {
        if (t == "string") return "const char*";
        return nexaTypeToCpp(t.empty() ? "int" : t);
    }
    std::string nexaTypeToCppExtern(const std::string& t) const {
        if (isPointerType(t)) {
            std::string pt = pointerPointeeType(t);
            if (pt == "char") return "const char*";
            if (pt == "void") return "void*";
            return nexaTypeToCppExtern(pt) + "*";
        }
        if (t == "string") return "const char*";
        if (t == "void") return "void";
        return nexaTypeToCpp(t);
    }
    void emitExternDecl(std::ostream& out, const AstNode& node) const {
        std::string retCpp = node.fnReturnType == "void" ? "void" : nexaTypeToCppExtern(node.fnReturnType);
        out << "extern \"C\" " << retCpp << " " << node.value << "(";
        for (size_t i = 0; i < node.paramNames.size(); i++) {
            if (i > 0) out << ", ";
            std::string ptype = "int";
            if (i < node.paramTypes.size() && !node.paramTypes[i].empty()) {
                ptype = nexaTypeToCppExtern(canonicalParamType(node, i));
            }
            out << ptype << " " << node.paramNames[i];
        }
        if (node.isVariadic) {
            if (!node.paramNames.empty()) out << ", ";
            out << "...";
        }
        out << ");\n";
    }

    bool exprIsEnumLike(const AstNode& e, const std::map<std::string, std::string>& varMap,
                        const std::map<std::string, bool>& varIsEnum) const {
        if (e.type == AstNode::Type::ExprVarRef) {
            auto it = varIsEnum.find(e.value);
            return it != varIsEnum.end() && it->second;
        }
        if (e.type == AstNode::Type::ExprMember && !e.children.empty() && e.children[0].type == AstNode::Type::ExprVarRef) {
            const std::string& base = e.children[0].value;
            if (varMap.find(base) != varMap.end()) return false;
            auto en = enumCppNames_.find(base);
            if (en == enumCppNames_.end()) return false;
            auto ev = enumVariants_.find(base);
            if (ev == enumVariants_.end()) return false;
            return ev->second.count(e.value) != 0;
        }
        return false;
    }

    std::string wrapExprForPrintf(const AstNode& e, const std::string& expr,
                                  const std::map<std::string, std::string>& varMap,
                                  const std::map<std::string, bool>& varIsEnum) const {
        if (exprIsEnumLike(e, varMap, varIsEnum)) return "static_cast<int>(" + expr + ")";
        return expr;
    }

    void emitIoPrintArg(std::ostringstream& out, const std::string& indent, const AstNode& arg,
                        std::map<std::string, std::string>& varMap,
                        std::map<std::string, bool>& varIsString,
                        std::map<std::string, bool>& varIsFloat,
                        std::map<std::string, bool>& varIsChar,
                        std::map<std::string, bool>& varIsBool,
                        const std::map<std::string, bool>& varIsEnum,
                        bool newline) {
        if (arg.type == AstNode::Type::ExprStringLiteral) {
            if (newline) {
                out << indent << "puts(\"" << escapeString(arg.value) << "\");\n";
            } else {
                out << indent << "fputs(\"" << escapeString(arg.value) << "\", stdout);\n";
            }
            return;
        }
        if (arg.type == AstNode::Type::ExprIntLiteral) {
            // The format has to follow the literal's own width, not `int`: printing a
            // literal that does not fit an int through "%d" reads the wrong number of
            // bytes off the varargs list, and io.println(9999999999) printed 1410065407.
            emitIntLiteralPrintf(out, indent, arg.value, newline);
            return;
        }
        if (arg.type == AstNode::Type::ExprBoolLiteral) {
            out << indent << "printf(\"%d" << (newline ? "\\n" : "") << "\", " << (arg.value == "true" ? "1" : "0") << ");\n";
            return;
        }
        std::string ntype = inferExprNexaType(arg);
        bool exprIsStr = (ntype == "string");
        bool exprIsF = (ntype == "float");
        bool exprIsC = (ntype == "char");
        bool exprIsBoolT = (ntype == "bool");
        bool exprIsPtr = isPointerType(ntype) || ntype == "null";
        bool isNexaEnum = !ntype.empty() && ntype.size() >= 5 && ntype.compare(0, 5, "enum:") == 0;
        std::string expr = emitExpr(arg, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
        expr = wrapExprForPrintf(arg, expr, varMap, varIsEnum);
        if (exprIsStr) {
            const bool strNeedsCStr = expr.empty() || expr[0] != '"';
            if (newline) {
                if (strNeedsCStr) {
                    out << indent << "printf(\"%s\\n\", " << expr << ".c_str());\n";
                } else {
                    out << indent << "puts(" << expr << ");\n";
                }
            } else {
                std::string carg = strNeedsCStr ? expr + ".c_str()" : expr;
                out << indent << "fputs(" << carg << ", stdout);\n";
            }
        } else if (exprIsF) {
            out << indent << "printf(\"%g" << (newline ? "\\n" : "") << "\", " << expr << ");\n";
        } else if (exprIsC) {
            out << indent << "printf(\"%c" << (newline ? "\\n" : "") << "\", " << expr << ");\n";
        } else if (exprIsBoolT) {
            // printf is variadic, so the argument must already be an int. std::vector<bool>
            // indexes to a proxy reference rather than a bool, and passing that through
            // varargs is undefined — io.println(flags[0]) printed garbage without this cast.
            out << indent << "printf(\"%d" << (newline ? "\\n" : "") << "\", static_cast<int>(" << expr << "));\n";
        } else if (exprIsPtr) {
            out << indent << "printf(\"%p" << (newline ? "\\n" : "") << "\", (void*)(" << expr << "));\n";
        } else if (ntype == "json") {
            out << indent << "printf(\"%s" << (newline ? "\\n" : "") << "\", (" << expr << ").stringify().c_str());\n";
        } else if (nexaIsNumericIntType(ntype)) {
            emitIntegerPrintf(out, indent, ntype, expr, newline);
        } else {
            std::string carg = isNexaEnum ? ("static_cast<int>(" + expr + ")") : expr;
            out << indent << "printf(\"%d" << (newline ? "\\n" : "") << "\", " << carg << ");\n";
        }
    }

    // printf for a literal argument, where the C++ type comes from the literal's own
    // magnitude rather than from a declared Nexa type. Anything that fits an int keeps
    // "%d" so the common case emits exactly what it did before; wider literals are cast
    // to a fixed width so the conversion matches on every target, not just LP64.
    void emitIntLiteralPrintf(std::ostringstream& out, const std::string& indent,
                              const std::string& text, bool newline) const {
        const IntLiteralText lit = parseIntLiteralText(text);
        const unsigned long long intMax = 2147483647ULL;
        // C++ types the digits before it applies the sign, so `-2147483648` is a
        // negated `long`, not an int. The magnitude alone decides the width.
        const bool fitsInt = lit.valid && lit.magnitude <= intMax;
        std::string fmt = "%d";
        std::string arg = emitIntLiteral(text);
        if (!fitsInt) {
            if (lit.valid && !lit.negative && lit.magnitude > 9223372036854775807ULL) {
                fmt = "%llu";
                arg = "static_cast<unsigned long long>(" + arg + ")";
            } else {
                fmt = "%lld";
                arg = "static_cast<long long>(" + arg + ")";
            }
        }
        out << indent << "printf(\"" << fmt << (newline ? "\\n" : "") << "\", " << arg << ");\n";
    }

    // printf for an integer argument whose Nexa type is known. Anything wider than an int is
    // cast to a fixed width that the conversion specifier names exactly, rather than printed
    // through "%ld"/"%zu": printf reads the argument back off the varargs list, where a
    // width mismatch is undefined behaviour and not a conversion, and the width of `long`
    // and `size_t` depends on the target. `long long` covers every one of them on every
    // target, and widening to it preserves the value. Same choice as emitIntLiteralPrintf.
    //
    // int-width types stay uncast so the common case emits what it always did, and so that
    // a future inference bug there still shows up as a -Wformat warning rather than silently
    // truncating behind a cast.
    void emitIntegerPrintf(std::ostringstream& out, const std::string& indent,
                           const std::string& ntype, const std::string& expr, bool newline) const {
        std::string fmt = "%d";
        std::string arg = expr;
        if (ntype == "unsigned int") {
            fmt = "%u";
        } else if (ntype == "unsigned short" || ntype == "unsigned char") {
            fmt = "%u";
            arg = "static_cast<unsigned int>(" + expr + ")";
        } else if (ntype == "unsigned long" || ntype == "size_t") {
            fmt = "%llu";
            arg = "static_cast<unsigned long long>(" + expr + ")";
        } else if (ntype == "long") {
            fmt = "%lld";
            arg = "static_cast<long long>(" + expr + ")";
        } else if (ntype == "short") {
            arg = "static_cast<int>(" + expr + ")";
        }
        out << indent << "printf(\"" << fmt;
        if (newline) out << "\\n";
        out << "\", " << arg << ");\n";
    }

    std::string emitOsStringArg(const AstNode& e,
                                const std::map<std::string, std::string>& varMap,
                                const std::map<std::string, bool>* varIsString,
                                const std::map<std::string, bool>* varIsFloat,
                                const std::map<std::string, bool>* varIsChar,
                                const std::map<std::string, bool>* varIsBool) {
        std::string v = emitExpr(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        if (exprIsString(e, *varIsString)) return "std::string(" + v + ")";
        return "std::to_string(" + v + ")";
    }

    std::string emitOsSpawnCall(const AstNode& e,
                                const std::map<std::string, std::string>& varMap,
                                const std::map<std::string, bool>* varIsString,
                                const std::map<std::string, bool>* varIsFloat,
                                const std::map<std::string, bool>* varIsChar,
                                const std::map<std::string, bool>* varIsBool) {
        const bool at = e.value == "at";
        const bool wait = e.value == "wait";
        const size_t start = at ? 1 : 0;
        std::string cwd = "std::string()";
        if (at && !e.children.empty()) {
            cwd = emitOsStringArg(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        }
        std::string s = "__nexa_os_spawn(std::vector<std::string>{";
        for (size_t i = start; i < e.children.size(); ++i) {
            if (i > start) s += ", ";
            s += emitOsStringArg(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        }
        s += "}, " + cwd + ", " + (wait ? "1" : "0") + ")";
        return s;
    }

    void varStructPush() const { varStructScopes_.emplace_back(); }
    void varStructPop() const {
        if (!varStructScopes_.empty()) varStructScopes_.pop_back();
    }
    void varStructDeclare(const std::string& nexaVar, const std::string& nexaStructName) const {
        if (!varStructScopes_.empty()) varStructScopes_.back()[nexaVar] = nexaStructName;
    }
    std::string varStructLookup(const std::string& nexaVar) const {
        for (auto it = varStructScopes_.rbegin(); it != varStructScopes_.rend(); ++it) {
            auto j = it->find(nexaVar);
            if (j != it->end()) return j->second;
        }
        return "";
    }
    std::string structTypeOfExprValue(const AstNode& e) const {
        if (e.type == AstNode::Type::ExprVarRef) {
            std::string s = varStructLookup(e.value);
            if (!s.empty()) return s;
            // varStructScopes_ is filled as codegen walks, so during
            // checkSemantics it is empty and only the declaration stack knows
            // a local's type. Without this, `s.xs` on a struct field inferred
            // as int, and semCheckGfxPoly reported gfx.poly(s.xs, ...) as
            // being handed an int -- a wrong answer about a correct program.
            std::string t = lookupNexaDecl(e.value);
            if (isPointerType(t)) t = pointerPointeeType(t);
            return isStructDeclType(t) ? structNameFromDecl(t) : std::string();
        }
        if (e.type == AstNode::Type::ExprStructLit) return e.value;
        if (e.type == AstNode::Type::ExprArrayIndex) {
            std::string t = inferExprNexaType(e);
            if (isPointerType(t)) {
                std::string pt = pointerPointeeType(t);
                if (isStructDeclType(pt)) return structNameFromDecl(pt);
            }
            if (isStructDeclType(t)) return structNameFromDecl(t);
            return "";
        }
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            std::string ft = fieldTypeOfMemberExpr(e);
            if (ft.size() >= 7 && ft.compare(0, 7, "struct:") == 0) return ft.substr(7);
            return "";
        }
        // Anything else that is known to yield a struct -- a call returning
        // one, r.value() on a Result[struct] -- so that .field on it reads the
        // field's type instead of falling back to int.
        std::string t = inferExprNexaType(e);
        if (isPointerType(t)) t = pointerPointeeType(t);
        if (isStructDeclType(t)) return structNameFromDecl(t);
        return "";
    }
    std::string fieldTypeOfMemberExpr(const AstNode& e) const {
        if (e.type != AstNode::Type::ExprMember || e.children.empty()) return "";
        std::string st;
        if (e.isArrowMember) {
            std::string baseT = inferExprNexaType(e.children[0]);
            if (!isPointerType(baseT)) return "";
            std::string pt = pointerPointeeType(baseT);
            if (!isStructDeclType(pt)) return "";
            st = structNameFromDecl(pt);
        } else {
            st = structTypeOfExprValue(e.children[0]);
        }
        if (st.empty()) return "";
        auto sit = structFields_.find(st);
        if (sit == structFields_.end()) return "";
        auto fit = sit->second.find(e.value);
        if (fit == sit->second.end()) return "";
        return fit->second;
    }

    // The half of std/http that listens rather than calls out. These are the
    // verbs whose runtime lives in the server block, and the ones that hand
    // back a plain int rather than a Result.
    static bool httpVerbIsServer(const std::string& m) {
        return m == "localhost" || m == "accept" || m == "reply" || m == "raw" || m == "close";
    }
    static bool httpVerbReturnsInt(const std::string& m) {
        return m == "reply" || m == "raw" || m == "close";
    }

    // Core string-method return-type classification (value.method(...)).
    static bool strMethodReturnsString(const std::string& m) {
        return m == "upper" || m == "lower" || m == "trim" || m == "replace" ||
               m == "substring" || m == "repeat";
    }
    static bool strMethodReturnsBool(const std::string& m) {
        return m == "contains" || m == "starts_with" || m == "ends_with";
    }

    static bool exprProducesString(const AstNode& e) {
        if (e.type == AstNode::Type::OsInfo) {
            const std::string& m = e.value;
            return m == "shell" || m == "newline" || m == "path_sep" || m == "lang"
                || m == "config_dir" || m == "cache_dir" || m == "desktop" || m == "endian";
        }
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsExec || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsExecutable || e.type == AstNode::Type::OsTempDir || e.type == AstNode::Type::OsArch || e.type == AstNode::Type::OsWhich || e.type == AstNode::Type::OsCwd || e.type == AstNode::Type::OsHostname || e.type == AstNode::Type::OsUsername || e.type == AstNode::Type::OsHome || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::OsClipGet || e.type == AstNode::Type::OsLoad || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::IoGetline || e.type == AstNode::Type::IoReadln || e.type == AstNode::Type::FileRead || e.type == AstNode::Type::ExprTrim || e.type == AstNode::Type::CryptoCall) return true;
        // http.reply/raw/close hand back a 1/0 int, so they concatenate as a
        // number; every other http.* call yields a Result.
        if (e.type == AstNode::Type::HttpCall) return !httpVerbReturnsInt(e.value);
        // tcp.recv hands back the bytes it read; every other tcp.* call is an int.
        if (e.type == AstNode::Type::TcpCall) return e.value == "recv";
        // udp.recv the same, and udp.sender is the address the bytes came from.
        if (e.type == AstNode::Type::UdpCall) return e.value == "recv" || e.value == "sender";
        if (e.type == AstNode::Type::JsonCall && e.value == "stringify") return true;
        // gfx3d.backend() is the one call in the module that answers with
        // text, so it is the one that concatenates instead of being counted.
        if (e.type == AstNode::Type::Gfx3dCall) return e.value == "backend" || e.value == "typed";
        if (e.type == AstNode::Type::ExprCast && e.value == "string") return true;
        if (e.type == AstNode::Type::FileCall) {
            const std::string& m = e.value;
            return m == "cwd" || m == "abspath" || m == "join" || m == "dirname" || m == "basename" || m == "extension";
        }
        if (e.type == AstNode::Type::StrMethod) return strMethodReturnsString(e.value);
        if (e.type == AstNode::Type::ExprAdd && e.children.size() >= 2) {
            return exprProducesString(e.children[0]) || exprProducesString(e.children[1]);
        }
        if (e.type == AstNode::Type::ExprTernary && e.children.size() >= 3) {
            return exprProducesString(e.children[1]) || exprProducesString(e.children[2]);
        }
        return false;
    }

    // Whether an array-valued initializer expression yields std::vector<std::string>.
    bool arrayInitProducesString(const AstNode& initExpr, const std::map<std::string, bool>& varIsString) const {
        if (initExpr.type == AstNode::Type::StrMethod && initExpr.value == "split") return true;
        if (initExpr.type == AstNode::Type::FileCall && initExpr.value == "list") return true;
        if (initExpr.type == AstNode::Type::OsInfo && initExpr.value == "environ") return true;
        return arrayLiteralProducesString(initExpr, varIsString);
    }

    bool forInElementIsString(const AstNode& coll) const {
        std::string t = inferExprNexaType(coll);
        if (t == "[]string") return true;
        if (t == "arrayelt:string") return true;
        if (coll.type == AstNode::Type::ExprVarRef) {
            std::string d = lookupNexaDecl(coll.value);
            if (d == "[]string") return true;
        }
        if (coll.type == AstNode::Type::StrMethod && coll.value == "split") return true;
        if (coll.type == AstNode::Type::FileCall && coll.value == "list") return true;
        if (coll.type == AstNode::Type::OsInfo && coll.value == "environ") return true;
        if (coll.type == AstNode::Type::ExprArrayLiteral) {
            std::map<std::string, bool> empty;
            return arrayLiteralProducesString(coll, empty);
        }
        return false;
    }

    bool arrayLiteralProducesString(const AstNode& arrNode, const std::map<std::string, bool>& varIsString) const {
        if (arrNode.type != AstNode::Type::ExprArrayLiteral) return false;
        for (const auto& c : arrNode.children) {
            if (c.type == AstNode::Type::ExprStringLiteral) return true;
            if (exprProducesString(c)) return true;
            if (exprIsString(c, varIsString)) return true;
        }
        return false;
    }

    bool exprIsString(const AstNode& e, const std::map<std::string, bool>& varIsString) const {
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            std::string ft = fieldTypeOfMemberExpr(e);
            return ft == "string";
        }
        if (e.type == AstNode::Type::ExprVarRef) {
            auto it = varIsString.find(e.value);
            return it != varIsString.end() && it->second;
        }
        if (e.type == AstNode::Type::ExprArrayIndex || e.type == AstNode::Type::ExprSlice) {
            return inferExprNexaType(e) == "string";
        }
        if (e.type == AstNode::Type::ExprCast) return e.value == "string";
        if (e.type == AstNode::Type::OsInfo) {
            const std::string& m = e.value;
            return m == "shell" || m == "newline" || m == "path_sep" || m == "lang"
                || m == "config_dir" || m == "cache_dir" || m == "desktop" || m == "endian";
        }
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsExec || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsExecutable || e.type == AstNode::Type::OsTempDir || e.type == AstNode::Type::OsArch || e.type == AstNode::Type::OsWhich || e.type == AstNode::Type::OsCwd || e.type == AstNode::Type::OsHostname || e.type == AstNode::Type::OsUsername || e.type == AstNode::Type::OsHome || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::OsClipGet || e.type == AstNode::Type::OsLoad || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::FileRead || e.type == AstNode::Type::IoReadln || e.type == AstNode::Type::IoGetline || e.type == AstNode::Type::ExprTrim || e.type == AstNode::Type::CryptoCall) return true;
        // http.reply/raw/close hand back a 1/0 int, so they concatenate as a
        // number; every other http.* call yields a Result.
        if (e.type == AstNode::Type::HttpCall) return !httpVerbReturnsInt(e.value);
        // tcp.recv hands back the bytes it read; every other tcp.* call is an int.
        if (e.type == AstNode::Type::TcpCall) return e.value == "recv";
        // udp.recv the same, and udp.sender is the address the bytes came from.
        if (e.type == AstNode::Type::UdpCall) return e.value == "recv" || e.value == "sender";
        if (e.type == AstNode::Type::JsonCall && e.value == "stringify") return true;
        if (e.type == AstNode::Type::FileCall) {
            const std::string& m = e.value;
            return m == "cwd" || m == "abspath" || m == "join" || m == "dirname" || m == "basename" || m == "extension";
        }
        if (e.type == AstNode::Type::StrMethod) return strMethodReturnsString(e.value);
        if (e.type == AstNode::Type::FnCall) {
            return inferExprNexaType(e) == "string";
        }
        if (e.type == AstNode::Type::ExprAdd && e.children.size() >= 2) {
            return exprIsString(e.children[0], varIsString) || exprIsString(e.children[1], varIsString);
        }
        if (e.type == AstNode::Type::ExprTernary && e.children.size() >= 3) {
            return exprIsString(e.children[1], varIsString) || exprIsString(e.children[2], varIsString);
        }
        return false;
    }

    bool exprIsFloat(const AstNode& e, const std::map<std::string, bool>& varIsFloat) const {
        if (e.type == AstNode::Type::ExprCast) return e.value == "float";
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            std::string ft = fieldTypeOfMemberExpr(e);
            return ft == "float";
        }
        if (e.type == AstNode::Type::ExprFloatLiteral) return true;
        if (e.type == AstNode::Type::TimeNowMs) return true;
        if (e.type == AstNode::Type::MathCall) return true;
        if (e.type == AstNode::Type::ExprVarRef) {
            auto it = varIsFloat.find(e.value);
            return it != varIsFloat.end() && it->second;
        }
        if (e.type == AstNode::Type::ExprAdd || e.type == AstNode::Type::ExprSub || e.type == AstNode::Type::ExprMul ||
            e.type == AstNode::Type::ExprDiv || e.type == AstNode::Type::ExprMod ||
            e.type == AstNode::Type::ExprBitAnd || e.type == AstNode::Type::ExprBitOr || e.type == AstNode::Type::ExprBitXor ||
            e.type == AstNode::Type::ExprShl || e.type == AstNode::Type::ExprShr) {
            if (e.children.size() >= 2)
                return exprIsFloat(e.children[0], varIsFloat) || exprIsFloat(e.children[1], varIsFloat);
        }
        if (e.type == AstNode::Type::ExprBitNot && e.children.size() >= 1) {
            return exprIsFloat(e.children[0], varIsFloat);
        }
        return false;
    }

    bool exprIsChar(const AstNode& e, const std::map<std::string, bool>& varIsChar) const {
        if (e.type == AstNode::Type::ExprCast) return e.value == "char" || e.value == "unsigned char";
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            std::string ft = fieldTypeOfMemberExpr(e);
            return ft == "char";
        }
        if (e.type == AstNode::Type::ExprCharLiteral) return true;
        if (e.type == AstNode::Type::ExprArrayIndex) {
            return inferExprNexaType(e) == "char";
        }
        if (e.type == AstNode::Type::ExprVarRef) {
            auto it = varIsChar.find(e.value);
            return it != varIsChar.end() && it->second;
        }
        return false;
    }

    bool exprIsBool(const AstNode& e, const std::map<std::string, bool>& varIsBool) const {
        if (e.type == AstNode::Type::ExprCast) return e.value == "bool";
        if (e.type == AstNode::Type::ExprBoolLiteral) return true;
        if (e.type == AstNode::Type::ExprVarRef) {
            auto it = varIsBool.find(e.value);
            return it != varIsBool.end() && it->second;
        }
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            return fieldTypeOfMemberExpr(e) == "bool";
        }
        if (e.type == AstNode::Type::StrMethod) return strMethodReturnsBool(e.value);
        return false;
    }

    static void astClassifyReturns(const AstNode& n, bool& hasValueReturn, bool& hasVoidReturn) {
        if (n.type == AstNode::Type::Return) {
            if (n.children.empty()) hasVoidReturn = true;
            else hasValueReturn = true;
            return;
        }
        for (const AstNode& c : n.children) astClassifyReturns(c, hasValueReturn, hasVoidReturn);
    }
    static void stmtsClassifyReturns(const std::vector<AstNode>& stmts, bool& hasValueReturn, bool& hasVoidReturn) {
        for (const AstNode& s : stmts) astClassifyReturns(s, hasValueReturn, hasVoidReturn);
    }
    static bool stmtsEndWithReturn(const std::vector<AstNode>& stmts) {
        return !stmts.empty() && stmts.back().type == AstNode::Type::Return;
    }

    void emitDefaultReturnForNexaFn(std::ostringstream& out, const std::string& nexaType) const {
        if (isPointerType(nexaType)) {
            out << "    return nullptr;\n";
        } else if (nexaIsNumericIntType(nexaType)) {
            out << "    return 0;\n";
        } else if (nexaType == "bool") {
            out << "    return false;\n";
        } else if (nexaType == "float") {
            out << "    return 0.0;\n";
        } else if (nexaType == "char") {
            out << "    return '\\0';\n";
        } else if (nexaType == "string") {
            out << "    return \"\";\n";
        } else if (isStructDeclType(nexaType)) {
            std::string cpp = structCppNames_.at(structNameFromDecl(nexaType));
            out << "    return " << cpp << "{};\n";
        } else if (isEnumDeclType(nexaType)) {
            std::string en = enumNameFromDecl(nexaType);
            out << "    return " << enumCppNames_.at(en) << "::" << enumFirstVariant_.at(en) << ";\n";
        } else if (nexaIsSliceType(nexaType) || nexaIsMapType(nexaType) || nexaIsFnType(nexaType) || nexaIsResultType(nexaType) || nexaType == "json") {
            out << "    return " << nexaTypeToCpp(nexaType) << "{};\n";
        } else {
            out << "    return 0;\n";
        }
    }

    std::string structMethodReturnCpp(const AstNode& node, bool* voidFnOut) const {
        bool hasValRet = false, hasVoidRet = false;
        stmtsClassifyReturns(node.children, hasValRet, hasVoidRet);
        if (hasValRet && hasVoidRet) {
            throw std::runtime_error("method '" + node.value + "' mixes 'return;' and 'return expr;'");
        }
        bool voidFn = false;
        std::string retCpp;
        if (!node.fnReturnType.empty()) {
            if (node.fnReturnType == "void") {
                if (hasValRet) throw std::runtime_error("cannot return a value from void method '" + node.value + "'");
                voidFn = true;
                retCpp = "void";
            } else {
                if (hasVoidRet) {
                    throw std::runtime_error("return with no value in method '" + node.value + "'");
                }
                retCpp = nexaTypeToCpp(node.fnReturnType);
            }
        } else {
            voidFn = !hasValRet;
            retCpp = hasValRet ? "int" : "void";
        }
        if (voidFnOut) *voidFnOut = voidFn;
        return retCpp;
    }

    void emitStructMethodDecl(std::ostringstream& out, const AstNode& node) {
        std::string retCpp = structMethodReturnCpp(node, nullptr);
        out << "    " << retCpp << " " << node.value << "(";
        for (size_t i = 0; i < node.paramNames.size(); i++) {
            if (i > 0) out << ", ";
            out << paramSigCpp(node, i);
        }
        out << ");\n";
    }

    void emitStructMethodBody(std::ostringstream& out, const AstNode& node, const std::string& cppStruct) {
        bool voidFn = false;
        std::string retCpp = structMethodReturnCpp(node, &voidFn);
        bool hasValRet = false, hasVoidRet = false;
        stmtsClassifyReturns(node.children, hasValRet, hasVoidRet);
        out << retCpp << " " << cppStruct << "::" << node.value << "(";
        std::map<std::string, std::string> varMap;
        int varIdx = 0;
        for (size_t i = 0; i < node.paramNames.size(); i++) {
            if (i > 0) out << ", ";
            std::string pname = preserveNames_ ? node.paramNames[i] : ("__nexa_param_" + std::to_string(i));
            out << paramSigCpp(node, i) << " " << pname;
            varMap[node.paramNames[i]] = pname;
        }
        out << ") {\n";
        varIdx = static_cast<int>(node.paramNames.size());
        std::map<std::string, bool> varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum;
        for (size_t i = 0; i < node.paramNames.size(); i++) {
            const std::string& pt = canonicalParamType(node, i);
            varIsString[node.paramNames[i]] = (pt == "string");
            varIsFloat[node.paramNames[i]] = (pt == "float");
            varIsChar[node.paramNames[i]] = (pt == "char");
            varIsBool[node.paramNames[i]] = (pt == "bool");
            varIsEnum[node.paramNames[i]] = isEnumDeclType(pt);
        }
        varStructPush();
        for (size_t i = 0; i < node.paramNames.size(); i++) {
            if (isStructDeclType(canonicalParamType(node, i))) {
                varStructDeclare(node.paramNames[i], structNameFromDecl(canonicalParamType(node, i)));
            }
        }
        nexaDeclStack_.push_back(globalNexaDecl_);
        for (size_t i = 0; i < node.paramNames.size(); i++) {
            nexaDeclStack_.back()[node.paramNames[i]] = canonicalParamType(node, i);
        }
        nexaDeclStack_.back()["self"] = node.receiverType.empty() ? std::string("struct:") : node.receiverType;
        if (isStructDeclType(node.receiverType)) {
            varStructDeclare("self", structNameFromDecl(node.receiverType));
        }
        methodSelfType_ = node.receiverType;
        emitFnRet_ = voidFn ? EmitFnRet::VoidFn : EmitFnRet::IntFn;
        emitBlockStatements(out, node.children, varMap, varIdx, varIsString, varIsConst, varIsFloat,
                            varIsChar, varIsBool, varIsEnum);
        emitFnRet_ = EmitFnRet::Main;
        methodSelfType_.clear();
        nexaDeclStack_.pop_back();
        varStructPop();
        emitImplicitFnTail(out, node, hasValRet);
        out << "}\n\n";
    }

    void emitImplicitFnTail(std::ostringstream& out, const AstNode& node, bool hasValRet) const {
        if (stmtsEndWithReturn(node.children)) {
            return;
        }
        if (!node.fnReturnType.empty()) {
            if (node.fnReturnType != "void") {
                emitDefaultReturnForNexaFn(out, node.fnReturnType);
            }
            return;
        }
        if (hasValRet) {
            out << "    return 0;\n";
        }
    }

    void emitBlock(std::ostringstream& out, const std::vector<AstNode>& children,
                   std::map<std::string, std::string>& varMap, int& varIdx,
                   std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                   std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                   std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                   const std::string& indent = "    ", bool inStringSwitchCase = false,
                   const std::map<std::string, std::string>* injectNexaDecl = nullptr) {
        nexaDeclStack_.push_back({});
        if (injectNexaDecl) {
            for (const auto& kv : *injectNexaDecl) nexaDeclStack_.back()[kv.first] = kv.second;
        }
        varStructPush();
        // Lexical scoping: declarations inside this block (including shadows of
        // outer names) must not survive past it, so snapshot the name map and
        // per-variable type flags and restore them on exit.
        std::map<std::string, std::string> savedVarMap = varMap;
        std::map<std::string, bool> savedIsString = varIsString;
        std::map<std::string, bool> savedIsConst = varIsConst;
        std::map<std::string, bool> savedIsFloat = varIsFloat;
        std::map<std::string, bool> savedIsChar = varIsChar;
        std::map<std::string, bool> savedIsBool = varIsBool;
        std::map<std::string, bool> savedIsEnum = varIsEnum;
        emitBlockStatements(out, children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
        varMap = std::move(savedVarMap);
        varIsString = std::move(savedIsString);
        varIsConst = std::move(savedIsConst);
        varIsFloat = std::move(savedIsFloat);
        varIsChar = std::move(savedIsChar);
        varIsBool = std::move(savedIsBool);
        varIsEnum = std::move(savedIsEnum);
        varStructPop();
        nexaDeclStack_.pop_back();
    }

    void emitBlockStatements(std::ostringstream& out, const std::vector<AstNode>& children,
                   std::map<std::string, std::string>& varMap, int& varIdx,
                   std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                   std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                   std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                   const std::string& indent = "    ", bool inStringSwitchCase = false) {
        for (const AstNode& child : children) {
            LineMarkScope lineMark(lineDirectives_, out, child, indent);
            if (child.type == AstNode::Type::Variable) {
                std::string vname = preserveNames_ ? child.value : ("__nexa_var_" + std::to_string(varIdx++));
                if (!preserveNames_) varMap[child.value] = vname;
                varIsConst[child.value] = child.isConst;
                bool isFloat = (!child.declType.empty() && child.declType == "float") || child.initIsFloat;
                bool isChar = (!child.declType.empty() && child.declType == "char") || child.initIsChar;
                bool isBool = (!child.declType.empty() && child.declType == "bool") || child.initIsBool;
                varIsFloat[child.value] = isFloat;
                varIsChar[child.value] = isChar;
                varIsBool[child.value] = isBool;
                varIsEnum[child.value] = !child.declType.empty() && isEnumDeclType(child.declType);
                bool isArray = child.initFromArray || (!child.children.empty() && child.children[0].type == AstNode::Type::ExprArrayLiteral);
                bool isStrArr = isArray && !child.children.empty() && arrayInitProducesString(child.children[0], varIsString);
                bool isStr = !isArray && !isPointerType(child.declType) && !isStructDeclType(child.declType) && !isCppDeclType(child.declType) && !isEnumDeclType(child.declType) && (!child.declType.empty() ? (child.declType == "string") : (child.initUninitialized || child.initFromReadln || child.initFromFileRead || (!child.initIsInt && !child.initIsBool && !child.initIsFloat && !child.initIsChar && !child.initFromDllLoad && child.children.empty()) ||
                    (!child.children.empty() && exprProducesString(child.children[0]))));
                // Refine from expression type (e.g. s[i] -> char, parts[i] -> string).
                if (child.declType.empty() && !isArray && !child.children.empty()) {
                    std::string it = inferExprNexaType(child.children[0]);
                    if (isPointerType(it)) {
                        isStr = false;
                        isChar = false;
                        isFloat = false;
                        isBool = false;
                        varIsChar[child.value] = false;
                        varIsFloat[child.value] = false;
                        varIsBool[child.value] = false;
                    } else if (it == "char") {
                        isChar = true;
                        isStr = false;
                        isFloat = false;
                        isBool = false;
                        varIsChar[child.value] = true;
                        varIsFloat[child.value] = false;
                        varIsBool[child.value] = false;
                    } else if (it == "string") {
                        isStr = true;
                        isChar = false;
                        varIsChar[child.value] = false;
                    } else if (it == "int") {
                        isStr = false;
                        isChar = false;
                        varIsChar[child.value] = false;
                    } else if (it == "float") {
                        isFloat = true;
                        isStr = false;
                        varIsFloat[child.value] = true;
                    } else if (it == "bool") {
                        isBool = true;
                        isStr = false;
                        varIsBool[child.value] = true;
                    } else if (it == "json" || nexaIsSliceType(it) || nexaIsMapType(it) || nexaIsFnType(it) || nexaIsResultType(it) || isStructDeclType(it)) {
                        isStr = false;
                        isChar = false;
                        isFloat = false;
                        isBool = false;
                        varIsChar[child.value] = false;
                        varIsFloat[child.value] = false;
                        varIsBool[child.value] = false;
                    }
                }
                varIsString[child.value] = isStr || isStrArr;
                {
                    std::string declForStruct = child.declType.empty() ? nexaDeclFromVariableAst(child) : child.declType;
                    if (isStructDeclType(declForStruct) && !child.isFixedArray) {
                        varStructDeclare(child.value, structNameFromDecl(declForStruct));
                    }
                }
                if (!nexaDeclStack_.empty()) {
                    nexaDeclStack_.back()[child.value] = nexaDeclFromVariableAst(child);
                }
                if (child.initFromReadln) {
                    out << indent << "fflush(stdout);\n";
                    out << indent << "char __nexa_buf[4096];\n";
                    out << indent << "if (fgets(__nexa_buf, sizeof(__nexa_buf), stdin)) { __nexa_buf[strcspn(__nexa_buf, \"\\n\")] = 0; }\n";
                    out << indent << "std::string " << vname << "(__nexa_buf);\n";
                } else if (child.initFromDllLoad) {
                    out << indent << "#ifdef _WIN32\n";
                    out << indent << "__nexa_dll_handles.push_back((void*)LoadLibraryA(\"" << escapeString(child.initValue) << "\"));\n";
                    out << indent << "#elif defined(NEXA_WASM) && !defined(__EMSCRIPTEN__)\n";
                    out << indent << "__nexa_dll_handles.push_back(nullptr);\n";
                    out << indent << "#else\n";
                    out << indent << "__nexa_dll_handles.push_back(dlopen(\"" << escapeString(child.initValue) << "\", RTLD_LAZY));\n";
                    out << indent << "#endif\n";
                    out << indent << "int " << vname << " = (int)__nexa_dll_handles.size() - 1;\n";
                } else if (!child.children.empty() && child.children[0].type == AstNode::Type::OsGetenv) {
                    const std::string& envName = child.children[0].value;
                    out << indent << "const char* __nexa_ge_" << varIdx << " = getenv(\"" << escapeString(envName) << "\");\n";
                    out << indent << "std::string " << vname << " = __nexa_ge_" << varIdx << " ? __nexa_ge_" << varIdx << " : \"\";\n";
                } else if (!child.children.empty() && child.children[0].type == AstNode::Type::OsPlatform) {
                    out << indent << "std::string " << vname << " = __nexa_os_platform();\n";
                } else if (!child.children.empty() && child.children[0].type == AstNode::Type::OsExeDir) {
                    out << indent << "std::string " << vname << " = __nexa_exe_dir();\n";
                } else if (!child.children.empty() && child.children[0].type == AstNode::Type::OsGrepKeys) {
                    out << indent << "std::string " << vname << " = __nexa_os_grepkeys();\n";
                } else if (!child.children.empty() && child.children[0].type == AstNode::Type::OsKeyPressed) {
                    out << indent << "int " << vname << " = __nexa_os_keypressed();\n";
                } else if (!child.children.empty() && child.children[0].type == AstNode::Type::OsGetProcessId) {
                    std::string rhs = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    if (!child.declType.empty() && child.declType == "string") {
                        out << indent << "std::string " << vname << " = std::to_string(" << rhs << ");\n";
                    } else {
                        out << indent << "int " << vname << " = " << rhs << ";\n";
                    }
                } else if (child.initUninitialized) {
                    std::string c = child.isConst ? "const " : "";
                    if (child.isFixedArray) {
                        std::string cfix = child.isConst ? "const " : "";
                        if (isStructDeclType(child.declType) || isCppDeclType(child.declType)) {
                            out << indent << cfix << nexaTypeToCpp(child.declType) << " " << vname << "[" << child.arraySize << "]{};\n";
                        } else {
                            std::string cppType = nexaTypeToCpp(child.declType);
                            out << indent << cfix << cppType << " " << vname << "[" << child.arraySize << "];\n";
                        }
                    } else if (!child.declType.empty() && isPointerType(child.declType)) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << " = nullptr;\n";
                    } else if (!child.declType.empty() && nexaIsNumericIntType(child.declType)) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << " = 0;\n";
                    } else if (!child.declType.empty() && child.declType == "bool") {
                        out << indent << c << "bool " << vname << " = false;\n";
                    } else if (!child.declType.empty() && child.declType == "float") {
                        out << indent << c << "double " << vname << " = 0.0;\n";
                    } else if (!child.declType.empty() && child.declType == "char") {
                        out << indent << c << "char " << vname << " = '\\0';\n";
                    } else if (!child.declType.empty() && isStructDeclType(child.declType)) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << "{};\n";
                    } else if (!child.declType.empty() && isCppDeclType(child.declType)) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << "{};\n";
                    } else if (!child.declType.empty() && isEnumDeclType(child.declType)) {
                        std::string en = enumNameFromDecl(child.declType);
                        std::string cpp = enumCppNames_.at(en);
                        out << indent << c << cpp << " " << vname << " = " << cpp << "::" << enumFirstVariant_.at(en) << ";\n";
                    } else if (!child.declType.empty() && (nexaIsSliceType(child.declType) || nexaIsMapType(child.declType) || nexaIsFnType(child.declType) || nexaIsResultType(child.declType) || child.declType == "json")) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << ";\n";
                    } else {
                        out << indent << c << "std::string " << vname << ";\n";
                    }
                } else if (child.initFromFileRead && !child.children.empty()) {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << "std::string " << vname << " = "
                        << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                } else if (isArray && !child.children.empty()) {
                    std::string c = child.isConst ? "const " : "";
                    std::string decl = nexaDeclFromVariableAst(child);
                    if (!nexaIsSliceType(decl)) decl = arrayInitProducesString(child.children[0], varIsString) ? "[]string" : "[]int";
                    out << indent << c << nexaTypeToCpp(decl) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                } else if (!child.children.empty() && !child.declType.empty() &&
                           (nexaIsSliceType(child.declType) || nexaIsMapType(child.declType) || nexaIsFnType(child.declType) || nexaIsResultType(child.declType) || child.declType == "json")) {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << " = "
                        << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                } else if (!child.children.empty()) {
                    std::string c = child.isConst ? "const " : "";
                    if (!child.declType.empty() && isPointerType(child.declType)) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else if (!child.declType.empty() && nexaIsNumericIntType(child.declType)) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else if (!child.declType.empty() && isEnumDeclType(child.declType)) {
                        std::string en = enumNameFromDecl(child.declType);
                        out << indent << c << enumCppNames_.at(en) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else if (!child.declType.empty() && isStructDeclType(child.declType)) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else if (!child.declType.empty() && isCppDeclType(child.declType)) {
                        out << indent << c << nexaTypeToCpp(child.declType) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else {
                        bool useBool = !child.declType.empty() ? (child.declType == "bool") : child.initIsBool;
                        bool useInt = !child.declType.empty() ? nexaIsNumericIntType(child.declType) : child.initIsInt;
                        bool useFloat = !child.declType.empty() ? (child.declType == "float") : child.initIsFloat;
                        bool useChar = !child.declType.empty() ? (child.declType == "char") : child.initIsChar;
                    std::string inferredPtr;
                    std::string inferredInt;
                    std::string inferredOther;
                    if (child.declType.empty()) {
                        std::string it = inferExprNexaType(child.children[0]);
                        if (isPointerType(it)) {
                            inferredPtr = it;
                        } else if (nexaIsNumericIntType(it)) {
                            inferredInt = it;
                        } else if (isStructDeclType(it) || nexaIsSliceType(it) || nexaIsMapType(it) || nexaIsFnType(it) || nexaIsResultType(it) || isEnumDeclType(it) || isCppDeclType(it) || it == "json") {
                            inferredOther = it;
                        } else if (it == "char") {
                                useChar = true;
                                useInt = false;
                                useFloat = false;
                                useBool = false;
                            } else if (it == "string") {
                                useChar = false;
                                useInt = false;
                                useFloat = false;
                                useBool = false;
                            } else if (it == "float") {
                                useFloat = true;
                                useInt = false;
                                useChar = false;
                                useBool = false;
                            } else if (it == "bool") {
                                useBool = true;
                                useInt = false;
                                useChar = false;
                                useFloat = false;
                            }
                        }
                        if (!inferredPtr.empty()) {
                            out << indent << c << nexaTypeToCpp(inferredPtr) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                        } else if (!inferredInt.empty()) {
                            out << indent << c << nexaTypeToCpp(inferredInt) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                        } else if (!inferredOther.empty()) {
                            out << indent << c << nexaTypeToCpp(inferredOther) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                        } else {
                            std::string cppType = c + (useBool ? "bool " : useFloat ? "double " : useChar ? "char " : (useInt ? "int " : "std::string "));
                            out << indent << cppType << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                        }
                    }
                } else if (child.initIsBool || (!child.declType.empty() && child.declType == "bool")) {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << "bool " << vname << " = " << (child.initValue == "true" ? "true" : "false") << ";\n";
                } else if (child.initIsInt || (!child.declType.empty() && nexaIsNumericIntType(child.declType))) {
                    std::string c = child.isConst ? "const " : "";
                    std::string cppT = (!child.declType.empty() && nexaIsNumericIntType(child.declType)) ? nexaTypeToCpp(child.declType) : "int";
                    out << indent << c << cppT << " " << vname << " = " << emitIntLiteral(child.initValue) << ";\n";
                } else {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << "std::string " << vname << " = " << emitCppStringValue(child.initValue) << ";\n";
                }
            } else if (child.type == AstNode::Type::IoPrintln) {
                if (!child.children.empty()) {
                    for (size_t ai = 0; ai < child.children.size(); ++ai) {
                        bool nl = (ai + 1 == child.children.size());
                        emitIoPrintArg(out, indent, child.children[ai], varMap, varIsString, varIsFloat, varIsChar, varIsBool, varIsEnum, nl);
                    }
                } else if (child.isVarRef) {
                    std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                    AstNode vref{AstNode::Type::ExprVarRef, child.value, {}};
                    std::string ntype = inferExprNexaType(vref);
                    bool isStr = (ntype == "string");
                    bool isF = (ntype == "float");
                    bool isC = (ntype == "char");
                    bool isNexaEnum = !ntype.empty() && ntype.size() >= 5 && ntype.compare(0, 5, "enum:") == 0;
                    if (isStr) {
                        out << indent << "puts(" << v << ".c_str());\n";
                    } else if (isF) {
                        out << indent << "printf(\"%g\\n\", " << v << ");\n";
                    } else if (isC) {
                        out << indent << "printf(\"%c\\n\", " << v << ");\n";
                    } else if (nexaIsNumericIntType(ntype)) {
                        emitIntegerPrintf(out, indent, ntype, v, true);
                    } else {
                        std::string arg = isNexaEnum ? ("static_cast<int>(" + v + ")") : v;
                        out << indent << "printf(\"%d\\n\", " << arg << ");\n";
                    }
                } else {
                    out << indent << "puts(\"" << escapeString(child.value) << "\");\n";
                }
            } else if (child.type == AstNode::Type::IoPrint) {
                if (!child.children.empty()) {
                    for (const AstNode& a : child.children) {
                        emitIoPrintArg(out, indent, a, varMap, varIsString, varIsFloat, varIsChar, varIsBool, varIsEnum, false);
                    }
                } else if (child.isVarRef) {
                    std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                    AstNode vref{AstNode::Type::ExprVarRef, child.value, {}};
                    std::string ntype = inferExprNexaType(vref);
                    bool isStr = (ntype == "string");
                    bool isF = (ntype == "float");
                    bool isC = (ntype == "char");
                    bool isNexaEnum = !ntype.empty() && ntype.size() >= 5 && ntype.compare(0, 5, "enum:") == 0;
                    if (isStr) {
                        out << indent << "fputs(" << v << ".c_str(), stdout);\n";
                    } else if (isF) {
                        out << indent << "printf(\"%g\", " << v << ");\n";
                    } else if (isC) {
                        out << indent << "printf(\"%c\", " << v << ");\n";
                    } else if (nexaIsNumericIntType(ntype)) {
                        emitIntegerPrintf(out, indent, ntype, v, false);
                    } else {
                        std::string arg = isNexaEnum ? ("static_cast<int>(" + v + ")") : v;
                        out << indent << "printf(\"%d\", " << arg << ");\n";
                    }
                } else {
                    out << indent << "fputs(\"" << escapeString(child.value) << "\", stdout);\n";
                }
            } else if (child.type == AstNode::Type::IoFlush) {
                out << indent << "fflush(stdout);\n";
            } else if (child.type == AstNode::Type::FileRead) {
                out << indent << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::FileWrite) {
                emitFileWriteOrAppend(out, indent, child, varMap, varIsString, varIsFloat, varIsChar, varIsBool, 0);
            } else if (child.type == AstNode::Type::FileAppend) {
                emitFileWriteOrAppend(out, indent, child, varMap, varIsString, varIsFloat, varIsChar, varIsBool, 1);
            } else if (child.type == AstNode::Type::FileExists) {
                out << indent << "(void)__nexa_file_exists("
                    << emitFilePathCStr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ");\n";
            } else if (child.type == AstNode::Type::FileMkdir) {
                out << indent << "(void)__nexa_file_mkdir("
                    << emitFilePathCStr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ");\n";
            } else if (child.type == AstNode::Type::FileCall) {
                out << indent << "(void)(" << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ");\n";
            } else if (child.type == AstNode::Type::GfxCall) {
                out << indent << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::Gfx3dCall) {
                out << indent << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::JsonCall) {
                out << indent << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::HttpCall) {
                // An http call whose answer nobody keeps still has to happen:
                // http.reply(...) and http.close(...) are written for what they
                // do, not for the 1/0 they hand back.
                out << indent << "(void)(" << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ");\n";
            } else if (child.type == AstNode::Type::TcpCall || child.type == AstNode::Type::UdpCall) {
                // Same as http: tcp.send(...) and udp.close(...) are written for
                // what they do, not for the count or the 1/0 they hand back.
                out << indent << "(void)(" << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ");\n";
            } else if (child.type == AstNode::Type::RandomSeed) {
                std::string seedExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_random_seed(" << seedExpr << ");\n";
            } else if (child.type == AstNode::Type::RandomInt) {
                std::string minExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string maxExpr = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_random_int(" << minExpr << ", " << maxExpr << ");\n";
            } else if (child.type == AstNode::Type::TimeSleep) {
                const AstNode& dur = child.children[0];
                if (dur.type == AstNode::Type::TimeSeconds && !dur.children.empty()) {
                    out << indent << "__nexa_time_sleep_ms(("
                        << emitExpr(dur.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ") * 1000);\n";
                } else if (dur.type == AstNode::Type::TimeMilliseconds && !dur.children.empty()) {
                    out << indent << "__nexa_time_sleep_ms("
                        << emitExpr(dur.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ");\n";
                } else {
                    out << indent << "__nexa_time_sleep_ms("
                        << emitExpr(dur, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ");\n";
                }
            } else if (child.type == AstNode::Type::ThreadJoin) {
                std::string idxExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_thread_join(" << idxExpr << ");\n";
            } else if (child.type == AstNode::Type::ThreadRun) {
                std::string idxExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string job = emitThreadJobFn(child.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                out << indent << "__nexa_thread_worker_run(" << idxExpr << ", " << job << ");\n";
            } else if (child.type == AstNode::Type::ThreadWorkerJoin) {
                std::string idxExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_thread_worker_join(" << idxExpr << ");\n";
            } else if (child.type == AstNode::Type::DllCall) {
                std::string h = preserveNames_ ? child.children[0].value : varMap.at(child.children[0].value);
                std::string paramTypes;
                std::string fnArgs;
                for (size_t ai = 1; ai < child.children.size(); ai++) {
                    if (ai > 1) {
                        paramTypes += ", ";
                        fnArgs += ", ";
                    }
                    const AstNode& arg = child.children[ai];
                    std::string nexaT = inferExprNexaType(arg);
                    if (nexaT.empty()) nexaT = exprIsString(arg, varIsString) ? "string" : "int";
                    std::string expr = emitExpr(arg, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    if (nexaT == "string") {
                        // exe is statically linked; DLL is not — std::string cannot cross that boundary.
                        paramTypes += "const char*";
                        if (arg.type == AstNode::Type::ExprStringLiteral) fnArgs += expr;
                        else fnArgs += "(" + expr + ").c_str()";
                    } else {
                        paramTypes += nexaTypeToCpp(nexaT);
                        fnArgs += expr;
                    }
                }
                out << indent << "{\n";
                out << indent << "#ifdef _WIN32\n";
                out << indent << "    void (*fn)(" << paramTypes << ") = (void(*)(" << paramTypes << "))GetProcAddress((HMODULE)__nexa_dll_handles[" << h << "], \"" << escapeString(child.value) << "\");\n";
                out << indent << "#elif defined(NEXA_WASM) && !defined(__EMSCRIPTEN__)\n";
                out << indent << "    void (*fn)(" << paramTypes << ") = nullptr;\n";
                out << indent << "#else\n";
                out << indent << "    void (*fn)(" << paramTypes << ") = (void(*)(" << paramTypes << "))dlsym(__nexa_dll_handles[" << h << "], \"" << escapeString(child.value) << "\");\n";
                out << indent << "#endif\n";
                out << indent << "    if (fn) fn(" << fnArgs << ");\n";
                out << indent << "}\n";
            } else if (child.type == AstNode::Type::OsSystem) {
                out << indent << "fflush(stdout);\n";
                if (!child.children.empty()) {
                    const AstNode& arg = child.children[0];
                    bool exprIsStr = exprIsString(arg, varIsString);
                    std::string expr = emitExpr(arg, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    if (!exprIsStr) {
                        out << indent << "std::system(" << expr << ");\n";
                    } else if (arg.type == AstNode::Type::ExprStringLiteral) {
                        out << indent << "std::system(" << expr << ");\n";
                    } else if (auto folded = tryFoldStringLiteralChain(arg, varIsString)) {
                        out << indent << "std::system(\"" << escapeString(*folded) << "\");\n";
                    } else {
                        out << indent << "std::system((" << expr << ").c_str());\n";
                    }
                } else if (child.isVarRef) {
                    std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                    out << indent << "std::system(" << v << ".c_str());\n";
                } else {
                    out << indent << "std::system(\"" << escapeString(child.value) << "\");\n";
                }
            } else if (child.type == AstNode::Type::OsHideConsoleWindow) {
                out << indent << "__nexa_os_hide_console_window();\n";
            } else if (child.type == AstNode::Type::OsShowConsoleWindow) {
                out << indent << "__nexa_os_show_console_window();\n";
            } else if (child.type == AstNode::Type::OsMinimizeConsoleWindow) {
                out << indent << "__nexa_os_minimize_console_window();\n";
            } else if (child.type == AstNode::Type::OsMaximizeConsoleWindow) {
                out << indent << "__nexa_os_maximize_console_window();\n";
            } else if (child.type == AstNode::Type::OsLock) {
                out << indent << "__nexa_os_lock();\n";
            } else if (child.type == AstNode::Type::OsShutdown) {
                out << indent << "__nexa_os_shutdown();\n";
            } else if (child.type == AstNode::Type::OsReboot) {
                out << indent << "__nexa_os_reboot();\n";
            } else if (child.type == AstNode::Type::OsSuspend) {
                out << indent << "__nexa_os_suspend();\n";
            } else if (child.type == AstNode::Type::OsLogout) {
                out << indent << "__nexa_os_logout();\n";
            } else if (child.type == AstNode::Type::OsSetVolume) {
                std::string p = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_os_set_volume(" << p << ");\n";
            } else if (child.type == AstNode::Type::OsGetVolume) {
                out << indent << "(void)__nexa_os_get_volume();\n";
            } else if (child.type == AstNode::Type::OsMute) {
                out << indent << "__nexa_os_set_mute(1);\n";
            } else if (child.type == AstNode::Type::OsUnmute) {
                out << indent << "__nexa_os_set_mute(0);\n";
            } else if (child.type == AstNode::Type::OsToggleMute) {
                out << indent << "__nexa_os_toggle_mute();\n";
            } else if (child.type == AstNode::Type::OsSetBrightness) {
                std::string p = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_os_set_brightness(" << p << ");\n";
            } else if (child.type == AstNode::Type::OsGetBrightness) {
                out << indent << "(void)__nexa_os_get_brightness();\n";
            } else if (child.type == AstNode::Type::OsClipSet) {
                std::string e = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                bool isStr = exprIsString(child.children[0], varIsString);
                std::string arg = isStr ? ("std::string(" + e + ")") : ("std::to_string(" + e + ")");
                out << indent << "__nexa_os_clip_set(" << arg << ");\n";
            } else if (child.type == AstNode::Type::OsType) {
                std::string e = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                bool isStr = exprIsString(child.children[0], varIsString);
                std::string arg = isStr ? ("std::string(" + e + ")") : ("std::to_string(" + e + ")");
                out << indent << "__nexa_os_type(" << arg << ");\n";
            } else if (child.type == AstNode::Type::OsClipGet) {
                out << indent << "(void)__nexa_os_clip_get();\n";
            } else if (child.type == AstNode::Type::OsNotify) {
                std::string te = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string me = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string ta = exprIsString(child.children[0], varIsString) ? ("std::string(" + te + ")") : ("std::to_string(" + te + ")");
                std::string ma = exprIsString(child.children[1], varIsString) ? ("std::string(" + me + ")") : ("std::to_string(" + me + ")");
                out << indent << "__nexa_os_notify(" << ta << ", " << ma << ");\n";
            } else if (child.type == AstNode::Type::OsOpen) {
                std::string e = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string arg = exprIsString(child.children[0], varIsString) ? ("std::string(" + e + ")") : ("std::to_string(" + e + ")");
                out << indent << "__nexa_os_open(" << arg << ");\n";
            } else if (child.type == AstNode::Type::OsLoad) {
                std::string p = emitOsStringArg(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_os_load(" << p << ");\n";
            } else if (child.type == AstNode::Type::OsSave) {
                std::string p = emitOsStringArg(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string d = emitOsStringArg(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_os_save(" << p << ", " << d << ");\n";
            } else if (child.type == AstNode::Type::OsPlay) {
                std::string p = emitOsStringArg(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_os_play(" << p << ");\n";
            } else if (child.type == AstNode::Type::OsSpawn) {
                out << indent << "(void)" << emitOsSpawnCall(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::OsWait) {
                std::string p = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_os_wait(" << p << ");\n";
            } else if (child.type == AstNode::Type::OsKill) {
                std::string p = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_os_kill(" << p << ");\n";
            } else if (child.type == AstNode::Type::OsUnsetenv) {
                std::string n = emitOsStringArg(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_os_unsetenv(" << n << ");\n";
            } else if (child.type == AstNode::Type::OsChdir) {
                std::string p = emitOsStringArg(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_os_chdir(" << p << ");\n";
            } else if (child.type == AstNode::Type::OsTempDir) {
                out << indent << "(void)__nexa_os_tempdir();\n";
            } else if (child.type == AstNode::Type::OsArch) {
                out << indent << "(void)__nexa_os_arch();\n";
            } else if (child.type == AstNode::Type::OsCpuCount) {
                out << indent << "(void)__nexa_os_cpu_count();\n";
            } else if (child.type == AstNode::Type::OsWhich) {
                std::string n = emitOsStringArg(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_os_which(" << n << ");\n";
            } else if (child.type == AstNode::Type::OsExecutable) {
                out << indent << "(void)__nexa_os_executable();\n";
            } else if (child.type == AstNode::Type::OsCwd) {
                out << indent << "(void)__nexa_os_cwd();\n";
            } else if (child.type == AstNode::Type::OsInfo) {
                out << indent << "(void)__nexa_os_" << child.value << "();\n";
            } else if (child.type == AstNode::Type::OsExit) {
                std::string code = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_os_exit(" << code << ");\n";
            } else if (child.type == AstNode::Type::OsSetenv) {
                std::string ne = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string ve = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string na = exprIsString(child.children[0], varIsString) ? ("std::string(" + ne + ")") : ("std::to_string(" + ne + ")");
                std::string va = exprIsString(child.children[1], varIsString) ? ("std::string(" + ve + ")") : ("std::to_string(" + ve + ")");
                out << indent << "__nexa_os_setenv(" << na << ", " << va << ");\n";
            } else if (child.type == AstNode::Type::OsHostname) {
                out << indent << "(void)__nexa_os_hostname();\n";
            } else if (child.type == AstNode::Type::OsUsername) {
                out << indent << "(void)__nexa_os_username();\n";
            } else if (child.type == AstNode::Type::OsHome) {
                out << indent << "(void)__nexa_os_home();\n";
            } else if (child.type == AstNode::Type::OsMessageBox) {
                std::string textExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string titleExpr = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                bool textIsStr = exprIsString(child.children[0], varIsString);
                bool titleIsStr = exprIsString(child.children[1], varIsString);
                std::string textArg = textIsStr ? textExpr : ("std::to_string(" + textExpr + ")");
                std::string titleArg = titleIsStr ? titleExpr : ("std::to_string(" + titleExpr + ")");
                out << indent << "__nexa_os_messagebox(" << textArg << ", " << titleArg << ");\n";
            } else if (child.type == AstNode::Type::OsGetProcessId) {
                if (!child.children.empty()) {
                    std::string nameExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    out << indent << "(void)__nexa_os_getprocessid_by_name(" << nameExpr << ");\n";
                } else {
                    out << indent << "(void)__nexa_os_getprocessid();\n";
                }
            } else if (child.type == AstNode::Type::FnCall) {
                out << indent << emitFnCallCpp(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool)
                    << ";\n";
            } else if (child.type == AstNode::Type::ExprCall || child.type == AstNode::Type::ExprLambda ||
                       child.type == AstNode::Type::ExprStructLit) {
                out << indent << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::StrMethod) {
                out << indent << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnMember) {
                std::string lhs = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string rhs = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                const std::string& op = child.value;
                if (op == "=") out << indent << lhs << " = " << rhs << ";\n";
                else if (op == "+=") {
                    if (child.children[0].type == AstNode::Type::ExprMember && !child.children[0].children.empty() &&
                        fieldTypeOfMemberExpr(child.children[0]) == "string") {
                        out << indent << lhs << " += " << emitConcatOperand(child.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) << ";\n";
                    } else {
                        out << indent << lhs << " += " << rhs << ";\n";
                    }
                }
                else if (op == "-=") out << indent << lhs << " -= " << rhs << ";\n";
                else if (op == "*=") out << indent << lhs << " *= " << rhs << ";\n";
                else if (op == "/=") out << indent << lhs << " /= " << rhs << ";\n";
                else if (op == "%=") out << indent << lhs << " %= " << rhs << ";\n";
                else if (op == "&=") out << indent << lhs << " &= " << rhs << ";\n";
                else if (op == "|=") out << indent << lhs << " |= " << rhs << ";\n";
                else if (op == "^=") out << indent << lhs << " ^= " << rhs << ";\n";
                else if (op == "<<=") out << indent << lhs << " <<= " << rhs << ";\n";
                else if (op == ">>=") out << indent << lhs << " >>= " << rhs << ";\n";
                else out << indent << lhs << " = " << rhs << ";\n";
            } else if (child.type == AstNode::Type::AssnDeref) {
                std::string ptr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string rhs = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                const std::string& op = child.value;
                out << indent << "(*" << ptr << ") " << op << " " << rhs << ";\n";
            } else if (child.type == AstNode::Type::StmtDelete) {
                if (child.children.empty()) {
                    throw std::runtime_error("delete requires a pointer expression");
                }
                std::string p = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                if (child.value == "[]") {
                    out << indent << "delete[] " << p << ";\n";
                } else {
                    out << indent << "delete " << p << ";\n";
                }
            } else if (child.type == AstNode::Type::Assignment) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string lhsT = lookupNexaDecl(child.value);
                if (lhsT.empty()) {
                    if (varIsBool.count(child.value) && varIsBool[child.value]) lhsT = "bool";
                    else if (varIsString.count(child.value) && varIsString[child.value]) lhsT = "string";
                }
                std::string rhsT = inferExprNexaType(child.children[0]);
                if (lhsT == "bool" && rhsT == "string") {
                    throw std::runtime_error("Cannot assign string to bool '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnIndex) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                if (child.children.size() < 2) {
                    throw std::runtime_error("Internal: index assignment missing value");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                std::string lhs = v;
                for (size_t i = 0; i + 1 < child.children.size(); i++) {
                    lhs += "[" + emitExpr(child.children[i], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) + "]";
                }
                const AstNode& rhs = child.children.back();
                std::string val = emitExpr(rhs, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string baseT = lookupNexaDecl(child.value);
                const bool single = child.children.size() == 2;
                const std::string idxOp = child.initValue.empty() ? "=" : child.initValue;
                if (idxOp != "=") {
                    out << indent << lhs << " " << idxOp << " " << val << ";\n";
                } else if (baseT == "json") {
                    std::string rhsT = inferExprNexaType(rhs);
                    if (rhsT != "json") val = "__nexa_json_from(" + val + ")";
                    out << indent << lhs << " = " << val << ";\n";
                } else if (single && baseT == "string") {
                    if (exprIsString(rhs, varIsString)) {
                        out << indent << lhs << " = (" << val << ").empty() ? '\\0' : (" << val << ")[0];\n";
                    } else {
                        out << indent << lhs << " = static_cast<char>(" << val << ");\n";
                    }
                } else {
                    out << indent << lhs << " = " << val << ";\n";
                }
            } else if (child.type == AstNode::Type::AssnAdd) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                if (varIsString.count(child.value) && varIsString.at(child.value)) {
                    // `s += x` appends in place. Emitting `s = s + x` instead built a whole
                    // new string every time, which turns the ordinary "build a string in a
                    // loop" into quadratic work. A struct field (`p.label += x`) has always
                    // emitted `+=`; a plain variable now does too. emitConcatOperand always
                    // yields a string-typed expression, so the two forms are equivalent.
                    out << indent << v << " += " << emitConcatOperand(child.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) << ";\n";
                } else {
                    out << indent << v << " = " << v << " + " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                }
            } else if (child.type == AstNode::Type::AssnSub) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " - " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnMul) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " * " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnDiv) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " / " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnMod) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " % " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnBitAnd) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " & " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnBitOr) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " | " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnBitXor) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                std::string rhs = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                if (varIsString.count(child.value) && varIsString.at(child.value)) {
                    out << indent << "{ int __nexa_k = " << rhs << "; for (size_t __nexa_i = 0; __nexa_i < " << v << ".size(); __nexa_i++) "
                        << v << "[__nexa_i] = (char)((unsigned char)" << v << "[__nexa_i] ^ (__nexa_k & 0xFF)); }\n";
                } else {
                    out << indent << v << " = " << v << " ^ " << rhs << ";\n";
                }
            } else if (child.type == AstNode::Type::AssnShl) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " << " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnShr) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " >> " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::InlineCpp) {
                emitInlineCppRaw(out, stripInlineCppIncludeLines(child.value), indent);
            } else if (child.type == AstNode::Type::IfElse) {
                emitIfElse(out, child, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
            } else if (child.type == AstNode::Type::Switch) {
                emitSwitch(out, child, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
            } else if (child.type == AstNode::Type::While) {
                std::string cond = emitCond(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "while (" << cond << ") {\n";
                emitBlock(out, child.children[1].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
                out << indent << "}\n";
            } else if (child.type == AstNode::Type::For) {
                std::string loopVar = preserveNames_ ? child.value : ("__nexa_for_" + std::to_string(varIdx++));
                auto it = varMap.find(child.value);
                std::string prevVal = (it != varMap.end()) ? it->second : "";
                bool prevStr = varIsString.count(child.value) ? varIsString[child.value] : false;
                bool prevConst = varIsConst.count(child.value) ? varIsConst[child.value] : false;
                bool prevFloat = varIsFloat.count(child.value) ? varIsFloat[child.value] : false;
                bool prevChar = varIsChar.count(child.value) ? varIsChar[child.value] : false;
                bool prevBool = varIsBool.count(child.value) ? varIsBool[child.value] : false;
                bool prevEnum = varIsEnum.count(child.value) ? varIsEnum[child.value] : false;
                varMap[child.value] = loopVar;
                varIsString[child.value] = false;
                varIsConst[child.value] = false;
                varIsFloat[child.value] = false;
                varIsChar[child.value] = false;
                varIsBool[child.value] = false;
                varIsEnum[child.value] = false;
                std::string countExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "for (int " << loopVar << " = 0; " << loopVar << " < " << countExpr << "; " << loopVar << "++) {\n";
                emitBlock(out, child.children[1].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
                out << indent << "}\n";
                if (!prevVal.empty()) { varMap[child.value] = prevVal; varIsString[child.value] = prevStr; varIsConst[child.value] = prevConst; varIsFloat[child.value] = prevFloat; varIsChar[child.value] = prevChar; varIsBool[child.value] = prevBool; varIsEnum[child.value] = prevEnum; }
                else { varMap.erase(child.value); varIsString.erase(child.value); varIsConst.erase(child.value); varIsFloat.erase(child.value); varIsChar.erase(child.value); varIsBool.erase(child.value); varIsEnum.erase(child.value); }
            } else if (child.type == AstNode::Type::ForIn) {
                std::string loopVar = preserveNames_ ? child.value : ("__nexa_for_" + std::to_string(varIdx++));
                auto it = varMap.find(child.value);
                std::string prevVal = (it != varMap.end()) ? it->second : "";
                bool prevStr = varIsString.count(child.value) ? varIsString[child.value] : false;
                bool prevConst = varIsConst.count(child.value) ? varIsConst[child.value] : false;
                bool prevFloat = varIsFloat.count(child.value) ? varIsFloat[child.value] : false;
                bool prevChar = varIsChar.count(child.value) ? varIsChar[child.value] : false;
                bool prevBool = varIsBool.count(child.value) ? varIsBool[child.value] : false;
                bool prevEnum = varIsEnum.count(child.value) ? varIsEnum[child.value] : false;
                std::string prevDecl;
                bool hadDecl = false;
                if (!nexaDeclStack_.empty()) {
                    auto dit = nexaDeclStack_.back().find(child.value);
                    if (dit != nexaDeclStack_.back().end()) {
                        prevDecl = dit->second;
                        hadDecl = true;
                    }
                }
                std::string collT = inferExprNexaType(child.children[0]);
                if (child.children[0].type == AstNode::Type::ExprVarRef) {
                    std::string d = lookupNexaDecl(child.children[0].value);
                    if (!d.empty()) collT = d;
                }
                std::string elemT = "int";
                bool mapKeys = false;
                std::string mapKeyT;
                std::string mapValT;
                const std::string& valueName = child.initValue;
                bool kv = !valueName.empty();
                if (nexaIsSliceType(collT)) {
                    elemT = nexaSliceElem(collT);
                } else if (nexaIsMapType(collT)) {
                    mapKeys = true;
                    std::string mk, mv;
                    nexaSplitMapType(collT, mk, mv);
                    mapKeyT = mk;
                    mapValT = mv;
                    elemT = mk;
                } else if (forInElementIsString(child.children[0])) {
                    elemT = "string";
                }
                if (kv && !mapKeys) {
                    throw std::runtime_error("for (k, v in ...) requires a map");
                }
                // Evaluate collection before binding the loop variable (avoids shadowing).
                std::string collExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                varMap[child.value] = loopVar;
                varIsString[child.value] = (elemT == "string");
                varIsConst[child.value] = false;
                varIsFloat[child.value] = (elemT == "float");
                varIsChar[child.value] = (elemT == "char");
                varIsBool[child.value] = (elemT == "bool");
                varIsEnum[child.value] = isEnumDeclType(elemT);
                if (!nexaDeclStack_.empty()) {
                    nexaDeclStack_.back()[child.value] = elemT;
                }
                std::string loopVal;
                std::string prevVal2;
                bool prevStr2 = false, prevConst2 = false, prevFloat2 = false, prevChar2 = false, prevBool2 = false, prevEnum2 = false;
                std::string prevDecl2;
                bool hadDecl2 = false;
                if (kv) {
                    loopVal = preserveNames_ ? valueName : ("__nexa_forv_" + std::to_string(varIdx++));
                    auto vit = varMap.find(valueName);
                    prevVal2 = (vit != varMap.end()) ? vit->second : "";
                    prevStr2 = varIsString.count(valueName) ? varIsString[valueName] : false;
                    prevConst2 = varIsConst.count(valueName) ? varIsConst[valueName] : false;
                    prevFloat2 = varIsFloat.count(valueName) ? varIsFloat[valueName] : false;
                    prevChar2 = varIsChar.count(valueName) ? varIsChar[valueName] : false;
                    prevBool2 = varIsBool.count(valueName) ? varIsBool[valueName] : false;
                    prevEnum2 = varIsEnum.count(valueName) ? varIsEnum[valueName] : false;
                    if (!nexaDeclStack_.empty()) {
                        auto dit = nexaDeclStack_.back().find(valueName);
                        if (dit != nexaDeclStack_.back().end()) {
                            prevDecl2 = dit->second;
                            hadDecl2 = true;
                        }
                    }
                    varMap[valueName] = loopVal;
                    varIsString[valueName] = (mapValT == "string");
                    varIsConst[valueName] = false;
                    varIsFloat[valueName] = (mapValT == "float");
                    varIsChar[valueName] = (mapValT == "char");
                    varIsBool[valueName] = (mapValT == "bool");
                    varIsEnum[valueName] = isEnumDeclType(mapValT);
                    if (!nexaDeclStack_.empty()) nexaDeclStack_.back()[valueName] = mapValT;
                }
                std::string collTmp = "__nexa_forin_" + std::to_string(varIdx++);
                out << indent << "{\n";
                if (mapKeys) {
                    out << indent << "    const " << nexaTypeToCpp(collT) << "& " << collTmp << " = " << collExpr << ";\n";
                    out << indent << "    for (const auto& __nexa_kv : " << collTmp << ") {\n";
                    out << indent << "        const " << nexaTypeToCpp(mapKeyT) << "& " << loopVar << " = __nexa_kv.first;\n";
                    if (kv) {
                        out << indent << "        const " << nexaTypeToCpp(mapValT) << "& " << loopVal << " = __nexa_kv.second;\n";
                    }
                } else {
                    out << indent << "    const " << nexaTypeToCpp(nexaIsSliceType(collT) ? collT : ("[]" + elemT))
                        << "& " << collTmp << " = " << collExpr << ";\n";
                    out << indent << "    for (const " << nexaTypeToCpp(elemT) << "& " << loopVar << " : " << collTmp << ") {\n";
                }
                emitBlock(out, child.children[1].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "        ", inStringSwitchCase);
                out << indent << "    }\n";
                out << indent << "}\n";
                if (!prevVal.empty()) {
                    varMap[child.value] = prevVal;
                    varIsString[child.value] = prevStr;
                    varIsConst[child.value] = prevConst;
                    varIsFloat[child.value] = prevFloat;
                    varIsChar[child.value] = prevChar;
                    varIsBool[child.value] = prevBool;
                    varIsEnum[child.value] = prevEnum;
                } else {
                    varMap.erase(child.value);
                    varIsString.erase(child.value);
                    varIsConst.erase(child.value);
                    varIsFloat.erase(child.value);
                    varIsChar.erase(child.value);
                    varIsBool.erase(child.value);
                    varIsEnum.erase(child.value);
                }
                if (!nexaDeclStack_.empty()) {
                    if (hadDecl) nexaDeclStack_.back()[child.value] = prevDecl;
                    else nexaDeclStack_.back().erase(child.value);
                }
                if (kv) {
                    if (!prevVal2.empty()) {
                        varMap[valueName] = prevVal2;
                        varIsString[valueName] = prevStr2;
                        varIsConst[valueName] = prevConst2;
                        varIsFloat[valueName] = prevFloat2;
                        varIsChar[valueName] = prevChar2;
                        varIsBool[valueName] = prevBool2;
                        varIsEnum[valueName] = prevEnum2;
                    } else {
                        varMap.erase(valueName);
                        varIsString.erase(valueName);
                        varIsConst.erase(valueName);
                        varIsFloat.erase(valueName);
                        varIsChar.erase(valueName);
                        varIsBool.erase(valueName);
                        varIsEnum.erase(valueName);
                    }
                    if (!nexaDeclStack_.empty()) {
                        if (hadDecl2) nexaDeclStack_.back()[valueName] = prevDecl2;
                        else nexaDeclStack_.back().erase(valueName);
                    }
                }
            } else if (child.type == AstNode::Type::TryCatch) {
                if (child.children.size() < 2) {
                    throw std::runtime_error("internal: try/catch missing try or catch block");
                }
                out << indent << "try {\n";
                emitBlock(out, child.children[0].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
                const std::string& catchNexa = child.value;
                if (catchNexa.empty()) {
                    out << indent << "} catch (...) {\n";
                    emitBlock(out, child.children[1].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
                    out << indent << "}\n";
                } else {
                    auto it = varMap.find(catchNexa);
                    std::string prevMapped = (it != varMap.end()) ? it->second : "";
                    bool hadMap = (it != varMap.end());
                    bool prevStr = varIsString.count(catchNexa) ? varIsString[catchNexa] : false;
                    bool hadStr = varIsString.count(catchNexa) > 0;
                    bool prevConst = varIsConst.count(catchNexa) ? varIsConst[catchNexa] : false;
                    bool hadConst = varIsConst.count(catchNexa) > 0;
                    bool prevFloat = varIsFloat.count(catchNexa) ? varIsFloat[catchNexa] : false;
                    bool hadFloat = varIsFloat.count(catchNexa) > 0;
                    bool prevChar = varIsChar.count(catchNexa) ? varIsChar[catchNexa] : false;
                    bool hadChar = varIsChar.count(catchNexa) > 0;
                    bool prevBool = varIsBool.count(catchNexa) ? varIsBool[catchNexa] : false;
                    bool hadBool = varIsBool.count(catchNexa) > 0;
                    bool prevEnum = varIsEnum.count(catchNexa) ? varIsEnum[catchNexa] : false;
                    bool hadEnum = varIsEnum.count(catchNexa) > 0;

                    std::string cppCatch = preserveNames_ ? catchNexa : ("__nexa_var_" + std::to_string(varIdx++));
                    varMap[catchNexa] = cppCatch;
                    varIsString[catchNexa] = true;
                    varIsConst[catchNexa] = false;

                    out << indent << "} catch (...) {\n";
                    std::string indIn = indent + "    ";
                    std::string indDeep = indent + "        ";
                    out << indIn << "std::string " << cppCatch << ";\n";
                    out << indIn << "try {\n";
                    out << indDeep << "throw;\n";
                    out << indIn << "} catch (const std::exception& __nexa_ex) {\n";
                    out << indDeep << cppCatch << " = std::string(__nexa_ex.what());\n";
                    out << indIn << "} catch (...) {\n";
                    out << indDeep << cppCatch << " = std::string(\"\");\n";
                    out << indIn << "}\n";

                    std::map<std::string, std::string> inj{{catchNexa, "string"}};
                    emitBlock(out, child.children[1].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indIn, inStringSwitchCase, &inj);

                    if (hadMap) varMap[catchNexa] = prevMapped;
                    else varMap.erase(catchNexa);
                    if (hadStr) varIsString[catchNexa] = prevStr;
                    else varIsString.erase(catchNexa);
                    if (hadConst) varIsConst[catchNexa] = prevConst;
                    else varIsConst.erase(catchNexa);
                    if (hadFloat) varIsFloat[catchNexa] = prevFloat;
                    else varIsFloat.erase(catchNexa);
                    if (hadChar) varIsChar[catchNexa] = prevChar;
                    else varIsChar.erase(catchNexa);
                    if (hadBool) varIsBool[catchNexa] = prevBool;
                    else varIsBool.erase(catchNexa);
                    if (hadEnum) varIsEnum[catchNexa] = prevEnum;
                    else varIsEnum.erase(catchNexa);

                    out << indent << "}\n";
                }
            } else if (child.type == AstNode::Type::Throw) {
                if (child.children.empty()) {
                    throw std::runtime_error("internal: throw without expression");
                }
                std::string thrown = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string ntype = inferExprNexaType(child.children[0]);
                if (ntype == "string") {
                    out << indent << "throw std::runtime_error(" << thrown << ");\n";
                } else {
                    out << indent << "throw (" << thrown << ");\n";
                }
            } else if (child.type == AstNode::Type::Return) {
                if (child.children.empty()) {
                    if (emitFnRet_ == EmitFnRet::Main) {
                        out << indent << "return 0;\n";
                    } else if (emitFnRet_ == EmitFnRet::VoidFn) {
                        out << indent << "return;\n";
                    } else {
                        throw std::runtime_error("return with no value in function that returns a value");
                    }
                } else {
                    if (emitFnRet_ == EmitFnRet::VoidFn) {
                        throw std::runtime_error("cannot return a value from void function");
                    }
                    out << indent << "return " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                }
            } else if (child.type == AstNode::Type::Break) {
                if (!inStringSwitchCase) out << indent << "break;\n";
            } else if (child.type == AstNode::Type::Continue) {
                if (!inStringSwitchCase) out << indent << "continue;\n";
            } else if (child.type == AstNode::Type::Goto) {
                out << indent << "goto nxa_lbl_" << child.value << ";\n";
            } else if (child.type == AstNode::Type::Label) {
                out << "nxa_lbl_" << child.value << ":;\n";
            } else if (child.type == AstNode::Type::IncPost) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " + 1;\n";
            } else if (child.type == AstNode::Type::DecPost) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " - 1;\n";
            }
        }
    }

    void emitIfElse(std::ostringstream& out, const AstNode& node,
                   std::map<std::string, std::string>& varMap, int& varIdx,
                   std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                   std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                   std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                   const std::string& indent, bool inStringSwitchCase = false) {
        std::string cond = emitCond(node.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
        out << indent << "if (" << cond << ") {\n";
        emitBlock(out, node.children[1].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
        out << indent << "}";
        if (node.children.size() > 2) {
            const AstNode& elsePart = node.children[2];
            if (elsePart.type == AstNode::Type::IfElse) {
                out << " else if (" << emitCond(elsePart.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ") {\n";
                emitBlock(out, elsePart.children[1].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
                out << indent << "}";
                if (elsePart.children.size() > 2) {
                    emitIfElseTail(out, elsePart.children[2], varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
                }
            } else {
                out << " else {\n";
                emitBlock(out, elsePart.children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
                out << indent << "}";
            }
        }
        out << "\n";
    }

    void emitSwitch(std::ostringstream& out, const AstNode& node,
                   std::map<std::string, std::string>& varMap, int& varIdx,
                   std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                   std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                   std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                   const std::string& indent, bool inStringSwitchCase = false) {
        std::string expr = emitExpr(node.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
        bool useStringSwitch = false;
        for (size_t i = 1; i < node.children.size(); i++) {
            if (node.children[i].type == AstNode::Type::SwitchCase && node.children[i].caseIsString) {
                useStringSwitch = true;
                break;
            }
        }
        if (useStringSwitch) {
            std::vector<const AstNode*> cases, defaults;
            for (size_t i = 1; i < node.children.size(); i++) {
                const AstNode& c = node.children[i];
                if (c.type != AstNode::Type::SwitchCase) continue;
                if (c.value == "default") defaults.push_back(&c);
                else cases.push_back(&c);
            }
            bool first = true;
            for (const AstNode* c : cases) {
                out << indent << (first ? "" : "else ") << "if (" << expr << " == " << emitCppStringValue(c->initValue) << ") {\n";
                first = false;
                emitBlock(out, c->children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", true);
                out << indent << "}\n";
            }
            for (const AstNode* c : defaults) {
                out << indent << (first ? "" : "else ") << "{\n";
                first = false;
                emitBlock(out, c->children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", true);
                out << indent << "}\n";
            }
        } else {
            out << indent << "switch (" << expr << ") {\n";
            for (size_t i = 1; i < node.children.size(); i++) {
                const AstNode& c = node.children[i];
                if (c.type != AstNode::Type::SwitchCase) continue;
                if (c.value == "default") {
                    out << indent << "default:";
                } else if (c.caseIsEnum) {
                    auto enIt = enumCppNames_.find(c.value);
                    if (enIt == enumCppNames_.end()) {
                        throw std::runtime_error("Unknown enum '" + c.value + "' in switch case");
                    }
                    auto vsIt = enumVariants_.find(c.value);
                    if (vsIt == enumVariants_.end() || !vsIt->second.count(c.initValue)) {
                        throw std::runtime_error("Unknown enum variant '" + c.initValue + "' for '" + c.value + "'");
                    }
                    out << indent << "case " << enIt->second << "::" << c.initValue << ":";
                } else {
                    out << indent << "case " << c.value << ":";
                }
                // Each case body gets its own braces. A case body is already a scope on the Nexa
                // side (the semantic checker and emitBlock both pop its declarations), and without
                // the braces a `let` here is a declaration a later case label jumps over, which
                // C++ rejects outright. Braces keep fall-through working: control still leaves the
                // block at the closing brace and lands on the next case label.
                out << " {\n";
                emitBlock(out, c.children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ");
                out << indent << "}\n";
            }
            out << indent << "}\n";
        }
    }

    void emitIfElseTail(std::ostringstream& out, const AstNode& part,
                       std::map<std::string, std::string>& varMap, int& varIdx,
                       std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                       std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                       std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                       const std::string& indent, bool inStringSwitchCase = false) {
        if (part.type == AstNode::Type::IfElse) {
            out << " else if (" << emitCond(part.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ") {\n";
            emitBlock(out, part.children[1].children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
            out << indent << "}";
            if (part.children.size() > 2) {
                emitIfElseTail(out, part.children[2], varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
            }
        } else {
            out << " else {\n";
            emitBlock(out, part.children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
            out << indent << "}";
        }
    }

    // A comparison operand may itself be a comparison (a > b == c < d);
    // route those through emitCond with parens, everything else through emitExpr.
    std::string emitCmpOperand(const AstNode& c, const std::map<std::string, std::string>& varMap,
                               const std::map<std::string, bool>* varIsString = nullptr,
                               const std::map<std::string, bool>* varIsFloat = nullptr,
                               const std::map<std::string, bool>* varIsChar = nullptr,
                               const std::map<std::string, bool>* varIsBool = nullptr) {
        switch (c.type) {
            case AstNode::Type::CondEq:
            case AstNode::Type::CondNe:
            case AstNode::Type::CondLt:
            case AstNode::Type::CondLe:
            case AstNode::Type::CondGt:
            case AstNode::Type::CondGe:
            case AstNode::Type::CondAnd:
            case AstNode::Type::CondOr:
                return "(" + emitCond(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            default:
                return emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        }
    }

    // The six nodes emitComparison handles: == != < <= > >=.
    static bool isComparisonNodeType(AstNode::Type t) {
        return t == AstNode::Type::CondEq || t == AstNode::Type::CondNe ||
               t == AstNode::Type::CondLt || t == AstNode::Type::CondLe ||
               t == AstNode::Type::CondGt || t == AstNode::Type::CondGe;
    }

    // Nothing but a literal, or a + of literals. Such an operand always constant-folds,
    // so a comparison between two of them emits `true`/`false` and no std::string at all.
    static bool isPlainStringLiteralChain(const AstNode& n) {
        if (n.type == AstNode::Type::ExprStringLiteral) return true;
        if (n.type == AstNode::Type::ExprAdd && n.children.size() >= 2) {
            return isPlainStringLiteralChain(n.children[0]) && isPlainStringLiteralChain(n.children[1]);
        }
        return false;
    }

    // Over-approximates "emitExpr may render this as a bare "..." rather than a std::string
    // value": the node kinds that fold a compile-time-known string through emitCppStringValue.
    // Deliberately syntactic -- callers may run before name resolution.
    static bool mayEmitBareCppStringLiteral(const AstNode& n) {
        switch (n.type) {
            case AstNode::Type::ExprStringLiteral:
            case AstNode::Type::ExprAdd:
            case AstNode::Type::StrMethod:
            case AstNode::Type::ExprSlice:
            case AstNode::Type::CryptoCall:
                return true;
            default:
                return false;
        }
    }

    // Apply a Nexa comparison operator to the result of std::string::compare (or of any
    // other three-way "negative / zero / positive" result).
    static bool comparisonHolds(const std::string& op, int cmp) {
        if (op == "==") return cmp == 0;
        if (op == "!=") return cmp != 0;
        if (op == "<") return cmp < 0;
        if (op == "<=") return cmp <= 0;
        if (op == ">") return cmp > 0;
        return cmp >= 0;  // ">="
    }

    // One emitter for all six comparison operators, because they share a trap: a comparison
    // with a string on BOTH sides must compare characters, not addresses.
    //
    // A bare C++ string literal is a `const char[N]` that decays to `const char*`, so
    // `"abc" < "abd"` in the generated code compares two addresses. That is unspecified, it
    // is not what the Nexa program asked for, and clang says so twice (-Warray-compare,
    // -Wstring-compare) right in the user's face. Only `==`/`!=` between *identical* spellings
    // appear to work, and only because the C++ compiler happens to pool equal literals.
    //
    // Two defences, in order:
    //   1. Constant-fold when both sides are known at compile time. This is the common case --
    //      NexaC already folds literal receivers for string methods -- and it emits no
    //      comparison at all, so there is nothing left to get wrong.
    //   2. Otherwise, if both sides still emit as bare literals, promote the left one to
    //      std::string so the operator resolves to a by-value comparison.
    // A string *variable* on either side needs neither: that side is already a std::string and
    // drags the other into a value comparison.
    std::string emitComparison(const AstNode& c, const std::string& op,
                               const std::map<std::string, std::string>& varMap,
                               const std::map<std::string, bool>* varIsString,
                               const std::map<std::string, bool>* varIsFloat,
                               const std::map<std::string, bool>* varIsChar,
                               const std::map<std::string, bool>* varIsBool) {
        if (c.children.size() >= 2) {
            if (auto L = tryFoldComparableString(c.children[0], varIsString)) {
                if (auto R = tryFoldComparableString(c.children[1], varIsString)) {
                    // std::string::compare orders by char_traits<char>, which is the same
                    // ordering the generated code would use at runtime.
                    return comparisonHolds(op, L->compare(*R)) ? "true" : "false";
                }
            }
        }
        std::string lhs = emitCmpOperand(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        std::string rhs = emitCmpOperand(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        if (isBareCppStringLiteral(lhs) && isBareCppStringLiteral(rhs)) lhs = asCppStdString(lhs);
        return lhs + " " + op + " " + rhs;
    }

    // A value written where a condition goes.
    //
    // Nexa's truthiness rules are not written down anywhere else, so they are
    // here: text is true when it is not empty, a number when it is not zero, a
    // pointer when it is not null. That is the whole list, and it is decided
    // from the expression's Nexa type rather than from which node it happens
    // to be -- which is why this replaced a switch that had to name every node
    // type and silently answered "false" for the ones nobody had added yet.
    //
    // `if (a + b)`, `if (len(xs))`, `if (os.getenv("HOME"))`, `if (math.abs(x))`
    // and every gfx.key in every program were all that default. They compiled,
    // they ran, and the branch never happened.
    //
    // What has no reading as a condition is refused by name instead. A Result
    // is the long-standing one; a json value, a slice and a map are the same
    // mistake wearing different clothes, and each is one call away from the
    // thing the program meant.
    std::string emitCondValue(const AstNode& c, const std::map<std::string, std::string>& varMap,
                              const std::map<std::string, bool>* varIsString,
                              const std::map<std::string, bool>* varIsFloat,
                              const std::map<std::string, bool>* varIsChar,
                              const std::map<std::string, bool>* varIsBool) {
        const std::string t = inferExprNexaType(c);
        if (nexaIsResultType(t)) {
            throw std::runtime_error("Result is not a condition; use .ok()");
        }
        if (t == "json") {
            throw std::runtime_error("a json value is not a condition; use .ok(), .is_null() or a comparison");
        }
        if (t.size() >= 2 && t[0] == '[' && t[1] == ']') {
            throw std::runtime_error("a slice is not a condition; use len(x) > 0");
        }
        if (t.size() >= 4 && t.compare(0, 4, "map[") == 0) {
            throw std::runtime_error("a map is not a condition; use len(x) > 0");
        }
        std::string v = emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        if (t == "string") return "!(" + v + ").empty()";
        return v;
    }

    std::string emitCond(const AstNode& c, const std::map<std::string, std::string>& varMap,
                         const std::map<std::string, bool>* varIsString = nullptr,
                         const std::map<std::string, bool>* varIsFloat = nullptr,
                         const std::map<std::string, bool>* varIsChar = nullptr,
                         const std::map<std::string, bool>* varIsBool = nullptr) {
        switch (c.type) {
            case AstNode::Type::CondEq:
                return emitComparison(c, "==", varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondNe:
                return emitComparison(c, "!=", varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondLt:
                return emitComparison(c, "<", varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondLe:
                return emitComparison(c, "<=", varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondGt:
                return emitComparison(c, ">", varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondGe:
                return emitComparison(c, ">=", varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondAnd: {
                std::string L = emitCond(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (c.children[0].type == AstNode::Type::CondOr) L = "(" + L + ")";
                std::string R = emitCond(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (c.children[1].type == AstNode::Type::CondOr) R = "(" + R + ")";
                return L + " && " + R;
            }
            case AstNode::Type::CondOr: {
                std::string L = emitCond(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (c.children[0].type == AstNode::Type::CondAnd) L = "(" + L + ")";
                std::string R = emitCond(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (c.children[1].type == AstNode::Type::CondAnd) R = "(" + R + ")";
                return L + " || " + R;
            }
            case AstNode::Type::CondNot:
                return "!(" + emitCond(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprTernary:
                return emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::ExprBoolLiteral:
                return c.value;
            case AstNode::Type::StrMethod:
                return emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::ExprIntLiteral:
            case AstNode::Type::ExprFloatLiteral:
            case AstNode::Type::ExprCharLiteral:
            case AstNode::Type::ExprVarRef:
                // A bare name goes through the same rules as anything else:
                // a string variable in an `if` is "not empty" rather than a
                // C++ error about std::string, and a Result or a slice is
                // told what to use instead.
                return emitCondValue(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::FileExists: {
                return "__nexa_file_exists(" + emitFilePathCStr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            }
            case AstNode::Type::FnCall:
            case AstNode::Type::ExprCall:
            case AstNode::Type::ExprLambda:
            case AstNode::Type::ExprStructLit:
            case AstNode::Type::ExprArrayIndex:
            case AstNode::Type::ExprSlice:
            case AstNode::Type::ExprMember:
            case AstNode::Type::JsonCall:
                return emitCondValue(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            // Every tcp.* call but recv is an int, and a handle or a byte count
            // is true exactly when it is not 0 -- so `if (tcp.connect(...))`
            // reads the way the failure value was chosen to read. recv is the
            // one that hands back bytes, and there "true" is "got some".
            case AstNode::Type::TcpCall: {
                std::string call = emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return c.value == "recv" ? "!(" + call + ").empty()" : call;
            }
            // udp.* is the same int-or-bytes split: a handle or a count is true
            // when it is not 0, and the two calls that hand back text are true
            // when there is text.
            case AstNode::Type::UdpCall: {
                std::string call = emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return (c.value == "recv" || c.value == "sender")
                    ? "!(" + call + ").empty()" : call;
            }
            // gfx.* and gfx3d.* answer 1/0, a handle or a count, and any of
            // those is true when it is not zero. The few that answer text are
            // true when there is text, which is the rule tcp.recv already
            // follows two cases up.
            //
            // Without these, `if (gfx.key("w"))` fell through to the default
            // below and became `if (false)`: the whole input family of both
            // modules read as never-happening, silently, in a program that
            // compiled and ran.
            case AstNode::Type::GfxCall: {
                std::string call = emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                const std::string& m = c.value;
                if (m == "drop" || m == "typed" || m == "opendialog" || m == "openfile"
                        || (m == "title" && c.children.empty())) {
                    return "!(" + call + ").empty()";
                }
                return call;
            }
            case AstNode::Type::Gfx3dCall: {
                std::string call = emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (c.value == "backend" || c.value == "typed") {
                    return "!(" + call + ").empty()";
                }
                return call;
            }
            case AstNode::Type::HttpCall:
            case AstNode::Type::ResultMake:
                throw std::runtime_error("Result is not a condition; use .ok()");
            default:
                // Everything else is a value, and a value is a condition by
                // the rules above -- not by whether anyone remembered to add
                // it to this switch.
                return emitCondValue(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        }
    }

    std::string emitStructLiteral(const AstNode& e,
                                 const std::map<std::string, std::string>& varMap,
                                 const std::map<std::string, bool>* varIsString,
                                 const std::map<std::string, bool>* varIsFloat,
                                 const std::map<std::string, bool>* varIsChar,
                                 const std::map<std::string, bool>* varIsBool) {
        auto oit = structFieldOrder_.find(e.value);
        if (oit == structFieldOrder_.end()) {
            throw std::runtime_error("Unknown struct type: " + e.value);
        }
        std::map<std::string, const AstNode*> provided;
        for (size_t i = 0; i < e.paramNames.size(); i++) {
            if (i < e.children.size()) provided[e.paramNames[i]] = &e.children[i];
        }
        const auto& fields = structFields_[e.value];
        for (const auto& kv : provided) {
            if (!fields.count(kv.first)) {
                throw std::runtime_error("Unknown field '" + kv.first + "' in " + e.value);
            }
        }
        std::string s = nexaTypeToCpp("struct:" + e.value) + "{";
        const std::vector<std::string>& order = oit->second;
        for (size_t i = 0; i < order.size(); i++) {
            if (i) s += ", ";
            auto pit = provided.find(order[i]);
            if (pit == provided.end()) {
                s += "{}";
            } else {
                s += emitExpr(*pit->second, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            }
        }
        s += "}";
        return s;
    }

    std::string emitLambdaExpr(const AstNode& e,
                              const std::map<std::string, std::string>& varMap,
                              const std::map<std::string, bool>* varIsString,
                              const std::map<std::string, bool>* varIsFloat,
                              const std::map<std::string, bool>* varIsChar,
                              const std::map<std::string, bool>* varIsBool) {
        std::string ty = fnTypeFromLambdaAst(e);
        std::vector<std::string> pts;
        std::string ret;
        if (!nexaSplitFnType(ty, pts, ret)) {
            throw std::runtime_error("Internal: invalid lambda type");
        }
        std::map<std::string, std::string> localMap = varMap;
        std::map<std::string, bool> localStr = varIsString ? *varIsString : std::map<std::string, bool>{};
        std::map<std::string, bool> localConst;
        std::map<std::string, bool> localFloat = varIsFloat ? *varIsFloat : std::map<std::string, bool>{};
        std::map<std::string, bool> localChar = varIsChar ? *varIsChar : std::map<std::string, bool>{};
        std::map<std::string, bool> localBool = varIsBool ? *varIsBool : std::map<std::string, bool>{};
        std::map<std::string, bool> localEnum;
        int varIdx = 10000;
        nexaDeclStack_.push_back({});
        varStructPush();
        std::string sig;
        for (size_t i = 0; i < e.paramNames.size(); i++) {
            if (i) sig += ", ";
            std::string nexaT = (i < pts.size()) ? pts[i] : "int";
            std::string pname = preserveNames_ ? e.paramNames[i] : ("__nexa_lam_" + std::to_string(i));
            sig += nexaTypeToCpp(nexaT) + " " + pname;
            localMap[e.paramNames[i]] = pname;
            localStr[e.paramNames[i]] = (nexaT == "string");
            localFloat[e.paramNames[i]] = (nexaT == "float");
            localChar[e.paramNames[i]] = (nexaT == "char");
            localBool[e.paramNames[i]] = (nexaT == "bool");
            localEnum[e.paramNames[i]] = isEnumDeclType(nexaT);
            nexaDeclStack_.back()[e.paramNames[i]] = nexaT;
            if (isStructDeclType(nexaT)) {
                varStructDeclare(e.paramNames[i], structNameFromDecl(nexaT));
            }
        }
        bool hasValRet = false, hasVoidRet = false;
        stmtsClassifyReturns(e.children, hasValRet, hasVoidRet);
        bool voidFn = (ret == "void");
        EmitFnRet savedRet = emitFnRet_;
        emitFnRet_ = voidFn ? EmitFnRet::VoidFn : EmitFnRet::IntFn;
        std::ostringstream body;
        emitBlockStatements(body, e.children, localMap, varIdx, localStr, localConst, localFloat,
                            localChar, localBool, localEnum);
        emitImplicitFnTail(body, e, hasValRet);
        emitFnRet_ = savedRet;
        varStructPop();
        nexaDeclStack_.pop_back();
        return nexaTypeToCpp(ty) + "([&](" + sig + ") -> " + nexaTypeToCpp(ret) + " {\n" + body.str() + "})";
    }

    // C++ precedence tier of a Nexa binary node, higher binds tighter.
    // -1 means "not a plain binary operator", so nothing flattens through it.
    static int cppBinaryPrec(AstNode::Type t) {
        switch (t) {
            case AstNode::Type::ExprMul:
            case AstNode::Type::ExprDiv:
            case AstNode::Type::ExprMod:    return 5;
            case AstNode::Type::ExprAdd:
            case AstNode::Type::ExprSub:    return 4;
            case AstNode::Type::ExprShl:
            case AstNode::Type::ExprShr:    return 3;
            case AstNode::Type::ExprBitAnd: return 2;
            case AstNode::Type::ExprBitXor: return 1;
            case AstNode::Type::ExprBitOr:  return 0;
            default:                        return -1;
        }
    }

    static const char* cppBinaryOp(AstNode::Type t) {
        switch (t) {
            case AstNode::Type::ExprMul:    return " * ";
            case AstNode::Type::ExprDiv:    return " / ";
            case AstNode::Type::ExprMod:    return " % ";
            case AstNode::Type::ExprAdd:    return " + ";
            case AstNode::Type::ExprSub:    return " - ";
            case AstNode::Type::ExprShl:    return " << ";
            case AstNode::Type::ExprShr:    return " >> ";
            case AstNode::Type::ExprBitAnd: return " & ";
            case AstNode::Type::ExprBitXor: return " ^ ";
            case AstNode::Type::ExprBitOr:  return " | ";
            default:                        return " ? ";
        }
    }

    // Emit a binary node as a bare `lhs op rhs`, with no parens of its own.
    //
    // Every one of these operators is left-associative in C++, so a left child
    // in the same precedence tier parses identically with or without its
    // parens: `a - b + c` is `(a - b) + c`. Emitting the parens anyway costs
    // one bracket-nesting level per term, and clang stops at 256 — a flat,
    // perfectly valid 300-term sum used to fail to build.
    //
    // Only a *same-tier* left child flattens. That is deliberately narrower
    // than "C++ would parse it the same": it keeps every paren that guards a
    // grouping C++ reads differently from Nexa (comparison and equality bind
    // looser here than `&`), and it stays clear of the mixed-operator shapes
    // compilers warn about — `a & b | c` (-Wparentheses), `a + b << c`
    // (-Wshift-op-parentheses). The right child keeps its parens, which is
    // what makes `a - (b - c)` survive.
    std::string emitBinaryChain(const AstNode& e, const std::map<std::string, std::string>& varMap,
        const std::map<std::string, bool>* varIsString,
        const std::map<std::string, bool>* varIsFloat,
        const std::map<std::string, bool>* varIsChar,
        const std::map<std::string, bool>* varIsBool) {
        static const std::map<std::string, bool> kEmptyTypeMap;
        const std::map<std::string, bool>& vIsStr = varIsString ? *varIsString : kEmptyTypeMap;
        const AstNode& l = e.children[0];
        std::string out = emitsPlainBinaryOp(l, vIsStr) && cppBinaryPrec(l.type) == cppBinaryPrec(e.type)
            ? emitBinaryChain(l, varMap, varIsString, varIsFloat, varIsChar, varIsBool)
            : emitExpr(l, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        out += cppBinaryOp(e.type);
        out += emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        return out;
    }

    // True when this node's C++ is literally `lhs op rhs`, which is the only
    // shape emitBinaryChain may flatten. Two binary nodes are not: `+` over
    // strings folds into a concat, and `^` with a string on the left becomes a
    // per-character lambda.
    bool emitsPlainBinaryOp(const AstNode& e, const std::map<std::string, bool>& varIsString) const {
        if (cppBinaryPrec(e.type) < 0 || e.children.size() < 2) return false;
        if (e.type == AstNode::Type::ExprAdd) return !exprIsString(e, varIsString);
        if (e.type == AstNode::Type::ExprBitXor) return !exprIsString(e.children[0], varIsString);
        return true;
    }

    std::string emitExpr(const AstNode& e, const std::map<std::string, std::string>& varMap,
        const std::map<std::string, bool>* varIsString = nullptr,
        const std::map<std::string, bool>* varIsFloat = nullptr,
        const std::map<std::string, bool>* varIsChar = nullptr,
        const std::map<std::string, bool>* varIsBool = nullptr) {
        static const std::map<std::string, bool> kEmptyTypeMap;
        const std::map<std::string, bool>& vIsStr = varIsString ? *varIsString : kEmptyTypeMap;
        switch (e.type) {
            case AstNode::Type::ExprIntLiteral:
                return emitIntLiteral(e.value);
            case AstNode::Type::ExprFloatLiteral:
                return e.value;
            case AstNode::Type::ExprCharLiteral: {
                if (e.value.empty()) return "'\\0'";
                unsigned char c = static_cast<unsigned char>(e.value[0]);
                if (c == '\'') return "'\\''";
                if (c == '\\') return "'\\\\'";
                if (c == '\n') return "'\\n'";
                if (c == '\t') return "'\\t'";
                if (c == '\r') return "'\\r'";
                if (c < 32 || c >= 127) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "'\\x%02X'", c);
                    return std::string(buf);
                }
                return "'" + std::string(1, c) + "'";
            }
            case AstNode::Type::ExprBoolLiteral:
                return e.value;
            case AstNode::Type::ExprStringLiteral:
                // C++ string literal (decays to const char*); avoids heap alloc vs std::string("...").
                // String concat must not produce const char* + const char* — see emitConcatOperand folds.
                return emitCppStringValue(e.value);
            case AstNode::Type::OsGetenv: {
                std::string s = "([]{ const char* __p = getenv(\"" + escapeString(e.value) + "\"); return __p ? std::string(__p) : std::string(\"\"); }())";
                return s;
            }
            case AstNode::Type::OsExec: {
                std::string cmd = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (exprIsString(e.children[0], *varIsString)) {
                    return "__nexa_os_exec(" + cmd + ")";
                }
                return "__nexa_os_exec(std::to_string(" + cmd + "))";
            }
            case AstNode::Type::OsSpawn:
                return emitOsSpawnCall(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::OsWait:
                return "__nexa_os_wait(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::OsKill:
                return "__nexa_os_kill(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::OsTempDir:
                return "__nexa_os_tempdir()";
            case AstNode::Type::OsArch:
                return "__nexa_os_arch()";
            case AstNode::Type::OsCpuCount:
                return "__nexa_os_cpu_count()";
            case AstNode::Type::OsWhich:
                return "__nexa_os_which(" + emitOsStringArg(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::OsExecutable:
                return "__nexa_os_executable()";
            case AstNode::Type::OsCwd:
                return "__nexa_os_cwd()";
            case AstNode::Type::OsInfo:
                return "__nexa_os_" + e.value + "()";
            case AstNode::Type::OsChdir:
                return "__nexa_os_chdir(" + emitOsStringArg(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::OsPlatform:
                return "__nexa_os_platform()";
            case AstNode::Type::OsGrepKeys:
                return "__nexa_os_grepkeys()";
            case AstNode::Type::OsKeyPressed:
                return "__nexa_os_keypressed()";
            case AstNode::Type::OsGetProcessId:
                if (!e.children.empty()) {
                    std::string nameExpr = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "__nexa_os_getprocessid_by_name(" + nameExpr + ")";
                }
                return "__nexa_os_getprocessid()";
            case AstNode::Type::OsExeDir:
                return "__nexa_exe_dir()";
            case AstNode::Type::OsHostname:
                return "__nexa_os_hostname()";
            case AstNode::Type::OsUsername:
                return "__nexa_os_username()";
            case AstNode::Type::OsHome:
                return "__nexa_os_home()";
            case AstNode::Type::OsGetVolume:
                return "__nexa_os_get_volume()";
            case AstNode::Type::OsGetBrightness:
                return "__nexa_os_get_brightness()";
            case AstNode::Type::OsClipGet:
                return "__nexa_os_clip_get()";
            case AstNode::Type::OsLoad:
                return "__nexa_os_load(" + emitOsStringArg(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::OsSave:
                return "__nexa_os_save(" + emitOsStringArg(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ", " +
                    emitOsStringArg(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::OsPlay:
                return "__nexa_os_play(" + emitOsStringArg(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::IoReadln: {
                return "([]{ fflush(stdout); char __b[4096]; if (fgets(__b, sizeof(__b), stdin)) __b[strcspn(__b, \"\\n\")] = 0; return std::string(__b); }())";
            }
            case AstNode::Type::IoGetline: {
                std::string src = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (e.children.size() >= 2) {
                    bool argIsString = exprIsString(e.children[1], vIsStr);
                    std::string arg2 = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    if (argIsString) {
                        return "__nexa_io_getline_by_key(" + src + ", " + arg2 + ")";
                    }
                    return "__nexa_io_getline(" + src + ", " + arg2 + ")";
                }
                return "__nexa_io_getline(" + src + ", 1)";
            }
            case AstNode::Type::IoToInt: {
                std::string s = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "__nexa_to_int(" + s + ")";
            }
            case AstNode::Type::FileRead: {
                return "__nexa_file_read(" + emitFilePathCStr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            }
            case AstNode::Type::FileWrite:
            case AstNode::Type::FileAppend: {
                static const std::map<std::string, bool> emptyFlags;
                return emitFileWriteOrAppendExpr(e, varMap,
                                                 varIsString ? *varIsString : emptyFlags,
                                                 varIsFloat ? *varIsFloat : emptyFlags,
                                                 varIsChar ? *varIsChar : emptyFlags,
                                                 varIsBool ? *varIsBool : emptyFlags,
                                                 e.type == AstNode::Type::FileAppend ? 1 : 0);
            }
            case AstNode::Type::FileExists: {
                return "(__nexa_file_exists(" + emitFilePathCStr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + "))";
            }
            case AstNode::Type::FileMkdir: {
                return "__nexa_file_mkdir(" + emitFilePathCStr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            }
            case AstNode::Type::FileCall: {
                const std::string& fn = e.value;
                if (fn == "cwd") return "__nexa_file_cwd()";
                std::string a0 = e.children.empty() ? "" : emitFilePathCStr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (fn == "remove") return "__nexa_file_remove(" + a0 + ")";
                if (fn == "remove_all") return "__nexa_file_remove_all(" + a0 + ")";
                if (fn == "list") return "__nexa_file_list(" + a0 + ")";
                if (fn == "isdir") return "__nexa_file_isdir(" + a0 + ")";
                if (fn == "isfile") return "__nexa_file_isfile(" + a0 + ")";
                if (fn == "size") return "__nexa_file_size(" + a0 + ")";
                if (fn == "chdir") return "__nexa_file_chdir(" + a0 + ")";
                if (fn == "abspath") return "__nexa_file_abspath(" + a0 + ")";
                if (fn == "dirname") return "__nexa_file_dirname(" + a0 + ")";
                if (fn == "basename") return "__nexa_file_basename(" + a0 + ")";
                if (fn == "extension") return "__nexa_file_extension(" + a0 + ")";
                if (fn == "rename" || fn == "copy" || fn == "join") {
                    std::string a1 = emitFilePathCStr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    if (fn == "rename") return "__nexa_file_rename(" + a0 + ", " + a1 + ")";
                    if (fn == "copy") return "__nexa_file_copy(" + a0 + ", " + a1 + ")";
                    return "__nexa_file_join(" + a0 + ", " + a1 + ")";
                }
                throw std::runtime_error("Internal: unknown file method '" + fn + "'");
            }
            case AstNode::Type::ExprLen: {
                if (e.children[0].type == AstNode::Type::ExprStringLiteral) {
                    return std::to_string(e.children[0].value.size());  // len("abc") is a constant
                }
                std::string s = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "(int)((" + asCppStdString(s) + ").size())";
            }
            case AstNode::Type::ExprTrim: {
                std::string w = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (e.children.size() >= 2) {
                    std::string p = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "([](const std::string& __w, const std::string& __p){ auto __nst = [](const std::string& __t)->std::string { size_t __a = __t.find_first_not_of(\" \\t\\n\\r\\f\\v\"); if (__a == std::string::npos) return std::string(); size_t __b = __t.find_last_not_of(\" \\t\\n\\r\\f\\v\"); return __t.substr(__a, __b - __a + 1); }; std::string __s = __nst(__w); if (!__p.empty() && __s.size() >= __p.size() && __s.compare(0, __p.size(), __p) == 0) return __nst(__s.substr(__p.size())); return __s; })(" + w + ", " + p + ")";
                }
                return "([](const std::string& __nexa_t){ size_t __a = __nexa_t.find_first_not_of(\" \\t\\n\\r\\f\\v\"); if (__a == std::string::npos) return std::string(); size_t __b = __nexa_t.find_last_not_of(\" \\t\\n\\r\\f\\v\"); return __nexa_t.substr(__a, __b - __a + 1); })(" + w + ")";
            }
            case AstNode::Type::RandomInt: {
                std::string minExpr = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                std::string maxExpr = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "__nexa_random_int(" + minExpr + ", " + maxExpr + ")";
            }
            case AstNode::Type::MathCall: {
                const std::string& fn = e.value;
                if (fn == "pi") return "3.14159265358979323846";
                if (fn == "e") return "2.71828182845904523536";
                std::string a0 = e.children.empty() ? "" : emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                std::string da0 = "static_cast<double>(" + a0 + ")";
                if (fn == "abs") return "std::abs(" + da0 + ")";
                if (fn == "min" || fn == "max") {
                    std::string a1 = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    // Explicit template argument keeps both operands double (avoids deduction failure on mixed int/float).
                    return "std::" + fn + "<double>(" + da0 + ", static_cast<double>(" + a1 + "))";
                }
                if (fn == "pow") {
                    std::string a1 = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "std::pow(" + da0 + ", static_cast<double>(" + a1 + "))";
                }
                // sqrt, floor, ceil, round, sin, cos, tan, log, log10, exp
                return "std::" + fn + "(" + da0 + ")";
            }
            case AstNode::Type::CryptoCall: {
                const std::string& fn = e.value;
                if (fn == "xor") {
                    std::string data = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    std::map<std::string, bool> emptyStr;
                    const auto& vIsStr = varIsString ? *varIsString : emptyStr;
                    if (e.children.size() == 2 && exprIsString(e.children[1], vIsStr)) {
                        std::string key = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                        return "__nexa_crypto_xor_key(" + data + ", " + key + ")";
                    }
                    std::string keys = "std::vector<int>{";
                    for (size_t i = 1; i < e.children.size(); ++i) {
                        if (i > 1) keys += ", ";
                        keys += emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    }
                    keys += "}";
                    return "__nexa_crypto_xor(" + data + ", " + keys + ")";
                }
                if (fn == "hex_encode" || fn == "hex_decode") {
                    if (!e.children.empty()) {
                        if (auto folded = tryFoldCryptoHex(fn, e.children[0], varIsString)) {
                            return emitCppStringValue(*folded);
                        }
                    }
                    std::map<std::string, bool> emptyM;
                    const auto& vs = varIsString ? *varIsString : emptyM;
                    const auto& vf = varIsFloat ? *varIsFloat : emptyM;
                    const auto& vc = varIsChar ? *varIsChar : emptyM;
                    const auto& vb = varIsBool ? *varIsBool : emptyM;
                    std::string a0 = emitConcatOperand(e.children[0], varMap, vs, vf, vc, vb);
                    return "__nexa_crypto_" + fn + "(" + a0 + ")";
                }
                if (fn == "sha256" || fn == "sha1" || fn == "base64_encode" || fn == "base64_decode") {
                    std::map<std::string, bool> emptyM;
                    const auto& vs = varIsString ? *varIsString : emptyM;
                    const auto& vf = varIsFloat ? *varIsFloat : emptyM;
                    const auto& vc = varIsChar ? *varIsChar : emptyM;
                    const auto& vb = varIsBool ? *varIsBool : emptyM;
                    std::string a0 = emitConcatOperand(e.children[0], varMap, vs, vf, vc, vb);
                    return "__nexa_crypto_" + fn + "(" + a0 + ")";
                }
                if (fn == "random_bytes") {
                    std::string n = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "__nexa_crypto_random_bytes(" + n + ")";
                }
                if (fn == "hmac_sha256") {
                    std::map<std::string, bool> emptyM;
                    const auto& vs = varIsString ? *varIsString : emptyM;
                    const auto& vf = varIsFloat ? *varIsFloat : emptyM;
                    const auto& vc = varIsChar ? *varIsChar : emptyM;
                    const auto& vb = varIsBool ? *varIsBool : emptyM;
                    std::string key = emitConcatOperand(e.children[0], varMap, vs, vf, vc, vb);
                    std::string data = emitConcatOperand(e.children[1], varMap, vs, vf, vc, vb);
                    return "__nexa_crypto_hmac_sha256(" + key + ", " + data + ")";
                }
                throw std::runtime_error("Internal: unknown crypto method '" + fn + "'");
            }
            case AstNode::Type::HttpCall: {
                const std::string& fn = e.value;
                auto arg = [&](size_t i) {
                    return emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                };
                // The optional trailing []string of raw header lines; absent
                // means an empty one, which every backend treats as "none".
                auto headers = [&](size_t i) {
                    return e.children.size() > i ? arg(i) : std::string("std::vector<std::string>()");
                };
                if (fn == "request") {
                    return "__nexa_http_request(" + arg(0) + ", " + arg(1) + ", " + arg(2) + ", " +
                        headers(3) + ")";
                }
                // The server half. http.localhost() with no port asks the OS
                // for a free one, which is port 0 on the wire.
                if (fn == "localhost") {
                    return "__nexa_http_localhost(" + (e.children.empty() ? std::string("0") : arg(0)) + ")";
                }
                if (fn == "accept") return "__nexa_http_accept(" + arg(0) + ")";
                if (fn == "close") return "__nexa_http_close(" + arg(0) + ")";
                if (fn == "raw") return "__nexa_http_raw(" + arg(0) + ", " + arg(1) + ")";
                if (fn == "reply") {
                    return "__nexa_http_reply(" + arg(0) + ", " + arg(1) + ", " + arg(2) + ", " +
                        headers(3) + ")";
                }
                std::string verb;
                for (size_t i = 0; i < fn.size(); i++) verb += (char)std::toupper((unsigned char)fn[i]);
                bool sendsBody = (fn == "post" || fn == "put" || fn == "patch");
                if (!sendsBody && fn != "get" && fn != "delete") {
                    throw std::runtime_error("Internal: unknown http method '" + fn + "'");
                }
                std::string body = sendsBody ? arg(1) : std::string("std::string()");
                return "__nexa_http_simple(\"" + verb + "\", " + arg(0) + ", " + body + ", " +
                    headers(sendsBody ? 2 : 1) + ")";
            }
            case AstNode::Type::TcpCall: {
                const std::string& fn = e.value;
                auto arg = [&](size_t i) {
                    return emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                };
                if (fn == "connect") return "__nexa_tcp_connect(" + arg(0) + ", " + arg(1) + ")";
                if (fn == "listen") return "__nexa_tcp_listen(" + arg(0) + ")";
                if (fn == "accept") return "__nexa_tcp_accept(" + arg(0) + ")";
                if (fn == "send") return "__nexa_tcp_send(" + arg(0) + ", " + arg(1) + ")";
                // tcp.recv(h) with no cap: 64K, which is a whole TCP window and
                // more than one read ever returns in practice.
                if (fn == "recv") {
                    return "__nexa_tcp_recv(" + arg(0) + ", " +
                        (e.children.size() > 1 ? arg(1) : std::string("65536")) + ")";
                }
                if (fn == "port") return "__nexa_tcp_port(" + arg(0) + ")";
                if (fn == "close") return "__nexa_tcp_close(" + arg(0) + ")";
                throw std::runtime_error("Internal: unknown tcp method '" + fn + "'");
            }
            case AstNode::Type::UdpCall: {
                const std::string& fn = e.value;
                auto arg = [&](size_t i) {
                    return emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                };
                if (fn == "open") return "__nexa_udp_open(" + arg(0) + ")";
                if (fn == "port") return "__nexa_udp_port(" + arg(0) + ")";
                if (fn == "send") {
                    return "__nexa_udp_send(" + arg(0) + ", " + arg(1) + ", " + arg(2) + ", " + arg(3) + ")";
                }
                // udp.recv(h) with no cap: 64K, which is past the largest
                // datagram IPv4 can carry, so nothing arrives truncated.
                if (fn == "recv") {
                    return "__nexa_udp_recv(" + arg(0) + ", " +
                        (e.children.size() > 1 ? arg(1) : std::string("65536")) + ")";
                }
                if (fn == "sender") return "__nexa_udp_sender(" + arg(0) + ")";
                if (fn == "sender_port") return "__nexa_udp_sender_port(" + arg(0) + ")";
                if (fn == "close") return "__nexa_udp_close(" + arg(0) + ")";
                throw std::runtime_error("Internal: unknown udp method '" + fn + "'");
            }
            case AstNode::Type::ResultMake: {
                if (e.value == "err") {
                    if (e.children.empty()) throw std::runtime_error("err(...) requires an error message");
                    std::string msg = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    std::string mt = inferExprNexaType(e.children[0]);
                    if (mt != "string") {
                        if (mt == "char" || mt == "unsigned char") {
                            msg = "std::string(1, static_cast<char>(" + msg + "))";
                        } else if (mt == "bool") {
                            msg = "((" + msg + ") ? std::string(\"true\") : std::string(\"false\"))";
                        } else if (mt == "float") {
                            msg = "__nexa_f2s(" + msg + ")";
                        } else {
                            msg = "std::to_string(" + msg + ")";
                        }
                    }
                    return "__nexa_result_err(" + msg + ")";
                }
                if (e.children.empty()) return "__nexa_result<void>::make_ok()";
                std::string innerT = inferExprNexaType(e.children[0]);
                std::string inner = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "__nexa_result<" + nexaTypeToCpp(innerT) + ">::make_ok(" + inner + ")";
            }
            case AstNode::Type::JsonCall: {
                const std::string& fn = e.value;
                auto a = [&](size_t i) {
                    return emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                };
                auto asJson = [&](size_t i) {
                    std::string s = a(i);
                    if (inferExprNexaType(e.children[i]) == "json") return s;
                    return std::string("__nexa_json_from(") + s + ")";
                };
                if (fn == "parse") {
                    if (e.children.empty()) throw std::runtime_error("json.parse expects a string");
                    return "__nexa_json_parse(" + a(0) + ")";
                }
                if (fn == "stringify") {
                    if (e.children.empty()) throw std::runtime_error("json.stringify expects a value");
                    std::string v = asJson(0);
                    if (e.children.size() >= 2) return "(" + v + ").stringify(" + a(1) + ")";
                    return "(" + v + ").stringify()";
                }
                if (fn == "of") {
                    if (e.children.empty()) throw std::runtime_error("json.of expects a value");
                    return asJson(0);
                }
                if (fn == "null") return "__nexa_json::nullv()";
                if (fn == "bool") {
                    if (e.children.empty()) throw std::runtime_error("json.bool expects a value");
                    return "__nexa_json::boolean(" + a(0) + ")";
                }
                if (fn == "int") {
                    if (e.children.empty()) throw std::runtime_error("json.int expects a value");
                    return "__nexa_json::number(static_cast<double>(" + a(0) + "))";
                }
                if (fn == "float") {
                    if (e.children.empty()) throw std::runtime_error("json.float expects a value");
                    return "__nexa_json::number(" + a(0) + ")";
                }
                if (fn == "string") {
                    if (e.children.empty()) throw std::runtime_error("json.string expects a value");
                    return "__nexa_json::str(" + a(0) + ")";
                }
                if (fn == "array") return "__nexa_json::arr()";
                if (fn == "object") return "__nexa_json::obj()";
                throw std::runtime_error("Internal: unknown json method '" + fn + "'");
            }
            case AstNode::Type::Gfx3dCall: {
                const std::string& fn = e.value;
                auto a = [&](size_t i) {
                    return emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                };
                // Every gfx3d builtin takes its arguments straight through in
                // the order they were written, so unlike gfx -- where open
                // fills in a scale and line picks between two helpers -- one
                // loop covers the lot.
                std::string args;
                for (size_t i = 0; i < e.children.size(); i++) {
                    if (i) args += ", ";
                    args += a(i);
                }
                // gfx3d.ambient reads with no argument and sets with one, so
                // the two forms are two functions rather than a sentinel
                // level that would collide with a real one.
                if (fn == "ambient" && e.children.empty()) {
                    return "__nexa_gfx3d_ambient_get()";
                }
                return "__nexa_gfx3d_" + fn + "(" + args + ")";
            }
            case AstNode::Type::GfxCall: {
                const std::string& fn = e.value;
                auto a = [&](size_t i) {
                    return emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                };
                if (fn == "open") {
                    std::string sc = e.children.size() >= 4 ? a(3) : "12";
                    return "__nexa_gfx_open(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + sc + ")";
                }
                if (fn == "close") return "__nexa_gfx_close()";
                if (fn == "resize") {
                    std::string sc = e.children.size() >= 3 ? a(2) : "-1";
                    return "__nexa_gfx_resize(" + a(0) + ", " + a(1) + ", " + sc + ")";
                }
                if (fn == "width") return "__nexa_gfx_width()";
                if (fn == "height") return "__nexa_gfx_height()";
                if (fn == "scale") return "__nexa_gfx_scale()";
                if (fn == "poll") return "__nexa_gfx_poll()";
                if (fn == "present") return "__nexa_gfx_present()";
                if (fn == "maxfps") return "__nexa_gfx_maxfps(" + a(0) + ")";
                if (fn == "closed") return "__nexa_gfx_closed()";
                if (fn == "key") return "__nexa_gfx_key(" + a(0) + ")";
                if (fn == "pressed") return "__nexa_gfx_pressed(" + a(0) + ")";
                if (fn == "released") return "__nexa_gfx_released(" + a(0) + ")";
                if (fn == "wheel") return "__nexa_gfx_wheel()";
                if (fn == "wheel_x") return "__nexa_gfx_wheel_x()";
                if (fn == "typed") return "__nexa_gfx_typed()";
                if (fn == "mouse_x") return "__nexa_gfx_mouse_x()";
                if (fn == "mouse_y") return "__nexa_gfx_mouse_y()";
                if (fn == "mouse") return "__nexa_gfx_mouse(" + a(0) + ")";
                if (fn == "clear") return "__nexa_gfx_clear(" + a(0) + ", " + a(1) + ", " + a(2) + ")";
                if (fn == "plot") return "__nexa_gfx_plot(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ")";
                if (fn == "get") return "__nexa_gfx_get(" + a(0) + ", " + a(1) + ")";
                if (fn == "fill") return "__nexa_gfx_fill(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ", " + a(5) + ", " + a(6) + ")";
                if (fn == "rect") return "__nexa_gfx_rect(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ", " + a(5) + ", " + a(6) + ")";
                if (fn == "line") {
                    std::string base = "(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ", " + a(5) + ", " + a(6);
                    if (e.children.size() >= 8) return "__nexa_gfx_line_thick" + base + ", " + a(7) + ")";
                    return "__nexa_gfx_line" + base + ")";
                }
                if (fn == "circle" || fn == "fill_circle") {
                    return "__nexa_gfx_" + fn + "(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ", " + a(5) + ")";
                }
                if (fn == "ellipse" || fn == "fill_ellipse") {
                    return "__nexa_gfx_" + fn + "(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ", " + a(5) + ", " + a(6) + ")";
                }
                // The two angles are the only non-integer arguments any shape
                // takes; the runtime reads them as double, so 45 and 45.5 both
                // arrive as themselves.
                if (fn == "arc" || fn == "pie" || fn == "round_rect" || fn == "fill_round_rect") {
                    return "__nexa_gfx_" + fn + "(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " +
                        a(4) + ", " + a(5) + ", " + a(6) + ", " + a(7) + ")";
                }
                if (fn == "tri" || fn == "fill_tri") {
                    return "__nexa_gfx_" + fn + "(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ", " +
                        a(5) + ", " + a(6) + ", " + a(7) + ", " + a(8) + ")";
                }
                // The point lists are checked in semCheckGfxPoly, which has the
                // source line to complain on.
                if (fn == "poly" || fn == "fill_poly") {
                    return "__nexa_gfx_" + fn + "(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ")";
                }
                if (fn == "text") {
                    std::string sc = e.children.size() >= 7 ? a(6) : "__nexa_gfx_text_scale()";
                    return "__nexa_gfx_text(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + a(4) + ", " + a(5) + ", " + sc + ")";
                }
                if (fn == "text_size") {
                    if (e.children.empty()) return "__nexa_gfx_text_scale()";
                    return "__nexa_gfx_text_size_set(" + a(0) + ")";
                }
                if (fn == "text_width") {
                    std::string sc = e.children.size() >= 2 ? a(1) : "-1";
                    return "__nexa_gfx_text_width(" + a(0) + ", " + sc + ")";
                }
                if (fn == "text_height") {
                    std::string sc = e.children.size() >= 2 ? a(1) : "-1";
                    return "__nexa_gfx_text_height(" + a(0) + ", " + sc + ")";
                }
                if (fn == "title") {
                    if (e.children.empty()) return "__nexa_gfx_title_get()";
                    return "__nexa_gfx_title_set(" + a(0) + ")";
                }
                if (fn == "drop") return "__nexa_gfx_drop()";
                if (fn == "fullscreen") {
                    std::string v = e.children.empty() ? "-1" : a(0);
                    return "__nexa_gfx_fullscreen(" + v + ")";
                }
                if (fn == "borderless") {
                    std::string v = e.children.empty() ? "-1" : a(0);
                    return "__nexa_gfx_borderless(" + v + ")";
                }
                if (fn == "ontop") {
                    std::string v = e.children.empty() ? "-1" : a(0);
                    return "__nexa_gfx_ontop(" + v + ")";
                }
                if (fn == "transparent") {
                    std::string v = e.children.empty() ? "-1" : a(0);
                    return "__nexa_gfx_transparent(" + v + ")";
                }
                if (fn == "audio") {
                    std::string r = e.children.empty() ? "44100" : a(0);
                    return "__nexa_gfx_audio(" + r + ")";
                }
                if (fn == "sample") return "__nexa_gfx_sample(" + a(0) + ")";
                if (fn == "audio_queued") return "__nexa_gfx_audio_queued()";
                // Flushing is "start playing what I have queued", so the mixer
                // gets its turn first. __nexa_gfx_mix_pump is an empty body in
                // a program that never plays a sound.
                if (fn == "audio_flush") return "(__nexa_gfx_mix_pump(), __nexa_gfx_audio_flush(), 0)";
                if (fn == "sound") return "__nexa_gfx_sound(" + a(0) + ")";
                if (fn == "play" || fn == "loop") {
                    std::string v = e.children.size() >= 2 ? a(1) : "255";
                    return "__nexa_gfx_voice_start(" + a(0) + ", " + v + ", " +
                        (fn == "loop" ? "1" : "0") + ")";
                }
                // gfx.stop() is every voice, which the runtime spells as voice 0.
                if (fn == "stop") return "__nexa_gfx_stop(" + (e.children.empty() ? "0" : a(0)) + ")";
                if (fn == "volume") {
                    if (e.children.empty()) return "__nexa_gfx_volume_get()";
                    return "__nexa_gfx_volume_set(" + a(0) + ")";
                }
                if (fn == "opendialog" || fn == "openfile") {
                    std::string f = e.children.empty() ? "std::string()" : a(0);
                    return "__nexa_gfx_opendialog(" + f + ")";
                }
                if (fn == "alpha") {
                    if (e.children.empty()) return "__nexa_gfx_alpha_get()";
                    return "__nexa_gfx_alpha_set(" + a(0) + ")";
                }
                if (fn == "save") return "__nexa_gfx_save(" + a(0) + ")";
                if (fn == "image") return "__nexa_gfx_image(" + a(0) + ")";
                if (fn == "decode") return "__nexa_gfx_decode(" + a(0) + ")";
                if (fn == "image_w") return "__nexa_gfx_image_w(" + a(0) + ")";
                if (fn == "image_h") return "__nexa_gfx_image_h(" + a(0) + ")";
                if (fn == "blit") {
                    std::string dw = "0", dh = "0", sx = "0", sy = "0", sw = "0", sh = "0";
                    if (e.children.size() == 5) {
                        dw = a(3);
                        dh = a(4);
                    } else if (e.children.size() == 7) {
                        sx = a(3);
                        sy = a(4);
                        sw = a(5);
                        sh = a(6);
                    } else if (e.children.size() >= 9) {
                        sx = a(3);
                        sy = a(4);
                        sw = a(5);
                        sh = a(6);
                        dw = a(7);
                        dh = a(8);
                    }
                    if (inferExprNexaType(e.children[2]) == "string") {
                        return "__nexa_gfx_blit_path(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + dw + ", " + dh + ", " + sx + ", " + sy + ", " + sw + ", " + sh + ")";
                    }
                    return "__nexa_gfx_blit(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + dw + ", " + dh + ", " + sx + ", " + sy + ", " + sw + ", " + sh + ")";
                }
                if (fn == "blit_rot") {
                    // 0 for an omitted destination size is the same "native
                    // size" sentinel gfx.blit uses.
                    std::string dw = "0", dh = "0";
                    if (e.children.size() >= 6) {
                        dw = a(4);
                        dh = a(5);
                    }
                    std::string base = "(" + a(0) + ", " + a(1) + ", " + a(2) + ", " + a(3) + ", " + dw + ", " + dh + ")";
                    if (inferExprNexaType(e.children[2]) == "string") {
                        return "__nexa_gfx_blit_rot_path" + base;
                    }
                    return "__nexa_gfx_blit_rot" + base;
                }
                if (fn == "icon") {
                    if (inferExprNexaType(e.children[0]) == "string") {
                        return "__nexa_gfx_icon_path(" + a(0) + ")";
                    }
                    return "__nexa_gfx_icon(" + a(0) + ")";
                }
                if (fn == "cursor") {
                    // -1 means "report the state", the way gfx.fullscreen's does.
                    std::string v = e.children.empty() ? "-1" : a(0);
                    return "__nexa_gfx_cursor(" + v + ")";
                }
                throw std::runtime_error("Internal: unknown gfx method '" + fn + "'");
            }
            case AstNode::Type::StrMethod: {
                if (auto folded = tryFoldStrMethodToExpr(e, varIsString)) return *folded;
                const std::string& m = e.value;
                std::string R = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                std::string A0 = e.children.size() > 1 ? emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) : "";
                std::string A1 = e.children.size() > 2 ? emitExpr(e.children[2], varMap, varIsString, varIsFloat, varIsChar, varIsBool) : "";
                if (m == "upper")
                    return "([](std::string __s){ for (char& __c : __s) __c = (char)std::toupper((unsigned char)__c); return __s; })(" + R + ")";
                if (m == "lower")
                    return "([](std::string __s){ for (char& __c : __s) __c = (char)std::tolower((unsigned char)__c); return __s; })(" + R + ")";
                if (m == "trim")
                    return "([](const std::string& __s){ size_t __a = __s.find_first_not_of(\" \\t\\n\\r\\f\\v\"); if (__a == std::string::npos) return std::string(); size_t __b = __s.find_last_not_of(\" \\t\\n\\r\\f\\v\"); return __s.substr(__a, __b - __a + 1); })(" + R + ")";
                if (m == "len") {
                    if (inferExprNexaType(e.children[0]) == "json") return R + ".len()";
                    return "([](const std::string& __s){ return (int)__s.size(); })(" + R + ")";
                }
                if (m == "contains") {
                    if (nexaIsSliceType(inferExprNexaType(e.children[0]))) {
                        return "([&](){ const auto& __nexa_v = " + R + "; const auto& __nexa_x = " + A0 +
                            "; for (const auto& __nexa_e : __nexa_v) { if (__nexa_e == __nexa_x) return true; } return false; })()";
                    }
                    return "([](const std::string& __s, const std::string& __p){ return __s.find(__p) != std::string::npos; })(" + R + ", " + A0 + ")";
                }
                if (m == "starts_with")
                    return "([](const std::string& __s, const std::string& __p){ return __s.size() >= __p.size() && __s.compare(0, __p.size(), __p) == 0; })(" + R + ", " + A0 + ")";
                if (m == "ends_with")
                    return "([](const std::string& __s, const std::string& __p){ return __s.size() >= __p.size() && __s.compare(__s.size() - __p.size(), __p.size(), __p) == 0; })(" + R + ", " + A0 + ")";
                if (m == "index_of") {
                    if (nexaIsSliceType(inferExprNexaType(e.children[0]))) {
                        return "([&](){ const auto& __nexa_v = " + R + "; const auto& __nexa_x = " + A0 +
                            "; for (int __nexa_i = 0; __nexa_i < (int)__nexa_v.size(); __nexa_i++) { if (__nexa_v[(size_t)__nexa_i] == __nexa_x) return __nexa_i; } return -1; })()";
                    }
                    return "([](const std::string& __s, const std::string& __p){ size_t __n = __s.find(__p); return __n == std::string::npos ? -1 : (int)__n; })(" + R + ", " + A0 + ")";
                }
                if (m == "repeat")
                    return "([](const std::string& __s, int __n){ std::string __o; for (int __i = 0; __i < __n; __i++) __o += __s; return __o; })(" + R + ", " + A0 + ")";
                if (m == "replace")
                    return "([](std::string __s, const std::string& __f, const std::string& __t){ if (__f.empty()) return __s; size_t __p = 0; while ((__p = __s.find(__f, __p)) != std::string::npos) { __s.replace(__p, __f.size(), __t); __p += __t.size(); } return __s; })(" + R + ", " + A0 + ", " + A1 + ")";
                if (m == "substring")
                    return "([](const std::string& __s, int __a, int __n){ if (__a < 0) __a = 0; if ((size_t)__a >= __s.size()) return std::string(); return __s.substr((size_t)__a, __n < 0 ? std::string::npos : (size_t)__n); })(" + R + ", " + A0 + ", " + A1 + ")";
                if (m == "split")
                    return "([](const std::string& __s, const std::string& __sep){ std::vector<std::string> __out; if (__sep.empty()) { __out.push_back(__s); return __out; } size_t __p = 0, __q; while ((__q = __s.find(__sep, __p)) != std::string::npos) { __out.push_back(__s.substr(__p, __q - __p)); __p = __q + __sep.size(); } __out.push_back(__s.substr(__p)); return __out; })(" + R + ", " + A0 + ")";
                return R;
            }
            case AstNode::Type::TimeSeconds: {
                std::string n = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "((" + n + ") * 1000)";
            }
            case AstNode::Type::TimeMilliseconds: {
                std::string n = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "(" + n + ")";
            }
            case AstNode::Type::TimeNowMs:
                return "__nexa_time_now_ms()";
            case AstNode::Type::ThreadSpawn: {
                if (e.children.empty()) {
                    size_t z = slotForZeroArgFunctionNamed(e.value);
                    return "__nexa_thread_spawn(&" + cppFnNameForSlot(z) + ")";
                }
                return "__nexa_thread_spawn_fn(" + emitThreadJobFn(e, varMap,
                    varIsString ? *varIsString : std::map<std::string, bool>{},
                    varIsFloat ? *varIsFloat : std::map<std::string, bool>{},
                    varIsChar ? *varIsChar : std::map<std::string, bool>{},
                    varIsBool ? *varIsBool : std::map<std::string, bool>{}) + ")";
            }
            case AstNode::Type::ThreadWorker:
                return "__nexa_thread_worker_create()";
            case AstNode::Type::ExprVarRef: {
                if (e.value == "self" && !methodSelfType_.empty()) return "(*this)";
                auto it = varMap.find(e.value);
                if (it != varMap.end()) return it->second;
                if (lookupNexaDecl(e.value).empty() && !uniqueNamedFnType(e.value).empty()) {
                    return cppFnNameForSlot(uniqueNamedFnSlot(e.value));
                }
                return e.value;
            }
            case AstNode::Type::ExprLambda:
                return emitLambdaExpr(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::ExprStructLit:
                return emitStructLiteral(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::ExprCall: {
                if (e.children.empty()) return "0";
                std::string s = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + "(";
                for (size_t i = 1; i < e.children.size(); i++) {
                    if (i > 1) s += ", ";
                    s += emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                }
                s += ")";
                return s;
            }
            case AstNode::Type::ExprArrayLiteral: {
                const std::string elemT = e.children.empty() ? emptySliceElemNexaType(e)
                                                             : arrayLiteralElemNexaType(e);
                std::string s = "std::vector<" + nexaTypeToCpp(elemT) + ">{";
                for (size_t i = 0; i < e.children.size(); i++) {
                    if (i > 0) s += ", ";
                    s += emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                }
                s += "}";
                return s;
            }
            case AstNode::Type::ExprArrayIndex: {
                if (e.children.size() >= 2) {
                    return emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + "[" +
                        emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + "]";
                }
                auto it = varMap.find(e.value);
                std::string v = (it != varMap.end()) ? it->second : e.value;
                return v + "[" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + "]";
            }
            case AstNode::Type::ExprSlice: {
                if (e.children.empty()) {
                    throw std::runtime_error("Internal: slice missing base");
                }
                std::string v = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                std::string baseT = inferExprNexaType(e.children[0]);
                std::string startExpr = "0";
                std::string endExpr = "(int)__nexa_s.size()";
                if (e.initValue == "both" && e.children.size() >= 3) {
                    startExpr = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    endExpr = emitExpr(e.children[2], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                } else if (e.initValue == "start" && e.children.size() >= 2) {
                    startExpr = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                } else if (e.initValue == "end" && e.children.size() >= 2) {
                    endExpr = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                }
                std::string bounds = "int __nexa_n = (int)__nexa_s.size(); int __nexa_a = " + startExpr +
                    "; int __nexa_b = " + endExpr +
                    "; if (__nexa_a < 0) __nexa_a = __nexa_n + __nexa_a; if (__nexa_b < 0) __nexa_b = __nexa_n + __nexa_b;"
                    " if (__nexa_a < 0) __nexa_a = 0; if (__nexa_b < 0) __nexa_b = 0;"
                    " if (__nexa_a > __nexa_n) __nexa_a = __nexa_n; if (__nexa_b > __nexa_n) __nexa_b = __nexa_n;"
                    " if (__nexa_b < __nexa_a) __nexa_b = __nexa_a; ";
                if (baseT == "string") {
                    return "([&](){ const auto& __nexa_s = " + asCppStdString(v) + "; " + bounds +
                        "return __nexa_s.substr((size_t)__nexa_a, (size_t)(__nexa_b - __nexa_a)); })()";
                }
                std::string cppT = nexaTypeToCpp(nexaIsSliceType(baseT) ? baseT : std::string("[]int"));
                return "([&](){ const auto& __nexa_s = " + asCppStdString(v) + "; " + bounds +
                    "return " + cppT + "(__nexa_s.begin() + __nexa_a, __nexa_s.begin() + __nexa_b); })()";
            }
            case AstNode::Type::ExprMember: {
                if (e.children.empty()) return e.value;
                if (!e.children.empty() && e.children[0].type == AstNode::Type::ExprVarRef) {
                    const std::string& base = e.children[0].value;
                    if (varMap.find(base) == varMap.end()) {
                        auto enIt = enumCppNames_.find(base);
                        if (enIt != enumCppNames_.end()) {
                            auto evIt = enumVariants_.find(base);
                            if (evIt != enumVariants_.end() && evIt->second.count(e.value)) {
                                return enIt->second + "::" + e.value;
                            }
                            throw std::runtime_error("Unknown enum variant '" + e.value + "' for '" + base + "'");
                        }
                    }
                }
                return emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool)
                    + (e.isArrowMember ? "->" : ".") + e.value;
            }
            case AstNode::Type::FnCall:
                return emitFnCallCpp(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::ExprAdd:
                if (exprIsString(e, vIsStr)) {
                    const std::map<std::string, bool>& vFl = varIsFloat ? *varIsFloat : kEmptyTypeMap;
                    const std::map<std::string, bool>& vCh = varIsChar ? *varIsChar : kEmptyTypeMap;
                    const std::map<std::string, bool>& vBo = varIsBool ? *varIsBool : kEmptyTypeMap;
                    // Delegate so literal chains fold to one "..." and never become const char* + const char*.
                    return emitConcatOperand(e, varMap, vIsStr, vFl, vCh, vBo);
                }
                [[fallthrough]];
            case AstNode::Type::ExprSub:
            case AstNode::Type::ExprMul:
            case AstNode::Type::ExprDiv:
            case AstNode::Type::ExprMod:
            case AstNode::Type::ExprBitAnd:
            case AstNode::Type::ExprBitOr:
            case AstNode::Type::ExprShl:
            case AstNode::Type::ExprShr:
                return "(" + emitBinaryChain(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitXor: {
                if (exprIsString(e.children[0], vIsStr) && !exprIsString(e.children[1], vIsStr)) {
                    std::string lhs = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    std::string rhs = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "([&]{ std::string __nexa_s = " + lhs + "; int __nexa_k = " + rhs
                        + "; for (size_t __nexa_i = 0; __nexa_i < __nexa_s.size(); __nexa_i++) "
                        "__nexa_s[__nexa_i] = (char)((unsigned char)__nexa_s[__nexa_i] ^ (__nexa_k & 0xFF)); return __nexa_s; }())";
                }
                return "(" + emitBinaryChain(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            }
            case AstNode::Type::ExprBitNot:
                return "(~" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprAddrOf:
                return "(&" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprDeref:
                return "(*" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprNull:
                return "nullptr";
            case AstNode::Type::ExprNew: {
                std::string cppT = nexaTypeToCpp(e.value);
                if (e.isFixedArray) {
                    if (e.children.empty()) {
                        throw std::runtime_error("new T[] requires a length");
                    }
                    std::string n = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "(new " + cppT + "[" + n + "]())";
                }
                return "(new " + cppT + "())";
            }
            case AstNode::Type::ExprSizeof: {
                if (!e.value.empty()) {
                    return "static_cast<int>(sizeof(" + nexaTypeToCpp(e.value) + "))";
                }
                if (e.children.empty()) return "0";
                return "static_cast<int>(sizeof(" +
                    emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + "))";
            }
            case AstNode::Type::ExprCast: {
                if (e.children.empty()) return "0";
                const std::string& to = e.value;
                std::string fromT = inferExprNexaType(e.children[0]);
                std::string inner = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (to == "int") {
                    if (fromT == "string") {
                        return "([](const std::string& __s){ char* __e=nullptr; long __v=std::strtol(__s.c_str(),&__e,10); return (__e==__s.c_str())?0:static_cast<int>(__v); })(std::string(" + inner + "))";
                    }
                    if (fromT == "bool") return "((" + inner + ") ? 1 : 0)";
                    return "static_cast<int>(" + inner + ")";
                }
                if (to == "float") {
                    if (fromT == "string") {
                        return "([](const std::string& __s){ char* __e=nullptr; double __v=std::strtod(__s.c_str(),&__e); return (__e==__s.c_str())?0.0:__v; })(std::string(" + inner + "))";
                    }
                    return "static_cast<double>(" + inner + ")";
                }
                if (to == "char") {
                    if (fromT == "string") {
                        return "((" + inner + ").empty() ? '\\0' : (" + inner + ")[0])";
                    }
                    return "static_cast<char>(" + inner + ")";
                }
                if (to == "unsigned char") {
                    if (fromT == "string") {
                        return "static_cast<unsigned char>((" + inner + ").empty() ? '\\0' : (" + inner + ")[0])";
                    }
                    return "static_cast<unsigned char>(" + inner + ")";
                }
                if (to == "unsigned int") {
                    if (fromT == "string") {
                        return "([](const std::string& __s){ char* __e=nullptr; unsigned long __v=std::strtoul(__s.c_str(),&__e,10); return (__e==__s.c_str())?0u:static_cast<unsigned int>(__v); })(std::string(" + inner + "))";
                    }
                    return "static_cast<unsigned int>(" + inner + ")";
                }
                if (to == "short") {
                    if (fromT == "string") {
                        return "([](const std::string& __s){ char* __e=nullptr; long __v=std::strtol(__s.c_str(),&__e,10); return (__e==__s.c_str())?static_cast<short>(0):static_cast<short>(__v); })(std::string(" + inner + "))";
                    }
                    return "static_cast<short>(" + inner + ")";
                }
                if (to == "unsigned short") {
                    if (fromT == "string") {
                        return "([](const std::string& __s){ char* __e=nullptr; unsigned long __v=std::strtoul(__s.c_str(),&__e,10); return (__e==__s.c_str())?static_cast<unsigned short>(0):static_cast<unsigned short>(__v); })(std::string(" + inner + "))";
                    }
                    return "static_cast<unsigned short>(" + inner + ")";
                }
                if (to == "long") {
                    if (fromT == "string") {
                        return "([](const std::string& __s){ char* __e=nullptr; long __v=std::strtol(__s.c_str(),&__e,10); return (__e==__s.c_str())?0L:__v; })(std::string(" + inner + "))";
                    }
                    return "static_cast<long>(" + inner + ")";
                }
                if (to == "unsigned long") {
                    if (fromT == "string") {
                        return "([](const std::string& __s){ char* __e=nullptr; unsigned long __v=std::strtoul(__s.c_str(),&__e,10); return (__e==__s.c_str())?0UL:__v; })(std::string(" + inner + "))";
                    }
                    return "static_cast<unsigned long>(" + inner + ")";
                }
                if (to == "size_t") {
                    if (fromT == "string") {
                        return "([](const std::string& __s){ char* __e=nullptr; unsigned long long __v=std::strtoull(__s.c_str(),&__e,10); return (__e==__s.c_str())?static_cast<std::size_t>(0):static_cast<std::size_t>(__v); })(std::string(" + inner + "))";
                    }
                    return "static_cast<std::size_t>(" + inner + ")";
                }
                if (to == "bool") {
                    if (fromT == "string") return "(!(" + inner + ").empty())";
                    return "static_cast<bool>(" + inner + ")";
                }
                if (to == "string") {
                    if (fromT == "string") return "std::string(" + inner + ")";
                    if (fromT == "char" || fromT == "unsigned char") return "std::string(1, static_cast<char>(" + inner + "))";
                    if (fromT == "bool") return "((" + inner + ") ? std::string(\"true\") : std::string(\"false\"))";
                    if (fromT == "float") return "__nexa_f2s(" + inner + ")";
                    return "std::to_string(" + inner + ")";
                }
                return inner;
            }
            case AstNode::Type::CondNot:
                return "(!" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprTernary: {
                std::string cond = emitCond(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                std::string t = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                std::string f = emitExpr(e.children[2], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (exprIsString(e, vIsStr) || exprProducesString(e)) {
                    if (e.children[1].type == AstNode::Type::ExprStringLiteral) t = "std::string(" + t + ")";
                    if (e.children[2].type == AstNode::Type::ExprStringLiteral) f = "std::string(" + f + ")";
                }
                return "((" + cond + ") ? (" + t + ") : (" + f + "))";
            }
            case AstNode::Type::CondAnd:
            case AstNode::Type::CondOr:
            case AstNode::Type::CondEq:
            case AstNode::Type::CondNe:
            case AstNode::Type::CondLt:
            case AstNode::Type::CondLe:
            case AstNode::Type::CondGt:
            case AstNode::Type::CondGe:
                return "(" + emitCond(e, varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            default:
                return "0";
        }
    }

    std::string emitFilePathCStr(const AstNode& path,
                                const std::map<std::string, std::string>& varMap,
                                const std::map<std::string, bool>* varIsString,
                                const std::map<std::string, bool>* varIsFloat,
                                const std::map<std::string, bool>* varIsChar,
                                const std::map<std::string, bool>* varIsBool) {
        static const std::map<std::string, bool> empty;
        const auto& vs = varIsString ? *varIsString : empty;
        if (auto folded = tryFoldStringLiteralChain(path, vs)) {
            return "\"" + escapeString(*folded) + "\"";
        }
        return "(" + emitExpr(path, varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ").c_str()";
    }

    // file.write/file.append as a single int-valued C++ expression (1 = success, 0 = failure).
    // Used for both statement position (the caller appends ';') and value position
    // (`let ok: int = file.write(p, s);`) so the two can never disagree.
    //
    // A foldable literal payload becomes a direct call. Anything else is passed to a lambda
    // taking `const std::string&`, which binds a string lvalue without copying and extends the
    // lifetime of a concat temporary across the call. The content argument is evaluated before
    // the path expression in the lambda body, matching the previous statement-only codegen.
    std::string emitFileWriteOrAppendExpr(const AstNode& child,
                                          const std::map<std::string, std::string>& varMap,
                                          const std::map<std::string, bool>& varIsString,
                                          const std::map<std::string, bool>& varIsFloat,
                                          const std::map<std::string, bool>& varIsChar,
                                          const std::map<std::string, bool>& varIsBool,
                                          int append) {
        std::string path = emitFilePathCStr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
        const AstNode& content = child.children[1];
        const std::string appendArg = std::to_string(append);
        if (auto folded = tryFoldStringLiteralChain(content, varIsString)) {
            return "__nexa_file_write(" + path + ", \"" + escapeString(*folded) + "\", " +
                   std::to_string(folded->size()) + ", " + appendArg + ")";
        }
        std::string contentCpp = exprIsString(content, varIsString)
            ? emitExpr(content, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool)
            : emitConcatOperand(content, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        return "([&](const std::string& __nexa_fc) -> int { return __nexa_file_write(" + path +
               ", __nexa_fc.data(), __nexa_fc.size(), " + appendArg + "); })(" + contentCpp + ")";
    }

    void emitFileWriteOrAppend(std::ostringstream& out, const std::string& indent, const AstNode& child,
                               const std::map<std::string, std::string>& varMap,
                               const std::map<std::string, bool>& varIsString,
                               const std::map<std::string, bool>& varIsFloat,
                               const std::map<std::string, bool>& varIsChar,
                               const std::map<std::string, bool>& varIsBool,
                               int append) {
        // The int result is deliberately discarded here: bare `file.write(p, s);` stays legal.
        out << indent
            << emitFileWriteOrAppendExpr(child, varMap, varIsString, varIsFloat, varIsChar, varIsBool, append)
            << ";\n";
    }

    static std::optional<int> tryFoldIntLiteral(const AstNode& n) {
        if (n.type != AstNode::Type::ExprIntLiteral) return std::nullopt;
        try {
            return std::stoi(n.value);
        } catch (...) {
            return std::nullopt;
        }
    }

    std::optional<std::string> tryFoldStrReceiver(const AstNode& n,
                                                 const std::map<std::string, bool>* varIsString) const {
        static const std::map<std::string, bool> empty;
        const auto& vs = varIsString ? *varIsString : empty;
        if (auto folded = tryFoldStringLiteralChain(n, vs)) return folded;
        if (n.type == AstNode::Type::ExprBoolLiteral || n.type == AstNode::Type::ExprIntLiteral ||
            n.type == AstNode::Type::ExprCharLiteral) {
            return n.value;
        }
        return std::nullopt;
    }

    std::optional<std::string> tryFoldStrMethodToRawString(const AstNode& e,
                                                          const std::map<std::string, bool>* varIsString) const {
        if (e.type != AstNode::Type::StrMethod || e.children.empty()) return std::nullopt;
        const std::string& m = e.value;
        if (!strMethodReturnsString(m)) return std::nullopt;
        auto recv = tryFoldStrReceiver(e.children[0], varIsString);
        if (!recv) return std::nullopt;
        std::string s = *recv;
        if (m == "upper") {
            for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }
        if (m == "lower") {
            for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }
        if (m == "trim") {
            const char* ws = " \t\n\r\f\v";
            size_t a = s.find_first_not_of(ws);
            if (a == std::string::npos) return std::string();
            size_t b = s.find_last_not_of(ws);
            return s.substr(a, b - a + 1);
        }
        if (m == "repeat") {
            if (e.children.size() < 2) return std::nullopt;
            auto nrep = tryFoldIntLiteral(e.children[1]);
            if (!nrep) return std::nullopt;
            std::string out;
            for (int i = 0; i < *nrep; i++) out += s;
            return out;
        }
        if (m == "replace") {
            if (e.children.size() < 3) return std::nullopt;
            auto a0 = tryFoldStrReceiver(e.children[1], varIsString);
            auto a1 = tryFoldStrReceiver(e.children[2], varIsString);
            if (!a0 || !a1) return std::nullopt;
            if (a0->empty()) return s;
            size_t p = 0;
            while ((p = s.find(*a0, p)) != std::string::npos) {
                s.replace(p, a0->size(), *a1);
                p += a1->size();
            }
            return s;
        }
        if (m == "substring") {
            if (e.children.size() < 3) return std::nullopt;
            auto a = tryFoldIntLiteral(e.children[1]);
            auto n = tryFoldIntLiteral(e.children[2]);
            if (!a || !n) return std::nullopt;
            int start = *a;
            int count = *n;
            if (start < 0) start = 0;
            if (static_cast<size_t>(start) >= s.size()) return std::string();
            if (count < 0) return s.substr(static_cast<size_t>(start));
            return s.substr(static_cast<size_t>(start), static_cast<size_t>(count));
        }
        return std::nullopt;
    }

    std::optional<std::string> tryFoldComparableString(const AstNode& n,
                                                       const std::map<std::string, bool>* varIsString) const {
        static const std::map<std::string, bool> empty;
        const auto& vs = varIsString ? *varIsString : empty;
        if (auto s = tryFoldStringLiteralChain(n, vs)) return s;
        if (auto s = tryFoldStrMethodToRawString(n, varIsString)) return s;
        if (n.type == AstNode::Type::CryptoCall && (n.value == "hex_encode" || n.value == "hex_decode") &&
            !n.children.empty()) {
            return tryFoldCryptoHex(n.value, n.children[0], varIsString);
        }
        return std::nullopt;
    }

    std::optional<std::string> tryFoldStrMethodToExpr(const AstNode& e,
                                                      const std::map<std::string, bool>* varIsString) const {
        if (e.type != AstNode::Type::StrMethod || e.children.empty()) return std::nullopt;
        if (auto raw = tryFoldStrMethodToRawString(e, varIsString)) {
            return emitCppStringValue(*raw);
        }
        const std::string& m = e.value;
        if (m == "split") return std::nullopt;
        auto recv = tryFoldStrReceiver(e.children[0], varIsString);
        if (!recv) return std::nullopt;
        std::string s = *recv;
        if (m == "len") return std::to_string(static_cast<int>(s.size()));
        if (e.children.size() < 2) return std::nullopt;
        auto a0 = tryFoldStrReceiver(e.children[1], varIsString);
        if (m == "contains") {
            if (!a0) return std::nullopt;
            return s.find(*a0) != std::string::npos ? "true" : "false";
        }
        if (m == "starts_with") {
            if (!a0) return std::nullopt;
            return (s.size() >= a0->size() && s.compare(0, a0->size(), *a0) == 0) ? "true" : "false";
        }
        if (m == "ends_with") {
            if (!a0) return std::nullopt;
            return (s.size() >= a0->size() && s.compare(s.size() - a0->size(), a0->size(), *a0) == 0) ? "true" : "false";
        }
        if (m == "index_of") {
            if (!a0) return std::nullopt;
            size_t p = s.find(*a0);
            return p == std::string::npos ? "-1" : std::to_string(static_cast<int>(p));
        }
        return std::nullopt;
    }

    // Records which gfx feature group a call reaches, so gfxRuntimeCpp can emit only
    // those (see GfxNeed in GfxRuntime.hpp). Only the program's own calls are recorded
    // here; the runtime's internal dependencies -- a blit needing the image table, a
    // filled circle needing the ellipse maths -- are closed over in Modules.hpp and
    // gfxRuntimeCpp respectively.
    //
    // open/close/closed/poll/present/clear/fullscreen need no flag: they are always
    // emitted. Anything not listed below is a call that reaches only core.
    static void noteGfxUsage(const AstNode& n, Modules::CppUsage& cppUsage) {
        const std::string& fn = n.value;
        // gfx.alpha(v) is the setter, which gfx.open calls itself; gfx.alpha() is the
        // reader, and only that needs emitting.
        if (fn == "alpha") {
            if (n.children.empty()) cppUsage.gfxAlpha = true;
        } else if (fn == "plot") {
            cppUsage.gfxPlot = true;
        } else if (fn == "get") {
            cppUsage.gfxGet = true;
        } else if (fn == "fill" || fn == "rect" || fn == "fill_circle" ||
                   fn == "fill_ellipse" || fn == "fill_tri" || fn == "fill_poly") {
            cppUsage.gfxShapesFill = true;
        } else if (fn == "circle" || fn == "ellipse" || fn == "tri" || fn == "poly") {
            cppUsage.gfxShapesOutline = true;
        } else if (fn == "arc") {
            cppUsage.gfxArc = true;
        } else if (fn == "pie") {
            cppUsage.gfxPie = true;
        } else if (fn == "round_rect") {
            cppUsage.gfxRoundRect = true;
        } else if (fn == "fill_round_rect") {
            cppUsage.gfxFillRoundRect = true;
        } else if (fn == "line") {
            // An eighth argument is the thickness, which is a different rasterizer.
            if (n.children.size() >= 8) cppUsage.gfxLineThick = true;
            else cppUsage.gfxLine = true;
        } else if (fn == "text" || fn == "text_size" || fn == "text_width" ||
                   fn == "text_height") {
            cppUsage.gfxText = true;
        } else if (fn == "mouse" || fn == "mouse_x" || fn == "mouse_y") {
            cppUsage.gfxMouse = true;
        } else if (fn == "key" || fn == "pressed" || fn == "released") {
            // gfx.key() is a live read every backend but the browser answers on
            // the spot; gfx.pressed/gfx.released compare this frame with the
            // last and need the snapshots, the name table and the pass over it
            // that gfx.poll() makes. The closure in Modules.hpp makes the edge
            // readers pull the live one in, since a snapshot is taken by asking
            // it once per name.
            cppUsage.gfxKeys = true;
            if (fn != "key") cppUsage.gfxKeyEdge = true;
        } else if (fn == "typed") {
            cppUsage.gfxTyped = true;
        } else if (fn == "wheel" || fn == "wheel_x") {
            cppUsage.gfxWheel = true;
        } else if (fn == "image" || fn == "decode") {
            cppUsage.gfxImage = true;
        } else if (fn == "blit") {
            // A path blit loads the file itself, and which overload a gfx.blit takes is
            // not known until the third argument's type is inferred, well after this
            // scan. Counting every blit as a load costs nothing: a handle blit has to
            // get its handle from gfx.image or gfx.decode, which set the flag anyway.
            cppUsage.gfxImage = true;
            cppUsage.gfxBlit = true;
        } else if (fn == "blit_rot") {
            // Same reasoning as gfx.blit: the path overload loads the file
            // itself, and which overload this is resolves too late to tell.
            cppUsage.gfxImage = true;
            cppUsage.gfxBlitRot = true;
        } else if (fn == "icon") {
            cppUsage.gfxImage = true;
            cppUsage.gfxIcon = true;
        } else if (fn == "cursor") {
            cppUsage.gfxCursor = true;
        } else if (fn == "image_w" || fn == "image_h") {
            cppUsage.gfxImageStore = true;
        } else if (fn == "save") {
            cppUsage.gfxSave = true;
        } else if (fn == "opendialog" || fn == "openfile") {
            cppUsage.gfxOpenDialog = true;
        } else if (fn == "drop") {
            cppUsage.gfxDrop = true;
        } else if (fn == "audio" || fn == "sample" || fn == "audio_queued" ||
                   fn == "audio_flush") {
            cppUsage.gfxAudio = true;
        } else if (fn == "sound" || fn == "play" || fn == "loop" ||
                   fn == "stop" || fn == "volume") {
            // The master volume is part of the mixer rather than draw state, so
            // unlike gfx.alpha both forms of gfx.volume pull the mixer in: a
            // volume with nothing to scale would be a reader of nothing.
            cppUsage.gfxSound = true;
        } else if (fn == "resize" || fn == "width" || fn == "height" ||
                   fn == "scale" || fn == "title") {
            cppUsage.gfxWindow = true;
        } else if (fn == "maxfps") {
            cppUsage.gfxMaxfps = true;
        } else if (fn == "borderless") {
            // Both forms: unlike gfx.alpha, the reader is the only thing that
            // can answer, because nothing else in the runtime takes a frame
            // off and so there is no state to read without this call.
            cppUsage.gfxBorderless = true;
        } else if (fn == "ontop") {
            // Both forms again, and for the same reason: the field is always
            // emitted, but the only thing that ever writes it is this call.
            cppUsage.gfxOntop = true;
        } else if (fn == "transparent") {
            // And a third time, with more riding on it: this flag is what the
            // X11 window's visual is chosen from, so a program that only ever
            // *asks* whether it is transparent still has to get the window
            // that could have been -- gfx.open runs long before the question.
            cppUsage.gfxTransparent = true;
        }
    }

    static bool cryptoArgIsLiteral(const AstNode& a) {
        return a.type == AstNode::Type::ExprStringLiteral ||
               a.type == AstNode::Type::ExprBoolLiteral ||
               a.type == AstNode::Type::ExprIntLiteral ||
               a.type == AstNode::Type::ExprCharLiteral;
    }

    static std::string cryptoHexEncodeBytes(const std::string& s) {
        static const char* hex = "0123456789abcdef";
        std::string out;
        out.resize(s.size() * 2);
        for (size_t i = 0; i < s.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            out[i * 2] = hex[c >> 4];
            out[i * 2 + 1] = hex[c & 0xF];
        }
        return out;
    }

    static std::string cryptoHexDecodeBytes(const std::string& hex) {
        auto nib = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        if (hex.size() % 2 != 0) return std::string();
        std::string out;
        out.resize(hex.size() / 2);
        for (size_t i = 0; i < out.size(); ++i) {
            int hi = nib(hex[i * 2]);
            int lo = nib(hex[i * 2 + 1]);
            if (hi < 0 || lo < 0) return std::string();
            out[i] = static_cast<char>((hi << 4) | lo);
        }
        return out;
    }

    static std::string cryptoLiteralBytes(const AstNode& a) {
        if (a.type == AstNode::Type::ExprStringLiteral) return a.value;
        if (a.type == AstNode::Type::ExprBoolLiteral) return a.value;
        if (a.type == AstNode::Type::ExprIntLiteral) return a.value;
        if (a.type == AstNode::Type::ExprCharLiteral) return a.value;
        return std::string();
    }

    std::optional<std::string> tryFoldCryptoHex(const std::string& fn, const AstNode& arg,
                                               const std::map<std::string, bool>* varIsString) const {
        std::string bytes;
        if (cryptoArgIsLiteral(arg)) {
            bytes = cryptoLiteralBytes(arg);
        } else {
            static const std::map<std::string, bool> empty;
            const auto& vs = varIsString ? *varIsString : empty;
            auto folded = tryFoldStringLiteralChain(arg, vs);
            if (!folded) return std::nullopt;
            bytes = *folded;
        }
        if (fn == "hex_encode") return cryptoHexEncodeBytes(bytes);
        if (fn == "hex_decode") return cryptoHexDecodeBytes(bytes);
        return std::nullopt;
    }

    std::optional<std::string> tryFoldStringLiteralChain(const AstNode& n,
                                                         const std::map<std::string, bool>& varIsString) const {
        if (n.type == AstNode::Type::ExprStringLiteral) return n.value;
        if (n.type == AstNode::Type::ExprAdd && n.children.size() >= 2 && exprIsString(n, varIsString)) {
            auto L = tryFoldStringLiteralChain(n.children[0], varIsString);
            auto R = tryFoldStringLiteralChain(n.children[1], varIsString);
            if (L && R) return *L + *R;
        }
        return std::nullopt;
    }

    // A string concat chain, emitted bare: `a + b + c`, not `((a + b) + c)`.
    // std::string's operator+ is left-associative too, so the nesting was pure
    // bracket depth — see emitBinaryChain. The std::string promotion only has
    // to happen once, at the head of the chain: everything after it is a
    // std::string + something, which is already a valid overload.
    std::string emitConcatChain(const AstNode& e,
                                const std::map<std::string, std::string>& varMap,
                                const std::map<std::string, bool>& varIsString,
                                const std::map<std::string, bool>& varIsFloat,
                                const std::map<std::string, bool>& varIsChar,
                                const std::map<std::string, bool>& varIsBool) {
        const AstNode& l = e.children[0];
        bool flatten = l.type == AstNode::Type::ExprAdd && l.children.size() >= 2
            && exprIsString(l, varIsString)
            && !tryFoldStringLiteralChain(l, varIsString).has_value();
        std::string out = flatten
            ? emitConcatChain(l, varMap, varIsString, varIsFloat, varIsChar, varIsBool)
            : asCppStdString(emitConcatOperand(l, varMap, varIsString, varIsFloat, varIsChar, varIsBool));
        out += " + ";
        out += emitConcatOperand(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
        return out;
    }

    std::string emitConcatOperand(const AstNode& child,
                                  const std::map<std::string, std::string>& varMap,
                                  const std::map<std::string, bool>& varIsString,
                                  const std::map<std::string, bool>& varIsFloat,
                                  const std::map<std::string, bool>& varIsChar,
                                  const std::map<std::string, bool>& varIsBool) {
        const std::map<std::string, bool>* pStr = &varIsString;
        const std::map<std::string, bool>* pFl = &varIsFloat;
        const std::map<std::string, bool>* pCh = &varIsChar;
        const std::map<std::string, bool>* pBo = &varIsBool;
        if (child.type == AstNode::Type::ExprAdd && exprIsString(child, varIsString)) {
            if (auto folded = tryFoldStringLiteralChain(child, varIsString)) {
                return emitCppStringValue(*folded);
            }
            return "(" + emitConcatChain(child, varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
        }
        if (child.type == AstNode::Type::ExprAdd) {
            if (exprIsFloat(child, varIsFloat)) {
                return "__nexa_f2s(" + emitExpr(child, varMap, pStr, pFl, pCh, pBo) + ")";
            }
            return "std::to_string(" + emitExpr(child, varMap, pStr, pFl, pCh, pBo) + ")";
        }
        if (exprIsString(child, varIsString) || exprProducesString(child)) {
            return emitExpr(child, varMap, pStr, pFl, pCh, pBo);
        }
        if (child.type == AstNode::Type::FnCall && inferExprNexaType(child) == "string") {
            return emitExpr(child, varMap, pStr, pFl, pCh, pBo);
        }
        if (exprIsFloat(child, varIsFloat)) {
            return "__nexa_f2s(" + emitExpr(child, varMap, pStr, pFl, pCh, pBo) + ")";
        }
        if (exprIsChar(child, varIsChar)) {
            return "std::string(1, " + emitExpr(child, varMap, pStr, pFl, pCh, pBo) + ")";
        }
        if (exprIsBool(child, varIsBool)) {
            std::string v = emitExpr(child, varMap, pStr, pFl, pCh, pBo);
            return "std::string(" + v + " ? \"true\" : \"false\")";
        }
        return "std::to_string(" + emitExpr(child, varMap, pStr, pFl, pCh, pBo) + ")";
    }

    // File-scope / namespace-scope C++ (no wrapping braces); required for top-level inline_cpp! with functions or main().
    void emitInlineCppFileScope(std::ostringstream& out, const std::string& body) {
        if (body.empty()) return;
        size_t start = 0;
        while (start < body.size()) {
            size_t nl = body.find('\n', start);
            if (nl == std::string::npos) {
                std::string line = body.substr(start);
                while (!line.empty() && line.back() == '\r') line.pop_back();
                out << line << "\n";
                break;
            }
            std::string line = body.substr(start, nl - start);
            while (!line.empty() && line.back() == '\r') line.pop_back();
            out << line << "\n";
            start = nl + 1;
        }
    }

    void emitInlineCppRaw(std::ostringstream& out, const std::string& body, const std::string& indent) {
        out << indent << "{\n";
        if (body.empty()) {
            out << indent << "}\n";
            return;
        }
        size_t start = 0;
        while (start < body.size()) {
            size_t nl = body.find('\n', start);
            if (nl == std::string::npos) {
                std::string line = body.substr(start);
                while (!line.empty() && line.back() == '\r') line.pop_back();
                out << indent << "    " << line << "\n";
                break;
            }
            std::string line = body.substr(start, nl - start);
            while (!line.empty() && line.back() == '\r') line.pop_back();
            out << indent << "    " << line << "\n";
            start = nl + 1;
        }
        out << indent << "}\n";
    }

    // String literals (and compile-time folds of them, e.g. "AB".lower() or
    // crypto.hex_encode("AB")) are emitted as bare C++ literals of type const char[N] so
    // that the common cases stay allocation-free. Any context that calls a std::string
    // member on the result, or adds two of them together, has to promote first —
    // `("a" + "b")` and `("abc").size()` are both ill-formed C++.
    static bool isBareCppStringLiteral(const std::string& emitted) {
        return !emitted.empty() && emitted[0] == '"';
    }

    static std::string asCppStdString(const std::string& emitted) {
        return isBareCppStringLiteral(emitted) ? "std::string(" + emitted + ")" : emitted;
    }

    std::string emitCppStringValue(const std::string& s) const {
        std::string esc = escapeString(s);
        if (s.find('\0') != std::string::npos) {
            return "std::string(\"" + esc + "\", " + std::to_string(s.size()) + ")";
        }
        return "\"" + esc + "\"";
    }

    std::string escapeString(const std::string& s) const {
        std::string out;
        bool hexPending = false;
        auto isHexDigit = [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
        };
        for (unsigned char c : s) {
            if (c == '\\') {
                out += "\\\\";
                hexPending = false;
            } else if (c == '"') {
                out += "\\\"";
                hexPending = false;
            } else if (c == '\n') {
                out += "\\n";
                hexPending = false;
            } else if (c == '\t') {
                out += "\\t";
                hexPending = false;
            } else if (c == '\r') {
                out += "\\r";
                hexPending = false;
            } else if (c < 32 || c >= 127) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\x%02X", c);
                out += buf;
                hexPending = true;
            } else {
                // C++ \x consumes every following hex digit. "\x89PNG" is one
                // value, not 0x89 + "PNG" — P is a hex digit. Close the literal.
                if (hexPending && isHexDigit(c)) out += "\"\"";
                out += static_cast<char>(c);
                hexPending = false;
            }
        }
        return out;
    }
};

}  // namespace nexa
