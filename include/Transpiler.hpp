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

    std::string transpile() {
        std::ostringstream out;

        Modules::CppUsage cppUsage;
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
                case AstNode::Type::OsGetenv: cppUsage.osGetenv = true; break;
                case AstNode::Type::OsPlatform: cppUsage.osPlatform = true; break;
                case AstNode::Type::OsExeDir: cppUsage.osExeDir = true; break;
                case AstNode::Type::OsGetProcessId: cppUsage.osGetProcessId = true; break;
                case AstNode::Type::OsHideConsoleWindow:
                case AstNode::Type::OsShowConsoleWindow:
                case AstNode::Type::OsMinimizeConsoleWindow:
                case AstNode::Type::OsMaximizeConsoleWindow: cppUsage.osWindowControl = true; break;
                case AstNode::Type::OsMessageBox: cppUsage.osMessageBox = true; break;
                case AstNode::Type::OsGrepKeys: cppUsage.osGrepKeys = true; break;
                case AstNode::Type::OsKeyPressed: cppUsage.osKeyPressed = true; break;
                case AstNode::Type::FileRead:
                case AstNode::Type::FileWrite:
                case AstNode::Type::FileAppend:
                case AstNode::Type::FileExists: cppUsage.file = true; break;
                case AstNode::Type::RandomInt:
                case AstNode::Type::RandomSeed: cppUsage.random = true; break;
                case AstNode::Type::TimeSleep:
                case AstNode::Type::TimeSeconds:
                case AstNode::Type::TimeMilliseconds:
                case AstNode::Type::TimeNowMs: cppUsage.time = true; break;
                case AstNode::Type::ThreadSpawn:
                case AstNode::Type::ThreadJoin: cppUsage.thread = true; break;
                case AstNode::Type::DllLoad:
                case AstNode::Type::DllCall: cppUsage.dll = true; break;
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
            if (n.type == AstNode::Type::Variable && (n.initUninitialized || n.initFromReadln || n.initFromFileRead || (!n.initIsInt && !n.initFromDllLoad && n.children.empty()))) needsString = true;
            if (n.type == AstNode::Type::Variable && !n.children.empty() && exprProducesString(n.children[0])) needsString = true;
            if (n.type == AstNode::Type::Variable && !n.children.empty() && n.children[0].type == AstNode::Type::ExprArrayLiteral) {
                for (const auto& c : n.children[0].children) { if (exprProducesString(c)) { needsString = true; break; } }
            }
            if ((n.type == AstNode::Type::IoPrintln || n.type == AstNode::Type::IoPrint) && !n.children.empty() && exprProducesString(n.children[0])) needsString = true;
            if (n.type == AstNode::Type::OsSystem && !n.children.empty() && exprProducesString(n.children[0])) needsString = true;
            if (n.type == AstNode::Type::While && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsString(c); }
            if (n.type == AstNode::Type::For && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsString(c); }
            if (n.type == AstNode::Type::IfElse) { for (size_t i = 1; i < n.children.size(); i++) { for (const auto& c : n.children[i].children) checkNeedsString(c); } }
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
            if (node.type == AstNode::Type::Variable && (node.declType == "string" || (node.initUninitialized && node.declType != "int") || node.initFromFileRead || (!node.initIsInt && !node.initFromDllLoad && node.children.empty()))) needsString = true;
            if (node.type == AstNode::Type::Variable && !node.children.empty() && exprProducesString(node.children[0])) needsString = true;
            if (node.type == AstNode::Type::Variable && !node.children.empty() && node.children[0].type == AstNode::Type::ExprArrayLiteral) {
                for (const auto& c : node.children[0].children) { if (exprProducesString(c)) { needsString = true; break; } }
            }
        }
        bool needsVector = false;
        std::function<void(const AstNode&)> checkNeedsVector = [&](const AstNode& n) {
            if (n.type == AstNode::Type::Variable && n.initFromArray) needsVector = true;
            if (n.type == AstNode::Type::Variable && !n.children.empty() && n.children[0].type == AstNode::Type::ExprArrayLiteral) needsVector = true;
            if (n.type == AstNode::Type::ExprArrayLiteral || n.type == AstNode::Type::ExprArrayIndex || n.type == AstNode::Type::AssnIndex) needsVector = true;
            if (n.type == AstNode::Type::While && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsVector(c); }
            if (n.type == AstNode::Type::For && n.children.size() > 1) { for (const auto& c : n.children[1].children) checkNeedsVector(c); }
            if (n.type == AstNode::Type::IfElse) { for (size_t i = 1; i < n.children.size(); i++) { for (const auto& c : n.children[i].children) checkNeedsVector(c); } }
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
            }
        }
        if (needsString && moduleCppIncludes.find("#include <string>\n") == std::string::npos) out << "#include <string>\n";
        if (needsVector && moduleCppIncludes.find("#include <vector>\n") == std::string::npos) out << "#include <vector>\n";
        if (!moduleCppIncludes.empty() || !inlineCppHoisted.empty() || needsString || needsVector) out << "\n";

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
                for (size_t i = 0; i < node.paramNames.size(); i++) {
                    if (i > 0) out << ", ";
                    std::string pname = preserveNames_ ? node.paramNames[i] : ("__nexa_param_" + std::to_string(i));
                    std::string ptype = "int";
                    if (i < node.paramTypes.size()) {
                        if (node.paramTypes[i] == "string") ptype = "std::string";
                        else if (node.paramTypes[i] == "bool") ptype = "bool";
                        else if (node.paramTypes[i] == "float") ptype = "double";
                        else if (node.paramTypes[i] == "char") ptype = "char";
                        else if (isStructDeclType(node.paramTypes[i])) {
                            ptype = structCppNames_.at(structNameFromDecl(node.paramTypes[i]));
                        } else if (isEnumDeclType(node.paramTypes[i])) {
                            ptype = enumCppNames_.at(enumNameFromDecl(node.paramTypes[i]));
                        }
                    }
                    out << ptype << " " << pname;
                    varMap[node.paramNames[i]] = pname;
                }
                out << ") {\n";
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
            bool isStrArr = isArray && !node.children.empty() && arrayLiteralProducesString(node.children[0], globalVarIsString);
            bool isStr = !isStructDeclType(node.declType) && !isEnumDeclType(node.declType) && (!node.declType.empty() ? (node.declType == "string") : (node.initUninitialized || (!node.initIsInt && !node.initIsBool && !node.initIsFloat && !node.initIsChar && !isArray && node.children.empty()) || (!node.children.empty() && (exprProducesString(node.children[0]) || isStrArr))));
            globalVarIsString[node.value] = isStr;
            globalVarIsArray[node.value] = isArray || node.isFixedArray;
            if (node.initUninitialized) {
                std::string c = node.isConst ? "const " : "";
                if (node.isFixedArray) {
                    std::string cppType = (node.declType == "unsigned char") ? "unsigned char" : (node.declType == "char") ? "char" : "int";
                    out << c << cppType << " " << vname << "[" << node.arraySize << "];\n";
                } else if (!node.declType.empty() && node.declType == "int") {
                    out << c << "int " << vname << " = 0;\n";
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
                bool strArr = arrayLiteralProducesString(node.children[0], globalVarIsString);
                out << c << (strArr ? "std::vector<std::string>" : "std::vector<int>") << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
            } else if (!node.children.empty()) {
                std::string c = node.isConst ? "const " : "";
                if (!node.declType.empty() && isEnumDeclType(node.declType)) {
                    std::string en = enumNameFromDecl(node.declType);
                    out << c << enumCppNames_.at(en) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else if (!node.declType.empty() && isStructDeclType(node.declType)) {
                    std::string sn = structNameFromDecl(node.declType);
                    out << c << structCppNames_.at(sn) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else {
                    bool useBool = !node.declType.empty() ? (node.declType == "bool") : node.initIsBool;
                    bool useInt = !node.declType.empty() ? (node.declType == "int") : node.initIsInt;
                    bool useFloat = !node.declType.empty() ? (node.declType == "float") : node.initIsFloat;
                    bool useChar = !node.declType.empty() ? (node.declType == "char") : node.initIsChar;
                    std::string cppType = c + (useBool ? "bool " : useFloat ? "double " : useChar ? "char " : (useInt ? "int " : "std::string "));
                    out << cppType << vname << " = " << emitExpr(node.children[0], globalVarMap, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                }
            } else if (node.initIsBool || (!node.declType.empty() && node.declType == "bool")) {
                std::string c = node.isConst ? "const " : "";
                out << c << "bool " << vname << " = " << (node.initValue == "true" ? "true" : "false") << ";\n";
            } else if (node.initIsInt || (!node.declType.empty() && node.declType == "int")) {
                std::string c = node.isConst ? "const " : "";
                out << c << "int " << vname << " = " << node.initValue << ";\n";
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
    };
    std::vector<FnOverloadSlot> fnOverloadSlots_;
    std::map<std::string, std::string> globalNexaDecl_;
    std::vector<std::map<std::string, std::string>> nexaDeclStack_;

    std::string canonicalParamType(const AstNode& fn, size_t i) const {
        if (i >= fn.paramTypes.size()) return "int";
        const std::string& pt = fn.paramTypes[i];
        return pt.empty() ? "int" : pt;
    }

    static bool typesMatchForOverload(const std::string& formal, const std::string& actual) {
        if (formal == actual) return true;
        if (formal == "float" && actual == "int") return true;
        return false;
    }

    std::string inferReturnNexaType(const AstNode& fn) const {
        if (!fn.fnReturnType.empty()) return fn.fnReturnType;
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
            if (sl.name != name || sl.paramTypes.size() != k) continue;
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
        std::vector<size_t> exact;
        for (size_t s : compat) {
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
        if (compat.size() == 1) return compat[0];
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
        if (preserveNames_) return ast_[sl.astIndex].value;
        return "__nexa_fn_" + std::to_string(slotIdx);
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
            if (!v.children.empty() && v.children[0].type == AstNode::Type::ExprArrayLiteral) {
                const AstNode& arr = v.children[0];
                if (!arr.children.empty()) return inferExprNexaType(arr.children[0]);
                return "int";
            }
            return "int";
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
                if (baseT == "[]string") return "string";
                return "int";
            }
            case AstNode::Type::IoToInt: return "int";
            case AstNode::Type::RandomInt: return "int";
            case AstNode::Type::OsGetProcessId: return "int";
            case AstNode::Type::TimeSeconds:
            case AstNode::Type::TimeMilliseconds:
                return "int";
            case AstNode::Type::TimeNowMs:
                return "float";
            case AstNode::Type::IoReadln:
                return "string";
            case AstNode::Type::FileRead:
            case AstNode::Type::IoGetline:
                return "string";
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
        std::string name = cppFnNameForSlot(slot);
        std::string s = name + "(";
        for (size_t i = 0; i < e.children.size(); i++) {
            if (i > 0) s += ", ";
            s += emitExpr(e.children[i], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
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
    static std::string structNameFromDecl(const std::string& declType) {
        return declType.substr(7);
    }
    static std::string enumNameFromDecl(const std::string& declType) {
        return declType.substr(5);
    }
    std::string nexaTypeToCpp(const std::string& t) const {
        if (t == "int") return "int";
        if (t == "string") return "std::string";
        if (t == "bool") return "bool";
        if (t == "float") return "double";
        if (t == "char") return "char";
        if (t == "unsigned char") return "unsigned char";
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
        throw std::runtime_error("Unknown type in struct: " + t);
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
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            std::string inner = structTypeOfExprValue(e.children[0]);
            if (inner.empty()) return "";
            auto sit = structFields_.find(inner);
            if (sit == structFields_.end()) return "";
            auto fit = sit->second.find(e.value);
            if (fit == sit->second.end()) return "";
            const std::string& ft = fit->second;
            if (ft.size() >= 7 && ft.compare(0, 7, "struct:") == 0) return ft.substr(7);
            return "";
        }
        return "";
    }
    std::string fieldTypeOfMemberExpr(const AstNode& e) const {
        if (e.type != AstNode::Type::ExprMember || e.children.empty()) return "";
        std::string st = structTypeOfExprValue(e.children[0]);
        if (st.empty()) return "";
        auto sit = structFields_.find(st);
        if (sit == structFields_.end()) return "";
        auto fit = sit->second.find(e.value);
        if (fit == sit->second.end()) return "";
        return fit->second;
    }

    static bool exprProducesString(const AstNode& e) {
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::IoGetline || e.type == AstNode::Type::ExprTrim) return true;
        if (e.type == AstNode::Type::ExprAdd && e.children.size() >= 2) {
            return exprProducesString(e.children[0]) || exprProducesString(e.children[1]);
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
            auto it = varIsString.find(e.value);
            return it != varIsString.end() && it->second;
        }
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::FileRead || e.type == AstNode::Type::IoReadln || e.type == AstNode::Type::IoGetline || e.type == AstNode::Type::ExprTrim) return true;
        if (e.type == AstNode::Type::ExprAdd && e.children.size() >= 2) {
            return exprIsString(e.children[0], varIsString) || exprIsString(e.children[1], varIsString);
        }
        return false;
    }

    bool exprIsFloat(const AstNode& e, const std::map<std::string, bool>& varIsFloat) const {
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            std::string ft = fieldTypeOfMemberExpr(e);
            return ft == "float";
        }
        if (e.type == AstNode::Type::ExprFloatLiteral) return true;
        if (e.type == AstNode::Type::TimeNowMs) return true;
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
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            std::string ft = fieldTypeOfMemberExpr(e);
            return ft == "char";
        }
        if (e.type == AstNode::Type::ExprCharLiteral) return true;
        if (e.type == AstNode::Type::ExprVarRef) {
            auto it = varIsChar.find(e.value);
            return it != varIsChar.end() && it->second;
        }
        return false;
    }

    bool exprIsBool(const AstNode& e, const std::map<std::string, bool>& varIsBool) const {
        if (e.type == AstNode::Type::ExprBoolLiteral) return true;
        if (e.type == AstNode::Type::ExprVarRef) {
            auto it = varIsBool.find(e.value);
            return it != varIsBool.end() && it->second;
        }
        if (e.type == AstNode::Type::ExprMember && !e.children.empty()) {
            return fieldTypeOfMemberExpr(e) == "bool";
        }
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
        if (nexaType == "int") {
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
                   const std::string& indent = "    ", bool inStringSwitchCase = false) {
        nexaDeclStack_.push_back({});
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
                bool isStrArr = isArray && !child.children.empty() && arrayLiteralProducesString(child.children[0], varIsString);
                bool isStr = !isArray && !isStructDeclType(child.declType) && !isEnumDeclType(child.declType) && (!child.declType.empty() ? (child.declType == "string") : (child.initUninitialized || child.initFromReadln || child.initFromFileRead || (!child.initIsInt && !child.initIsBool && !child.initIsFloat && !child.initIsChar && !child.initFromDllLoad && child.children.empty()) ||
                    (!child.children.empty() && exprProducesString(child.children[0]))));
                varIsString[child.value] = isStr || isStrArr;
                if (!child.declType.empty() && isStructDeclType(child.declType)) {
                    varStructDeclare(child.value, structNameFromDecl(child.declType));
                }
                if (!nexaDeclStack_.empty()) {
                    nexaDeclStack_.back()[child.value] = nexaDeclFromVariableAst(child);
                }
                if (child.initFromReadln) {
                    out << indent << "char __nexa_buf[4096];\n";
                    out << indent << "if (fgets(__nexa_buf, sizeof(__nexa_buf), stdin)) { __nexa_buf[strcspn(__nexa_buf, \"\\n\")] = 0; }\n";
                    out << indent << "std::string " << vname << "(__nexa_buf);\n";
                } else if (child.initFromDllLoad) {
#ifdef _WIN32
                    out << indent << "__nexa_dll_handles.push_back((void*)LoadLibraryA(\"" << escapeString(child.initValue) << "\"));\n";
#else
                    out << indent << "__nexa_dll_handles.push_back(dlopen(\"" << escapeString(child.initValue) << "\", RTLD_LAZY));\n";
#endif
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
                        std::string cppType = (child.declType == "unsigned char") ? "unsigned char" : (child.declType == "char") ? "char" : "int";
                        out << indent << c << cppType << " " << vname << "[" << child.arraySize << "];\n";
                    } else if (!child.declType.empty() && child.declType == "int") {
                        out << indent << c << "int " << vname << " = 0;\n";
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
                    bool strArr = arrayLiteralProducesString(child.children[0], varIsString);
                    out << indent << c << (strArr ? "std::vector<std::string>" : "std::vector<int>") << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                } else if (!child.children.empty()) {
                    std::string c = child.isConst ? "const " : "";
                    if (!child.declType.empty() && isEnumDeclType(child.declType)) {
                        std::string en = enumNameFromDecl(child.declType);
                        out << indent << c << enumCppNames_.at(en) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else if (!child.declType.empty() && isStructDeclType(child.declType)) {
                        std::string sn = structNameFromDecl(child.declType);
                        out << indent << c << structCppNames_.at(sn) << " " << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else {
                        bool useBool = !child.declType.empty() ? (child.declType == "bool") : child.initIsBool;
                        bool useInt = !child.declType.empty() ? (child.declType == "int") : child.initIsInt;
                        bool useFloat = !child.declType.empty() ? (child.declType == "float") : child.initIsFloat;
                        bool useChar = !child.declType.empty() ? (child.declType == "char") : child.initIsChar;
                        std::string cppType = c + (useBool ? "bool " : useFloat ? "double " : useChar ? "char " : (useInt ? "int " : "std::string "));
                        out << indent << cppType << vname << " = " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    }
                } else if (child.initIsBool || (!child.declType.empty() && child.declType == "bool")) {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << "bool " << vname << " = " << (child.initValue == "true" ? "true" : "false") << ";\n";
                } else if (child.initIsInt || (!child.declType.empty() && child.declType == "int")) {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << "int " << vname << " = " << child.initValue << ";\n";
                } else {
                    std::string c = child.isConst ? "const " : "";
                    out << indent << c << "std::string " << vname << " = \"" << escapeString(child.initValue) << "\";\n";
                }
            } else if (child.type == AstNode::Type::IoPrintln) {
                if (!child.children.empty()) {
                    const AstNode& arg0 = child.children[0];
                    if (arg0.type == AstNode::Type::ExprStringLiteral) {
                        out << indent << "puts(\"" << escapeString(arg0.value) << "\"); fflush(stdout);\n";
                    } else if (arg0.type == AstNode::Type::ExprIntLiteral) {
                        out << indent << "printf(\"%d\\n\", " << arg0.value << "); fflush(stdout);\n";
                    } else if (arg0.type == AstNode::Type::ExprBoolLiteral) {
                        out << indent << "printf(\"%d\\n\", " << (arg0.value == "true" ? "1" : "0") << "); fflush(stdout);\n";
                    } else {
                    std::string ntype = inferExprNexaType(child.children[0]);
                    bool exprIsStr = (ntype == "string");
                    bool exprIsF = (ntype == "float");
                    bool exprIsC = (ntype == "char");
                    bool exprIsBoolT = (ntype == "bool");
                    bool isNexaEnum = !ntype.empty() && ntype.size() >= 5 && ntype.compare(0, 5, "enum:") == 0;
                    std::string expr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    expr = wrapExprForPrintf(child.children[0], expr, varMap, varIsEnum);
                    if (exprIsStr) {
                        const bool strNeedsCStr = expr.empty() || expr[0] != '"';
                        if (strNeedsCStr) {
                            std::string arg = expr + ".c_str()";
                            out << indent << "printf(\"%s\\n\", " << arg << "); fflush(stdout);\n";
                        } else {
                            out << indent << "puts(" << expr << "); fflush(stdout);\n";
                        }
                    } else if (exprIsF) {
                        out << indent << "printf(\"%g\\n\", " << expr << "); fflush(stdout);\n";
                    } else if (exprIsC) {
                        out << indent << "printf(\"%c\\n\", " << expr << "); fflush(stdout);\n";
                    } else if (exprIsBoolT) {
                        out << indent << "printf(\"%d\\n\", " << expr << "); fflush(stdout);\n";
                    } else {
                        std::string arg = isNexaEnum ? ("static_cast<int>(" + expr + ")") : expr;
                        out << indent << "printf(\"%d\\n\", " << arg << "); fflush(stdout);\n";
                    }
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
                        out << indent << "puts(" << v << ".c_str()); fflush(stdout);\n";
                    } else if (isF) {
                        out << indent << "printf(\"%g\\n\", " << v << "); fflush(stdout);\n";
                    } else if (isC) {
                        out << indent << "printf(\"%c\\n\", " << v << "); fflush(stdout);\n";
                    } else {
                        std::string arg = isNexaEnum ? ("static_cast<int>(" + v + ")") : v;
                        out << indent << "printf(\"%d\\n\", " << arg << "); fflush(stdout);\n";
                    }
                } else {
                    out << indent << "puts(\"" << escapeString(child.value) << "\"); fflush(stdout);\n";
                }
            } else if (child.type == AstNode::Type::IoPrint) {
                if (!child.children.empty()) {
                    const AstNode& arg0 = child.children[0];
                    if (arg0.type == AstNode::Type::ExprStringLiteral) {
                        out << indent << "fputs(\"" << escapeString(arg0.value) << "\", stdout);\n";
                    } else if (arg0.type == AstNode::Type::ExprIntLiteral) {
                        out << indent << "printf(\"%d\", " << arg0.value << ");\n";
                    } else if (arg0.type == AstNode::Type::ExprBoolLiteral) {
                        out << indent << "printf(\"%d\", " << (arg0.value == "true" ? "1" : "0") << ");\n";
                    } else {
                    std::string ntype = inferExprNexaType(child.children[0]);
                    bool exprIsStr = (ntype == "string");
                    bool exprIsF = (ntype == "float");
                    bool exprIsC = (ntype == "char");
                    bool exprIsBoolT = (ntype == "bool");
                    bool isNexaEnum = !ntype.empty() && ntype.size() >= 5 && ntype.compare(0, 5, "enum:") == 0;
                    std::string expr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    expr = wrapExprForPrintf(child.children[0], expr, varMap, varIsEnum);
                    if (exprIsStr) {
                        const bool strNeedsCStr = expr.empty() || expr[0] != '"';
                        std::string arg = strNeedsCStr ? expr + ".c_str()" : expr;
                        out << indent << "fputs(" << arg << ", stdout);\n";
                    } else {
                        std::string fmt = exprIsF ? "%g" : exprIsC ? "%c" : exprIsBoolT ? "%d" : "%d";
                        std::string arg = isNexaEnum ? ("static_cast<int>(" + expr + ")") : expr;
                        out << indent << "printf(\"" << fmt << "\", " << arg << ");\n";
                    }
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
                    } else {
                        std::string fmt = isF ? "%g" : isC ? "%c" : "%d";
                        std::string arg = isNexaEnum ? ("static_cast<int>(" + v + ")") : v;
                        out << indent << "printf(\"" << fmt << "\", " << arg << ");\n";
                    }
                } else {
                    out << indent << "fputs(\"" << escapeString(child.value) << "\", stdout);\n";
                }
            } else if (child.type == AstNode::Type::IoFlush) {
                out << indent << "fflush(stdout);\n";
            } else if (child.type == AstNode::Type::FileRead) {
                out << indent << emitExpr(child, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::FileWrite) {
                std::string pathExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string contentExpr = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "{\n";
                out << indent << "    std::ofstream __f(" << pathExpr << ");\n";
                out << indent << "    __f << " << contentExpr << ";\n";
                out << indent << "}\n";
            } else if (child.type == AstNode::Type::FileAppend) {
                std::string pathExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string contentExpr = emitExpr(child.children[1], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "{\n";
                out << indent << "    std::ofstream __f(" << pathExpr << ", std::ios::app);\n";
                out << indent << "    __f << " << contentExpr << ";\n";
                out << indent << "}\n";
            } else if (child.type == AstNode::Type::FileExists) {
                std::string pathExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)(std::filesystem::exists(" << pathExpr << ") ? 1 : 0);\n";
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
                    out << indent << "std::this_thread::sleep_for(std::chrono::seconds("
                        << emitExpr(dur.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << "));\n";
                } else if (dur.type == AstNode::Type::TimeMilliseconds && !dur.children.empty()) {
                    out << indent << "std::this_thread::sleep_for(std::chrono::milliseconds("
                        << emitExpr(dur.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << "));\n";
                } else {
                    out << indent << "std::this_thread::sleep_for(std::chrono::milliseconds("
                        << emitExpr(dur, varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << "));\n";
                }
            } else if (child.type == AstNode::Type::ThreadJoin) {
                std::string idxExpr = emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_thread_join(" << idxExpr << ");\n";
            } else if (child.type == AstNode::Type::DllCall) {
                std::string h = preserveNames_ ? child.children[0].value : varMap.at(child.children[0].value);
#ifdef _WIN32
                out << indent << "{\n";
                out << indent << "    void (*fn)() = (void(*)())GetProcAddress((HMODULE)__nexa_dll_handles[" << h << "], \"" << escapeString(child.value) << "\");\n";
                out << indent << "    if (fn) fn();\n";
                out << indent << "}\n";
#else
                out << indent << "{\n";
                out << indent << "    void (*fn)() = (void(*)())dlsym(__nexa_dll_handles[" << h << "], \"" << escapeString(child.value) << "\");\n";
                out << indent << "    if (fn) fn();\n";
                out << indent << "}\n";
#endif
            } else if (child.type == AstNode::Type::OsSystem) {
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
            } else if (child.type == AstNode::Type::Assignment) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
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
                out << indent << v << "[" << idx << "] = " << val << ";\n";
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
                out << indent << v << " = " << v << " ^ " << emitExpr(child.children[0], varMap, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
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
                if (c.children.size() >= 2 && c.children[0].type == AstNode::Type::ExprStringLiteral &&
                    c.children[1].type == AstNode::Type::ExprStringLiteral) {
                    return (c.children[0].value == c.children[1].value) ? "true" : "false";
                }
                return emitExpr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " == " +
                       emitExpr(c.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::CondNe:
                if (c.children.size() >= 2 && c.children[0].type == AstNode::Type::ExprStringLiteral &&
                    c.children[1].type == AstNode::Type::ExprStringLiteral) {
                    return (c.children[0].value != c.children[1].value) ? "true" : "false";
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
            case AstNode::Type::ExprIntLiteral:
            case AstNode::Type::ExprFloatLiteral:
            case AstNode::Type::ExprCharLiteral:
            case AstNode::Type::ExprVarRef:
                return emitExpr(c, varMap, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::FileExists: {
                std::string pathExpr = emitExpr(c.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "std::filesystem::exists(" + pathExpr + ")";
            }
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
            case AstNode::Type::IoReadln: {
                return "([]{ char __b[4096]; if (fgets(__b, sizeof(__b), stdin)) __b[strcspn(__b, \"\\n\")] = 0; return std::string(__b); }())";
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
                std::string pathExpr = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "([]{ std::ifstream __f(" + pathExpr + "); std::stringstream __ss; __ss << __f.rdbuf(); return __ss.str(); }())";
            }
            case AstNode::Type::FileExists: {
                std::string pathExpr = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "(std::filesystem::exists(" + pathExpr + ") ? 1 : 0)";
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
            case AstNode::Type::TimeSeconds: {
                std::string n = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(" + n + ")).count())";
            }
            case AstNode::Type::TimeMilliseconds: {
                std::string n = emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool);
                return "static_cast<int>(std::chrono::milliseconds(" + n + ").count())";
            }
            case AstNode::Type::TimeNowMs:
                return "__nexa_time_now_ms()";
            case AstNode::Type::ThreadSpawn: {
                size_t z = slotForZeroArgFunctionNamed(e.value);
                return "__nexa_thread_spawn(&" + cppFnNameForSlot(z) + ")";
            }
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
                return emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + "." + e.value;
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
            case AstNode::Type::ExprBitXor:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " ^ " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprShl:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " << " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprShr:
                return "(" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + " >> " + emitExpr(e.children[1], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitNot:
                return "(~" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondNot:
                return "(!" + emitExpr(e.children[0], varMap, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            default:
                return "0";
        }
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
            return "std::to_string(" + emitExpr(child, varMap, pStr, pFl, pCh, pBo) + ")";
        }
        if (exprIsString(child, varIsString) || exprProducesString(child)) {
            return emitExpr(child, varMap, pStr, pFl, pCh, pBo);
        }
        if (exprIsFloat(child, varIsFloat)) {
            return "std::to_string(" + emitExpr(child, varMap, pStr, pFl, pCh, pBo) + ")";
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

    std::string escapeString(const std::string& s) {
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
