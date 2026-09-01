#pragma once

#include "Parser.hpp"
#include "Modules.hpp"
#include <string>
#include <sstream>
#include <cstdio>
#include <map>
#include <set>
#include <functional>
#include <vector>
#include <cctype>
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

// Converts Nexa AST to C++ source code
class Transpiler {
public:
    Transpiler(const std::vector<AstNode>& ast, const Modules& modules, bool preserveNames = false, bool buildDll = false)
        : ast_(ast), modules_(modules), preserveNames_(preserveNames), buildDll_(buildDll) {}

    // Valid after transpile(): which C++ features the generated code actually uses.
    // Lets the build step drop exception/RTTI machinery when nothing needs it.
    const Modules::CppUsage& cppUsage() const { return cppUsage_; }

    std::string transpile() {
        std::ostringstream out;

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
                case AstNode::Type::HttpCall: cppUsage.http = true; break;
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
                case AstNode::Type::TryCatch:
                case AstNode::Type::Throw: cppUsage.exceptions = true; break;
                default: break;
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
        structCppNames_.clear();
        enumCppNames_.clear();
        enumVariants_.clear();
        enumFirstVariant_.clear();
        std::set<std::string> typeNames;
        int structId = 0;
        int enumId = 0;
        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::StructDef) {
                if (!typeNames.insert(node.value).second) {
                    throw std::runtime_error("Duplicate type name '" + node.value + "'");
                }
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    structFields_[node.value][node.paramNames[i]] = node.paramTypes[i];
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
            if (n.type == AstNode::Type::Variable && !n.children.empty() && n.children[0].type == AstNode::Type::ExprArrayLiteral) {
                for (const auto& c : n.children[0].children) { if (exprProducesString(c)) { needsString = true; break; } }
            }
            if (n.type == AstNode::Type::IoPrintln || n.type == AstNode::Type::IoPrint) {
                for (const AstNode& a : n.children) {
                    if (exprProducesString(a)) { needsString = true; break; }
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
        if (needsString && moduleCppIncludes.find("#include <string>\n") == std::string::npos) out << "#include <string>\n";
        if (needsVector && moduleCppIncludes.find("#include <vector>\n") == std::string::npos) out << "#include <vector>\n";
        // Float->string helper that matches io.print's "%g" formatting (e.g. 12.0 -> "12", not "12.000000").
        if (needsString && moduleCppIncludes.find("#include <cstdio>\n") == std::string::npos) out << "#include <cstdio>\n";
        if (needsCstdlib && moduleCppIncludes.find("#include <cstdlib>\n") == std::string::npos) out << "#include <cstdlib>\n";
        if (needsCstddef && moduleCppIncludes.find("#include <cstddef>\n") == std::string::npos) out << "#include <cstddef>\n";
        if (!moduleCppIncludes.empty() || !inlineCppHoisted.empty() || needsString || needsVector || needsCstdlib || needsCstddef) out << "\n";
        if (needsString) out << "[[maybe_unused]] static std::string __nexa_f2s(double __v) { char __b[32]; std::snprintf(__b, sizeof(__b), \"%g\", __v); return std::string(__b); }\n\n";

        bool wroteUserCppHeaders = false;
        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::CppHeaderInclude) {
                out << node.value << "\n";
                wroteUserCppHeaders = true;
            }
        }
        if (wroteUserCppHeaders) out << "\n";

        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::StructDef) {
                const std::string& nexaName = node.value;
                std::string cppName = structCppNames_.at(nexaName);
                out << "struct " << cppName << " {\n";
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    out << "    " << nexaTypeToCpp(node.paramTypes[i]) << " " << node.paramNames[i] << ";\n";
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
        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::Variable && isStructDeclType(node.declType)) {
                varStructScopes_[0][node.value] = structNameFromDecl(node.declType);
            }
        }

        buildFnOverloadTableAndInitGlobalNexaDecl();

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
                    out << (buildDll_ ? dllExportParamCpp(nexaT) : nexaTypeToCpp(nexaT));
                }
                out << ");\n";
                wroteAnyProto = true;
            }
            if (wroteAnyProto) out << "\n";
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
                        out << nexaTypeToCpp(nexaT) << " " << pname;
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
            globalVarIsString[node.value] = isStr;
            globalVarIsArray[node.value] = isArray || node.isFixedArray;
            if (node.initUninitialized) {
                std::string c = node.isConst ? "const " : "";
                if (node.isFixedArray) {
                    if (isStructDeclType(node.declType)) {
                        std::string sn = structNameFromDecl(node.declType);
                        out << c << structCppNames_.at(sn) << " " << vname << "[" << node.arraySize << "]{};\n";
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
                    std::string sn = structNameFromDecl(node.declType);
                    out << c << structCppNames_.at(sn) << " " << vname << "{};\n";
                } else if (!node.declType.empty() && isEnumDeclType(node.declType)) {
                    std::string en = enumNameFromDecl(node.declType);
                    std::string cpp = enumCppNames_.at(en);
                    out << c << cpp << " " << vname << " = " << cpp << "::" << enumFirstVariant_.at(en) << ";\n";
                } else {
                    out << c << "std::string " << vname << ";\n";
                }
            } else if (isArray && !node.children.empty()) {
                std::string c = node.isConst ? "const " : "";
                bool strArr = arrayInitProducesString(node.children[0], globalVarIsString);
                out << c << (strArr ? "std::vector<std::string>" : "std::vector<int>") << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
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
                    std::string sn = structNameFromDecl(node.declType);
                    out << c << structCppNames_.at(sn) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else {
                    bool useBool = !node.declType.empty() ? (node.declType == "bool") : node.initIsBool;
                    bool useInt = !node.declType.empty() ? nexaIsNumericIntType(node.declType) : node.initIsInt;
                    bool useFloat = !node.declType.empty() ? (node.declType == "float") : node.initIsFloat;
                    bool useChar = !node.declType.empty() ? (node.declType == "char") : node.initIsChar;
                    std::string inferred;
                    if (node.declType.empty() && !node.children.empty()) {
                        inferred = inferExprNexaType(node.children[0]);
                    }
                    if (!inferred.empty() && isPointerType(inferred)) {
                        out << c << nexaTypeToCpp(inferred) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                    } else if (!inferred.empty() && nexaIsNumericIntType(inferred)) {
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
                out << c << cppT << " " << vname << " = " << node.initValue << ";\n";
            } else {
                std::string c = node.isConst ? "const " : "";
                out << c << "std::string " << vname << " = \"" << escapeString(node.initValue) << "\";\n";
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

        std::set<std::string> seenIncludeLines;
        std::ostringstream filtered;
        std::istringstream in(out.str());
        std::string line;
        while (std::getline(in, line)) {
            std::string key = line;
            while (!key.empty() && key.back() == '\r') key.pop_back();
            if (key.rfind("#include <", 0) == 0) {
                if (!seenIncludeLines.insert(key).second) continue;
            }
            filtered << key << "\n";
        }
        return filtered.str();
    }

private:
    const std::vector<AstNode>& ast_;
    const Modules& modules_;
    bool preserveNames_;
    bool buildDll_;
    Modules::CppUsage cppUsage_;
    // While emitting a function or main body: how bare `return;` / value returns are interpreted
    enum class EmitFnRet { Main, IntFn, VoidFn };
    mutable EmitFnRet emitFnRet_ = EmitFnRet::Main;
    std::map<std::string, std::map<std::string, std::string>> structFields_;
    std::map<std::string, std::string> structCppNames_;
    std::map<std::string, std::string> enumCppNames_;
    std::map<std::string, std::set<std::string>> enumVariants_;
    std::map<std::string, std::string> enumFirstVariant_;
    mutable std::vector<std::map<std::string, std::string>> varStructScopes_;

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

    size_t resolveOverload(const std::string& name,
                           const std::vector<std::string>& actualArgTypes) const {
        const size_t k = actualArgTypes.size();
        std::vector<size_t> compat;
        for (size_t s = 0; s < fnOverloadSlots_.size(); ++s) {
            const FnOverloadSlot& sl = fnOverloadSlots_[s];
            if (sl.name != name) continue;
            if (k < sl.minArgs || k > sl.paramTypes.size()) continue;
            bool ok = true;
            for (size_t i = 0; i < k; ++i) {
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
            for (size_t i = 0; i < k; ++i) {
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
        return "[=]() { " + body.str() + "}";
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
                if (!arr.children.empty() && inferExprNexaType(arr.children[0]) == "string") {
                    return "[]string";
                }
                return "[]int";
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
                auto enIt = enumCppNames_.find(e.value);
                if (enIt != enumCppNames_.end()) return "enum:" + e.value;
                return "int";
            }
            case AstNode::Type::ExprMember:
                if (!e.children.empty()) {
                    std::string ft = fieldTypeOfMemberExpr(e);
                    if (!ft.empty()) return ft;
                }
                return "int";
            case AstNode::Type::FnCall: {
                std::vector<std::string> argT;
                argT.reserve(e.children.size());
                for (const AstNode& a : e.children) argT.push_back(inferExprNexaType(a));
                size_t slot = resolveOverload(e.value, argT);
                return inferReturnNexaType(ast_[fnOverloadSlots_[slot].astIndex]);
            }
            case AstNode::Type::ExprAdd:
                if (e.children.size() >= 2) {
                    std::string t0 = inferExprNexaType(e.children[0]);
                    std::string t1 = inferExprNexaType(e.children[1]);
                    if (t0 == "string" || t1 == "string") return "string";
                    if (t0 == "float" || t1 == "float") return "float";
                    return "int";
                }
                if (e.children.size() >= 1) {
                    std::string t0 = inferExprNexaType(e.children[0]);
                    return t0 == "float" ? "float" : "int";
                }
                return "int";
            case AstNode::Type::ExprSub:
            case AstNode::Type::ExprMul:
            case AstNode::Type::ExprDiv:
            case AstNode::Type::ExprMod:
            case AstNode::Type::ExprBitAnd:
            case AstNode::Type::ExprBitOr:
            case AstNode::Type::ExprBitXor:
            case AstNode::Type::ExprShl:
            case AstNode::Type::ExprShr:
            case AstNode::Type::ExprBitNot:
                if (e.children.size() >= 1) {
                    std::string t0 = inferExprNexaType(e.children[0]);
                    if (e.children.size() >= 2) {
                        std::string t1 = inferExprNexaType(e.children[1]);
                        if (t0 == "float" || t1 == "float") return "float";
                    }
                    return t0 == "float" ? "float" : "int";
                }
                return "int";
            case AstNode::Type::ExprLen: return "int";
            case AstNode::Type::ExprTrim: return "string";
            case AstNode::Type::ExprArrayLiteral:
                if (!e.children.empty()) return "arrayelt:" + inferExprNexaType(e.children[0]);
                return "int";
            case AstNode::Type::ExprArrayIndex: {
                std::string baseT = lookupNexaDecl(e.value);
                if (isPointerType(baseT)) return pointerPointeeType(baseT);
                if (baseT == "[]string") return "string";
                if (baseT == "string") return "char";
                if (isStructDeclType(baseT)) return baseT;
                if (nexaIsIntegerType(baseT) || baseT == "bool" || baseT == "float") {
                    return baseT;
                }
                return "int";
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
                return "string";
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
            case AstNode::Type::CondNot:
                return "bool";
            default:
                return "int";
        }
    }

    std::string emitFnCallCpp(const AstNode& e,
                              const std::map<std::string, std::string>& varMap,
                              const std::map<std::string, bool>* varIsString,
                              const std::map<std::string, bool>* varIsFloat,
                              const std::map<std::string, bool>* varIsChar,
                              const std::map<std::string, bool>* varIsBool) {
        std::vector<std::string> argT;
        argT.reserve(e.children.size());
        for (const AstNode& a : e.children) argT.push_back(inferExprNexaType(a));
        size_t slot = resolveOverload(e.value, argT);
        const FnOverloadSlot& sl = fnOverloadSlots_[slot];
        const AstNode& fn = ast_[sl.astIndex];
        std::string name = cppFnNameForSlot(slot);
        std::string s = name + "(";
        for (size_t i = 0; i < fn.paramNames.size(); i++) {
            if (i > 0) s += ", ";
            if (i < e.children.size()) {
                std::string arg = emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (fn.isExtern && i < fn.paramTypes.size()) {
                    const std::string& pt = fn.paramTypes[i];
                    if (isPointerType(pt) && pointerPointeeType(pt) == "char" &&
                        exprIsString(e.children[i], *varIsString) &&
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
        if (t.size() >= 7 && t.compare(0, 7, "struct:") == 0) {
            std::string n = t.substr(7);
            auto it = structCppNames_.find(n);
            if (it == structCppNames_.end()) throw std::runtime_error("Unknown struct type: " + n);
            return it->second;
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
            out << indent << "printf(\"%d" << (newline ? "\\n" : "") << "\", " << arg.value << ");\n";
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
            out << indent << "printf(\"%d" << (newline ? "\\n" : "") << "\", " << expr << ");\n";
        } else if (exprIsPtr) {
            out << indent << "printf(\"%p" << (newline ? "\\n" : "") << "\", (void*)(" << expr << "));\n";
        } else if (nexaIsNumericIntType(ntype)) {
            emitIntegerPrintf(out, indent, ntype, expr, newline);
        } else {
            std::string carg = isNexaEnum ? ("static_cast<int>(" + expr + ")") : expr;
            out << indent << "printf(\"%d" << (newline ? "\\n" : "") << "\", " << carg << ");\n";
        }
    }

    void emitIntegerPrintf(std::ostringstream& out, const std::string& indent,
                           const std::string& ntype, const std::string& expr, bool newline) const {
        std::string fmt = "%d";
        std::string arg = expr;
        if (ntype == "unsigned int") {
            fmt = "%u";
        } else if (ntype == "unsigned short") {
            fmt = "%u";
            arg = "static_cast<unsigned int>(" + expr + ")";
        } else if (ntype == "unsigned long") {
            fmt = "%lu";
        } else if (ntype == "long") {
            fmt = "%ld";
        } else if (ntype == "size_t") {
            fmt = "%zu";
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
        if (e.type == AstNode::Type::ExprVarRef) return varStructLookup(e.value);
        if (e.type == AstNode::Type::ExprArrayIndex) {
            std::string t = lookupNexaDecl(e.value);
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
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsExec || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsExecutable || e.type == AstNode::Type::OsTempDir || e.type == AstNode::Type::OsArch || e.type == AstNode::Type::OsWhich || e.type == AstNode::Type::OsCwd || e.type == AstNode::Type::OsHostname || e.type == AstNode::Type::OsUsername || e.type == AstNode::Type::OsHome || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::OsClipGet || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::IoGetline || e.type == AstNode::Type::IoReadln || e.type == AstNode::Type::FileRead || e.type == AstNode::Type::ExprTrim || e.type == AstNode::Type::CryptoCall || e.type == AstNode::Type::HttpCall) return true;
        if (e.type == AstNode::Type::ExprCast && e.value == "string") return true;
        if (e.type == AstNode::Type::FileCall) {
            const std::string& m = e.value;
            return m == "cwd" || m == "abspath" || m == "join" || m == "dirname" || m == "basename" || m == "extension";
        }
        if (e.type == AstNode::Type::StrMethod) return strMethodReturnsString(e.value);
        if (e.type == AstNode::Type::ExprAdd && e.children.size() >= 2) {
            return exprProducesString(e.children[0]) || exprProducesString(e.children[1]);
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
        if (e.type == AstNode::Type::ExprArrayIndex) {
            return inferExprNexaType(e) == "string";
        }
        if (e.type == AstNode::Type::ExprCast) return e.value == "string";
        if (e.type == AstNode::Type::OsInfo) {
            const std::string& m = e.value;
            return m == "shell" || m == "newline" || m == "path_sep" || m == "lang"
                || m == "config_dir" || m == "cache_dir" || m == "desktop" || m == "endian";
        }
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsExec || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsExecutable || e.type == AstNode::Type::OsTempDir || e.type == AstNode::Type::OsArch || e.type == AstNode::Type::OsWhich || e.type == AstNode::Type::OsCwd || e.type == AstNode::Type::OsHostname || e.type == AstNode::Type::OsUsername || e.type == AstNode::Type::OsHome || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::OsClipGet || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::FileRead || e.type == AstNode::Type::IoReadln || e.type == AstNode::Type::IoGetline || e.type == AstNode::Type::ExprTrim || e.type == AstNode::Type::CryptoCall || e.type == AstNode::Type::HttpCall) return true;
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
        } else {
            out << "    return 0;\n";
        }
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
        emitBlockStatements(out, children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
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
                bool isStr = !isArray && !isPointerType(child.declType) && !isStructDeclType(child.declType) && !isEnumDeclType(child.declType) && (!child.declType.empty() ? (child.declType == "string") : (child.initUninitialized || child.initFromReadln || child.initFromFileRead || (!child.initIsInt && !child.initIsBool && !child.initIsFloat && !child.initIsChar && !child.initFromDllLoad && child.children.empty()) ||
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
                    }
                }
                varIsString[child.value] = isStr || isStrArr;
                if (!child.declType.empty() && isStructDeclType(child.declType) && !child.isFixedArray) {
                    varStructDeclare(child.value, structNameFromDecl(child.declType));
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
                        if (isStructDeclType(child.declType)) {
                            std::string sn = structNameFromDecl(child.declType);
                            out << indent << cfix << structCppNames_.at(sn) << " " << vname << "[" << child.arraySize << "]{};\n";
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
                        std::string sn = structNameFromDecl(child.declType);
                        out << indent << c << structCppNames_.at(sn) << " " << vname << "{};\n";
                    } else if (!child.declType.empty() && isEnumDeclType(child.declType)) {
                        std::string en = enumNameFromDecl(child.declType);
                        std::string cpp = enumCppNames_.at(en);
                        out << indent << c << cpp << " " << vname << " = " << cpp << "::" << enumFirstVariant_.at(en) << ";\n";
                    } else {
                        out << indent << c << "std::string " << vname << ";\n";
                    }
                } else if (child.initFromFileRead && !child.children.empty()) {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << "std::string " << vname << " = "
                        << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                } else if (isArray && !child.children.empty()) {
                    std::string c = child.isConst ? "const " : "";
                    bool strArr = arrayInitProducesString(child.children[0], varIsString);
                    out << indent << c << (strArr ? "std::vector<std::string>" : "std::vector<int>") << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
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
                        std::string sn = structNameFromDecl(child.declType);
                        out << indent << c << structCppNames_.at(sn) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else {
                        bool useBool = !child.declType.empty() ? (child.declType == "bool") : child.initIsBool;
                        bool useInt = !child.declType.empty() ? nexaIsNumericIntType(child.declType) : child.initIsInt;
                        bool useFloat = !child.declType.empty() ? (child.declType == "float") : child.initIsFloat;
                        bool useChar = !child.declType.empty() ? (child.declType == "char") : child.initIsChar;
                        std::string inferredPtr;
                        std::string inferredInt;
                        if (child.declType.empty()) {
                            std::string it = inferExprNexaType(child.children[0]);
                            if (isPointerType(it)) {
                                inferredPtr = it;
                            } else if (nexaIsNumericIntType(it)) {
                                inferredInt = it;
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
                    out << indent << c << cppT << " " << vname << " = " << child.initValue << ";\n";
                } else {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << "std::string " << vname << " = \"" << escapeString(child.initValue) << "\";\n";
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
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                std::string idx = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string val = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                // String scalar: s[i] = char/int. Arrays: arr[i] = element.
                std::string baseT = lookupNexaDecl(child.value);
                if (baseT == "string") {
                    if (exprIsString(child.children[1], varIsString)) {
                        // Allow s[i] = "x" when RHS is a 1-char string expression.
                        out << indent << v << "[" << idx << "] = (" << val << ").empty() ? '\\0' : (" << val << ")[0];\n";
                    } else {
                        out << indent << v << "[" << idx << "] = static_cast<char>(" << val << ");\n";
                    }
                } else {
                    out << indent << v << "[" << idx << "] = " << val << ";\n";
                }
            } else if (child.type == AstNode::Type::AssnAdd) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                if (varIsString.count(child.value) && varIsString.at(child.value)) {
                    out << indent << v << " = " << v << " + " << emitConcatOperand(child.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) << ";\n";
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
                bool elemIsString = forInElementIsString(child.children[0]);
                // Evaluate collection before binding the loop variable (avoids shadowing).
                std::string collExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                varMap[child.value] = loopVar;
                varIsString[child.value] = elemIsString;
                varIsConst[child.value] = false;
                varIsFloat[child.value] = false;
                varIsChar[child.value] = false;
                varIsBool[child.value] = false;
                varIsEnum[child.value] = false;
                if (!nexaDeclStack_.empty()) {
                    nexaDeclStack_.back()[child.value] = elemIsString ? "string" : "int";
                }
                std::string collTmp = "__nexa_forin_" + std::to_string(varIdx++);
                out << indent << "{\n";
                if (elemIsString) {
                    out << indent << "    const std::vector<std::string>& " << collTmp << " = " << collExpr << ";\n";
                    out << indent << "    for (const std::string& " << loopVar << " : " << collTmp << ") {\n";
                } else {
                    out << indent << "    const std::vector<int>& " << collTmp << " = " << collExpr << ";\n";
                    out << indent << "    for (int " << loopVar << " : " << collTmp << ") {\n";
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
                out << indent << (first ? "" : "else ") << "if (" << expr << " == \"" << escapeString(c->initValue) << "\") {\n";
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
                    out << indent << "default:\n";
                } else if (c.caseIsEnum) {
                    auto enIt = enumCppNames_.find(c.value);
                    if (enIt == enumCppNames_.end()) {
                        throw std::runtime_error("Unknown enum '" + c.value + "' in switch case");
                    }
                    auto vsIt = enumVariants_.find(c.value);
                    if (vsIt == enumVariants_.end() || !vsIt->second.count(c.initValue)) {
                        throw std::runtime_error("Unknown enum variant '" + c.initValue + "' for '" + c.value + "'");
                    }
                    out << indent << "case " << enIt->second << "::" << c.initValue << ":\n";
                } else {
                    out << indent << "case " << c.value << ":\n";
                }
                emitBlock(out, c.children, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ");
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

    std::string emitCond(const AstNode& c, const std::map<std::string, std::string>& varMap,
                         const std::map<std::string, bool>* varIsString = nullptr,
                         const std::map<std::string, bool>* varIsFloat = nullptr,
                         const std::map<std::string, bool>* varIsChar = nullptr,
                         const std::map<std::string, bool>* varIsBool = nullptr) {
        switch (c.type) {
            case AstNode::Type::CondEq:
                if (c.children.size() >= 2) {
                    if (auto L = tryFoldComparableString(c.children[0], varIsString)) {
                        if (auto R = tryFoldComparableString(c.children[1], varIsString)) {
                            return (*L == *R) ? "true" : "false";
                        }
                    }
                }
                return emitExpr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " == " +
                       emitExpr(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondNe:
                if (c.children.size() >= 2) {
                    if (auto L = tryFoldComparableString(c.children[0], varIsString)) {
                        if (auto R = tryFoldComparableString(c.children[1], varIsString)) {
                            return (*L != *R) ? "true" : "false";
                        }
                    }
                }
                return emitExpr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " != " +
                       emitExpr(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondLt:
                return emitExpr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " < " + emitExpr(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondLe:
                return emitExpr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " <= " + emitExpr(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondGt:
                return emitExpr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " > " + emitExpr(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondGe:
                return emitExpr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " >= " + emitExpr(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
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
            case AstNode::Type::ExprBoolLiteral:
                return c.value;
            case AstNode::Type::StrMethod:
                return emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::ExprIntLiteral:
            case AstNode::Type::ExprFloatLiteral:
            case AstNode::Type::ExprCharLiteral:
            case AstNode::Type::ExprVarRef:
                return emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::FileExists: {
                return "__nexa_file_exists(" + emitFilePathCStr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            }
            case AstNode::Type::FnCall:
            case AstNode::Type::ExprArrayIndex:
            case AstNode::Type::ExprMember:
                return emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            default:
                return "false";
        }
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
                return e.value;
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
                if (c < 32 || c == 127) {
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
                return "\"" + escapeString(e.value) + "\"";
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
                std::string s = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "(int)((" + s + ").size())";
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
                            return "\"" + escapeString(*folded) + "\"";
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
                std::string url = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (fn == "get") return "__nexa_http_get(" + url + ")";
                if (fn == "post") {
                    std::string body = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                    return "__nexa_http_post(" + url + ", " + body + ")";
                }
                throw std::runtime_error("Internal: unknown http method '" + fn + "'");
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
                if (m == "len")
                    return "([](const std::string& __s){ return (int)__s.size(); })(" + R + ")";
                if (m == "contains")
                    return "([](const std::string& __s, const std::string& __p){ return __s.find(__p) != std::string::npos; })(" + R + ", " + A0 + ")";
                if (m == "starts_with")
                    return "([](const std::string& __s, const std::string& __p){ return __s.size() >= __p.size() && __s.compare(0, __p.size(), __p) == 0; })(" + R + ", " + A0 + ")";
                if (m == "ends_with")
                    return "([](const std::string& __s, const std::string& __p){ return __s.size() >= __p.size() && __s.compare(__s.size() - __p.size(), __p.size(), __p) == 0; })(" + R + ", " + A0 + ")";
                if (m == "index_of")
                    return "([](const std::string& __s, const std::string& __p){ size_t __n = __s.find(__p); return __n == std::string::npos ? -1 : (int)__n; })(" + R + ", " + A0 + ")";
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
                auto it = varMap.find(e.value);
                return (it != varMap.end()) ? it->second : e.value;
            }
            case AstNode::Type::ExprArrayLiteral: {
                bool isStrArr = arrayLiteralProducesString(e, vIsStr);
                std::string s = isStrArr ? "std::vector<std::string>{" : "std::vector<int>{";
                for (size_t i = 0; i < e.children.size(); i++) {
                    if (i > 0) s += ", ";
                    s += emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                }
                s += "}";
                return s;
            }
            case AstNode::Type::ExprArrayIndex: {
                auto it = varMap.find(e.value);
                std::string v = (it != varMap.end()) ? it->second : e.value;
                return v + "[" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + "]";
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
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " + "
                    + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprSub:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " - " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprMul:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " * " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprDiv:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " / " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprMod:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " % " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitAnd:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " & " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitOr:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " | " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitXor: {
                bool lhsStr = exprIsString(e.children[0], vIsStr);
                bool rhsStr = exprIsString(e.children[1], vIsStr);
                std::string lhs = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                std::string rhs = emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                if (lhsStr && !rhsStr) {
                    return "([&]{ std::string __nexa_s = " + lhs + "; int __nexa_k = " + rhs
                        + "; for (size_t __nexa_i = 0; __nexa_i < __nexa_s.size(); __nexa_i++) "
                        "__nexa_s[__nexa_i] = (char)((unsigned char)__nexa_s[__nexa_i] ^ (__nexa_k & 0xFF)); return __nexa_s; }())";
                }
                return "(" + lhs + " ^ " + rhs + ")";
            }
            case AstNode::Type::ExprShl:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " << " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprShr:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " >> " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
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

    void emitFileWriteOrAppend(std::ostringstream& out, const std::string& indent, const AstNode& child,
                               const std::map<std::string, std::string>& varMap,
                               const std::map<std::string, bool>& varIsString,
                               const std::map<std::string, bool>& varIsFloat,
                               const std::map<std::string, bool>& varIsChar,
                               const std::map<std::string, bool>& varIsBool,
                               int append) {
        std::string path = emitFilePathCStr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
        const AstNode& content = child.children[1];
        if (auto folded = tryFoldStringLiteralChain(content, varIsString)) {
            out << indent << "__nexa_file_write(" << path << ", \"" << escapeString(*folded) << "\", "
                << folded->size() << ", " << append << ");\n";
            return;
        }
        if (exprIsString(content, varIsString)) {
            out << indent << "{\n";
            out << indent << "    const std::string& __nexa_fc = "
                << emitExpr(content, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            out << indent << "    __nexa_file_write(" << path << ", __nexa_fc.data(), __nexa_fc.size(), " << append << ");\n";
            out << indent << "}\n";
            return;
        }
        out << indent << "{\n";
        out << indent << "    const std::string __nexa_fc = "
            << emitConcatOperand(content, varMap, varIsString, varIsFloat, varIsChar, varIsBool) << ";\n";
        out << indent << "    __nexa_file_write(" << path << ", __nexa_fc.data(), __nexa_fc.size(), " << append << ");\n";
        out << indent << "}\n";
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
            return "\"" + escapeString(*raw) + "\"";
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
                return "\"" + escapeString(*folded) + "\"";
            }
            return "(" + emitConcatOperand(child.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " + "
                + emitConcatOperand(child.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
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

    std::string escapeString(const std::string& s) const {
        std::string out;
        for (unsigned char c : s) {
            if (c == '\\') out += "\\\\";
            else if (c == '"') out += "\\\"";
            else if (c == '\n') out += "\\n";
            else if (c == '\t') out += "\\t";
            else if (c == '\r') out += "\\r";
            else if (c < 32 || c == 127) {
                char buf[5];
                snprintf(buf, sizeof(buf), "\\x%02X", c);
                out += buf;
            }
            else out += c;
        }
        return out;
    }
};

}  // namespace nexa
