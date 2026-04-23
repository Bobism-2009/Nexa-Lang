#pragma once

#include "Parser.hpp"
#include "Modules.hpp"
#include <string>
#include <sstream>
#include <cstdio>
#include <map>
#include <set>
#include <functional>
#include <cctype>

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

        // C++ includes from enabled modules
        if (buildDll_) {
            out << "#ifdef _WIN32\n";
            out << "#define NEXA_EXPORT __declspec(dllexport)\n";
            out << "#else\n";
            out << "#define NEXA_EXPORT __attribute__((visibility(\"default\")))\n";
            out << "#endif\n\n";
        }
        out << modules_.getCppIncludes();
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
        }
        if (needsString) out << "#include <string>\n";
        if (needsVector) out << "#include <vector>\n";
        if (!modules_.getCppIncludes().empty() || !inlineCppHoisted.empty() || needsString || needsVector) out << "\n";

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

        // Build function name map (for expressions in globals and functions)
        std::map<std::string, int> fnIndex;
        int idx = 0;
        for (const AstNode& node : ast_) {
            if (node.type == AstNode::Type::Function) {
                fnIndex[node.value] = idx++;
            }
        }

        auto fnName = [&](const std::string& name) -> std::string {
            if (preserveNames_) return name;
            auto it = fnIndex.find(name);
            if (it != fnIndex.end()) return "__nexa_fn_" + std::to_string(it->second);
            return name;  // fallback (e.g. main)
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
        for (const AstNode& node : ast_) {
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
                std::string cppName = fnName(node.value);
                bool hasValRet = false, hasVoidRet = false;
                stmtsClassifyReturns(node.children, hasValRet, hasVoidRet);
                if (hasValRet && hasVoidRet) {
                    throw std::runtime_error("function '" + node.value + "' mixes 'return;' and 'return expr;'");
                }
                std::string retType = hasValRet ? "int" : "void";
                out << (buildDll_ ? "extern \"C\" NEXA_EXPORT " : "static ") << retType << " " << cppName << "(";
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
                emitFnRet_ = hasValRet ? EmitFnRet::IntFn : EmitFnRet::VoidFn;
                emitBlockStatements(out, node.children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum);
                emitFnRet_ = EmitFnRet::Main;
                varStructPop();
                if (hasValRet && !stmtsEndWithReturn(node.children)) {
                    out << "    return 0;\n";
                }
                out << "}\n\n";
                continue;
            }
            if (node.type == AstNode::Type::MainFunction) {
                if (!buildDll_) {
                    wroteMain = true;
                    out << "int main() {\n";
                    std::map<std::string, std::string> varMap = globalVarMap;
                    int varIdx = 0;
                    std::map<std::string, bool> varIsString = globalVarIsString;
                    std::map<std::string, bool> varIsConst = globalVarIsConst;
                    std::map<std::string, bool> varIsFloat = globalVarIsFloat;
                    std::map<std::string, bool> varIsChar = globalVarIsChar;
                    std::map<std::string, bool> varIsBool = globalVarIsBool;
                    std::map<std::string, bool> varIsEnum = globalVarIsEnum;
                    bool mainValRet = false, mainVoidRet = false;
                    stmtsClassifyReturns(node.children, mainValRet, mainVoidRet);
                    if (mainValRet && mainVoidRet) {
                        throw std::runtime_error("main mixes 'return;' and 'return expr;'");
                    }
                    varStructPush();
                    emitFnRet_ = EmitFnRet::Main;
                    emitBlockStatements(out, node.children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum);
                    emitFnRet_ = EmitFnRet::Main;
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
                out << c << (strArr ? "std::vector<std::string>" : "std::vector<int>") << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, fnName, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
            } else if (!node.children.empty()) {
                std::string c = node.isConst ? "const " : "";
                if (!node.declType.empty() && isEnumDeclType(node.declType)) {
                    std::string en = enumNameFromDecl(node.declType);
                    out << c << enumCppNames_.at(en) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, fnName, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else if (!node.declType.empty() && isStructDeclType(node.declType)) {
                    std::string sn = structNameFromDecl(node.declType);
                    out << c << structCppNames_.at(sn) << " " << vname << " = " << emitExpr(node.children[0], globalVarMap, fnName, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
                } else {
                    bool useBool = !node.declType.empty() ? (node.declType == "bool") : node.initIsBool;
                    bool useInt = !node.declType.empty() ? (node.declType == "int") : node.initIsInt;
                    bool useFloat = !node.declType.empty() ? (node.declType == "float") : node.initIsFloat;
                    bool useChar = !node.declType.empty() ? (node.declType == "char") : node.initIsChar;
                    std::string cppType = c + (useBool ? "bool " : useFloat ? "double " : useChar ? "char " : (useInt ? "int " : "std::string "));
                    out << cppType << vname << " = " << emitExpr(node.children[0], globalVarMap, fnName, &globalVarIsString, &globalVarIsFloat, &globalVarIsChar, &globalVarIsBool) << ";\n";
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
                std::string initName = fnName("__init__");
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
                    out << "    " << fnName("__init__") << "();\n";
                    out << "    return 0;\n";
                    out << "}\n";
                }
            }
        }

        return out.str();
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

    using FnNameFn = std::function<std::string(const std::string&)>;

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
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::ExprStringLiteral) return true;
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
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::FileRead || e.type == AstNode::Type::IoReadln) return true;
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

    void emitBlock(std::ostringstream& out, const std::vector<AstNode>& children, const FnNameFn& fnName,
                   std::map<std::string, std::string>& varMap, int& varIdx,
                   std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                   std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                   std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                   const std::string& indent = "    ", bool inStringSwitchCase = false) {
        varStructPush();
        emitBlockStatements(out, children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
        varStructPop();
    }

    void emitBlockStatements(std::ostringstream& out, const std::vector<AstNode>& children, const FnNameFn& fnName,
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
                    std::string pathExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    out << indent << "std::string " << vname << ";\n";
                    out << indent << "{\n";
                    out << indent << "    std::ifstream __f(" << pathExpr << ");\n";
                    out << indent << "    std::stringstream __ss; __ss << __f.rdbuf(); " << vname << " = __ss.str();\n";
                    out << indent << "}\n";
                } else if (isArray && !child.children.empty()) {
                    std::string c = child.isConst ? "const " : "";
                    bool strArr = arrayLiteralProducesString(child.children[0], varIsString);
                    out << indent << c << (strArr ? "std::vector<std::string>" : "std::vector<int>") << " " << vname << " = " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                } else if (!child.children.empty()) {
                    std::string c = child.isConst ? "const " : "";
                    if (!child.declType.empty() && isEnumDeclType(child.declType)) {
                        std::string en = enumNameFromDecl(child.declType);
                        out << indent << c << enumCppNames_.at(en) << " " << vname << " = " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else if (!child.declType.empty() && isStructDeclType(child.declType)) {
                        std::string sn = structNameFromDecl(child.declType);
                        out << indent << c << structCppNames_.at(sn) << " " << vname << " = " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                    } else {
                        bool useBool = !child.declType.empty() ? (child.declType == "bool") : child.initIsBool;
                        bool useInt = !child.declType.empty() ? (child.declType == "int") : child.initIsInt;
                        bool useFloat = !child.declType.empty() ? (child.declType == "float") : child.initIsFloat;
                        bool useChar = !child.declType.empty() ? (child.declType == "char") : child.initIsChar;
                        std::string cppType = c + (useBool ? "bool " : useFloat ? "double " : useChar ? "char " : (useInt ? "int " : "std::string "));
                        out << indent << cppType << vname << " = " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
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
                    bool exprIsStr = exprIsString(child.children[0], varIsString);
                    bool exprIsF = exprIsFloat(child.children[0], varIsFloat);
                    bool exprIsC = exprIsChar(child.children[0], varIsChar);
                    std::string expr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    expr = wrapExprForPrintf(child.children[0], expr, varMap, varIsEnum);
                    std::string fmt = exprIsStr ? "%s" : exprIsF ? "%g" : exprIsC ? "%c" : "%d";
                    std::string arg = exprIsStr ? expr + ".c_str()" : expr;
                    out << indent << "printf(\"" << fmt << "\\n\", " << arg << ");\n";
                } else if (child.isVarRef) {
                    std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                    bool isStr = varIsString.count(child.value) && varIsString.at(child.value);
                    bool isF = varIsFloat.count(child.value) && varIsFloat.at(child.value);
                    bool isC = varIsChar.count(child.value) && varIsChar.at(child.value);
                    bool isEn = varIsEnum.count(child.value) && varIsEnum.at(child.value);
                    std::string fmt = isStr ? "%s" : isF ? "%g" : isC ? "%c" : "%d";
                    std::string arg = isStr ? v + ".c_str()" : (isEn ? ("static_cast<int>(" + v + ")") : v);
                    out << indent << "printf(\"" << fmt << "\\n\", " << arg << ");\n";
                } else {
                    out << indent << "printf(\"" << escapeStringForPrintf(child.value) << "\\n\");\n";
                }
            } else if (child.type == AstNode::Type::IoPrint) {
                if (!child.children.empty()) {
                    bool exprIsStr = exprIsString(child.children[0], varIsString);
                    bool exprIsF = exprIsFloat(child.children[0], varIsFloat);
                    bool exprIsC = exprIsChar(child.children[0], varIsChar);
                    std::string expr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    expr = wrapExprForPrintf(child.children[0], expr, varMap, varIsEnum);
                    std::string fmt = exprIsStr ? "%s" : exprIsF ? "%g" : exprIsC ? "%c" : "%d";
                    std::string arg = exprIsStr ? expr + ".c_str()" : expr;
                    out << indent << "printf(\"" << fmt << "\", " << arg << ");\n";
                } else if (child.isVarRef) {
                    std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                    bool isStr = varIsString.count(child.value) && varIsString.at(child.value);
                    bool isF = varIsFloat.count(child.value) && varIsFloat.at(child.value);
                    bool isC = varIsChar.count(child.value) && varIsChar.at(child.value);
                    bool isEn = varIsEnum.count(child.value) && varIsEnum.at(child.value);
                    std::string fmt = isStr ? "%s" : isF ? "%g" : isC ? "%c" : "%d";
                    std::string arg = isStr ? v + ".c_str()" : (isEn ? ("static_cast<int>(" + v + ")") : v);
                    out << indent << "printf(\"" << fmt << "\", " << arg << ");\n";
                } else {
                    out << indent << "printf(\"" << escapeStringForPrintf(child.value) << "\");\n";
                }
            } else if (child.type == AstNode::Type::FileRead) {
                std::string pathExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "{\n";
                out << indent << "    std::ifstream __f(" << pathExpr << ");\n";
                out << indent << "    std::stringstream __ss; __ss << __f.rdbuf();\n";
                out << indent << "}\n";
            } else if (child.type == AstNode::Type::FileWrite) {
                std::string pathExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string contentExpr = emitExpr(child.children[1], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "{\n";
                out << indent << "    std::ofstream __f(" << pathExpr << ");\n";
                out << indent << "    __f << " << contentExpr << ";\n";
                out << indent << "}\n";
            } else if (child.type == AstNode::Type::FileAppend) {
                std::string pathExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string contentExpr = emitExpr(child.children[1], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "{\n";
                out << indent << "    std::ofstream __f(" << pathExpr << ", std::ios::app);\n";
                out << indent << "    __f << " << contentExpr << ";\n";
                out << indent << "}\n";
            } else if (child.type == AstNode::Type::FileExists) {
                std::string pathExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)(std::filesystem::exists(" << pathExpr << ") ? 1 : 0);\n";
            } else if (child.type == AstNode::Type::RandomSeed) {
                std::string seedExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "__nexa_random_seed(" << seedExpr << ");\n";
            } else if (child.type == AstNode::Type::RandomInt) {
                std::string minExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string maxExpr = emitExpr(child.children[1], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "(void)__nexa_random_int(" << minExpr << ", " << maxExpr << ");\n";
            } else if (child.type == AstNode::Type::TimeSleep) {
                const AstNode& dur = child.children[0];
                if (dur.type == AstNode::Type::TimeSeconds && !dur.children.empty()) {
                    out << indent << "std::this_thread::sleep_for(std::chrono::seconds("
                        << emitExpr(dur.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << "));\n";
                } else if (dur.type == AstNode::Type::TimeMilliseconds && !dur.children.empty()) {
                    out << indent << "std::this_thread::sleep_for(std::chrono::milliseconds("
                        << emitExpr(dur.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << "));\n";
                } else {
                    out << indent << "std::this_thread::sleep_for(std::chrono::milliseconds("
                        << emitExpr(dur, varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << "));\n";
                }
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
                    bool exprIsStr = exprIsString(child.children[0], varIsString);
                    std::string expr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                    out << indent << "std::system(" << (exprIsStr ? expr + ".c_str()" : expr) << ");\n";
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
                std::string textExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string titleExpr = emitExpr(child.children[1], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                bool textIsStr = exprIsString(child.children[0], varIsString);
                bool titleIsStr = exprIsString(child.children[1], varIsString);
                std::string textArg = textIsStr ? textExpr : ("std::to_string(" + textExpr + ")");
                std::string titleArg = titleIsStr ? titleExpr : ("std::to_string(" + titleExpr + ")");
                out << indent << "__nexa_os_messagebox(" << textArg << ", " << titleArg << ");\n";
            } else if (child.type == AstNode::Type::FnCall) {
                out << indent << fnName(child.value) << "(";
                for (size_t i = 0; i < child.children.size(); i++) {
                    if (i > 0) out << ", ";
                    out << emitExpr(child.children[i], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                }
                out << ");\n";
            } else if (child.type == AstNode::Type::AssnMember) {
                std::string lhs = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string rhs = emitExpr(child.children[1], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                const std::string& op = child.value;
                if (op == "=") out << indent << lhs << " = " << rhs << ";\n";
                else if (op == "+=") {
                    if (child.children[0].type == AstNode::Type::ExprMember && !child.children[0].children.empty() &&
                        fieldTypeOfMemberExpr(child.children[0]) == "string") {
                        out << indent << lhs << " += " << emitConcatOperand(child.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) << ";\n";
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
                out << indent << v << " = " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnIndex) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                std::string idx = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                std::string val = emitExpr(child.children[1], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << v << "[" << idx << "] = " << val << ";\n";
            } else if (child.type == AstNode::Type::AssnAdd) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                if (varIsString.count(child.value) && varIsString.at(child.value)) {
                    out << indent << v << " = " << v << " + " << emitConcatOperand(child.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) << ";\n";
                } else {
                    out << indent << v << " = " << v << " + " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
                }
            } else if (child.type == AstNode::Type::AssnSub) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " - " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnMul) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " * " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnDiv) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " / " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnMod) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " % " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnBitAnd) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " & " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnBitOr) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " | " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnBitXor) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " ^ " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnShl) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " << " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::AssnShr) {
                if (varIsConst.count(child.value) && varIsConst[child.value]) {
                    throw std::runtime_error("Cannot assign to const variable '" + child.value + "'");
                }
                std::string v = preserveNames_ ? child.value : varMap.at(child.value);
                out << indent << v << " = " << v << " >> " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
            } else if (child.type == AstNode::Type::InlineCpp) {
                emitInlineCppRaw(out, stripInlineCppIncludeLines(child.value), indent);
            } else if (child.type == AstNode::Type::IfElse) {
                emitIfElse(out, child, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
            } else if (child.type == AstNode::Type::Switch) {
                emitSwitch(out, child, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
            } else if (child.type == AstNode::Type::While) {
                std::string cond = emitCond(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "while (" << cond << ") {\n";
                emitBlock(out, child.children[1].children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
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
                std::string countExpr = emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
                out << indent << "for (int " << loopVar << " = 0; " << loopVar << " < " << countExpr << "; " << loopVar << "++) {\n";
                emitBlock(out, child.children[1].children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
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
                    out << indent << "return " << emitExpr(child.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ";\n";
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

    void emitIfElse(std::ostringstream& out, const AstNode& node, const FnNameFn& fnName,
                   std::map<std::string, std::string>& varMap, int& varIdx,
                   std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                   std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                   std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                   const std::string& indent, bool inStringSwitchCase = false) {
        std::string cond = emitCond(node.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
        out << indent << "if (" << cond << ") {\n";
        emitBlock(out, node.children[1].children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
        out << indent << "}";
        if (node.children.size() > 2) {
            const AstNode& elsePart = node.children[2];
            if (elsePart.type == AstNode::Type::IfElse) {
                out << " else if (" << emitCond(elsePart.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ") {\n";
                emitBlock(out, elsePart.children[1].children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
                out << indent << "}";
                if (elsePart.children.size() > 2) {
                    emitIfElseTail(out, elsePart.children[2], fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
                }
            } else {
                out << " else {\n";
                emitBlock(out, elsePart.children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
                out << indent << "}";
            }
        }
        out << "\n";
    }

    void emitSwitch(std::ostringstream& out, const AstNode& node, const FnNameFn& fnName,
                   std::map<std::string, std::string>& varMap, int& varIdx,
                   std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                   std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                   std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                   const std::string& indent, bool inStringSwitchCase = false) {
        std::string expr = emitExpr(node.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool);
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
                out << indent << (first ? "" : "else ") << "if (" << expr << " == std::string(\"" << escapeString(c->initValue) << "\")) {\n";
                first = false;
                emitBlock(out, c->children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", true);
                out << indent << "}\n";
            }
            for (const AstNode* c : defaults) {
                out << indent << (first ? "" : "else ") << "{\n";
                first = false;
                emitBlock(out, c->children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", true);
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
                emitBlock(out, c.children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ");
            }
            out << indent << "}\n";
        }
    }

    void emitIfElseTail(std::ostringstream& out, const AstNode& part, const FnNameFn& fnName,
                       std::map<std::string, std::string>& varMap, int& varIdx,
                       std::map<std::string, bool>& varIsString, std::map<std::string, bool>& varIsConst,
                       std::map<std::string, bool>& varIsFloat, std::map<std::string, bool>& varIsChar,
                       std::map<std::string, bool>& varIsBool, std::map<std::string, bool>& varIsEnum,
                       const std::string& indent, bool inStringSwitchCase = false) {
        if (part.type == AstNode::Type::IfElse) {
            out << " else if (" << emitCond(part.children[0], varMap, fnName, &varIsString, &varIsFloat, &varIsChar, &varIsBool) << ") {\n";
            emitBlock(out, part.children[1].children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
            out << indent << "}";
            if (part.children.size() > 2) {
                emitIfElseTail(out, part.children[2], fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent, inStringSwitchCase);
            }
        } else {
            out << " else {\n";
            emitBlock(out, part.children, fnName, varMap, varIdx, varIsString, varIsConst, varIsFloat, varIsChar, varIsBool, varIsEnum, indent + "    ", inStringSwitchCase);
            out << indent << "}";
        }
    }

    std::string emitCond(const AstNode& c, const std::map<std::string, std::string>& varMap, const FnNameFn& fnName,
                         const std::map<std::string, bool>* varIsString = nullptr,
                         const std::map<std::string, bool>* varIsFloat = nullptr,
                         const std::map<std::string, bool>* varIsChar = nullptr,
                         const std::map<std::string, bool>* varIsBool = nullptr) {
        switch (c.type) {
            case AstNode::Type::CondEq:
                return "(" + emitExpr(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " == " + emitExpr(c.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondNe:
                return "(" + emitExpr(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " != " + emitExpr(c.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondLt:
                return "(" + emitExpr(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " < " + emitExpr(c.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondLe:
                return "(" + emitExpr(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " <= " + emitExpr(c.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondGt:
                return "(" + emitExpr(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " > " + emitExpr(c.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondGe:
                return "(" + emitExpr(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " >= " + emitExpr(c.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondAnd:
                return "(" + emitCond(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " && " + emitCond(c.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondOr:
                return "(" + emitCond(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " || " + emitCond(c.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::CondNot:
                return "!(" + emitCond(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBoolLiteral:
                return c.value;
            case AstNode::Type::ExprIntLiteral:
            case AstNode::Type::ExprFloatLiteral:
            case AstNode::Type::ExprCharLiteral:
            case AstNode::Type::ExprVarRef:
                return emitExpr(c, varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
            case AstNode::Type::FileExists: {
                std::string pathExpr = emitExpr(c.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                return "std::filesystem::exists(" + pathExpr + ")";
            }
            default:
                return "false";
        }
    }

    std::string emitExpr(const AstNode& e, const std::map<std::string, std::string>& varMap, const FnNameFn& fnName,
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
                return "std::string(\"" + escapeString(e.value) + "\")";
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
            case AstNode::Type::OsExeDir:
                return "__nexa_exe_dir()";
            case AstNode::Type::IoReadln: {
                return "([]{ char __b[4096]; if (fgets(__b, sizeof(__b), stdin)) __b[strcspn(__b, \"\\n\")] = 0; return std::string(__b); }())";
            }
            case AstNode::Type::IoToInt: {
                std::string s = emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                return "__nexa_to_int(" + s + ")";
            }
            case AstNode::Type::FileRead: {
                std::string pathExpr = emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                return "([]{ std::ifstream __f(" + pathExpr + "); std::stringstream __ss; __ss << __f.rdbuf(); return __ss.str(); }())";
            }
            case AstNode::Type::FileExists: {
                std::string pathExpr = emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                return "(std::filesystem::exists(" + pathExpr + ") ? 1 : 0)";
            }
            case AstNode::Type::ExprLen: {
                std::string s = emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                return "(int)((" + s + ").size())";
            }
            case AstNode::Type::RandomInt: {
                std::string minExpr = emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                std::string maxExpr = emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                return "__nexa_random_int(" + minExpr + ", " + maxExpr + ")";
            }
            case AstNode::Type::TimeSeconds: {
                std::string n = emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                return "static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(" + n + ")).count())";
            }
            case AstNode::Type::TimeMilliseconds: {
                std::string n = emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                return "static_cast<int>(std::chrono::milliseconds(" + n + ").count())";
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
                    s += emitExpr(e.children[i], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                }
                s += "}";
                return s;
            }
            case AstNode::Type::ExprArrayIndex: {
                auto it = varMap.find(e.value);
                std::string v = (it != varMap.end()) ? it->second : e.value;
                return v + "[" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + "]";
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
                return emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + "." + e.value;
            }
            case AstNode::Type::FnCall: {
                std::string s = fnName(e.value) + "(";
                for (size_t i = 0; i < e.children.size(); i++) {
                    if (i > 0) s += ", ";
                    s += emitExpr(e.children[i], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool);
                }
                s += ")";
                return s;
            }
            case AstNode::Type::ExprAdd:
                if (exprIsString(e, vIsStr)) {
                    const std::map<std::string, bool>& vFl = varIsFloat ? *varIsFloat : kEmptyTypeMap;
                    const std::map<std::string, bool>& vCh = varIsChar ? *varIsChar : kEmptyTypeMap;
                    const std::map<std::string, bool>& vBo = varIsBool ? *varIsBool : kEmptyTypeMap;
                    return "(" + emitConcatOperand(e.children[0], varMap, fnName, vIsStr, vFl, vCh, vBo) + " + "
                        + emitConcatOperand(e.children[1], varMap, fnName, vIsStr, vFl, vCh, vBo) + ")";
                }
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " + "
                    + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprSub:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " - " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprMul:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " * " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprDiv:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " / " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprMod:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " % " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitAnd:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " & " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitOr:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " | " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitXor:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " ^ " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprShl:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " << " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprShr:
                return "(" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " >> " + emitExpr(e.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            case AstNode::Type::ExprBitNot:
                return "(~" + emitExpr(e.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
            default:
                return "0";
        }
    }

    std::string emitConcatOperand(const AstNode& child,
                                  const std::map<std::string, std::string>& varMap, const FnNameFn& fnName,
                                  const std::map<std::string, bool>& varIsString,
                                  const std::map<std::string, bool>& varIsFloat,
                                  const std::map<std::string, bool>& varIsChar,
                                  const std::map<std::string, bool>& varIsBool) {
        const std::map<std::string, bool>* pStr = &varIsString;
        const std::map<std::string, bool>* pFl = &varIsFloat;
        const std::map<std::string, bool>* pCh = &varIsChar;
        const std::map<std::string, bool>* pBo = &varIsBool;
        if (child.type == AstNode::Type::ExprAdd && exprIsString(child, varIsString)) {
            return "(" + emitConcatOperand(child.children[0], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + " + "
                + emitConcatOperand(child.children[1], varMap, fnName, varIsString, varIsFloat, varIsChar, varIsBool) + ")";
        }
        if (child.type == AstNode::Type::ExprAdd) {
            return "std::to_string(" + emitExpr(child, varMap, fnName, pStr, pFl, pCh, pBo) + ")";
        }
        if (exprIsString(child, varIsString) || exprProducesString(child)) {
            return emitExpr(child, varMap, fnName, pStr, pFl, pCh, pBo);
        }
        if (exprIsFloat(child, varIsFloat)) {
            return "std::to_string(" + emitExpr(child, varMap, fnName, pStr, pFl, pCh, pBo) + ")";
        }
        if (exprIsChar(child, varIsChar)) {
            return "std::string(1, " + emitExpr(child, varMap, fnName, pStr, pFl, pCh, pBo) + ")";
        }
        if (exprIsBool(child, varIsBool)) {
            std::string v = emitExpr(child, varMap, fnName, pStr, pFl, pCh, pBo);
            return "std::string(" + v + " ? \"true\" : \"false\")";
        }
        return "std::to_string(" + emitExpr(child, varMap, fnName, pStr, pFl, pCh, pBo) + ")";
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

    std::string escapeStringForPrintf(const std::string& s) {
        std::string out;
        for (unsigned char c : s) {
            if (c == '\\') out += "\\\\";
            else if (c == '"') out += "\\\"";
            else if (c == '%') out += "%%";
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
