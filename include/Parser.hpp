#pragma once

#include "Lexer.hpp"
#include "Modules.hpp"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#include <cctype>

namespace nexa {

// AST node types - simple structure for our minimal grammar
struct AstNode {
    enum class Type { Include, CppHeaderInclude, IoPrint, IoPrintln, IoFlush, IoReadln, IoGetline, IoToInt, MainFunction, Function, FnCall, Variable, Assignment, OsSystem, OsExec, OsGetenv, OsPlatform, OsExeDir, OsGetProcessId,
                      OsHideConsoleWindow, OsShowConsoleWindow, OsMinimizeConsoleWindow, OsMaximizeConsoleWindow,
                      OsMessageBox, OsGrepKeys, OsKeyPressed,
                      OsLock, OsShutdown, OsReboot, OsSuspend, OsLogout,
                      OsSetVolume, OsGetVolume, OsMute, OsUnmute, OsToggleMute,
                      OsSetBrightness, OsGetBrightness,
                      OsClipSet, OsClipGet,
                      OsType,
                      OsNotify, OsOpen, OsLoad, OsSave, OsPlay, OsSpawn, OsWait, OsKill,
                      OsTempDir, OsArch, OsCpuCount, OsWhich, OsUnsetenv, OsExecutable, OsCwd, OsChdir,
                      OsInfo,
                      OsExit, OsHostname, OsUsername, OsHome, OsSetenv,
                      DllLoad, DllCall,
                      FileRead, FileWrite, FileAppend, FileExists, FileMkdir, FileCall,
                      RandomInt, RandomSeed,
                      MathCall, CryptoCall, HttpCall, GfxCall, JsonCall,
                      ResultMake,
                      StrMethod,
                      TimeSleep, TimeSeconds, TimeMilliseconds, TimeNowMs,
                      ThreadSpawn, ThreadJoin, ThreadWorker, ThreadRun, ThreadWorkerJoin,
                      IfElse,
                      Switch,
                      SwitchCase,
                      While,
                      For,
                      ForIn,
                      Return,
                      Break,
                      Continue,
                      Goto,
                      Label,
                      TryCatch,
                      Throw,
                      IncPost,
                      DecPost,
                      AssnAdd,
                      AssnSub,
                      AssnMul,
                      AssnDiv,
                      AssnMod,
                      AssnBitAnd,
                      AssnBitOr,
                      AssnBitXor,
                      AssnShl,
                      AssnShr,
                      Block,
                      ExprIntLiteral, ExprFloatLiteral, ExprCharLiteral, ExprBoolLiteral, ExprVarRef, ExprAdd, ExprSub, ExprMul, ExprDiv, ExprMod,
                      ExprBitAnd, ExprBitOr, ExprBitXor, ExprShl, ExprShr, ExprBitNot,
                      ExprArrayLiteral, ExprArrayIndex, ExprSlice,
                      ExprCast,
                      ExprAddrOf,
                      ExprDeref,
                      ExprNull,
                      ExprNew,
                      StmtDelete,
                      ExprSizeof,
                      CondEq, CondNe, CondLt, CondGt, CondLe, CondGe,
                      CondAnd, CondOr, CondNot,
                      ExprTernary,
                      ExprLambda,
                      ExprCall,
                      ExprStructLit,
                      ExprStringLiteral,
                      ExprLen,
                      ExprTrim,
                      AssnIndex,
                      AssnDeref,
                      StructDef,
                      EnumDef,
                      ExprMember,
                      AssnMember,
                      InlineCpp };
    Type type;
    std::string value;           // for Include path, CppHeaderInclude line, string literal, variable name, or TryCatch catch binding
    std::vector<AstNode> children;
    std::vector<std::string> paramNames;   // for Function: parameter names
    std::vector<std::string> paramTypes;   // for Function: "int", "string", or "" (default int)
    std::vector<bool> paramHasDefault;     // for Function: true if this param has a default
    std::vector<AstNode> paramDefaults;    // for Function: default expr when paramHasDefault[i]
    std::string initValue;       // for Variable: literal initializer value
    bool initIsInt = false;     // for Variable: true = int, false = string
    bool initFromReadln = false; // for Variable: true = io.readln()
    bool initFromDllLoad = false; // for Variable: true = dll.load("path")
    bool initFromArray = false;   // for Variable: true = array literal
    bool initFromFileRead = false; // for Variable: true = file.read()
    bool initUninitialized = false; // for Variable: true = let x; (defaults to int 0)
    bool initIsBool = false;       // for Variable: true = let x = true/false
    bool initIsFloat = false;      // for Variable: true = let x = 3.14
    bool initIsChar = false;       // for Variable: true = let x = 'a'
    std::string declType = "";  // for Variable: int/short/long/size_t (and unsigned), string, bool, float, char, or "" (inferred)
    bool isVarRef = false;      // for IoPrint/IoPrintln: true = print variable, false = print string
    bool caseIsString = false;  // for SwitchCase: true = case "str", false = case 42
    bool caseIsEnum = false;    // for SwitchCase: Enum.variant; value = enum name, initValue = variant
    bool isConst = false;       // for Variable: true = let const x = ...
    bool isFixedArray = false;  // for Variable: true = let x: type[size]; (fixed-size buffer)
    bool isArrowMember = false; // for ExprMember: true = ptr->field, false = value.field
    std::string arraySize = ""; // for Variable: size for fixed array, e.g. "4080"
    std::string fnReturnType = ""; // Function / MainFunction: explicit ": type" before `{`; empty = infer from returns
    bool isExtern = false;         // Function: true = extern fn (C linkage declaration, no body)
    bool isVariadic = false;       // Function: true = trailing ... (C varargs)
    std::string receiverType = ""; // Function: "struct:Point" when this is a method inside a struct
    // Source provenance, stamped by the parser on statements and on primary expressions.
    // Semantic checks that run after parsing (Transpiler::checkSemantics) report through
    // these, so a diagnosis can name the file and line even for nodes that came from an
    // included .nxa. 0/"" means "not stamped" — callers fall back to the nearest parent.
    size_t line = 0;
    std::string srcFile = "";
};

inline bool nexaIsIntegerType(const std::string& t) {
    return t == "int" || t == "unsigned int" || t == "short" || t == "unsigned short"
        || t == "long" || t == "unsigned long" || t == "size_t"
        || t == "char" || t == "unsigned char";
}

inline bool nexaIsNumericIntType(const std::string& t) {
    return nexaIsIntegerType(t) && t != "char";
}

// Conversion rank of an integer type, ordered so that the higher-ranked operand of a binary
// integer operation is never a narrower type than the one the operation really has in C++.
// Deliberately coarser than C++'s own rank: `long` and `size_t` are different widths on LP64
// and LLP64, so this is the order that holds on every target Nexa emits for rather than the
// one a single ABI would give. A non-integer type ranks 0, so callers can pass anything.
//   0  char/short and their unsigned forms - integer-promoted to int before the operation
//   1  int
//   2  unsigned int
//   3  long
//   4  unsigned long
//   5  size_t - unsigned, and at least as wide as unsigned long on every supported target
inline int nexaIntConversionRank(const std::string& t) {
    if (t == "int") return 1;
    if (t == "unsigned int") return 2;
    if (t == "long") return 3;
    if (t == "unsigned long") return 4;
    if (t == "size_t") return 5;
    return 0;
}

// Type of `a <op> b` for the arithmetic and bitwise operators: Nexa's stand-in for C++'s
// usual arithmetic conversions. Pass the same type twice for a unary operator, whose result
// is just its promoted operand.
//
// One case is approximate. `long <op> unsigned int` is `long` here, which is exact on LP64
// (long is 64-bit, so it absorbs every unsigned int) but is really `unsigned long` on LLP64,
// where the two are both 32 bits. The two answers have the same width on that target, so
// nothing truncates; only the modelled signedness differs.
inline std::string nexaArithIntResultType(const std::string& a, const std::string& b) {
    const int ra = nexaIntConversionRank(a);
    const int rb = nexaIntConversionRank(b);
    switch (ra > rb ? ra : rb) {
        case 5: return "size_t";
        case 4: return "unsigned long";
        case 3: return "long";
        case 2: return "unsigned int";
        default: return "int";
    }
}

inline bool nexaIsSliceType(const std::string& t) {
    return t.size() >= 2 && t[0] == '[' && t[1] == ']';
}

inline std::string nexaSliceElem(const std::string& t) {
    return nexaIsSliceType(t) ? t.substr(2) : std::string();
}

inline bool nexaIsMapType(const std::string& t) {
    return t.size() >= 4 && t.compare(0, 4, "map[") == 0;
}

inline bool nexaIsResultType(const std::string& t) {
    return t.size() >= 8 && t.compare(0, 7, "Result[") == 0 && t.back() == ']';
}

inline std::string nexaResultInner(const std::string& t) {
    return nexaIsResultType(t) ? t.substr(7, t.size() - 8) : std::string();
}

inline std::string nexaMakeResultType(const std::string& inner) {
    return "Result[" + inner + "]";
}

inline bool nexaSplitMapType(const std::string& t, std::string& key, std::string& val) {
    if (!nexaIsMapType(t)) return false;
    int depth = 0;
    for (size_t i = 4; i < t.size(); i++) {
        if (t[i] == '[') depth++;
        else if (t[i] == ']') {
            if (depth == 0) {
                key = t.substr(4, i - 4);
                val = t.substr(i + 1);
                return !key.empty() && !val.empty();
            }
            depth--;
        }
    }
    return false;
}

inline bool nexaIsFnType(const std::string& t) {
    return t.size() >= 3 && t.compare(0, 3, "fn(") == 0;
}

inline std::string nexaMakeFnType(const std::vector<std::string>& params, const std::string& ret) {
    std::string s = "fn(";
    for (size_t i = 0; i < params.size(); i++) {
        if (i) s += ",";
        s += params[i];
    }
    s += "):";
    s += ret.empty() ? "int" : ret;
    return s;
}

inline bool nexaSplitFnType(const std::string& t, std::vector<std::string>& params, std::string& ret) {
    if (!nexaIsFnType(t)) return false;
    params.clear();
    size_t i = 3;
    int depth = 1;
    size_t start = 3;
    while (i < t.size() && depth > 0) {
        char c = t[i];
        if (c == '(') depth++;
        else if (c == ')') {
            depth--;
            if (depth == 0) {
                if (i > start) params.push_back(t.substr(start, i - start));
                i++;
                break;
            }
        } else if (c == ',' && depth == 1) {
            params.push_back(t.substr(start, i - start));
            start = i + 1;
        }
        i++;
    }
    if (depth != 0 || i >= t.size() || t[i] != ':') return false;
    ret = t.substr(i + 1);
    return !ret.empty();
}

class Parser {
public:
    Parser(std::vector<Token> tokens, Modules& modules,
           const std::string& currentFilePath = "",
           std::set<std::string>* includedFiles = nullptr,
           const std::vector<std::string>* packagePaths = nullptr)
        : tokens_(std::move(tokens)), modules_(modules), pos_(0),
          currentFilePath_(currentFilePath), includedFiles_(includedFiles),
          packagePaths_(packagePaths) {}

    std::vector<AstNode> parse() {
        try {
            return parseProgram();
        } catch (const std::runtime_error& e) {
            const std::string msg = e.what();
            if (!currentFilePath_.empty() && msg.find(currentFilePath_) == std::string::npos) {
                throw std::runtime_error(currentFilePath_ + ": " + msg);
            }
            throw;
        }
    }

    std::vector<AstNode> parseProgram() {
        std::vector<AstNode> ast;
        while (pos_ < tokens_.size()) {
            const Token& t = peek();
            if (t.type == TokenType::Eof) break;
            const size_t stmtLine = t.line;
            const size_t stampFrom = ast.size();
            if (t.type == TokenType::Include) {
                std::vector<AstNode> incNodes = parseInclude();
                noteTypeDefs(incNodes);
                for (AstNode& n : incNodes) ast.push_back(std::move(n));
            } else if (t.type == TokenType::Extern) {
                ast.push_back(parseExternFunction());
            } else if (t.type == TokenType::Fn) {
                if (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::Main) {
                    ast.push_back(parseMainFunction());
                } else {
                    ast.push_back(parseFunction());
                }
            } else if (t.type == TokenType::Struct) {
                ast.push_back(parseStruct());
            } else if (t.type == TokenType::Enum) {
                ast.push_back(parseEnum());
            } else if (t.type == TokenType::Let) {
                ast.push_back(parseVariable());
            } else if (t.type == TokenType::InlineCppBlock) {
                if (!modules_.hasInlineCpp()) {
                    throw std::runtime_error("inline_cpp! requires #include <std/inline> at line " + std::to_string(t.line));
                }
                const Token& tok = peek();
                std::string body = tok.value;
                advance();
                AstNode node{AstNode::Type::InlineCpp, "", {}};
                node.value = std::move(body);
                ast.push_back(std::move(node));
                if (peek().type == TokenType::Semicolon) advance();
            } else {
                throw std::runtime_error("Unexpected token at line " + std::to_string(t.line));
            }
            for (size_t i = stampFrom; i < ast.size(); ++i) stampSourceLoc(ast[i], stmtLine);
        }
        return ast;
    }

private:
    // Record where a node came from. Nodes parsed out of an #include'd .nxa are stamped by
    // that file's sub-parser first, so an already-stamped node keeps its own origin.
    void stampSourceLoc(AstNode& n, size_t line) {
        if (n.line != 0) return;
        n.line = line;
        n.srcFile = currentFilePath_;
    }

    std::vector<Token> tokens_;
    Modules& modules_;
    size_t pos_;
    std::string currentFilePath_;
    std::set<std::string>* includedFiles_;
    const std::vector<std::string>* packagePaths_;
    std::set<std::string> structNames_;
    std::map<std::string, std::set<std::string>> structFieldNames_;
    std::set<std::string> enumNames_;
    // Recursive-descent depth across expressions and statement bodies combined.
    // Without a cap, pathological nesting (hundreds of parens or braces)
    // overflows the C++ stack and segfaults the compiler instead of erroring.
    int nestDepth_ = 0;
    static constexpr int kMaxNestDepth = 256;
    struct NestGuard {
        Parser& p;
        NestGuard(Parser& p_, size_t line) : p(p_) {
            if (++p.nestDepth_ > kMaxNestDepth) {
                throw std::runtime_error("Code is nested too deeply (limit " +
                    std::to_string(kMaxNestDepth) + " levels) at line " + std::to_string(line));
            }
        }
        ~NestGuard() { --p.nestDepth_; }
    };

    static bool exprProducesString(const AstNode& e) {
        if (e.type == AstNode::Type::OsInfo) {
            const std::string& m = e.value;
            return m == "shell" || m == "newline" || m == "path_sep" || m == "lang"
                || m == "config_dir" || m == "cache_dir" || m == "desktop" || m == "endian";
        }
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsExec || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsExecutable || e.type == AstNode::Type::OsTempDir || e.type == AstNode::Type::OsArch || e.type == AstNode::Type::OsWhich || e.type == AstNode::Type::OsCwd || e.type == AstNode::Type::OsHostname || e.type == AstNode::Type::OsUsername || e.type == AstNode::Type::OsHome || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::OsClipGet || e.type == AstNode::Type::OsLoad || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::FileRead || e.type == AstNode::Type::IoReadln || e.type == AstNode::Type::IoGetline || e.type == AstNode::Type::ExprTrim || e.type == AstNode::Type::CryptoCall) return true;
        if (e.type == AstNode::Type::JsonCall && e.value == "stringify") return true;
        if (e.type == AstNode::Type::ExprCast && e.value == "string") return true;
        if (e.type == AstNode::Type::FileCall) {
            const std::string& m = e.value;
            return m == "cwd" || m == "abspath" || m == "join" || m == "dirname" || m == "basename" || m == "extension";
        }
        if (e.type == AstNode::Type::StrMethod) {
            const std::string& m = e.value;
            return m == "upper" || m == "lower" || m == "trim" || m == "replace" ||
                   m == "substring" || m == "repeat";
        }
        if (e.type == AstNode::Type::ExprAdd && e.children.size() >= 2) {
            return exprProducesString(e.children[0]) || exprProducesString(e.children[1]);
        }
        if (e.type == AstNode::Type::ExprTernary && e.children.size() >= 3) {
            return exprProducesString(e.children[1]) || exprProducesString(e.children[2]);
        }
        return false;
    }

    const Token& peek() const {
        if (pos_ >= tokens_.size()) return tokens_.back();
        return tokens_[pos_];
    }

    const Token& advance() {
        if (pos_ < tokens_.size()) pos_++;
        return tokens_[pos_ - 1];
    }

    bool match(TokenType type) {
        if (pos_ < tokens_.size() && tokens_[pos_].type == type) {
            advance();
            return true;
        }
        return false;
    }

    static std::string trimIncludeRaw(std::string s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
        return s.substr(i);
    }

    static bool isCppHeaderIncludePath(const std::string& path) {
        size_t dot = path.rfind('.');
        if (dot == std::string::npos || dot + 1 >= path.size()) return false;
        std::string ext = path.substr(dot);
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".hh";
    }

    // C/C++ std headers with no extension: #include <cstring>
    static bool isBareCppStdHeader(const std::string& path) {
        if (path.empty() || path.find('/') != std::string::npos || path.find('\\') != std::string::npos) {
            return false;
        }
        if (path.find('.') != std::string::npos) return false;
        static const char* kNames[] = {
            "cstring", "cstdio", "cstdlib", "cmath", "ctime", "cstdint", "cstddef",
            "cctype", "cerrno", "climits", "cfloat", "csignal", "csetjmp", "cstdarg",
            "cstdbool", "cwchar", "cwctype", "cuchar", "cassert", "cinttypes",
            "iostream", "iomanip", "fstream", "sstream", "string", "string_view",
            "vector", "array", "deque", "list", "map", "set", "unordered_map",
            "unordered_set", "queue", "stack", "algorithm", "functional", "memory",
            "utility", "optional", "variant", "any", "tuple", "iterator", "numeric",
            "limits", "type_traits", "chrono", "thread", "mutex", "atomic", "future",
            "filesystem", "regex", "complex", "bitset", "stdexcept", "exception",
            "new", "typeinfo", "locale", "span", "ranges", "format", "concepts",
            "compare", "numbers", "bit", "initializer_list",
            nullptr
        };
        for (int i = 0; kNames[i]; i++) {
            if (path == kNames[i]) return true;
        }
        return false;
    }

    static bool astHasMemberAccess(const AstNode& e) {
        if (e.type == AstNode::Type::ExprMember) return true;
        for (const AstNode& c : e.children) {
            if (astHasMemberAccess(c)) return true;
        }
        return false;
    }

    std::string parseTypeName() {
        // Pointer types: *int, **char, *Point, etc. (prefix *, like Go / systems style)
        if (match(TokenType::Star)) {
            return std::string("*") + parseTypeName();
        }
        // Growable slice: []T, [][]int, []*Point
        if (match(TokenType::LBracket)) {
            if (!match(TokenType::RBracket)) {
                throw std::runtime_error("Expected ']' in []T at line " + std::to_string(peek().line));
            }
            return "[]" + parseTypeName();
        }
        if (match(TokenType::Enum)) {
            const Token& t = peek();
            if (t.type != TokenType::Identifier) {
                throw std::runtime_error("Expected enum name at line " + std::to_string(t.line));
            }
            advance();
            return "enum:" + t.value;
        }
        if (match(TokenType::Fn)) {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after fn in type at line " + std::to_string(peek().line));
            }
            std::vector<std::string> params;
            if (peek().type != TokenType::RParen) {
                for (;;) {
                    params.push_back(parseTypeName());
                    if (!match(TokenType::Comma)) break;
                }
            }
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' in fn(...) type at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::Colon)) {
                throw std::runtime_error("Expected ': Ret' after fn(...) type at line " + std::to_string(peek().line));
            }
            std::string ret = parseTypeName();
            return nexaMakeFnType(params, ret);
        }
        const Token& t = peek();
        if (t.type == TokenType::Identifier && t.value == "unsigned") {
            advance();
            const Token& t2 = peek();
            if (t2.type != TokenType::Identifier) {
                throw std::runtime_error("Expected 'char', 'int', 'short', or 'long' after 'unsigned' at line " + std::to_string(t2.line));
            }
            if (t2.value == "char") {
                advance();
                return "unsigned char";
            }
            if (t2.value == "int") {
                advance();
                return "unsigned int";
            }
            if (t2.value == "short") {
                advance();
                return "unsigned short";
            }
            if (t2.value == "long") {
                advance();
                return "unsigned long";
            }
            throw std::runtime_error("Expected 'char', 'int', 'short', or 'long' after 'unsigned' at line " + std::to_string(t2.line));
        }
        if (t.type != TokenType::Identifier) {
            throw std::runtime_error("Expected type name at line " + std::to_string(t.line));
        }
        std::string v = t.value;
        advance();
        if (peek().type == TokenType::ColonColon) {
            while (match(TokenType::ColonColon)) {
                const Token& part = peek();
                if (part.type != TokenType::Identifier) {
                    throw std::runtime_error("Expected name after '::' in type at line " + std::to_string(part.line));
                }
                v += "::";
                v += part.value;
                advance();
            }
            return "cpp:" + v;
        }
        if (v == "map") {
            if (!match(TokenType::LBracket)) {
                throw std::runtime_error("Expected '[' after map at line " + std::to_string(peek().line));
            }
            std::string key = parseTypeName();
            if (!match(TokenType::RBracket)) {
                throw std::runtime_error("Expected ']' after map key type at line " + std::to_string(peek().line));
            }
            std::string val = parseTypeName();
            return "map[" + key + "]" + val;
        }
        if (v == "Result" || v == "result") {
            if (!match(TokenType::LBracket)) {
                throw std::runtime_error("Expected '[' after Result at line " + std::to_string(peek().line));
            }
            std::string inner = parseTypeName();
            if (!match(TokenType::RBracket)) {
                throw std::runtime_error("Expected ']' after Result inner type at line " + std::to_string(peek().line));
            }
            return nexaMakeResultType(inner);
        }
        if (v == "int") return "int";
        if (v == "short") return "short";
        if (v == "long") return "long";
        if (v == "size_t") return "size_t";
        if (v == "string") return "string";
        if (v == "bool") return "bool";
        if (v == "float") return "float";
        if (v == "char") return "char";
        if (v == "void") return "void";
        if (v == "Json" || v == "json") {
            if (!modules_.hasJson()) {
                throw std::runtime_error("Json requires #include <std/json>");
            }
            return "json";
        }
        if (enumNames_.count(v)) return "enum:" + v;
        return "struct:" + v;
    }

    void noteStructFields(const AstNode& n) {
        if (n.type != AstNode::Type::StructDef) return;
        structNames_.insert(n.value);
        std::set<std::string>& fields = structFieldNames_[n.value];
        for (const std::string& f : n.paramNames) fields.insert(f);
    }

    void noteTypeDefs(const std::vector<AstNode>& nodes) {
        for (const AstNode& n : nodes) {
            if (n.type == AstNode::Type::StructDef) noteStructFields(n);
            else if (n.type == AstNode::Type::EnumDef) enumNames_.insert(n.value);
        }
    }

    bool looksLikeTypeAt(size_t p) const {
        if (p >= tokens_.size()) return false;
        const Token& t = tokens_[p];
        if (t.type == TokenType::Star) return looksLikeTypeAt(p + 1);
        if (t.type == TokenType::LBracket) {
            if (p + 1 < tokens_.size() && tokens_[p + 1].type == TokenType::RBracket) {
                return looksLikeTypeAt(p + 2);
            }
            return false;
        }
        if (t.type == TokenType::Enum) return true;
        if (t.type == TokenType::Fn) return true;
        if (t.type != TokenType::Identifier) return false;
        if (p + 1 < tokens_.size() && tokens_[p + 1].type == TokenType::ColonColon) return true;
        const std::string& v = t.value;
        if (v == "int" || v == "short" || v == "long" || v == "size_t" ||
            v == "float" || v == "char" || v == "bool" || v == "string" ||
            v == "void" || v == "unsigned" || v == "map" || v == "Json" || v == "json" ||
            v == "Result" || v == "result") {
            return true;
        }
        return structNames_.count(v) != 0 || enumNames_.count(v) != 0;
    }

    bool looksLikeType() const {
        return looksLikeTypeAt(pos_);
    }

    AstNode parseSizeof() {
        size_t line = peek().line;
        if (!match(TokenType::Sizeof)) {
            throw std::runtime_error("Expected 'sizeof' at line " + std::to_string(line));
        }
        if (match(TokenType::LParen)) {
            if (looksLikeType()) {
                std::string ty = parseTypeName();
                if (ty == "void") {
                    throw std::runtime_error("sizeof(void) is not allowed at line " + std::to_string(line));
                }
                if (!match(TokenType::RParen)) {
                    throw std::runtime_error("Expected ')' after sizeof type at line " + std::to_string(peek().line));
                }
                return {AstNode::Type::ExprSizeof, ty, {}};
            }
            AstNode inner = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after sizeof(...) at line " + std::to_string(peek().line));
            }
            return {AstNode::Type::ExprSizeof, "", {std::move(inner)}};
        }
        return {AstNode::Type::ExprSizeof, "", {parseUnary()}};
    }

    static bool isAllocatableType(const std::string& ty) {
        if (ty.empty() || ty == "void") return false;
        return true;
    }

    AstNode parseNewExpr() {
        size_t line = peek().line;
        if (!match(TokenType::New)) {
            throw std::runtime_error("Expected 'new' at line " + std::to_string(line));
        }
        std::string ty = parseTypeName();
        if (!isAllocatableType(ty)) {
            throw std::runtime_error("Cannot allocate type '" + ty + "' at line " + std::to_string(line));
        }
        AstNode node{AstNode::Type::ExprNew, ty, {}};
        if (match(TokenType::LBracket)) {
            node.children.push_back(parseExpression());
            if (!match(TokenType::RBracket)) {
                throw std::runtime_error("Expected ']' after new T[...] at line " + std::to_string(peek().line));
            }
            node.isFixedArray = true;
        }
        return node;
    }

    AstNode parseDeleteStmt() {
        size_t line = peek().line;
        if (!match(TokenType::Delete)) {
            throw std::runtime_error("Expected 'delete' at line " + std::to_string(line));
        }
        bool isArray = false;
        if (match(TokenType::LBracket)) {
            if (!match(TokenType::RBracket)) {
                throw std::runtime_error("Expected ']' after 'delete[' at line " + std::to_string(peek().line));
            }
            isArray = true;
        }
        AstNode expr = parseExpression();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after delete at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::StmtDelete, isArray ? "[]" : "", {std::move(expr)}};
    }

    AstNode parseStruct() {
        size_t line = peek().line;
        if (!match(TokenType::Struct)) {
            throw std::runtime_error("Expected 'struct' at line " + std::to_string(line));
        }
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected struct name at line " + std::to_string(nameTok.line));
        }
        advance();
        std::string sname = nameTok.value;
        if (sname == "main") {
            throw std::runtime_error("Invalid struct name 'main' at line " + std::to_string(line));
        }
        structNames_.insert(sname);
        structFieldNames_[sname];
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::StructDef, sname, {}};
        while (peek().type != TokenType::RBrace) {
            if (peek().type == TokenType::Eof) {
                throw std::runtime_error("Unclosed struct body starting at line " + std::to_string(line));
            }
            if (peek().type == TokenType::Fn) {
                AstNode meth = parseFunction();
                if (meth.value == "self") {
                    throw std::runtime_error("Invalid method name 'self' at line " + std::to_string(line));
                }
                for (const std::string& f : node.paramNames) {
                    if (f == meth.value) {
                        throw std::runtime_error("Method '" + meth.value + "' collides with field '" + f +
                            "' at line " + std::to_string(line));
                    }
                }
                meth.receiverType = "struct:" + sname;
                node.children.push_back(std::move(meth));
                continue;
            }
            const Token& fieldTok = peek();
            if (fieldTok.type != TokenType::Identifier) {
                throw std::runtime_error("Expected field name or method at line " + std::to_string(fieldTok.line));
            }
            if (fieldTok.value == "self") {
                throw std::runtime_error("Invalid field name 'self' at line " + std::to_string(fieldTok.line));
            }
            advance();
            node.paramNames.push_back(fieldTok.value);
            structFieldNames_[sname].insert(fieldTok.value);
            if (!match(TokenType::Colon)) {
                throw std::runtime_error("Expected ':' after field name at line " + std::to_string(peek().line));
            }
            node.paramTypes.push_back(parseTypeName());
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after struct field at line " + std::to_string(peek().line));
            }
        }
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' at end of struct at line " + std::to_string(peek().line));
        }
        match(TokenType::Semicolon);
        return node;
    }

    AstNode parseEnum() {
        size_t line = peek().line;
        if (!match(TokenType::Enum)) {
            throw std::runtime_error("Expected 'enum' at line " + std::to_string(line));
        }
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected enum name at line " + std::to_string(nameTok.line));
        }
        advance();
        std::string ename = nameTok.value;
        if (ename == "main") {
            throw std::runtime_error("Invalid enum name 'main' at line " + std::to_string(line));
        }
        enumNames_.insert(ename);
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::EnumDef, ename, {}};
        while (peek().type != TokenType::RBrace) {
            if (peek().type == TokenType::Eof) {
                throw std::runtime_error("Unclosed enum body starting at line " + std::to_string(line));
            }
            const Token& vTok = peek();
            if (vTok.type != TokenType::Identifier) {
                throw std::runtime_error("Expected variant name at line " + std::to_string(vTok.line));
            }
            advance();
            node.paramNames.push_back(vTok.value);
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after enum variant at line " + std::to_string(peek().line));
            }
        }
        if (node.paramNames.empty()) {
            throw std::runtime_error("Enum must have at least one variant at line " + std::to_string(line));
        }
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' at end of enum at line " + std::to_string(peek().line));
        }
        match(TokenType::Semicolon);
        return node;
    }

    // ident.method(...) or ident.field.method(...) as a statement (result discarded).
    // Distinguished from member assignment (ident.field = ...).
    bool looksLikeIndexedDotCallStmt() const {
        if (pos_ + 3 >= tokens_.size()) return false;
        if (tokens_[pos_].type != TokenType::Identifier) return false;
        if (tokens_[pos_ + 1].type != TokenType::LBracket) return false;
        int depth = 0;
        for (size_t i = pos_ + 1; i < tokens_.size(); i++) {
            if (tokens_[i].type == TokenType::LBracket) depth++;
            else if (tokens_[i].type == TokenType::RBracket) {
                depth--;
                if (depth == 0) {
                    size_t j = i + 1;
                    while (j < tokens_.size() && tokens_[j].type == TokenType::LBracket) {
                        int d2 = 0;
                        size_t k = j;
                        for (; k < tokens_.size(); k++) {
                            if (tokens_[k].type == TokenType::LBracket) d2++;
                            else if (tokens_[k].type == TokenType::RBracket) {
                                d2--;
                                if (d2 == 0) {
                                    j = k + 1;
                                    break;
                                }
                            }
                        }
                        if (k >= tokens_.size()) return false;
                    }
                    if (j + 2 < tokens_.size() &&
                        (tokens_[j].type == TokenType::Dot || tokens_[j].type == TokenType::Arrow) &&
                        tokens_[j + 1].type == TokenType::Identifier &&
                        tokens_[j + 2].type == TokenType::LParen) {
                        return true;
                    }
                    return false;
                }
            }
        }
        return false;
    }

    bool looksLikeDotMethodCallStmt() const {
        if (pos_ + 1 >= tokens_.size()) return false;
        if (tokens_[pos_].type != TokenType::Identifier) return false;
        if (tokens_[pos_ + 1].type != TokenType::Dot && tokens_[pos_ + 1].type != TokenType::Arrow)
            return false;
        size_t i = pos_ + 1;
        while (i + 1 < tokens_.size() &&
               (tokens_[i].type == TokenType::Dot || tokens_[i].type == TokenType::Arrow)) {
            if (tokens_[i + 1].type != TokenType::Identifier) return false;
            i += 2;
            if (i < tokens_.size() && tokens_[i].type == TokenType::LParen) return true;
        }
        return false;
    }

    AstNode parseExprStatement() {
        AstNode expr = parseExpression();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after expression at line " + std::to_string(peek().line));
        }
        return expr;
    }

    AstNode parseMemberAssignment() {
        size_t line = peek().line;
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected variable name at line " + std::to_string(line));
        }
        advance();
        AstNode cur{AstNode::Type::ExprVarRef, nameTok.value, {}};
        while (peek().type == TokenType::Dot || peek().type == TokenType::Arrow) {
            bool arrow = peek().type == TokenType::Arrow;
            advance();
            const Token& ftok = peek();
            if (ftok.type != TokenType::Identifier) {
                throw std::runtime_error(std::string("Expected field name after '") + (arrow ? "->" : ".") +
                    "' at line " + std::to_string(ftok.line));
            }
            advance();
            AstNode mem{AstNode::Type::ExprMember, ftok.value, {std::move(cur)}};
            mem.isArrowMember = arrow;
            cur = std::move(mem);
        }
        if (cur.type != AstNode::Type::ExprMember) {
            throw std::runtime_error("Expected member assignment (e.g. obj.field = ... or ptr->field = ...) at line " + std::to_string(line));
        }
        std::string op = "=";
        if (match(TokenType::Assign)) {
            op = "=";
        } else if (match(TokenType::PlusAssign)) {
            op = "+=";
        } else if (match(TokenType::MinusAssign)) {
            op = "-=";
        } else if (match(TokenType::StarAssign)) {
            op = "*=";
        } else if (match(TokenType::SlashAssign)) {
            op = "/=";
        } else if (match(TokenType::PercentAssign)) {
            op = "%=";
        } else if (match(TokenType::BitAndAssign)) {
            op = "&=";
        } else if (match(TokenType::BitOrAssign)) {
            op = "|=";
        } else if (match(TokenType::BitXorAssign)) {
            op = "^=";
        } else if (match(TokenType::ShlAssign)) {
            op = "<<=";
        } else if (match(TokenType::ShrAssign)) {
            op = ">>=";
        } else {
            throw std::runtime_error("Expected '=' or compound assignment at line " + std::to_string(peek().line));
        }
        AstNode expr = parseExpression();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::AssnMember, op, {std::move(cur), std::move(expr)}};
        return node;
    }

    // The std modules Modules.hpp knows how to emit (SYNTAX/Modules.txt).
    static const std::vector<std::string>& knownStdModules() {
        static const std::vector<std::string> mods = {
            "std/io", "std/os", "std/file", "std/dll", "std/random", "std/math",
            "std/crypto", "std/http", "std/json", "std/time", "std/thread",
            "std/gfx", "std/inline",
        };
        return mods;
    }

    static bool isKnownStdModule(const std::string& path) {
        for (const std::string& m : knownStdModules()) {
            if (m == path) return true;
        }
        return false;
    }

    static std::string knownStdModuleList() {
        std::string out;
        for (const std::string& m : knownStdModules()) {
            if (!out.empty()) out += ", ";
            out += m;
        }
        return out;
    }

    std::vector<AstNode> parseInclude() {
        const Token& t = advance();
        std::string raw = t.value;
        size_t angleStart = raw.find('<');
        size_t angleEnd = raw.find('>');
        size_t quoteStart = raw.find('"');
        size_t quoteEnd = raw.rfind('"');

        if (angleStart != std::string::npos && angleEnd != std::string::npos && angleEnd > angleStart) {
            std::string path = raw.substr(angleStart + 1, angleEnd - angleStart - 1);
            if (path.size() >= 4 && path.substr(0, 4) == "std/") {
                if (path == "std/ui") {
                    throw std::runtime_error("std/ui has been removed");
                }
                if (path == "std/wait") {
                    throw std::runtime_error("std/wait has been removed; use #include <std/time>");
                }
                // A misspelled std module used to be accepted silently; the program then
                // failed later with "json.* requires #include <std/json>" pointing at code
                // that looked correct. Reject the include itself instead.
                if (!isKnownStdModule(path)) {
                    throw std::runtime_error("Unknown module '" + path + "' at line " + std::to_string(t.line) +
                                             ". Valid modules: " + knownStdModuleList());
                }
                // #include <std/io> - built-in module
                modules_.enable(path);
                return {{AstNode::Type::Include, path, {}}};
            }
            if (isCppHeaderIncludePath(path) || isBareCppStdHeader(path)) {
                modules_.noteCppHeader();
                return {{AstNode::Type::CppHeaderInclude, trimIncludeRaw(raw), {}}};
            }
            // #include <pkg/module> - package
            std::string pkgPath = path;
            if (pkgPath.size() < 4 || pkgPath.substr(pkgPath.size() - 4) != ".nxa") pkgPath += ".nxa";
            if (packagePaths_) {
                for (const std::string& root : *packagePaths_) {
                    std::filesystem::path full = std::filesystem::path(root) / pkgPath;
                    std::ifstream in(full);
                    if (in) {
                        std::string absPath = std::filesystem::absolute(full).string();
                        if (includedFiles_) {
                            if (includedFiles_->count(absPath)) return {};
                            includedFiles_->insert(absPath);
                        }
                        std::stringstream buf;
                        buf << in.rdbuf();
                        in.close();
                        Lexer lexer(buf.str(), absPath);
                        std::vector<Token> subTokens = lexer.tokenize();
                        Parser subParser(std::move(subTokens), modules_, absPath, includedFiles_, packagePaths_);
                        return subParser.parse();
                    }
                }
            }
            throw std::runtime_error("Cannot find package: " + path);
        }
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos && quoteEnd > quoteStart) {
            // #include "file.nxa" - file include
            std::string relPath = raw.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            std::filesystem::path resolved;
            if (!relPath.empty() && (relPath[0] == '/' || (relPath.size() > 1 && relPath[1] == ':'))) {
                resolved = relPath;
            } else {
                std::filesystem::path base = std::filesystem::path(currentFilePath_).parent_path();
                resolved = (base / relPath).lexically_normal();
            }
            std::string absPath = std::filesystem::absolute(resolved).string();
            if (isCppHeaderIncludePath(relPath)) {
                std::ifstream in(absPath);
                if (!in) {
                    throw std::runtime_error("Cannot open header: " + absPath);
                }
                in.close();
                if (includedFiles_) {
                    if (includedFiles_->count(absPath)) return {};
                    includedFiles_->insert(absPath);
                }
                modules_.noteCppHeader();
                std::string gen = std::string("#include \"") + std::filesystem::path(absPath).generic_string() + "\"";
                return {{AstNode::Type::CppHeaderInclude, gen, {}}};
            }
            if (includedFiles_) {
                if (includedFiles_->count(absPath)) return {};  // already included, skip
                includedFiles_->insert(absPath);
            }
            std::ifstream in(absPath);
            if (!in) {
                throw std::runtime_error("Cannot open included file: " + absPath);
            }
            std::stringstream buf;
            buf << in.rdbuf();
            in.close();
            Lexer lexer(buf.str(), absPath);
            std::vector<Token> subTokens = lexer.tokenize();
            Parser subParser(std::move(subTokens), modules_, absPath, includedFiles_, packagePaths_);
            return subParser.parse();
        }
        throw std::runtime_error("Invalid #include at line " + std::to_string(t.line));
    }

    std::string parseMainArgsSliceType() {
        size_t bracketLine = peek().line;
        if (!match(TokenType::LBracket)) {
            throw std::runtime_error("Expected '[' starting []string slice type at line " + std::to_string(bracketLine));
        }
        if (!match(TokenType::RBracket)) {
            throw std::runtime_error("Expected ']' in []string at line " + std::to_string(peek().line));
        }
        const Token& elem = peek();
        if (elem.type != TokenType::Identifier || elem.value != "string") {
            throw std::runtime_error("main parameter type must be []string at line " + std::to_string(elem.line));
        }
        advance();
        return "[]string";
    }

    AstNode parseMainFunction() {
        size_t line = peek().line;
        if (!match(TokenType::Fn)) {
            throw std::runtime_error("Expected 'fn' at line " + std::to_string(line));
        }
        if (!match(TokenType::Main)) {
            throw std::runtime_error("Expected 'main' at line " + std::to_string(line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(line));
        }

        AstNode mainNode{AstNode::Type::MainFunction, "", {}};
        if (peek().type != TokenType::RParen) {
            const Token& argTok = peek();
            if (argTok.type != TokenType::Identifier) {
                throw std::runtime_error("Expected parameter name at line " + std::to_string(argTok.line));
            }
            std::string pname = argTok.value;
            advance();
            if (!match(TokenType::Colon)) {
                throw std::runtime_error("Expected ':' after parameter name at line " + std::to_string(peek().line));
            }
            std::string ptype = parseMainArgsSliceType();
            mainNode.paramNames.push_back(std::move(pname));
            mainNode.paramTypes.push_back(std::move(ptype));
            if (peek().type == TokenType::Comma) {
                throw std::runtime_error(
                    "fn main accepts at most one parameter (args: []string) at line " + std::to_string(peek().line));
            }
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after main(…) at line " + std::to_string(peek().line));
        }

        std::string mainReturnType;
        if (match(TokenType::Colon)) {
            mainReturnType = parseTypeName();
        }
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' at line " + std::to_string(peek().line));
        }

        mainNode.fnReturnType = std::move(mainReturnType);
        mainNode.children = parseBlock();
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' at line " + std::to_string(peek().line));
        }

        return mainNode;
    }

    AstNode parseFunction() {
        size_t line = peek().line;
        if (!match(TokenType::Fn)) {
            throw std::runtime_error("Expected 'fn' at line " + std::to_string(line));
        }
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected function name at line " + std::to_string(nameTok.line));
        }
        advance();
        std::string name = nameTok.value;
        if (name == "main") {
            throw std::runtime_error("Use fn main() for entry point at line " + std::to_string(line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        std::vector<std::string> params;
        std::vector<std::string> types;
        std::vector<bool> hasDefault;
        std::vector<AstNode> defaults;
        bool seenDefault = false;
        if (peek().type != TokenType::RParen) {
            for (;;) {
                const Token& p = peek();
                if (p.type != TokenType::Identifier) {
                    throw std::runtime_error("Expected parameter name at line " + std::to_string(p.line));
                }
                params.push_back(p.value);
                advance();
                std::string ptype = "int";  // default type
                if (match(TokenType::Colon)) {
                    ptype = parseTypeName();
                }
                types.push_back(ptype);
                if (match(TokenType::Assign)) {
                    seenDefault = true;
                    hasDefault.push_back(true);
                    defaults.push_back(parseExpression());
                } else {
                    if (seenDefault) {
                        throw std::runtime_error(
                            "Parameter '" + params.back() + "' must have a default value (defaults must be trailing) at line " +
                            std::to_string(p.line));
                    }
                    hasDefault.push_back(false);
                    defaults.push_back(AstNode{AstNode::Type::ExprIntLiteral, "0", {}});
                }
                if (!match(TokenType::Comma)) break;
            }
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        std::string fnReturnType;
        if (match(TokenType::Colon)) {
            fnReturnType = parseTypeName();
        }
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' at line " + std::to_string(peek().line));
        }
        AstNode fnNode{AstNode::Type::Function, name, {}};
        fnNode.fnReturnType = std::move(fnReturnType);
        fnNode.paramNames = std::move(params);
        fnNode.paramTypes = std::move(types);
        fnNode.paramHasDefault = std::move(hasDefault);
        fnNode.paramDefaults = std::move(defaults);
        fnNode.children = parseBlock();
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' at line " + std::to_string(peek().line));
        }
        return fnNode;
    }

    AstNode parseExternFunction() {
        size_t line = peek().line;
        if (!match(TokenType::Extern)) {
            throw std::runtime_error("Expected 'extern' at line " + std::to_string(line));
        }
        if (!match(TokenType::Fn)) {
            throw std::runtime_error("Expected 'fn' after extern at line " + std::to_string(peek().line));
        }
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected function name at line " + std::to_string(nameTok.line));
        }
        advance();
        std::string name = nameTok.value;
        if (name == "main") {
            throw std::runtime_error("extern fn cannot be named main at line " + std::to_string(line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        std::vector<std::string> params;
        std::vector<std::string> types;
        if (peek().type != TokenType::RParen && peek().type != TokenType::Ellipsis) {
            for (;;) {
                const Token& p = peek();
                if (p.type != TokenType::Identifier) {
                    throw std::runtime_error("Expected parameter name at line " + std::to_string(p.line));
                }
                params.push_back(p.value);
                advance();
                std::string ptype = "int";
                if (match(TokenType::Colon)) {
                    ptype = parseTypeName();
                }
                if (match(TokenType::Assign)) {
                    throw std::runtime_error("extern fn parameters cannot have default values at line " + std::to_string(peek().line));
                }
                types.push_back(ptype);
                if (peek().type == TokenType::Ellipsis) break;
                if (!match(TokenType::Comma)) break;
            }
        }
        bool variadic = match(TokenType::Ellipsis);
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Colon)) {
            throw std::runtime_error("extern fn requires an explicit return type (e.g. : int or : void) at line " + std::to_string(peek().line));
        }
        std::string fnReturnType = parseTypeName();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after extern fn declaration at line " + std::to_string(peek().line));
        }
        AstNode fnNode{AstNode::Type::Function, name, {}};
        fnNode.isExtern = true;
        fnNode.isVariadic = variadic;
        fnNode.fnReturnType = std::move(fnReturnType);
        fnNode.paramNames = std::move(params);
        fnNode.paramTypes = std::move(types);
        return fnNode;
    }

    std::vector<AstNode> parseBlock(AstNode* parentIf = nullptr, bool singleStatement = false) {
        std::vector<AstNode> stmts;
        while (pos_ < tokens_.size()) {
            const Token& t = peek();
            if (t.type == TokenType::RBrace) break;
            if (singleStatement && t.type == TokenType::Semicolon) { advance(); break; }
            size_t before = stmts.size();
            const size_t stmtLine = t.line;
            if (t.type == TokenType::Let) {
                stmts.push_back(parseVariable());
            } else if (t.type == TokenType::If) {
                stmts.push_back(parseIf());
            } else if (t.type == TokenType::Else) {
                AstNode* attachTo = nullptr;
                if (!stmts.empty() && stmts.back().type == AstNode::Type::IfElse) {
                    attachTo = &stmts.back();
                } else if (stmts.empty() && parentIf && parentIf->type == AstNode::Type::IfElse) {
                    attachTo = parentIf;
                }
                if (!attachTo) {
                    throw std::runtime_error("else without matching if at line " + std::to_string(t.line));
                }
                advance();
                if (peek().type == TokenType::If) {
                    AstNode elseIfPart = parseIf();
                    attachTo->children.push_back(elseIfPart);
                } else {
                    if (!match(TokenType::LBrace)) {
                        throw std::runtime_error("Expected '{' after else at line " + std::to_string(peek().line));
                    }
                    AstNode elseBlock{AstNode::Type::Block, "", {}};
                    elseBlock.children = parseBlock();
                    attachTo->children.push_back(elseBlock);
                    if (!match(TokenType::RBrace)) {
                        throw std::runtime_error("Expected '}' at line " + std::to_string(peek().line));
                    }
                }
            } else if (t.type == TokenType::Switch) {
                stmts.push_back(parseSwitch());
            } else if (t.type == TokenType::While) {
                stmts.push_back(parseWhile());
            } else if (t.type == TokenType::For) {
                stmts.push_back(parseFor());
            } else if (t.type == TokenType::Try) {
                stmts.push_back(parseTryCatch());
            } else if (t.type == TokenType::Throw) {
                stmts.push_back(parseThrowStmt());
            } else if (t.type == TokenType::InlineCppBlock) {
                if (!modules_.hasInlineCpp()) {
                    throw std::runtime_error("inline_cpp! requires #include <std/inline> at line " + std::to_string(t.line));
                }
                const Token& tok = peek();
                std::string body = tok.value;
                advance();
                AstNode node{AstNode::Type::InlineCpp, "", {}};
                node.value = std::move(body);
                stmts.push_back(std::move(node));
                if (peek().type == TokenType::Semicolon) advance();
            } else if (t.type == TokenType::Identifier && t.value == "io") {
                stmts.push_back(parseIoCall());
            } else if (t.type == TokenType::Identifier && t.value == "os") {
                stmts.push_back(parseOsCall());
            } else if (t.type == TokenType::Identifier && t.value == "dll") {
                stmts.push_back(parseDllCall());
            } else if (t.type == TokenType::Identifier && t.value == "file") {
                stmts.push_back(parseFileCall());
            } else if (t.type == TokenType::Identifier && t.value == "thread") {
                stmts.push_back(parseThreadCall());
            } else if (t.type == TokenType::Identifier && t.value == "time") {
                stmts.push_back(parseTimeCall());
            } else if (t.type == TokenType::Identifier && t.value == "random") {
                stmts.push_back(parseRandomCall());
            } else if (t.type == TokenType::Identifier && t.value == "gfx") {
                stmts.push_back(parseGfxCall(true));
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       (tokens_[pos_ + 1].type == TokenType::Dot || tokens_[pos_ + 1].type == TokenType::Arrow)) {
                const Token& id2 = tokens_[pos_ + 2];
                bool isPathFile = tokens_[pos_ + 1].type == TokenType::Dot &&
                    id2.type == TokenType::Identifier && (id2.value == "Write" || id2.value == "Append") &&
                    pos_ + 3 < tokens_.size() && tokens_[pos_ + 3].type == TokenType::LParen;
                if (isPathFile) {
                    stmts.push_back(parsePathVarFileCall());
                } else if (looksLikeDotMethodCallStmt()) {
                    stmts.push_back(parseExprStatement());
                } else {
                    stmts.push_back(parseMemberAssignment());
                }
            } else if (t.type == TokenType::Identifier && (t.value == "getprocessid" || t.value == "getpid") &&
                       pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LParen) {
                stmts.push_back(parseOsGetProcessIdBareStmt());
            } else if (t.type == TokenType::Identifier && t.value == "ui") {
                throw std::runtime_error("std/ui has been removed at line " + std::to_string(t.line));
            } else if (t.type == TokenType::Star) {
                stmts.push_back(parseDerefAssignment());
            } else if (t.type == TokenType::Delete) {
                stmts.push_back(parseDeleteStmt());
            } else if (t.type == TokenType::LParen || t.type == TokenType::Fn) {
                stmts.push_back(parseExprStatement());
            } else if (looksLikeStructLiteral()) {
                stmts.push_back(parseExprStatement());
            } else if (looksLikeQualifiedFnCall() ||
                       (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                        tokens_[pos_ + 1].type == TokenType::LParen)) {
                stmts.push_back(parseFnCall());
            } else if (t.type == TokenType::Goto) {
                stmts.push_back(parseGoto());
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       tokens_[pos_ + 1].type == TokenType::Colon) {
                stmts.push_back(parseLabel());
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size()) {
                TokenType next = tokens_[pos_ + 1].type;
                if (next == TokenType::LBracket) {
                    if (looksLikeIndexedDotCallStmt()) {
                        stmts.push_back(parseExprStatement());
                    } else {
                        stmts.push_back(parseIndexedAssignment());
                    }
                } else if (next == TokenType::Assign || next == TokenType::PlusAssign || next == TokenType::MinusAssign ||
                    next == TokenType::StarAssign || next == TokenType::SlashAssign || next == TokenType::PercentAssign ||
                    next == TokenType::BitAndAssign || next == TokenType::BitOrAssign || next == TokenType::BitXorAssign ||
                    next == TokenType::ShlAssign || next == TokenType::ShrAssign) {
                    stmts.push_back(parseAssignment());
                } else if (next == TokenType::PlusPlus || next == TokenType::MinusMinus) {
                    stmts.push_back(parseIncDec());
                } else {
                    throw std::runtime_error("Unexpected token at line " + std::to_string(t.line));
                }
            } else if (t.type == TokenType::Return) {
                stmts.push_back(parseReturn());
            } else if (t.type == TokenType::Break) {
                stmts.push_back(parseBreak());
            } else if (t.type == TokenType::Continue) {
                stmts.push_back(parseContinue());
            } else if (t.type == TokenType::Eof) {
                throw std::runtime_error("Unexpected end of file inside block (missing '}') at line " +
                                         std::to_string(t.line));
            } else {
                throw std::runtime_error("Unexpected token at line " + std::to_string(t.line));
            }
            for (size_t i = before; i < stmts.size(); ++i) stampSourceLoc(stmts[i], stmtLine);
            if (singleStatement && stmts.size() > before) break;
        }
        return stmts;
    }

    // Parse a braced block { ... } OR a single braceless statement (C-style),
    // returning a Block node either way.
    AstNode parseBody(AstNode* parentIf = nullptr) {
        NestGuard guard(*this, peek().line);
        AstNode block{AstNode::Type::Block, "", {}};
        if (match(TokenType::LBrace)) {
            block.children = parseBlock(parentIf);
            if (!match(TokenType::RBrace)) {
                throw std::runtime_error("Expected '}' at line " + std::to_string(peek().line));
            }
        } else {
            block.children = parseBlock(parentIf, /*singleStatement=*/true);
            if (block.children.empty()) {
                throw std::runtime_error("Expected '{' or a statement at line " + std::to_string(peek().line));
            }
        }
        return block;
    }

    AstNode parseLenExpr() {
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "len") {
            throw std::runtime_error("Expected 'len' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after len at line " + std::to_string(peek().line));
        }
        AstNode arg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' in len(...) at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::ExprLen, "", {arg}};
    }

    AstNode parseTrimExpr() {
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "trim") {
            throw std::runtime_error("Expected 'trim' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after trim at line " + std::to_string(peek().line));
        }
        AstNode arg = parseExpression();
        AstNode node{AstNode::Type::ExprTrim, "", {arg}};
        if (match(TokenType::Comma)) {
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' in trim(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    bool looksLikeQualifiedFnCall() const {
        size_t p = pos_;
        if (p < tokens_.size() && tokens_[p].type == TokenType::ColonColon) p++;
        if (p >= tokens_.size() || tokens_[p].type != TokenType::Identifier) return false;
        p++;
        bool qualified = (pos_ < tokens_.size() && tokens_[pos_].type == TokenType::ColonColon);
        while (p < tokens_.size() && tokens_[p].type == TokenType::ColonColon) {
            qualified = true;
            p++;
            if (p >= tokens_.size() || tokens_[p].type != TokenType::Identifier) return false;
            p++;
        }
        return qualified && p < tokens_.size() && tokens_[p].type == TokenType::LParen;
    }

    std::string parseCppQualifiedName() {
        std::string name;
        if (match(TokenType::ColonColon)) name = "::";
        const Token& first = peek();
        if (first.type != TokenType::Identifier) {
            throw std::runtime_error("Expected name after '::' at line " + std::to_string(first.line));
        }
        name += first.value;
        advance();
        while (match(TokenType::ColonColon)) {
            const Token& part = peek();
            if (part.type != TokenType::Identifier) {
                throw std::runtime_error("Expected name after '::' at line " + std::to_string(part.line));
            }
            name += "::";
            name += part.value;
            advance();
        }
        return name;
    }

    bool looksLikeStructLiteral() const {
        if (peek().type != TokenType::Identifier) return false;
        if (!structNames_.count(peek().value)) return false;
        return pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LBrace;
    }

    AstNode parseStructLiteral() {
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected struct name at line " + std::to_string(nameTok.line));
        }
        std::string sname = nameTok.value;
        size_t line = nameTok.line;
        advance();
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' after struct name at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::ExprStructLit, sname, {}};
        std::set<std::string> seen;
        auto fieldsIt = structFieldNames_.find(sname);
        while (peek().type != TokenType::RBrace) {
            if (peek().type == TokenType::Eof) {
                throw std::runtime_error("Unclosed struct literal starting at line " + std::to_string(line));
            }
            const Token& ftok = peek();
            if (ftok.type != TokenType::Identifier) {
                throw std::runtime_error("Expected field name in " + sname + " literal at line " + std::to_string(ftok.line));
            }
            std::string fname = ftok.value;
            if (fieldsIt != structFieldNames_.end() && !fieldsIt->second.count(fname)) {
                throw std::runtime_error("Unknown field '" + fname + "' in " + sname + " at line " + std::to_string(ftok.line));
            }
            if (!seen.insert(fname).second) {
                throw std::runtime_error("Duplicate field '" + fname + "' in " + sname + " literal at line " + std::to_string(ftok.line));
            }
            advance();
            if (!match(TokenType::Colon)) {
                throw std::runtime_error("Expected ':' after field '" + fname + "' at line " + std::to_string(peek().line));
            }
            node.paramNames.push_back(std::move(fname));
            node.children.push_back(parseTernary());
            if (match(TokenType::Comma)) continue;
            break;
        }
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' after struct literal at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseLambda() {
        size_t line = peek().line;
        if (!match(TokenType::Fn)) {
            throw std::runtime_error("Expected 'fn' at line " + std::to_string(line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after fn at line " + std::to_string(peek().line));
        }
        std::vector<std::string> params;
        std::vector<std::string> types;
        if (peek().type != TokenType::RParen) {
            for (;;) {
                const Token& p = peek();
                if (p.type != TokenType::Identifier) {
                    throw std::runtime_error("Expected parameter name at line " + std::to_string(p.line));
                }
                params.push_back(p.value);
                advance();
                std::string ptype = "int";
                if (match(TokenType::Colon)) {
                    ptype = parseTypeName();
                }
                types.push_back(ptype);
                if (match(TokenType::Assign)) {
                    throw std::runtime_error("Lambda parameters cannot have defaults at line " + std::to_string(p.line));
                }
                if (!match(TokenType::Comma)) break;
            }
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after lambda parameters at line " + std::to_string(peek().line));
        }
        std::string fnReturnType;
        if (match(TokenType::Colon)) {
            fnReturnType = parseTypeName();
        }
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' after lambda signature at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::ExprLambda, "", {}};
        node.fnReturnType = std::move(fnReturnType);
        node.paramNames = std::move(params);
        node.paramTypes = std::move(types);
        node.children = parseBlock();
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' after lambda body at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parsePostfixCalls(AstNode e) {
        while (peek().type == TokenType::LParen) {
            advance();
            AstNode call{AstNode::Type::ExprCall, "", {}};
            call.children.push_back(std::move(e));
            if (peek().type != TokenType::RParen) {
                for (;;) {
                    call.children.push_back(parseValueExpr());
                    if (!match(TokenType::Comma)) break;
                }
            }
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after call at line " + std::to_string(peek().line));
            }
            e = std::move(call);
        }
        return applyIndexAndDotPostfix(std::move(e));
    }

    AstNode parseFnCallExpr() {
        std::string name = parseCppQualifiedName();
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        std::vector<AstNode> args;
        if (peek().type != TokenType::RParen) {
            for (;;) {
                args.push_back(parseValueExpr());
                if (!match(TokenType::Comma)) break;
            }
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::FnCall, name, {}};
        node.children = std::move(args);
        return node;
    }

    AstNode parseReturn() {
        size_t line = peek().line;
        if (!match(TokenType::Return)) {
            throw std::runtime_error("Expected 'return' at line " + std::to_string(line));
        }
        if (match(TokenType::Semicolon)) {
            return {AstNode::Type::Return, "", {}};
        }
        AstNode expr = parseValueExpr();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after return at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::Return, "", {}};
        node.children.push_back(expr);
        return node;
    }

    AstNode parseBreak() {
        if (!match(TokenType::Break)) {
            throw std::runtime_error("Expected 'break' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after break at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::Break, "", {}};
    }

    AstNode parseContinue() {
        if (!match(TokenType::Continue)) {
            throw std::runtime_error("Expected 'continue' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after continue at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::Continue, "", {}};
    }

    AstNode parseGoto() {
        if (!match(TokenType::Goto)) {
            throw std::runtime_error("Expected 'goto' at line " + std::to_string(peek().line));
        }
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected label name after 'goto' at line " + std::to_string(nameTok.line));
        }
        std::string label = nameTok.value;
        advance();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after goto " + label + " at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::Goto, label, {}};
    }

    // A label definition:  name:
    AstNode parseLabel() {
        const Token& nameTok = peek();
        std::string label = nameTok.value;
        advance();  // identifier
        if (!match(TokenType::Colon)) {
            throw std::runtime_error("Expected ':' after label " + label + " at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::Label, label, {}};
    }

    AstNode parseTryCatch() {
        size_t line = peek().line;
        if (!match(TokenType::Try)) {
            throw std::runtime_error("Expected 'try' at line " + std::to_string(line));
        }
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' after try at line " + std::to_string(peek().line));
        }
        AstNode tryBlock{AstNode::Type::Block, "", {}};
        tryBlock.children = parseBlock();
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' after try block at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Catch)) {
            throw std::runtime_error("Expected 'catch' after try at line " + std::to_string(peek().line));
        }
        std::string catchBinding;
        if (peek().type == TokenType::LParen) {
            advance();
            const Token& bindTok = peek();
            if (bindTok.type != TokenType::Identifier) {
                throw std::runtime_error("Expected catch variable name at line " + std::to_string(bindTok.line));
            }
            catchBinding = bindTok.value;
            advance();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after catch variable at line " + std::to_string(peek().line));
            }
        }
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' after catch at line " + std::to_string(peek().line));
        }
        AstNode catchBlock{AstNode::Type::Block, "", {}};
        catchBlock.children = parseBlock();
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' after catch block at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::TryCatch, catchBinding, {}};
        node.children.push_back(std::move(tryBlock));
        node.children.push_back(std::move(catchBlock));
        return node;
    }

    AstNode parseThrowStmt() {
        size_t line = peek().line;
        if (!match(TokenType::Throw)) {
            throw std::runtime_error("Expected 'throw' at line " + std::to_string(line));
        }
        AstNode expr = parseExpression();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after throw at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::Throw, "", {}};
        node.children.push_back(std::move(expr));
        return node;
    }

    AstNode parseIncDec() {
        const Token& nameTok = peek();
        advance();
        std::string name = nameTok.value;
        bool isInc = match(TokenType::PlusPlus);
        if (!isInc && !match(TokenType::MinusMinus)) {
            throw std::runtime_error("Expected '++' or '--' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after " + std::string(isInc ? "++" : "--") + " at line " + std::to_string(peek().line));
        }
        return {isInc ? AstNode::Type::IncPost : AstNode::Type::DecPost, name, {}};
    }

    AstNode parseFnCall() {
        AstNode node = parseFnCallExpr();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseIndexedAssignment() {
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected array name at line " + std::to_string(nameTok.line));
        }
        advance();
        std::string name = nameTok.value;
        std::vector<AstNode> indices;
        do {
            if (!match(TokenType::LBracket)) {
                throw std::runtime_error("Expected '[' at line " + std::to_string(peek().line));
            }
            if (peek().type == TokenType::Colon) {
                throw std::runtime_error("Cannot assign to a slice xs[a:b] at line " + std::to_string(peek().line));
            }
            AstNode idx = parseExpression();
            if (peek().type == TokenType::Colon) {
                throw std::runtime_error("Cannot assign to a slice xs[a:b] at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::RBracket)) {
                throw std::runtime_error("Expected ']' at line " + std::to_string(peek().line));
            }
            indices.push_back(std::move(idx));
        } while (peek().type == TokenType::LBracket);
        if (peek().type == TokenType::Dot || peek().type == TokenType::Arrow) {
            AstNode cur{AstNode::Type::ExprVarRef, name, {}};
            for (AstNode& idx : indices) {
                AstNode indexed{AstNode::Type::ExprArrayIndex, "", {}};
                indexed.children.push_back(std::move(cur));
                indexed.children.push_back(std::move(idx));
                cur = std::move(indexed);
            }
            while (peek().type == TokenType::Dot || peek().type == TokenType::Arrow) {
                bool arrow = peek().type == TokenType::Arrow;
                advance();
                const Token& ftok = peek();
                if (ftok.type != TokenType::Identifier) {
                    throw std::runtime_error(std::string("Expected field name after '") + (arrow ? "->" : ".") +
                        "' at line " + std::to_string(ftok.line));
                }
                advance();
                AstNode mem{AstNode::Type::ExprMember, ftok.value, {std::move(cur)}};
                mem.isArrowMember = arrow;
                cur = std::move(mem);
            }
            std::string op = "=";
            if (match(TokenType::Assign)) op = "=";
            else if (match(TokenType::PlusAssign)) op = "+=";
            else if (match(TokenType::MinusAssign)) op = "-=";
            else if (match(TokenType::StarAssign)) op = "*=";
            else if (match(TokenType::SlashAssign)) op = "/=";
            else if (match(TokenType::PercentAssign)) op = "%=";
            else if (match(TokenType::BitAndAssign)) op = "&=";
            else if (match(TokenType::BitOrAssign)) op = "|=";
            else if (match(TokenType::BitXorAssign)) op = "^=";
            else if (match(TokenType::ShlAssign)) op = "<<=";
            else if (match(TokenType::ShrAssign)) op = ">>=";
            else {
                throw std::runtime_error("Expected '=' or compound assignment at line " + std::to_string(peek().line));
            }
            AstNode expr = parseExpression();
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
            }
            return {AstNode::Type::AssnMember, op, {std::move(cur), std::move(expr)}};
        }
        std::string idxOp = "=";
        if (match(TokenType::Assign)) idxOp = "=";
        else if (match(TokenType::PlusAssign)) idxOp = "+=";
        else if (match(TokenType::MinusAssign)) idxOp = "-=";
        else if (match(TokenType::StarAssign)) idxOp = "*=";
        else if (match(TokenType::SlashAssign)) idxOp = "/=";
        else if (match(TokenType::PercentAssign)) idxOp = "%=";
        else if (match(TokenType::BitAndAssign)) idxOp = "&=";
        else if (match(TokenType::BitOrAssign)) idxOp = "|=";
        else if (match(TokenType::BitXorAssign)) idxOp = "^=";
        else if (match(TokenType::ShlAssign)) idxOp = "<<=";
        else if (match(TokenType::ShrAssign)) idxOp = ">>=";
        else {
            throw std::runtime_error("Expected '=' or compound assignment at line " + std::to_string(peek().line));
        }
        AstNode expr = parseExpression();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::AssnIndex, name, {}};
        node.initValue = idxOp;
        node.children = std::move(indices);
        node.children.push_back(std::move(expr));
        return node;
    }

    // *p = expr;  *p += n;  etc.
    AstNode parseDerefAssignment() {
        size_t line = peek().line;
        if (!match(TokenType::Star)) {
            throw std::runtime_error("Expected '*' for pointer assignment at line " + std::to_string(line));
        }
        AstNode ptrExpr = parseUnary();
        std::string op = "=";
        if (match(TokenType::Assign)) op = "=";
        else if (match(TokenType::PlusAssign)) op = "+=";
        else if (match(TokenType::MinusAssign)) op = "-=";
        else if (match(TokenType::StarAssign)) op = "*=";
        else if (match(TokenType::SlashAssign)) op = "/=";
        else if (match(TokenType::PercentAssign)) op = "%=";
        else if (match(TokenType::BitAndAssign)) op = "&=";
        else if (match(TokenType::BitOrAssign)) op = "|=";
        else if (match(TokenType::BitXorAssign)) op = "^=";
        else if (match(TokenType::ShlAssign)) op = "<<=";
        else if (match(TokenType::ShrAssign)) op = ">>=";
        else {
            throw std::runtime_error("Expected '=' after *ptr at line " + std::to_string(peek().line));
        }
        AstNode rhs = parseExpression();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' after pointer assignment at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::AssnDeref, op, {std::move(ptrExpr), std::move(rhs)}};
    }

    AstNode parseAssignment() {
        size_t line = peek().line;
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected variable name at line " + std::to_string(line));
        }
        advance();
        std::string name = nameTok.value;
        AstNode::Type assnType = AstNode::Type::Assignment;
        if (match(TokenType::Assign)) {
            assnType = AstNode::Type::Assignment;
        } else if (match(TokenType::PlusAssign)) {
            assnType = AstNode::Type::AssnAdd;
        } else if (match(TokenType::MinusAssign)) {
            assnType = AstNode::Type::AssnSub;
        } else if (match(TokenType::StarAssign)) {
            assnType = AstNode::Type::AssnMul;
        } else if (match(TokenType::SlashAssign)) {
            assnType = AstNode::Type::AssnDiv;
        } else if (match(TokenType::PercentAssign)) {
            assnType = AstNode::Type::AssnMod;
        } else if (match(TokenType::BitAndAssign)) {
            assnType = AstNode::Type::AssnBitAnd;
        } else if (match(TokenType::BitOrAssign)) {
            assnType = AstNode::Type::AssnBitOr;
        } else if (match(TokenType::BitXorAssign)) {
            assnType = AstNode::Type::AssnBitXor;
        } else if (match(TokenType::ShlAssign)) {
            assnType = AstNode::Type::AssnShl;
        } else if (match(TokenType::ShrAssign)) {
            assnType = AstNode::Type::AssnShr;
        } else {
            throw std::runtime_error("Expected '=', compound assignment (e.g. '+=', '&=', '<<=') at line " + std::to_string(peek().line));
        }
        AstNode expr = parseExpression();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        AstNode node{assnType, name, {}};
        node.children.push_back(expr);
        return node;
    }

    // Relational level: < <= > >=, left-associative, chains freely.
    AstNode parseRelational() {
        AstNode left = parseExpression();
        while (true) {
            if (match(TokenType::Less)) {
                left = AstNode{AstNode::Type::CondLt, "", {std::move(left), parseExpression()}};
            } else if (match(TokenType::LessEq)) {
                left = AstNode{AstNode::Type::CondLe, "", {std::move(left), parseExpression()}};
            } else if (match(TokenType::Greater)) {
                left = AstNode{AstNode::Type::CondGt, "", {std::move(left), parseExpression()}};
            } else if (match(TokenType::GreaterEq)) {
                left = AstNode{AstNode::Type::CondGe, "", {std::move(left), parseExpression()}};
            } else {
                break;
            }
        }
        return left;
    }

    // Equality level: == !=, looser than relational, so a > b == c < d
    // parses as (a > b) == (c < d).
    AstNode parseCondition() {
        AstNode left = parseRelational();
        while (true) {
            if (match(TokenType::Equals)) {
                left = AstNode{AstNode::Type::CondEq, "", {std::move(left), parseRelational()}};
            } else if (match(TokenType::NotEquals)) {
                left = AstNode{AstNode::Type::CondNe, "", {std::move(left), parseRelational()}};
            } else {
                break;
            }
        }
        return left;
    }

    AstNode parseLogicalOr() {
        AstNode left = parseLogicalAnd();
        while (match(TokenType::Or)) {
            AstNode n{AstNode::Type::CondOr, "", {left, parseLogicalAnd()}};
            left = n;
        }
        return left;
    }

    AstNode parseTernary() {
        AstNode cond = parseLogicalOr();
        if (!match(TokenType::Question)) return cond;
        AstNode thenExpr = parseTernary();
        if (!match(TokenType::Colon)) {
            throw std::runtime_error("Expected ':' in ternary expression at line " + std::to_string(peek().line));
        }
        AstNode elseExpr = parseTernary();
        return {AstNode::Type::ExprTernary, "", {std::move(cond), std::move(thenExpr), std::move(elseExpr)}};
    }

    AstNode parseLogicalAnd() {
        AstNode left = parseLogicalNot();
        while (match(TokenType::And)) {
            AstNode n{AstNode::Type::CondAnd, "", {left, parseLogicalNot()}};
            left = n;
        }
        return left;
    }

    AstNode parseLogicalNot() {
        if (match(TokenType::Not)) {
            AstNode inner = parseLogicalNot();
            return {AstNode::Type::CondNot, "", {inner}};
        }
        // '(' is handled deeper down in parsePrimary so that arithmetic operators
        // following a parenthesized sub-expression continue to work
        // (e.g. (a + b) / 2 inside a let initializer).
        return parseCondition();
    }

    AstNode parseIf() {
        size_t line = peek().line;
        if (!match(TokenType::If)) {
            throw std::runtime_error("Expected 'if' at line " + std::to_string(line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode cond = parseTernary();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        AstNode thenBlock{AstNode::Type::Block, "", {}};
        AstNode ifNode{AstNode::Type::IfElse, "", {cond, thenBlock}};
        ifNode.children[1] = parseBody(&ifNode);
        if (match(TokenType::Else)) {
            if (peek().type == TokenType::If) {
                AstNode elseIfPart = parseIf();
                ifNode.children.push_back(elseIfPart);
            } else {
                ifNode.children.push_back(parseBody());
            }
        }
        return ifNode;
    }

    AstNode parseSwitch() {
        size_t line = peek().line;
        if (!match(TokenType::Switch)) {
            throw std::runtime_error("Expected 'switch' at line " + std::to_string(line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after switch at line " + std::to_string(peek().line));
        }
        AstNode expr = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' at line " + std::to_string(peek().line));
        }
        AstNode switchNode{AstNode::Type::Switch, "", {expr}};
        bool hasDefault = false;
        bool hasIntCase = false;
        bool hasStringCase = false;
        bool hasEnumCase = false;
        while (pos_ < tokens_.size() && peek().type != TokenType::RBrace) {
            if (match(TokenType::Case)) {
                const Token& valTok = peek();
                AstNode caseNode{AstNode::Type::SwitchCase, "", {}};
                if (valTok.type == TokenType::Number) {
                    caseNode.value = valTok.value;
                    hasIntCase = true;
                    advance();
                } else if (valTok.type == TokenType::String) {
                    caseNode.initValue = valTok.value;
                    caseNode.caseIsString = true;
                    hasStringCase = true;
                    advance();
                } else if (valTok.type == TokenType::Identifier) {
                    AstNode fac = parseFactor();
                    if (fac.type != AstNode::Type::ExprMember || fac.children.empty() ||
                        fac.children[0].type != AstNode::Type::ExprVarRef) {
                        throw std::runtime_error("case value must be an integer, string literal, or Enum.variant at line " + std::to_string(valTok.line));
                    }
                    caseNode.caseIsEnum = true;
                    caseNode.value = fac.children[0].value;
                    caseNode.initValue = fac.value;
                    hasEnumCase = true;
                } else if (valTok.type == TokenType::Default) {
                    if (hasDefault) {
                        throw std::runtime_error("Duplicate default at line " + std::to_string(valTok.line));
                    }
                    hasDefault = true;
                    advance();
                    if (!match(TokenType::Colon)) {
                        throw std::runtime_error("Expected ':' after case default at line " + std::to_string(peek().line));
                    }
                    AstNode defaultNode{AstNode::Type::SwitchCase, "default", {}};
                    defaultNode.children = parseSwitchBody();
                    switchNode.children.push_back(defaultNode);
                    continue;
                } else {
                    throw std::runtime_error("case value must be an integer or string literal at line " + std::to_string(valTok.line));
                }
                if (!match(TokenType::Colon)) {
                    throw std::runtime_error("Expected ':' after case value at line " + std::to_string(peek().line));
                }
                caseNode.children = parseSwitchBody();
                switchNode.children.push_back(caseNode);
            } else if (match(TokenType::Default)) {
                if (hasDefault) {
                    throw std::runtime_error("Duplicate default at line " + std::to_string(peek().line));
                }
                hasDefault = true;
                if (!match(TokenType::Colon)) {
                    throw std::runtime_error("Expected ':' after default at line " + std::to_string(peek().line));
                }
                AstNode defaultNode{AstNode::Type::SwitchCase, "default", {}};
                defaultNode.children = parseSwitchBody();
                switchNode.children.push_back(defaultNode);
            } else {
                throw std::runtime_error("Expected 'case' or 'default' at line " + std::to_string(peek().line));
            }
        }
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' at line " + std::to_string(peek().line));
        }
        if (hasIntCase && hasStringCase) {
            throw std::runtime_error("switch cannot mix int and string cases at line " + std::to_string(line));
        }
        if (hasEnumCase && (hasIntCase || hasStringCase)) {
            throw std::runtime_error("switch cannot mix enum cases with int or string cases at line " + std::to_string(line));
        }
        return switchNode;
    }

    std::vector<AstNode> parseSwitchBody() {
        std::vector<AstNode> stmts;
        while (pos_ < tokens_.size()) {
            const Token& t = peek();
            if (t.type == TokenType::RBrace || t.type == TokenType::Case || t.type == TokenType::Default) break;
            const size_t before = stmts.size();
            const size_t stmtLine = t.line;
            if (t.type == TokenType::Let) {
                stmts.push_back(parseVariable());
            } else if (t.type == TokenType::If) {
                stmts.push_back(parseIf());
            } else if (t.type == TokenType::Else) {
                AstNode* attachTo = nullptr;
                if (!stmts.empty() && stmts.back().type == AstNode::Type::IfElse) {
                    attachTo = &stmts.back();
                }
                if (!attachTo) {
                    throw std::runtime_error("else without matching if at line " + std::to_string(t.line));
                }
                advance();
                if (peek().type == TokenType::If) {
                    AstNode elseIfPart = parseIf();
                    attachTo->children.push_back(elseIfPart);
                } else {
                    if (!match(TokenType::LBrace)) {
                        throw std::runtime_error("Expected '{' after else at line " + std::to_string(peek().line));
                    }
                    AstNode elseBlock{AstNode::Type::Block, "", {}};
                    elseBlock.children = parseBlock();
                    attachTo->children.push_back(elseBlock);
                    if (!match(TokenType::RBrace)) {
                        throw std::runtime_error("Expected '}' at line " + std::to_string(peek().line));
                    }
                }
            } else if (t.type == TokenType::Switch) {
                stmts.push_back(parseSwitch());
            } else if (t.type == TokenType::While) {
                stmts.push_back(parseWhile());
            } else if (t.type == TokenType::For) {
                stmts.push_back(parseFor());
            } else if (t.type == TokenType::Try) {
                stmts.push_back(parseTryCatch());
            } else if (t.type == TokenType::Throw) {
                stmts.push_back(parseThrowStmt());
            } else if (t.type == TokenType::InlineCppBlock) {
                if (!modules_.hasInlineCpp()) {
                    throw std::runtime_error("inline_cpp! requires #include <std/inline> at line " + std::to_string(t.line));
                }
                const Token& tok = peek();
                std::string body = tok.value;
                advance();
                AstNode node{AstNode::Type::InlineCpp, "", {}};
                node.value = std::move(body);
                stmts.push_back(std::move(node));
                if (peek().type == TokenType::Semicolon) advance();
            } else if (t.type == TokenType::Identifier && t.value == "io") {
                stmts.push_back(parseIoCall());
            } else if (t.type == TokenType::Identifier && t.value == "os") {
                stmts.push_back(parseOsCall());
            } else if (t.type == TokenType::Identifier && t.value == "dll") {
                stmts.push_back(parseDllCall());
            } else if (t.type == TokenType::Identifier && t.value == "file") {
                stmts.push_back(parseFileCall());
            } else if (t.type == TokenType::Identifier && t.value == "thread") {
                stmts.push_back(parseThreadCall());
            } else if (t.type == TokenType::Identifier && t.value == "time") {
                stmts.push_back(parseTimeCall());
            } else if (t.type == TokenType::Identifier && t.value == "random") {
                stmts.push_back(parseRandomCall());
            } else if (t.type == TokenType::Identifier && t.value == "gfx") {
                stmts.push_back(parseGfxCall(true));
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       (tokens_[pos_ + 1].type == TokenType::Dot || tokens_[pos_ + 1].type == TokenType::Arrow)) {
                const Token& id2 = tokens_[pos_ + 2];
                bool isPathFile = tokens_[pos_ + 1].type == TokenType::Dot &&
                    id2.type == TokenType::Identifier && (id2.value == "Write" || id2.value == "Append") &&
                    pos_ + 3 < tokens_.size() && tokens_[pos_ + 3].type == TokenType::LParen;
                if (isPathFile) {
                    stmts.push_back(parsePathVarFileCall());
                } else if (looksLikeDotMethodCallStmt()) {
                    stmts.push_back(parseExprStatement());
                } else {
                    stmts.push_back(parseMemberAssignment());
                }
            } else if (t.type == TokenType::Identifier && (t.value == "getprocessid" || t.value == "getpid") &&
                       pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LParen) {
                stmts.push_back(parseOsGetProcessIdBareStmt());
            } else if (t.type == TokenType::Identifier && t.value == "ui") {
                throw std::runtime_error("std/ui has been removed at line " + std::to_string(t.line));
            } else if (t.type == TokenType::Star) {
                stmts.push_back(parseDerefAssignment());
            } else if (t.type == TokenType::Delete) {
                stmts.push_back(parseDeleteStmt());
            } else if (t.type == TokenType::LParen || t.type == TokenType::Fn) {
                stmts.push_back(parseExprStatement());
            } else if (looksLikeStructLiteral()) {
                stmts.push_back(parseExprStatement());
            } else if (looksLikeQualifiedFnCall() ||
                       (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                        tokens_[pos_ + 1].type == TokenType::LParen)) {
                stmts.push_back(parseFnCall());
            } else if (t.type == TokenType::Goto) {
                stmts.push_back(parseGoto());
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       tokens_[pos_ + 1].type == TokenType::Colon) {
                stmts.push_back(parseLabel());
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size()) {
                TokenType next = tokens_[pos_ + 1].type;
                if (next == TokenType::LBracket) {
                    if (looksLikeIndexedDotCallStmt()) {
                        stmts.push_back(parseExprStatement());
                    } else {
                        stmts.push_back(parseIndexedAssignment());
                    }
                } else if (next == TokenType::Assign || next == TokenType::PlusAssign || next == TokenType::MinusAssign ||
                    next == TokenType::StarAssign || next == TokenType::SlashAssign || next == TokenType::PercentAssign ||
                    next == TokenType::BitAndAssign || next == TokenType::BitOrAssign || next == TokenType::BitXorAssign ||
                    next == TokenType::ShlAssign || next == TokenType::ShrAssign) {
                    stmts.push_back(parseAssignment());
                } else if (next == TokenType::PlusPlus || next == TokenType::MinusMinus) {
                    stmts.push_back(parseIncDec());
                } else {
                    throw std::runtime_error("Unexpected token at line " + std::to_string(t.line));
                }
            } else if (t.type == TokenType::Return) {
                stmts.push_back(parseReturn());
            } else if (t.type == TokenType::Break) {
                stmts.push_back(parseBreak());
            } else if (t.type == TokenType::Continue) {
                stmts.push_back(parseContinue());
            } else if (t.type != TokenType::Eof) {
                throw std::runtime_error("Unexpected token at line " + std::to_string(t.line));
            } else {
                break;
            }
            // Same as parseBlock: a statement in a case body has to carry its own origin, or a
            // later diagnostic about it falls back to the enclosing switch's line.
            for (size_t i = before; i < stmts.size(); ++i) stampSourceLoc(stmts[i], stmtLine);
        }
        return stmts;
    }

    AstNode parseWhile() {
        size_t line = peek().line;
        if (!match(TokenType::While)) {
            throw std::runtime_error("Expected 'while' at line " + std::to_string(line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode cond = parseTernary();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        AstNode block = parseBody();
        return {AstNode::Type::While, "", {cond, block}};
    }

    AstNode parseFor() {
        size_t line = peek().line;
        if (!match(TokenType::For)) {
            throw std::runtime_error("Expected 'for' at line " + std::to_string(line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after for at line " + std::to_string(peek().line));
        }
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected loop variable name at line " + std::to_string(nameTok.line));
        }
        advance();
        std::string varName = nameTok.value;
        // for (x in coll) { ... }
        if (peek().type == TokenType::Identifier && peek().value == "in") {
            advance();
            AstNode collExpr = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after for (... in ...) at line " + std::to_string(peek().line));
            }
            AstNode block = parseBody();
            return {AstNode::Type::ForIn, varName, {collExpr, block}};
        }
        // for (i, n) { ... }  or  for (k, v in m) { ... }
        if (!match(TokenType::Comma)) {
            throw std::runtime_error("Expected ',' or 'in' in for loop at line " + std::to_string(peek().line));
        }
        if (peek().type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Identifier && tokens_[pos_ + 1].value == "in") {
            std::string valueName = peek().value;
            advance();
            advance(); // 'in'
            AstNode collExpr = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after for (... in ...) at line " + std::to_string(peek().line));
            }
            AstNode block = parseBody();
            AstNode node{AstNode::Type::ForIn, varName, {collExpr, block}};
            node.initValue = valueName;
            return node;
        }
        AstNode countExpr = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after for loop count at line " + std::to_string(peek().line));
        }
        AstNode block = parseBody();
        AstNode forNode{AstNode::Type::For, varName, {countExpr, block}};
        return forNode;
    }

    AstNode parseIndexOrSliceOn(AstNode base) {
        size_t line = peek().line;
        if (peek().type == TokenType::Colon) {
            advance();
            AstNode node{AstNode::Type::ExprSlice, "", {}};
            node.children.push_back(std::move(base));
            if (peek().type == TokenType::RBracket) {
                node.initValue = "all";
            } else {
                node.initValue = "end";
                node.children.push_back(parseExpression());
            }
            if (!match(TokenType::RBracket)) {
                throw std::runtime_error("Expected ']' after slice at line " + std::to_string(peek().line));
            }
            return node;
        }
        AstNode start = parseExpression();
        if (match(TokenType::Colon)) {
            AstNode node{AstNode::Type::ExprSlice, "", {}};
            node.children.push_back(std::move(base));
            node.children.push_back(std::move(start));
            if (peek().type == TokenType::RBracket) {
                node.initValue = "start";
            } else {
                node.initValue = "both";
                node.children.push_back(parseExpression());
            }
            if (!match(TokenType::RBracket)) {
                throw std::runtime_error("Expected ']' after slice at line " + std::to_string(peek().line));
            }
            return node;
        }
        if (!match(TokenType::RBracket)) {
            throw std::runtime_error("Expected ']' at line " + std::to_string(line));
        }
        AstNode indexed{AstNode::Type::ExprArrayIndex, "", {}};
        indexed.children.push_back(std::move(base));
        indexed.children.push_back(std::move(start));
        return indexed;
    }

    AstNode applyIndexAndDotPostfix(AstNode cur) {
        for (;;) {
            if (match(TokenType::LBracket)) {
                cur = parseIndexOrSliceOn(std::move(cur));
                continue;
            }
            if (peek().type == TokenType::Dot || peek().type == TokenType::Arrow) {
                cur = parseDotChain(std::move(cur));
                continue;
            }
            break;
        }
        return cur;
    }

    // Parses a chain of `.field` / `->field` accesses and `.method(args)` calls on a base expression.
    AstNode parseDotChain(AstNode cur) {
        while (peek().type == TokenType::Dot || peek().type == TokenType::Arrow) {
            bool arrow = peek().type == TokenType::Arrow;
            advance();
            const Token& ftok = peek();
            if (ftok.type != TokenType::Identifier) {
                throw std::runtime_error(std::string("Expected field name after '") + (arrow ? "->" : ".") +
                    "' at line " + std::to_string(ftok.line));
            }
            advance();
            // value.method(args): core string methods — only with '.', not '->'
            if (!arrow && peek().type == TokenType::LParen) {
                std::string m = ftok.value;
                // method name -> argument count
                static const std::map<std::string, int> strMethods = {
                    {"upper", 0}, {"lower", 0}, {"trim", 0}, {"len", 0},
                    {"contains", 1}, {"starts_with", 1}, {"ends_with", 1},
                    {"index_of", 1}, {"repeat", 1}, {"split", 1},
                    {"replace", 2}, {"substring", 2}
                };
                auto mit = strMethods.find(m);
                if (mit != strMethods.end()) {
                    advance();  // consume '('
                    AstNode call{AstNode::Type::StrMethod, m, {std::move(cur)}};
                    for (int i = 0; i < mit->second; i++) {
                        if (i > 0 && !match(TokenType::Comma)) {
                            throw std::runtime_error("Expected ',' in ." + m + "(...) at line " + std::to_string(peek().line));
                        }
                        call.children.push_back(parseExpression());
                    }
                    if (!match(TokenType::RParen)) {
                        throw std::runtime_error("Expected ')' after ." + m + "(...) at line " + std::to_string(peek().line));
                    }
                    cur = std::move(call);
                    continue;
                }
            }
            if (peek().type == TokenType::LParen) {
                if (arrow) {
                    throw std::runtime_error("Methods use '.' not '->' at line " + std::to_string(ftok.line));
                }
                advance();
                AstNode call{AstNode::Type::FnCall, ftok.value, {}};
                call.initValue = ".";
                call.children.push_back(std::move(cur));
                if (peek().type != TokenType::RParen) {
                    for (;;) {
                        call.children.push_back(parseExpression());
                        if (!match(TokenType::Comma)) break;
                    }
                }
                if (!match(TokenType::RParen)) {
                    throw std::runtime_error("Expected ')' after ." + ftok.value + "(...) at line " + std::to_string(peek().line));
                }
                cur = std::move(call);
                continue;
            }
            AstNode mem{AstNode::Type::ExprMember, ftok.value, {std::move(cur)}};
            mem.isArrowMember = arrow;
            cur = std::move(mem);
        }
        return cur;
    }

    // Arithmetic/bitwise level only. parseCondition() layers comparisons on top of this,
    // so this must NOT reach parseTernary() or the grammar becomes left-recursive.
    AstNode parseExpression() {
        return parseBitOr();
    }

    // Full expression: arithmetic, comparisons, && / || / !, and ?:. Use this wherever a
    // value is expected (call arguments, return values, array elements) so that
    // `f(a == b)`, `return a > b;` and `io.println(c ? 1 : 2)` parse as documented.
    AstNode parseValueExpr() {
        return parseTernary();
    }

    AstNode parseBitOr() {
        AstNode left = parseBitXor();
        while (match(TokenType::BitOr)) {
            AstNode n{AstNode::Type::ExprBitOr, "", {left, parseBitXor()}};
            left = n;
        }
        return left;
    }

    AstNode parseBitXor() {
        AstNode left = parseBitAnd();
        while (match(TokenType::BitXor)) {
            AstNode n{AstNode::Type::ExprBitXor, "", {left, parseBitAnd()}};
            left = n;
        }
        return left;
    }

    AstNode parseBitAnd() {
        AstNode left = parseShift();
        while (match(TokenType::BitAnd)) {
            AstNode n{AstNode::Type::ExprBitAnd, "", {left, parseShift()}};
            left = n;
        }
        return left;
    }

    AstNode parseShift() {
        AstNode left = parseAdditive();
        while (true) {
            if (match(TokenType::Shl)) {
                AstNode n{AstNode::Type::ExprShl, "", {left, parseAdditive()}};
                left = n;
            } else if (match(TokenType::Shr)) {
                AstNode n{AstNode::Type::ExprShr, "", {left, parseAdditive()}};
                left = n;
            } else {
                break;
            }
        }
        return left;
    }

    AstNode parseAdditive() {
        AstNode left = parseTerm();
        while (true) {
            if (match(TokenType::Plus)) {
                AstNode n{AstNode::Type::ExprAdd, "", {left, parseTerm()}};
                left = n;
            } else if (match(TokenType::Minus)) {
                AstNode n{AstNode::Type::ExprSub, "", {left, parseTerm()}};
                left = n;
            } else {
                break;
            }
        }
        return left;
    }

    AstNode parseTerm() {
        AstNode left = parseUnary();
        while (true) {
            if (match(TokenType::Star)) {
                AstNode n{AstNode::Type::ExprMul, "", {left, parseUnary()}};
                left = n;
            } else if (match(TokenType::Slash)) {
                AstNode n{AstNode::Type::ExprDiv, "", {left, parseUnary()}};
                left = n;
            } else if (match(TokenType::Percent)) {
                AstNode n{AstNode::Type::ExprMod, "", {left, parseUnary()}};
                left = n;
            } else {
                break;
            }
        }
        return left;
    }

    AstNode parseUnary() {
        NestGuard guard(*this, peek().line);
        // C-style cast: (int)x, (float)y, (string)z, ...
        if (peek().type == TokenType::LParen && pos_ + 2 < tokens_.size()) {
            const Token& t1 = tokens_[pos_ + 1];
            const Token& t2 = tokens_[pos_ + 2];
            if (t1.type == TokenType::Identifier && t2.type == TokenType::RParen) {
                const std::string& tn = t1.value;
                if (tn == "int" || tn == "short" || tn == "long" || tn == "size_t" ||
                    tn == "float" || tn == "char" || tn == "bool" || tn == "string") {
                    advance();  // (
                    advance();  // type
                    advance();  // )
                    AstNode inner = parseUnary();
                    return {AstNode::Type::ExprCast, tn, {std::move(inner)}};
                }
            }
            // (unsigned char|int|short|long)x
            if (t1.type == TokenType::Identifier && t1.value == "unsigned" &&
                pos_ + 3 < tokens_.size() &&
                tokens_[pos_ + 2].type == TokenType::Identifier &&
                (tokens_[pos_ + 2].value == "char" || tokens_[pos_ + 2].value == "int" ||
                 tokens_[pos_ + 2].value == "short" || tokens_[pos_ + 2].value == "long") &&
                tokens_[pos_ + 3].type == TokenType::RParen) {
                advance();  // (
                advance();  // unsigned
                std::string ut = "unsigned " + tokens_[pos_].value;
                advance();  // char|int|short|long
                advance();  // )
                AstNode inner = parseUnary();
                return {AstNode::Type::ExprCast, ut, {std::move(inner)}};
            }
        }
        if (match(TokenType::Minus)) {
            AstNode inner = parseUnary();
            AstNode zero{AstNode::Type::ExprIntLiteral, "0", {}};
            return {AstNode::Type::ExprSub, "", {std::move(zero), std::move(inner)}};
        }
        if (match(TokenType::Not)) {
            AstNode inner = parseUnary();
            return {AstNode::Type::CondNot, "", {std::move(inner)}};
        }
        if (match(TokenType::BitNot)) {
            return {AstNode::Type::ExprBitNot, "", {parseUnary()}};
        }
        // *p  (dereference) — must be before parseFactor; distinct from binary *
        if (match(TokenType::Star)) {
            return {AstNode::Type::ExprDeref, "", {parseUnary()}};
        }
        // &x  (address-of) — distinct from binary &
        if (match(TokenType::BitAnd)) {
            return {AstNode::Type::ExprAddrOf, "", {parseUnary()}};
        }
        if (peek().type == TokenType::New) {
            return parseNewExpr();
        }
        if (peek().type == TokenType::Sizeof) {
            return parseSizeof();
        }
        return parseFactor();
    }

    AstNode parseArrayLiteral() {
        if (!match(TokenType::LBracket)) {
            throw std::runtime_error("Expected '[' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::ExprArrayLiteral, "", {}};
        if (peek().type != TokenType::RBracket) {
            for (;;) {
                node.children.push_back(parseValueExpr());
                if (!match(TokenType::Comma)) break;
            }
        }
        if (!match(TokenType::RBracket)) {
            throw std::runtime_error("Expected ']' at line " + std::to_string(peek().line));
        }
        return node;
    }

    // Every primary expression flows through here, so this is the one place that has to
    // stamp expression nodes for later semantic diagnostics (undefined names, bad fields).
    AstNode parseFactor() {
        const size_t exprLine = peek().line;
        AstNode n = parseFactorInner();
        stampSourceLoc(n, exprLine);
        return n;
    }

    AstNode parseFactorInner() {
        if (peek().type == TokenType::Fn) {
            return parsePostfixCalls(parseLambda());
        }
        if (looksLikeStructLiteral()) {
            return parsePostfixCalls(parseStructLiteral());
        }
        if (peek().type == TokenType::LBracket) {
            return applyIndexAndDotPostfix(parseArrayLiteral());
        }
        if (match(TokenType::Number)) {
            return {AstNode::Type::ExprIntLiteral, tokens_[pos_ - 1].value, {}};
        }
        if (match(TokenType::Float)) {
            return {AstNode::Type::ExprFloatLiteral, tokens_[pos_ - 1].value, {}};
        }
        if (match(TokenType::Char)) {
            return {AstNode::Type::ExprCharLiteral, tokens_[pos_ - 1].value, {}};
        }
        if (match(TokenType::True)) {
            return {AstNode::Type::ExprBoolLiteral, "true", {}};
        }
        if (match(TokenType::False)) {
            return {AstNode::Type::ExprBoolLiteral, "false", {}};
        }
        if (peek().type == TokenType::Identifier && peek().value == "null") {
            advance();
            return {AstNode::Type::ExprNull, "null", {}};
        }
        if (match(TokenType::String)) {
            return applyIndexAndDotPostfix({AstNode::Type::ExprStringLiteral, tokens_[pos_ - 1].value, {}});
        }
        if (peek().type == TokenType::Identifier && peek().value == "io" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            std::string method = tokens_[pos_ + 2].value;
            if (method == "readln") {
                AstNode n = parseIoReadlnExpr();
                if (peek().type == TokenType::Dot || peek().type == TokenType::Arrow) {
                    return parseDotChain(std::move(n));
                }
                return n;
            }
            if (method == "read_int") return parseIoReadIntExpr();
            if (method == "getline") return parseIoGetlineExpr();
            if (method == "to_int") return parseIoToIntExpr();
            if (method == "trim") return parseIoTrimExpr();
        }
        if (peek().type == TokenType::Identifier && peek().value == "os" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            std::string method = tokens_[pos_ + 2].value;
            if (method == "getenv") return parseOsGetenv();
            if (method == "system") return parseOsExec();
            if (method == "platform") return parseOsPlatform();
            if (method == "exe_dir") return parseOsExeDir();
            if (method == "hostname") return parseOsHostname();
            if (method == "username" || method == "user") return parseOsUsername();
            if (method == "home") return parseOsHome();
            if (method == "getprocessid" || method == "getpid" || method == "GetProcessID") return parseOsGetProcessId();
            if (method == "grepkeys" || method == "getkey") return parseOsGrepKeys();
            if (method == "keypressed") return parseOsKeyPressed();
            if (method == "get_volume") return parseOsGetVolume();
            if (method == "get_brightness") return parseOsGetBrightness();
            if (method == "clip_get") return parseOsClipGet();
            // Any other os.* expression (load, play, spawn, cwd, ...) goes through parseOsCall.
            // Do not fall through to identifier + parseDotChain — that treats os.load as a string method.
            return parseOsCall(false);
        }
        if (peek().type == TokenType::Identifier && (peek().value == "getprocessid" || peek().value == "getpid") &&
            pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LParen) {
            return parseOsGetProcessIdBareExpr();
        }
        if (peek().type == TokenType::Identifier && peek().value == "file" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            return parseFileExpr();
        }
        if (peek().type == TokenType::Identifier && peek().value == "random" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier &&
            tokens_[pos_ + 2].value == "int") {
            return parseRandomInt();
        }
        if (peek().type == TokenType::Identifier && peek().value == "math" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            return parseMathCall();
        }
        if (peek().type == TokenType::Identifier && peek().value == "crypto" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            return parseCryptoCall();
        }
        if (peek().type == TokenType::Identifier && peek().value == "gfx" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            return parseGfxCall(false);
        }
        if (peek().type == TokenType::Identifier && peek().value == "http" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            return applyIndexAndDotPostfix(parseHttpCall());
        }
        if (peek().type == TokenType::Identifier && peek().value == "json" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            return applyIndexAndDotPostfix(parseJsonCall());
        }
        if (peek().type == TokenType::Identifier && peek().value == "time" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            std::string method = tokens_[pos_ + 2].value;
            if (method == "seconds") return parseTimeSeconds();
            if (method == "milliseconds") return parseTimeMilliseconds();
            if (method == "now_ms") return parseTimeNowMs();
        }
        if (peek().type == TokenType::Identifier && peek().value == "thread" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            std::string method = tokens_[pos_ + 2].value;
            if (method == "worker") return parseThreadWorkerExpr();
            if (method == "spawn") return parseThreadSpawnExpr();
        }
        if (peek().type == TokenType::Identifier && peek().value == "ui") {
            throw std::runtime_error("std/ui has been removed at line " + std::to_string(peek().line));
        }
        if (peek().type == TokenType::Identifier &&
            (peek().value == "ok" || peek().value == "err") &&
            pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LParen) {
            return applyIndexAndDotPostfix(parseResultMake());
        }
        if (peek().type == TokenType::Identifier && peek().value == "len" && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::LParen) {
            return parseLenExpr();
        }
        if (peek().type == TokenType::Identifier && peek().value == "trim" && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::LParen) {
            return parseTrimExpr();
        }
        if (looksLikeQualifiedFnCall() ||
            (peek().type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
             tokens_[pos_ + 1].type == TokenType::LParen)) {
            return applyIndexAndDotPostfix(parseFnCallExpr());
        }
        if (match(TokenType::Identifier)) {
            std::string name = tokens_[pos_ - 1].value;
            return applyIndexAndDotPostfix({AstNode::Type::ExprVarRef, name, {}});
        }
        if (match(TokenType::LParen)) {
            // Use the full top-level (parseTernary) so parenthesized boolean expressions
            // such as (a && b) and (flag ? "a" : "b") parse in arithmetic contexts.
            AstNode e = parseTernary();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
            }
            return parsePostfixCalls(std::move(e));
        }
        throw std::runtime_error("Expected number, variable, or (expression) at line " + std::to_string(peek().line));
    }

    AstNode parseIoCall() {
        size_t line = peek().line;
        if (!modules_.hasIo()) {
            throw std::runtime_error("io.* requires #include <std/io> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "io") {
            throw std::runtime_error("Expected 'io' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected io method (print, println, flush, ...) at line " +
                                     std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        advance();
        if (method == "flush") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after io.flush at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after io.flush() at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
            }
            return {AstNode::Type::IoFlush, "", {}};
        }
        bool isPrintln = (method == "println");
        if (method != "print" && !isPrintln) {
            throw std::runtime_error("Expected 'print', 'println', or 'flush' at line " +
                                     std::to_string(methodTok.line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        const Token& argTok = peek();
        AstNode result{isPrintln ? AstNode::Type::IoPrintln : AstNode::Type::IoPrint, "", {}};
        if (argTok.type == TokenType::String || argTok.type == TokenType::Number ||
            argTok.type == TokenType::Identifier || argTok.type == TokenType::LParen ||
            argTok.type == TokenType::True || argTok.type == TokenType::False ||
            argTok.type == TokenType::Float || argTok.type == TokenType::Char ||
            argTok.type == TokenType::Star || argTok.type == TokenType::BitAnd ||
            argTok.type == TokenType::Minus || argTok.type == TokenType::Not ||
            argTok.type == TokenType::BitNot || argTok.type == TokenType::New ||
            argTok.type == TokenType::Sizeof) {
            result.children.push_back(parseValueExpr());
            while (match(TokenType::Comma)) {
                result.children.push_back(parseValueExpr());
            }
        } else {
            throw std::runtime_error("Expected string or expression at line " + std::to_string(argTok.line));
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return result;
    }

    void finishOsCall(bool requireSemicolon, size_t line) {
        if (requireSemicolon && !match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(line));
        }
    }

    AstNode parseOsCall(bool requireSemicolon = true) {
        size_t line = peek().line;
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.* requires #include <std/os> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected os method at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        advance();
        if (method == "hideconsolewindow" || method == "showconsolewindow" ||
            method == "minimizeconsolewindow" || method == "minimiseconsolewindow" ||
            method == "maximizeconsolewindow" || method == "maximiseconsolewindow") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os." + method + " at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            AstNode::Type nodeType = AstNode::Type::OsHideConsoleWindow;
            if (method == "showconsolewindow") nodeType = AstNode::Type::OsShowConsoleWindow;
            else if (method == "minimizeconsolewindow" || method == "minimiseconsolewindow") nodeType = AstNode::Type::OsMinimizeConsoleWindow;
            else if (method == "maximizeconsolewindow" || method == "maximiseconsolewindow") nodeType = AstNode::Type::OsMaximizeConsoleWindow;
            return {nodeType, "", {}};
        }
        if (method == "lock" || method == "shutdown" || method == "reboot" ||
            method == "suspend" || method == "logout" ||
            method == "mute" || method == "unmute" || method == "toggle_mute") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os." + method + " at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            AstNode::Type nodeType = AstNode::Type::OsLock;
            if (method == "shutdown") nodeType = AstNode::Type::OsShutdown;
            else if (method == "reboot") nodeType = AstNode::Type::OsReboot;
            else if (method == "suspend") nodeType = AstNode::Type::OsSuspend;
            else if (method == "logout") nodeType = AstNode::Type::OsLogout;
            else if (method == "mute") nodeType = AstNode::Type::OsMute;
            else if (method == "unmute") nodeType = AstNode::Type::OsUnmute;
            else if (method == "toggle_mute") nodeType = AstNode::Type::OsToggleMute;
            return {nodeType, "", {}};
        }
        if (method == "set_volume" || method == "set_brightness") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os." + method + " at line " + std::to_string(peek().line));
            }
            AstNode arg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os." + method + "(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            AstNode::Type nodeType = (method == "set_brightness") ? AstNode::Type::OsSetBrightness : AstNode::Type::OsSetVolume;
            return {nodeType, "", {arg}};
        }
        if (method == "get_volume" || method == "get_brightness") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os." + method + " at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            AstNode::Type nodeType = (method == "get_brightness") ? AstNode::Type::OsGetBrightness : AstNode::Type::OsGetVolume;
            return {nodeType, "", {}};
        }
        if (method == "clip_set") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os.clip_set at line " + std::to_string(peek().line));
            }
            AstNode arg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os.clip_set(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {AstNode::Type::OsClipSet, "", {arg}};
        }
        if (method == "type" || method == "type_text") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os." + method + " at line " + std::to_string(peek().line));
            }
            AstNode arg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os." + method + "(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {AstNode::Type::OsType, "", {arg}};
        }
        if (method == "clip_get") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os.clip_get at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {AstNode::Type::OsClipGet, "", {}};
        }
        if (method == "notify") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os.notify at line " + std::to_string(peek().line));
            }
            AstNode titleArg = parseExpression();
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in os.notify(title, message) at line " + std::to_string(peek().line));
            }
            AstNode msgArg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os.notify(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {AstNode::Type::OsNotify, "", {titleArg, msgArg}};
        }
        if (method == "open") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os.open at line " + std::to_string(peek().line));
            }
            AstNode arg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os.open(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {AstNode::Type::OsOpen, "", {arg}};
        }
        if (method == "save") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os.save at line " + std::to_string(peek().line));
            }
            AstNode pathArg = parseExpression();
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' after os.save path at line " + std::to_string(peek().line));
            }
            AstNode dataArg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os.save(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {AstNode::Type::OsSave, "", {pathArg, dataArg}};
        }
        if (method == "load" || method == "play") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os." + method + " at line " + std::to_string(peek().line));
            }
            AstNode arg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os." + method + "(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {method == "load" ? AstNode::Type::OsLoad : AstNode::Type::OsPlay, "", {arg}};
        }
        if (method == "spawn" || method == "spawn_wait" || method == "spawn_at") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os." + method + " at line " + std::to_string(peek().line));
            }
            std::string tag = "";
            if (method == "spawn_wait") tag = "wait";
            else if (method == "spawn_at") tag = "at";
            AstNode node{AstNode::Type::OsSpawn, tag, {}};
            node.children.push_back(parseExpression());
            while (match(TokenType::Comma)) {
                node.children.push_back(parseExpression());
            }
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os." + method + "(...) at line " + std::to_string(peek().line));
            }
            if (node.children.empty()) {
                throw std::runtime_error("os." + method + " requires a program at line " + std::to_string(methodTok.line));
            }
            if (method == "spawn_at" && node.children.size() < 2) {
                throw std::runtime_error("os.spawn_at(cwd, prog [, arg...]) requires a directory and program at line " + std::to_string(methodTok.line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return node;
        }
        if (method == "wait" || method == "kill") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os." + method + " at line " + std::to_string(peek().line));
            }
            AstNode arg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os." + method + "(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {method == "wait" ? AstNode::Type::OsWait : AstNode::Type::OsKill, "", {arg}};
        }
        if (method == "total_mem" || method == "avail_mem" || method == "page_size" ||
            method == "uptime" || method == "shell" || method == "newline" ||
            method == "path_sep" || method == "lang" || method == "isatty" ||
            method == "environ" || method == "env" || method == "config_dir" ||
            method == "cache_dir" || method == "desktop" || method == "endian") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os." + method + " at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            std::string tag = method;
            if (method == "env") tag = "environ";
            return {AstNode::Type::OsInfo, tag, {}};
        }
        if (method == "tempdir" || method == "tmpdir" || method == "arch" ||
            method == "cpu_count" || method == "nproc" ||
            method == "executable" || method == "exe" || method == "cwd") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os." + method + " at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            AstNode::Type nodeType = AstNode::Type::OsTempDir;
            if (method == "arch") nodeType = AstNode::Type::OsArch;
            else if (method == "cpu_count" || method == "nproc") nodeType = AstNode::Type::OsCpuCount;
            else if (method == "executable" || method == "exe") nodeType = AstNode::Type::OsExecutable;
            else if (method == "cwd") nodeType = AstNode::Type::OsCwd;
            return {nodeType, "", {}};
        }
        if (method == "which" || method == "unsetenv" || method == "chdir") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os." + method + " at line " + std::to_string(peek().line));
            }
            AstNode arg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os." + method + "(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            AstNode::Type nodeType = AstNode::Type::OsWhich;
            if (method == "unsetenv") nodeType = AstNode::Type::OsUnsetenv;
            else if (method == "chdir") nodeType = AstNode::Type::OsChdir;
            return {nodeType, "", {arg}};
        }
        if (method == "messagebox") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os.messagebox at line " + std::to_string(peek().line));
            }
            AstNode textArg = parseExpression();
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in os.messagebox(text, title) at line " + std::to_string(peek().line));
            }
            AstNode titleArg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            AstNode node{AstNode::Type::OsMessageBox, "", {textArg, titleArg}};
            return node;
        }
        if (method == "exit") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os.exit at line " + std::to_string(peek().line));
            }
            AstNode codeArg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os.exit(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {AstNode::Type::OsExit, "", {codeArg}};
        }
        if (method == "setenv") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os.setenv at line " + std::to_string(peek().line));
            }
            AstNode nameArg = parseExpression();
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in os.setenv(name, value) at line " + std::to_string(peek().line));
            }
            AstNode valueArg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os.setenv(...) at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return {AstNode::Type::OsSetenv, "", {nameArg, valueArg}};
        }
        if (method == "hostname" || method == "username" || method == "user" || method == "home") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os." + method + " at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            if (method == "hostname") return {AstNode::Type::OsHostname, "", {}};
            if (method == "home") return {AstNode::Type::OsHome, "", {}};
            return {AstNode::Type::OsUsername, "", {}};
        }
        if (method == "getprocessid" || method == "getpid" || method == "GetProcessID") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after os." + method + " at line " + std::to_string(peek().line));
            }
            AstNode node{AstNode::Type::OsGetProcessId, "", {}};
            if (peek().type != TokenType::RParen) {
                node.children.push_back(parseExpression());
            }
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after os." + method + " at line " + std::to_string(peek().line));
            }
            finishOsCall(requireSemicolon, peek().line);
            return node;
        }
        if (method != "system") {
            throw std::runtime_error("Unknown os method '" + method + "' at line " + std::to_string(methodTok.line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        const Token& argTok = peek();
        AstNode result{AstNode::Type::OsSystem, "", {}};
        if (argTok.type == TokenType::String || argTok.type == TokenType::Identifier ||
            argTok.type == TokenType::LParen) {
            result.children.push_back(parseExpression());
            result.isVarRef = false;
        } else {
            throw std::runtime_error("Expected string or expression at line " + std::to_string(argTok.line));
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        finishOsCall(requireSemicolon, peek().line);
        return result;
    }

    AstNode parseOsSpawn() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.spawn requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "spawn") {
            throw std::runtime_error("Expected 'spawn' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after os.spawn at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::OsSpawn, "", {}};
        node.children.push_back(parseExpression());
        while (match(TokenType::Comma)) {
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after os.spawn(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseOsExec() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.system requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "system") {
            throw std::runtime_error("Expected 'system' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after os.system at line " + std::to_string(peek().line));
        }
        AstNode cmdArg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after os.system(...) at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsExec, "", {cmdArg}};
    }

    AstNode parseOsGetenv() {
        size_t line = peek().line;
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.getenv requires #include <std/os> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "getenv") {
            throw std::runtime_error("Expected 'getenv' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        const Token& argTok = peek();
        if (argTok.type != TokenType::String) {
            throw std::runtime_error("Expected string for env var name at line " + std::to_string(argTok.line));
        }
        advance();
        std::string envName = tokens_[pos_ - 1].value;
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsGetenv, envName, {}};
    }

    AstNode parseOsGetVolume() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.get_volume requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "get_volume") {
            throw std::runtime_error("Expected 'get_volume' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after get_volume at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsGetVolume, "", {}};
    }

    AstNode parseOsGetBrightness() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.get_brightness requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "get_brightness") {
            throw std::runtime_error("Expected 'get_brightness' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after get_brightness at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsGetBrightness, "", {}};
    }

    AstNode parseOsClipGet() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.clip_get requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "clip_get") {
            throw std::runtime_error("Expected 'clip_get' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after clip_get at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsClipGet, "", {}};
    }

    AstNode parseOsPlatform() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.platform requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "platform") {
            throw std::runtime_error("Expected 'platform' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after platform at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsPlatform, "", {}};
    }

    AstNode parseOsExeDir() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.exe_dir requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "exe_dir") {
            throw std::runtime_error("Expected 'exe_dir' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after exe_dir at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsExeDir, "", {}};
    }

    AstNode parseOsHostname() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.hostname requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot) || !match(TokenType::Identifier) || tokens_[pos_ - 1].value != "hostname") {
            throw std::runtime_error("Expected os.hostname at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after os.hostname at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsHostname, "", {}};
    }

    AstNode parseOsUsername() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.username requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot) || !match(TokenType::Identifier) ||
            (tokens_[pos_ - 1].value != "username" && tokens_[pos_ - 1].value != "user")) {
            throw std::runtime_error("Expected os.username at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after os.username at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsUsername, "", {}};
    }

    AstNode parseOsHome() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.home requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot) || !match(TokenType::Identifier) || tokens_[pos_ - 1].value != "home") {
            throw std::runtime_error("Expected os.home at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after os.home at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsHome, "", {}};
    }

    AstNode parseOsGetProcessId() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.getprocessid requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier)) {
            throw std::runtime_error("Expected 'getprocessid' at line " + std::to_string(peek().line));
        }
        std::string method = tokens_[pos_ - 1].value;
        if (method != "getprocessid" && method != "getpid" && method != "GetProcessID") {
            throw std::runtime_error("Expected 'getprocessid' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after os." + method + " at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::OsGetProcessId, "", {}};
        if (peek().type != TokenType::RParen) {
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after os." + method + " at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseOsGetProcessIdBareExpr() {
        size_t line = peek().line;
        if (!modules_.hasOs()) {
            throw std::runtime_error("getprocessid requires #include <std/os> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier)) {
            throw std::runtime_error("Expected 'getprocessid' at line " + std::to_string(peek().line));
        }
        std::string method = tokens_[pos_ - 1].value;
        if (method != "getprocessid" && method != "getpid") {
            throw std::runtime_error("Expected 'getprocessid' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after " + method + " at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::OsGetProcessId, "", {}};
        if (peek().type != TokenType::RParen) {
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after " + method + " at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseOsGetProcessIdBareStmt() {
        AstNode node = parseOsGetProcessIdBareExpr();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseOsGrepKeys() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.grepkeys requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier)) {
            throw std::runtime_error("Expected 'grepkeys' or 'getkey' at line " + std::to_string(peek().line));
        }
        std::string method = tokens_[pos_ - 1].value;
        if (method != "grepkeys" && method != "getkey") {
            throw std::runtime_error("Expected 'grepkeys' or 'getkey' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after os." + method + " at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsGrepKeys, "", {}};
    }

    AstNode parseOsKeyPressed() {
        if (!modules_.hasOs()) {
            throw std::runtime_error("os.keypressed requires #include <std/os> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "os") {
            throw std::runtime_error("Expected 'os' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "keypressed") {
            throw std::runtime_error("Expected 'keypressed' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after os.keypressed at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::OsKeyPressed, "", {}};
    }

    AstNode parseIoReadlnExpr() {
        size_t line = peek().line;
        if (!modules_.hasIo()) {
            throw std::runtime_error("io.readln requires #include <std/io> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "io") {
            throw std::runtime_error("Expected 'io' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "readln") {
            throw std::runtime_error("Expected io.readln at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after readln at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::IoReadln, "", {}};
    }

    // Same codegen as io.to_int(io.readln()) — one line without a temporary string binding.
    AstNode parseIoReadIntExpr() {
        size_t line = peek().line;
        if (!modules_.hasIo()) {
            throw std::runtime_error("io.read_int requires #include <std/io> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "io") {
            throw std::runtime_error("Expected 'io' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "read_int") {
            throw std::runtime_error("Expected io.read_int at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after read_int at line " + std::to_string(peek().line));
        }
        AstNode inner{AstNode::Type::IoReadln, "", {}};
        return {AstNode::Type::IoToInt, "", {std::move(inner)}};
    }

    AstNode parseIoGetlineExpr() {
        size_t line = peek().line;
        if (!modules_.hasIo()) {
            throw std::runtime_error("io.getline requires #include <std/io> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "io") {
            throw std::runtime_error("Expected 'io' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "getline") {
            throw std::runtime_error("Expected io.getline at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after getline at line " + std::to_string(peek().line));
        }
        AstNode sourceArg = parseExpression();
        AstNode node{AstNode::Type::IoGetline, "", {}};
        node.children.push_back(sourceArg);
        if (match(TokenType::Comma)) {
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after io.getline(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseIoToIntExpr() {
        size_t line = peek().line;
        if (!modules_.hasIo()) {
            throw std::runtime_error("io.to_int requires #include <std/io> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "io") {
            throw std::runtime_error("Expected 'io' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "to_int") {
            throw std::runtime_error("Expected io.to_int at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode arg;
        if (peek().type == TokenType::RParen) {
            arg = {AstNode::Type::ExprStringLiteral, "", {}};
        } else {
            arg = parseExpression();
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::IoToInt, "", {arg}};
    }

    AstNode parseIoTrimExpr() {
        size_t line = peek().line;
        if (!modules_.hasIo()) {
            throw std::runtime_error("io.trim requires #include <std/io> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "io") {
            throw std::runtime_error("Expected 'io' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "trim") {
            throw std::runtime_error("Expected io.trim at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after trim at line " + std::to_string(peek().line));
        }
        AstNode arg = parseExpression();
        AstNode node{AstNode::Type::ExprTrim, "", {arg}};
        if (match(TokenType::Comma)) {
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after io.trim(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    static std::string canonicalizeFileMethod(const std::string& method) {
        if (method == "delete") return "remove";
        if (method == "move") return "rename";
        if (method == "listdir") return "list";
        if (method == "name") return "basename";
        if (method == "parent") return "dirname";
        return method;
    }

    static bool isLegacyFileMethod(const std::string& method) {
        return method == "read" || method == "write" || method == "append" || method == "exists" || method == "mkdir";
    }

    static bool isExtendedFileMethod(const std::string& method) {
        static const std::set<std::string> m = {
            "remove", "remove_all", "rename", "copy", "list",
            "isdir", "isfile", "size", "cwd", "chdir",
            "abspath", "join", "dirname", "basename", "extension"
        };
        return m.count(method) > 0;
    }

    AstNode parseFileExpr() {
        size_t line = peek().line;
        if (!modules_.hasFile()) {
            throw std::runtime_error("file.* requires #include <std/file> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "file") {
            throw std::runtime_error("Expected 'file' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected file method at line " + std::to_string(methodTok.line));
        }
        std::string method = canonicalizeFileMethod(methodTok.value);
        advance();
        if (!isLegacyFileMethod(method) && !isExtendedFileMethod(method)) {
            throw std::runtime_error("Unknown file method 'file." + methodTok.value + "' at line " + std::to_string(methodTok.line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after file." + method + " at line " + std::to_string(peek().line));
        }
        AstNode node;
        if (method == "read") {
            node = {AstNode::Type::FileRead, "", {}};
        } else if (method == "write") {
            node = {AstNode::Type::FileWrite, "", {}};
        } else if (method == "append") {
            node = {AstNode::Type::FileAppend, "", {}};
        } else if (method == "exists") {
            node = {AstNode::Type::FileExists, "", {}};
        } else if (method == "mkdir") {
            node = {AstNode::Type::FileMkdir, "", {}};
        } else {
            node = {AstNode::Type::FileCall, method, {}};
        }
        if (method == "cwd") {
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after file.cwd() at line " + std::to_string(peek().line));
            }
            return node;
        }
        node.children.push_back(parseExpression());
        if (method == "write" || method == "append" ||
            method == "rename" || method == "copy" || method == "join") {
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in file." + method + "(a, b) at line " + std::to_string(peek().line));
            }
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after file." + method + "(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseFileCall() {
        size_t line = peek().line;
        if (!modules_.hasFile()) {
            throw std::runtime_error("file.* requires #include <std/file> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "file") {
            throw std::runtime_error("Expected 'file' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected file method at line " + std::to_string(methodTok.line));
        }
        std::string method = canonicalizeFileMethod(methodTok.value);
        if (!isLegacyFileMethod(method) && !isExtendedFileMethod(method)) {
            throw std::runtime_error("Unknown file method 'file." + methodTok.value + "' at line " + std::to_string(methodTok.line));
        }
        advance();
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode node;
        if (method == "read") {
            node = {AstNode::Type::FileRead, "", {}};
        } else if (method == "write") {
            node = {AstNode::Type::FileWrite, "", {}};
        } else if (method == "append") {
            node = {AstNode::Type::FileAppend, "", {}};
        } else if (method == "exists") {
            node = {AstNode::Type::FileExists, "", {}};
        } else if (method == "mkdir") {
            node = {AstNode::Type::FileMkdir, "", {}};
        } else {
            node = {AstNode::Type::FileCall, method, {}};
        }
        if (method != "cwd") {
            node.children.push_back(parseExpression());
            if (method == "write" || method == "append" || method == "rename" || method == "copy" || method == "join") {
                if (!match(TokenType::Comma)) {
                    throw std::runtime_error("Expected ',' in file." + method + "(...) at line " + std::to_string(peek().line));
                }
                node.children.push_back(parseExpression());
            }
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseRandomInt() {
        if (!modules_.hasRandom()) {
            throw std::runtime_error("random.int requires #include <std/random> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "random") {
            throw std::runtime_error("Expected 'random' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "int") {
            throw std::runtime_error("Expected random.int at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode minArg = parseExpression();
        if (!match(TokenType::Comma)) {
            throw std::runtime_error("Expected ',' in random.int(min, max) at line " + std::to_string(peek().line));
        }
        AstNode maxArg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::RandomInt, "", {minArg, maxArg}};
    }

    AstNode parseMathCall() {
        size_t line = peek().line;
        if (!modules_.hasMath()) {
            throw std::runtime_error("math.* requires #include <std/math> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "math") {
            throw std::runtime_error("Expected 'math' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected math function name at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        advance();
        AstNode node{AstNode::Type::MathCall, method, {}};
        // Constants: math.pi, math.e (no call parentheses)
        if (method == "pi" || method == "e") {
            return node;
        }
        // One-argument functions
        static const std::set<std::string> oneArg = {
            "abs", "sqrt", "floor", "ceil", "round",
            "sin", "cos", "tan", "log", "log10", "exp"
        };
        // Two-argument functions
        static const std::set<std::string> twoArg = {"pow", "min", "max"};
        if (oneArg.find(method) == oneArg.end() && twoArg.find(method) == twoArg.end()) {
            throw std::runtime_error("Unknown math function 'math." + method +
                "' at line " + std::to_string(methodTok.line) +
                " (use abs, min, max, pow, sqrt, floor, ceil, round, sin, cos, tan, log, log10, exp, pi, e)");
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after math." + method + " at line " + std::to_string(peek().line));
        }
        node.children.push_back(parseExpression());
        if (twoArg.find(method) != twoArg.end()) {
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in math." + method + "(a, b) at line " + std::to_string(peek().line));
            }
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after math." + method + " arguments at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseCryptoCall() {
        size_t line = peek().line;
        if (!modules_.hasCrypto()) {
            throw std::runtime_error("crypto.* requires #include <std/crypto> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "crypto") {
            throw std::runtime_error("Expected 'crypto' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected crypto method at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        advance();
        static const std::set<std::string> oneArg = {
            "sha256", "sha1", "hex_encode", "hex_decode",
            "base64_encode", "base64_decode", "random_bytes"
        };
        static const std::set<std::string> twoArg = {"hmac_sha256"};
        bool isXor = (method == "xor");
        if (!isXor && oneArg.find(method) == oneArg.end() && twoArg.find(method) == twoArg.end()) {
            throw std::runtime_error("Unknown crypto function 'crypto." + method +
                "' at line " + std::to_string(methodTok.line) +
                " (use xor, sha256, sha1, hmac_sha256, hex_encode, hex_decode, base64_encode, base64_decode, random_bytes)");
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after crypto." + method + " at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::CryptoCall, method, {}};
        node.children.push_back(parseExpression());
        if (isXor) {
            while (match(TokenType::Comma)) {
                node.children.push_back(parseExpression());
            }
            if (node.children.size() < 2) {
                throw std::runtime_error("crypto.xor(data, key...) requires at least one key at line " + std::to_string(line));
            }
        } else if (twoArg.find(method) != twoArg.end()) {
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in crypto." + method + "(a, b) at line " + std::to_string(peek().line));
            }
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after crypto." + method + "(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseResultMake() {
        const Token& nameTok = peek();
        size_t line = nameTok.line;
        if (nameTok.type != TokenType::Identifier || (nameTok.value != "ok" && nameTok.value != "err")) {
            throw std::runtime_error("Expected ok(...) or err(...) at line " + std::to_string(line));
        }
        std::string kind = nameTok.value;
        advance();
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after " + kind + " at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::ResultMake, kind, {}};
        if (kind == "ok") {
            if (peek().type != TokenType::RParen) {
                node.children.push_back(parseExpression());
                if (peek().type == TokenType::Comma) {
                    throw std::runtime_error("ok(...) takes at most one value at line " + std::to_string(peek().line));
                }
            }
        } else {
            if (peek().type == TokenType::RParen) {
                throw std::runtime_error("err(...) requires an error message at line " + std::to_string(line));
            }
            node.children.push_back(parseExpression());
            if (peek().type == TokenType::Comma) {
                throw std::runtime_error("err(...) takes one argument at line " + std::to_string(peek().line));
            }
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after " + kind + "(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseHttpCall() {
        size_t line = peek().line;
        if (!modules_.hasHttp()) {
            throw std::runtime_error("http.* requires #include <std/http> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "http") {
            throw std::runtime_error("Expected 'http' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected http method at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        advance();
        if (method != "get" && method != "post") {
            throw std::runtime_error("Unknown http function 'http." + method +
                "' at line " + std::to_string(methodTok.line) + " (use get, post)");
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after http." + method + " at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::HttpCall, method, {}};
        node.children.push_back(parseExpression());
        if (method == "post") {
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in http.post(url, body) at line " + std::to_string(peek().line));
            }
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after http." + method + "(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseJsonCall() {
        size_t line = peek().line;
        if (!modules_.hasJson()) {
            throw std::runtime_error("json.* requires #include <std/json> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "json") {
            throw std::runtime_error("Expected 'json' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected json method at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        advance();
        if (method != "parse" && method != "stringify" && method != "of" && method != "null" &&
            method != "bool" && method != "int" && method != "float" && method != "string" &&
            method != "array" && method != "object") {
            throw std::runtime_error("Unknown json function 'json." + method +
                "' at line " + std::to_string(methodTok.line) +
                " (use parse, stringify, of, null, bool, int, float, string, array, object)");
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after json." + method + " at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::JsonCall, method, {}};
        if (method == "null" || method == "array" || method == "object") {
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after json." + method + "() at line " + std::to_string(peek().line));
            }
            return node;
        }
        node.children.push_back(parseExpression());
        if (method == "stringify" && match(TokenType::Comma)) {
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after json." + method + "(...) at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseDllCall() {
        size_t line = peek().line;
        if (!modules_.hasDll()) {
            throw std::runtime_error("dll.call requires #include <std/dll> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "dll") {
            throw std::runtime_error("Expected 'dll' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier || methodTok.value != "call") {
            throw std::runtime_error("Expected 'call' at line " + std::to_string(methodTok.line));
        }
        advance();
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        const Token& handleTok = peek();
        if (handleTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected handle variable at line " + std::to_string(handleTok.line));
        }
        advance();
        std::string handleVar = handleTok.value;
        if (!match(TokenType::Comma)) {
            throw std::runtime_error("Expected ',' at line " + std::to_string(peek().line));
        }
        const Token& symTok = peek();
        if (symTok.type != TokenType::String) {
            throw std::runtime_error("Expected symbol name string at line " + std::to_string(symTok.line));
        }
        advance();
        AstNode result{AstNode::Type::DllCall, symTok.value, {}};
        result.children.push_back({AstNode::Type::ExprVarRef, handleVar, {}});
        while (match(TokenType::Comma)) {
            result.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return result;
    }

    AstNode parsePathVarFileCall() {
        size_t line = peek().line;
        if (!modules_.hasFile()) {
            throw std::runtime_error("pathVar.Write/Append requires #include <std/file> at line " + std::to_string(line));
        }
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected variable name at line " + std::to_string(line));
        }
        advance();
        std::string pathVar = nameTok.value;
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier || (methodTok.value != "Write" && methodTok.value != "Append")) {
            throw std::runtime_error("Expected .Write or .Append at line " + std::to_string(peek().line));
        }
        bool isAppend = (methodTok.value == "Append");
        advance();
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode pathArg{AstNode::Type::ExprVarRef, pathVar, {}};
        AstNode contentArg = parseExpression();
        AstNode node{isAppend ? AstNode::Type::FileAppend : AstNode::Type::FileWrite, "", {}};
        node.children.push_back(pathArg);
        node.children.push_back(contentArg);
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseGfxCall(bool requireSemicolon) {
        size_t line = peek().line;
        if (!modules_.hasGfx()) {
            throw std::runtime_error("gfx.* requires #include <std/gfx> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "gfx") {
            throw std::runtime_error("Expected 'gfx' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected gfx method at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        advance();
        int argc = -1;
        int argcMax = -1;
        if (method == "open") { argc = 3; argcMax = 4; }
        else if (method == "resize") { argc = 2; argcMax = 3; }
        else if (method == "close" || method == "poll" || method == "present" || method == "closed"
                 || method == "mouse_x" || method == "mouse_y" || method == "drop"
                 || method == "width" || method == "height" || method == "scale"
                 || method == "audio_queued" || method == "audio_flush") argc = 0;
        else if (method == "fullscreen") { argc = 0; argcMax = 1; }
        else if (method == "key" || method == "pressed" || method == "mouse") argc = 1;
        else if (method == "get") argc = 2;
        else if (method == "clear") argc = 3;
        else if (method == "plot") argc = 5;
        else if (method == "fill") argc = 7;
        else if (method == "line") argc = 7;
        else if (method == "text") { argc = 6; argcMax = 7; }
        else if (method == "text_size") { argc = 0; argcMax = 1; }
        else if (method == "text_width" || method == "text_height") { argc = 1; argcMax = 2; }
        else if (method == "title") { argc = 0; argcMax = 1; }
        else if (method == "opendialog" || method == "openfile") { argc = 0; argcMax = 1; if (method == "openfile") method = "opendialog"; }
        else if (method == "image" || method == "decode" || method == "image_w" || method == "image_h"
                 || method == "sample") argc = 1;
        else if (method == "audio") { argc = 0; argcMax = 1; }
        else if (method == "blit") { argc = 3; argcMax = 9; }
        else {
            throw std::runtime_error("Unknown gfx method 'gfx." + method +
                "' at line " + std::to_string(methodTok.line) +
                " (use open, close, resize, width, height, scale, title, poll, closed, clear, plot, fill, line, text, text_size, text_width, text_height, get, present, image, decode, image_w, image_h, blit, key, pressed, mouse_x, mouse_y, mouse, audio, sample, audio_queued, audio_flush, fullscreen)");
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' after gfx." + method + " at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::GfxCall, method, {}};
        if (peek().type != TokenType::RParen) {
            node.children.push_back(parseExpression());
            while (match(TokenType::Comma)) {
                node.children.push_back(parseExpression());
            }
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after gfx." + method + "(...) at line " + std::to_string(peek().line));
        }
        int got = (int)node.children.size();
        if (argcMax >= 0) {
            if (method == "blit" && got != 3 && got != 5 && got != 7 && got != 9) {
                throw std::runtime_error("gfx.blit(x, y, src[, w, h]) or gfx.blit(x, y, src, sx, sy, sw, sh[, dw, dh]) at line " + std::to_string(line));
            }
            if (got < argc || got > argcMax) {
                if (method == "resize") {
                    throw std::runtime_error("gfx.resize(w, h[, scale]) at line " + std::to_string(line));
                }
                if (method == "text") {
                    throw std::runtime_error("gfx.text(x, y, s, r, g, b[, scale]) at line " + std::to_string(line));
                }
                if (method == "text_size") {
                    throw std::runtime_error("gfx.text_size() or gfx.text_size(n) at line " + std::to_string(line));
                }
                if (method == "text_width") {
                    throw std::runtime_error("gfx.text_width(s[, scale]) at line " + std::to_string(line));
                }
                if (method == "text_height") {
                    throw std::runtime_error("gfx.text_height(s[, scale]) at line " + std::to_string(line));
                }
                if (method == "title") {
                    throw std::runtime_error("gfx.title() or gfx.title(s) at line " + std::to_string(line));
                }
                if (method == "opendialog") {
                    throw std::runtime_error("gfx.opendialog([filter]) at line " + std::to_string(line));
                }
                if (method == "audio") {
                    throw std::runtime_error("gfx.audio([rate]) at line " + std::to_string(line));
                }
                if (method == "fullscreen") {
                    throw std::runtime_error("gfx.fullscreen() or gfx.fullscreen(on) at line " + std::to_string(line));
                }
                throw std::runtime_error("gfx.open(title, w, h[, scale]) at line " + std::to_string(line));
            }
        } else if (got != argc) {
            throw std::runtime_error("gfx." + method + " argument count at line " + std::to_string(line));
        }
        if (requireSemicolon && !match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseRandomCall() {
        size_t line = peek().line;
        if (!modules_.hasRandom()) {
            throw std::runtime_error("random.* requires #include <std/random> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "random") {
            throw std::runtime_error("Expected 'random' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected random method (int, seed) at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        if (method != "int" && method != "seed") {
            throw std::runtime_error("Expected random.int or random.seed at line " + std::to_string(methodTok.line));
        }
        advance();
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode arg1 = parseExpression();
        AstNode node{method == "int" ? AstNode::Type::RandomInt : AstNode::Type::RandomSeed, "", {}};
        node.children.push_back(arg1);
        if (method == "int") {
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in random.int(min, max) at line " + std::to_string(peek().line));
            }
            node.children.push_back(parseExpression());
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseTimeCall() {
        size_t line = peek().line;
        if (!modules_.hasTime()) {
            throw std::runtime_error("time.* requires #include <std/time> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "time") {
            throw std::runtime_error("Expected 'time' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier || methodTok.value != "sleep") {
            throw std::runtime_error("Expected time.sleep at line " + std::to_string(peek().line));
        }
        advance();
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode durationArg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::TimeSleep, "", {}};
        node.children.push_back(durationArg);
        return node;
    }

    AstNode parseTimeSeconds() {
        if (!modules_.hasTime()) {
            throw std::runtime_error(
                "time.seconds requires #include <std/time> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "time") {
            throw std::runtime_error("Expected 'time' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "seconds") {
            throw std::runtime_error("Expected time.seconds at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode arg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::TimeSeconds, "", {arg}};
    }

    AstNode parseTimeMilliseconds() {
        if (!modules_.hasTime()) {
            throw std::runtime_error(
                "time.milliseconds requires #include <std/time> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "time") {
            throw std::runtime_error("Expected 'time' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "milliseconds") {
            throw std::runtime_error("Expected time.milliseconds at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode arg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::TimeMilliseconds, "", {arg}};
    }

    AstNode parseTimeNowMs() {
        if (!modules_.hasTime()) {
            throw std::runtime_error(
                "time.now_ms requires #include <std/time> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "time") {
            throw std::runtime_error("Expected 'time' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "now_ms") {
            throw std::runtime_error("Expected time.now_ms at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::TimeNowMs, "", {}};
    }

    // Parses fn_name, fn(args), or os.*(...) for thread.spawn / thread.run.
    AstNode parseThreadJob() {
        if (peek().type == TokenType::Identifier && peek().value == "os") {
            AstNode call = parseOsCall(false);
            return {AstNode::Type::ThreadSpawn, "", {call}};
        }
        if (peek().type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::LParen) {
            AstNode call = parseFnCallExpr();
            if (call.children.empty()) {
                return {AstNode::Type::ThreadSpawn, call.value, {}};
            }
            return {AstNode::Type::ThreadSpawn, "", {call}};
        }
        const Token& fnTok = peek();
        if (fnTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected function or os.* call at line " + std::to_string(fnTok.line));
        }
        advance();
        return {AstNode::Type::ThreadSpawn, fnTok.value, {}};
    }

    AstNode parseThreadWorkerExpr() {
        size_t line = peek().line;
        if (!modules_.hasThread()) {
            throw std::runtime_error("thread.worker requires #include <std/thread> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "thread") {
            throw std::runtime_error("Expected 'thread' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "worker") {
            throw std::runtime_error("Expected thread.worker at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
            throw std::runtime_error("Expected '()' after thread.worker at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::ThreadWorker, "", {}};
    }

    AstNode parseThreadSpawnExpr() {
        size_t line = peek().line;
        if (!modules_.hasThread()) {
            throw std::runtime_error("thread.spawn requires #include <std/thread> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "thread") {
            throw std::runtime_error("Expected 'thread' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "spawn") {
            throw std::runtime_error("Expected thread.spawn at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode job = parseThreadJob();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after thread.spawn(...) at line " + std::to_string(peek().line));
        }
        return job;
    }

    AstNode parseThreadCall() {
        size_t line = peek().line;
        if (!modules_.hasThread()) {
            throw std::runtime_error("thread.* requires #include <std/thread> at line " + std::to_string(line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "thread") {
            throw std::runtime_error("Expected 'thread' at line " + std::to_string(line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        const Token& methodTok = peek();
        if (methodTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected thread method at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        advance();
        if (method == "run") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after thread.run at line " + std::to_string(peek().line));
            }
            AstNode handleArg = parseExpression();
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' in thread.run(worker, job) at line " + std::to_string(peek().line));
            }
            AstNode job = parseThreadJob();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after thread.run(...) at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after thread.run(...) at line " + std::to_string(peek().line));
            }
            return {AstNode::Type::ThreadRun, "", {handleArg, job}};
        }
        if (method == "worker_join") {
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' after thread.worker_join at line " + std::to_string(peek().line));
            }
            AstNode handleArg = parseExpression();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' after thread.worker_join(...) at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after thread.worker_join(...) at line " + std::to_string(peek().line));
            }
            return {AstNode::Type::ThreadWorkerJoin, "", {handleArg}};
        }
        if (method != "join") {
            throw std::runtime_error("Expected thread.join, thread.run, or thread.worker_join at line " + std::to_string(methodTok.line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode handleArg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::ThreadJoin, "", {handleArg}};
    }

    AstNode parseVariable() {
        size_t line = peek().line;
        if (!match(TokenType::Let)) {
            throw std::runtime_error("Expected 'let' at line " + std::to_string(line));
        }
        bool isConstVar = match(TokenType::Const);
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected variable name at line " + std::to_string(nameTok.line));
        }
        advance();
        std::string name = nameTok.value;
        std::string declType = "";
        bool isFixedArray = false;
        std::string arraySize = "";
        if (match(TokenType::Colon)) {
            declType = parseTypeName();
            if (match(TokenType::LBracket)) {
                const Token& sizeTok = peek();
                if (sizeTok.type != TokenType::Number) {
                    throw std::runtime_error("Fixed array size must be a constant integer at line " + std::to_string(sizeTok.line));
                }
                arraySize = sizeTok.value;
                advance();
                if (!match(TokenType::RBracket)) {
                    throw std::runtime_error("Expected ']' after array size at line " + std::to_string(peek().line));
                }
                isFixedArray = true;
                if (declType == "string" || declType == "bool" || declType == "float" ||
                    (declType.size() >= 5 && declType.compare(0, 5, "enum:") == 0) ||
                    (!declType.empty() && declType[0] == '*') ||
                    declType == "void") {
                    throw std::runtime_error("Fixed array only supports integer types, char, or structs at line " + std::to_string(peek().line));
                }
            }
        }
        if (!match(TokenType::Assign)) {
            if (isConstVar && !isFixedArray) {
                throw std::runtime_error("const variable must have an initializer at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected '=' or ';' at line " + std::to_string(peek().line));
            }
            AstNode node{AstNode::Type::Variable, name, {}};
            node.initUninitialized = true;
            node.declType = declType;
            node.initIsInt = nexaIsNumericIntType(declType);
            node.initIsBool = (declType == "bool");
            node.initIsFloat = (declType == "float");
            node.initIsChar = (declType == "char");
            node.isFixedArray = isFixedArray;
            node.arraySize = arraySize;
            if (declType.size() >= 7 && declType.compare(0, 7, "struct:") == 0) {
                node.initIsInt = false;
                node.initIsBool = false;
                node.initIsFloat = false;
                node.initIsChar = false;
            }
            if (declType.size() >= 5 && declType.compare(0, 5, "enum:") == 0) {
                node.initIsInt = false;
                node.initIsBool = false;
                node.initIsFloat = false;
                node.initIsChar = false;
            }
            if (nexaIsFnType(declType) || nexaIsResultType(declType) || declType == "json") {
                node.initIsInt = false;
                node.initIsBool = false;
                node.initIsFloat = false;
                node.initIsChar = false;
            }
            return node;
        }
        if (isFixedArray) {
            throw std::runtime_error("Fixed array cannot have an initializer; use let name: type[size]; at line " + std::to_string(peek().line));
        }
        const Token& initTok = peek();
        AstNode node{AstNode::Type::Variable, name, {}};
        if (initTok.type == TokenType::Identifier && initTok.value == "dll") {
            if (!modules_.hasDll()) {
                throw std::runtime_error("dll.load requires #include <std/dll> at line " + std::to_string(line));
            }
            advance();
            if (!match(TokenType::Dot)) {
                throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
            }
            const Token& methodTok = peek();
            if (methodTok.type != TokenType::Identifier || methodTok.value != "load") {
                throw std::runtime_error("Expected 'load' at line " + std::to_string(methodTok.line));
            }
            advance();
            if (!match(TokenType::LParen)) {
                throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
            }
            const Token& pathTok = peek();
            if (pathTok.type != TokenType::String) {
                throw std::runtime_error("Expected string path at line " + std::to_string(pathTok.line));
            }
            advance();
            node.initValue = pathTok.value;
            node.initFromDllLoad = true;
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
            }
        } else {
            // Any expression (calls, unary ! / ~ / -, string/array/io/file/..., parens, etc.).
            // parseTernary() also covers comparisons, arithmetic, and `cond ? a : b`.
            node.children.push_back(parseTernary());
            AstNode& b = node.children.back();
            if (b.type == AstNode::Type::IoReadln) {
                node.initFromReadln = true;
                node.children.clear();
            } else if (b.type == AstNode::Type::ExprArrayLiteral) {
                node.initFromArray = true;
                node.initIsInt = false;
            } else if (b.type == AstNode::Type::FileRead) {
                node.initFromFileRead = true;
                node.initIsInt = false;
            } else if (b.type == AstNode::Type::FileCall &&
                       (b.value == "cwd" || b.value == "abspath" || b.value == "join" ||
                        b.value == "dirname" || b.value == "basename" || b.value == "extension")) {
                node.initIsInt = false;
            } else if (b.type == AstNode::Type::FileCall && b.value == "list") {
                node.initFromArray = true;
                node.initIsInt = false;
                node.declType = "[]string";
            } else if (b.type == AstNode::Type::OsInfo && b.value == "environ") {
                node.initFromArray = true;
                node.initIsInt = false;
                node.declType = "[]string";
            } else if (b.type == AstNode::Type::ExprBoolLiteral) {
                node.initIsBool = true;
            } else if (b.type == AstNode::Type::ExprFloatLiteral) {
                node.initIsFloat = true;
            } else if (b.type == AstNode::Type::TimeNowMs) {
                node.initIsFloat = true;
            } else if (b.type == AstNode::Type::MathCall) {
                node.initIsFloat = true;
            } else if (b.type == AstNode::Type::StrMethod && b.value == "split") {
                node.initFromArray = true;
                node.initIsInt = false;
                node.declType = "[]string";
            } else if (b.type == AstNode::Type::StrMethod &&
                       (b.value == "contains" || b.value == "starts_with" || b.value == "ends_with")) {
                node.initIsBool = true;
            } else if (b.type == AstNode::Type::ExprCharLiteral) {
                node.initIsChar = true;
            } else if (b.type == AstNode::Type::ExprLambda || b.type == AstNode::Type::ExprStructLit) {
                node.initIsInt = false;
            } else if (b.type == AstNode::Type::JsonCall) {
                node.initIsInt = false;
            } else if (b.type == AstNode::Type::ResultMake || b.type == AstNode::Type::HttpCall) {
                node.initIsInt = false;
            } else if (b.type == AstNode::Type::ExprSlice) {
                node.initIsInt = false;
            } else {
                node.initIsInt = !exprProducesString(b);
            }
        }
        if (declType.empty() && !node.children.empty() && astHasMemberAccess(node.children.back()) &&
            node.children.back().type != AstNode::Type::ExprStructLit &&
            node.children.back().type != AstNode::Type::ExprLambda) {
            throw std::runtime_error("let with '.' access requires an explicit type (e.g. let x: int = s.field or let x: enum E = E.A) at line " + std::to_string(line));
        }
        if (!declType.empty()) {
            node.declType = declType;
            node.initIsInt = nexaIsNumericIntType(declType);
            node.initIsBool = (declType == "bool");
            node.initIsFloat = (declType == "float");
            node.initIsChar = (declType == "char");
            if (declType.size() >= 7 && declType.compare(0, 7, "struct:") == 0) {
                node.initIsInt = false;
                node.initIsBool = false;
                node.initIsFloat = false;
                node.initIsChar = false;
            }
            if (declType.size() >= 5 && declType.compare(0, 5, "enum:") == 0) {
                node.initIsInt = false;
                node.initIsBool = false;
                node.initIsFloat = false;
                node.initIsChar = false;
            }
            if (nexaIsFnType(declType) || nexaIsSliceType(declType) || nexaIsMapType(declType) ||
                nexaIsResultType(declType) || declType == "json") {
                node.initIsInt = false;
                node.initIsBool = false;
                node.initIsFloat = false;
                node.initIsChar = false;
            }
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        node.isConst = isConstVar;
        return node;
    }
};

}  // namespace nexa
