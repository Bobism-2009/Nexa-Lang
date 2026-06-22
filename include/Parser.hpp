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
                      OsNotify, OsOpen,
                      DllLoad, DllCall,
                      FileRead, FileWrite, FileAppend, FileExists, FileMkdir,
                      RandomInt, RandomSeed,
                      MathCall,
                      StrMethod,
                      TimeSleep, TimeSeconds, TimeMilliseconds, TimeNowMs,
                      ThreadSpawn, ThreadJoin,
                      IfElse,
                      Switch,
                      SwitchCase,
                      While,
                      For,
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
                      ExprArrayLiteral, ExprArrayIndex,
                      CondEq, CondNe, CondLt, CondGt, CondLe, CondGe,
                      CondAnd, CondOr, CondNot,
                      ExprStringLiteral,
                      ExprLen,
                      ExprTrim,
                      AssnIndex,
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
    std::string declType = "";  // for Variable: "int", "string", "bool", "float", "char", "unsigned char", or "" (inferred)
    bool isVarRef = false;      // for IoPrint/IoPrintln: true = print variable, false = print string
    bool caseIsString = false;  // for SwitchCase: true = case "str", false = case 42
    bool caseIsEnum = false;    // for SwitchCase: Enum.variant; value = enum name, initValue = variant
    bool isConst = false;       // for Variable: true = let const x = ...
    bool isFixedArray = false;  // for Variable: true = let x: type[size]; (fixed-size buffer)
    std::string arraySize = ""; // for Variable: size for fixed array, e.g. "4080"
    std::string fnReturnType = ""; // Function / MainFunction: explicit ": type" before `{`; empty = infer from returns
};

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
        std::vector<AstNode> ast;
        while (pos_ < tokens_.size()) {
            const Token& t = peek();
            if (t.type == TokenType::Eof) break;
            if (t.type == TokenType::Include) {
                std::vector<AstNode> incNodes = parseInclude();
                for (AstNode& n : incNodes) ast.push_back(std::move(n));
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
        }
        return ast;
    }

private:
    std::vector<Token> tokens_;
    Modules& modules_;
    size_t pos_;
    std::string currentFilePath_;
    std::set<std::string>* includedFiles_;
    const std::vector<std::string>* packagePaths_;

    static bool exprProducesString(const AstNode& e) {
        if (e.type == AstNode::Type::OsGetenv || e.type == AstNode::Type::OsExec || e.type == AstNode::Type::OsPlatform || e.type == AstNode::Type::OsExeDir || e.type == AstNode::Type::OsGrepKeys || e.type == AstNode::Type::OsClipGet || e.type == AstNode::Type::ExprStringLiteral || e.type == AstNode::Type::FileRead || e.type == AstNode::Type::IoReadln || e.type == AstNode::Type::IoGetline || e.type == AstNode::Type::ExprTrim) return true;
        if (e.type == AstNode::Type::StrMethod) {
            const std::string& m = e.value;
            return m == "upper" || m == "lower" || m == "trim" || m == "replace" ||
                   m == "substring" || m == "repeat";
        }
        if (e.type == AstNode::Type::ExprAdd && e.children.size() >= 2) {
            return exprProducesString(e.children[0]) || exprProducesString(e.children[1]);
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

    static bool astHasMemberAccess(const AstNode& e) {
        if (e.type == AstNode::Type::ExprMember) return true;
        for (const AstNode& c : e.children) {
            if (astHasMemberAccess(c)) return true;
        }
        return false;
    }

    std::string parseTypeName() {
        if (match(TokenType::Enum)) {
            const Token& t = peek();
            if (t.type != TokenType::Identifier) {
                throw std::runtime_error("Expected enum name at line " + std::to_string(t.line));
            }
            advance();
            return "enum:" + t.value;
        }
        const Token& t = peek();
        if (t.type == TokenType::Identifier && t.value == "unsigned") {
            advance();
            const Token& t2 = peek();
            if (t2.type != TokenType::Identifier || t2.value != "char") {
                throw std::runtime_error("Expected 'char' after 'unsigned' at line " + std::to_string(t2.line));
            }
            advance();
            return "unsigned char";
        }
        if (t.type != TokenType::Identifier) {
            throw std::runtime_error("Expected type name at line " + std::to_string(t.line));
        }
        std::string v = t.value;
        advance();
        if (v == "int") return "int";
        if (v == "string") return "string";
        if (v == "bool") return "bool";
        if (v == "float") return "float";
        if (v == "char") return "char";
        if (v == "void") return "void";
        return "struct:" + v;
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
        if (!match(TokenType::LBrace)) {
            throw std::runtime_error("Expected '{' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::StructDef, sname, {}};
        while (peek().type != TokenType::RBrace) {
            if (peek().type == TokenType::Eof) {
                throw std::runtime_error("Unclosed struct body starting at line " + std::to_string(line));
            }
            const Token& fieldTok = peek();
            if (fieldTok.type != TokenType::Identifier) {
                throw std::runtime_error("Expected field name at line " + std::to_string(fieldTok.line));
            }
            advance();
            node.paramNames.push_back(fieldTok.value);
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

    AstNode parseMemberAssignment() {
        size_t line = peek().line;
        const Token& nameTok = peek();
        if (nameTok.type != TokenType::Identifier) {
            throw std::runtime_error("Expected variable name at line " + std::to_string(line));
        }
        advance();
        AstNode cur{AstNode::Type::ExprVarRef, nameTok.value, {}};
        while (match(TokenType::Dot)) {
            const Token& ftok = peek();
            if (ftok.type != TokenType::Identifier) {
                throw std::runtime_error("Expected field name after '.' at line " + std::to_string(ftok.line));
            }
            advance();
            AstNode mem{AstNode::Type::ExprMember, ftok.value, {std::move(cur)}};
            cur = std::move(mem);
        }
        if (cur.type != AstNode::Type::ExprMember) {
            throw std::runtime_error("Expected member assignment (e.g. obj.field = ...) at line " + std::to_string(line));
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
                // #include <std/io> - built-in module
                modules_.enable(path);
                return {{AstNode::Type::Include, path, {}}};
            }
            if (isCppHeaderIncludePath(path)) {
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
                        Lexer lexer(buf.str());
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
            Lexer lexer(buf.str());
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
        if (peek().type != TokenType::RParen) {
            for (;;) {
                const Token& p = peek();
                if (p.type != TokenType::Identifier) {
                    throw std::runtime_error("Expected parameter name at line " + std::to_string(p.line));
                }
                params.push_back(p.value);
                advance();
                std::string ptype = "int";  // default
                if (match(TokenType::Colon)) {
                    ptype = parseTypeName();
                }
                types.push_back(ptype);
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
        fnNode.children = parseBlock();
        if (!match(TokenType::RBrace)) {
            throw std::runtime_error("Expected '}' at line " + std::to_string(peek().line));
        }
        return fnNode;
    }

    std::vector<AstNode> parseBlock(AstNode* parentIf = nullptr, bool singleStatement = false) {
        std::vector<AstNode> stmts;
        while (pos_ < tokens_.size()) {
            const Token& t = peek();
            if (t.type == TokenType::RBrace) break;
            if (singleStatement && t.type == TokenType::Semicolon) { advance(); break; }
            size_t before = stmts.size();
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
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       tokens_[pos_ + 1].type == TokenType::Dot) {
                const Token& id2 = tokens_[pos_ + 2];
                bool isPathFile = id2.type == TokenType::Identifier && (id2.value == "Write" || id2.value == "Append") &&
                    pos_ + 3 < tokens_.size() && tokens_[pos_ + 3].type == TokenType::LParen;
                if (isPathFile) {
                    stmts.push_back(parsePathVarFileCall());
                } else {
                    stmts.push_back(parseMemberAssignment());
                }
            } else if (t.type == TokenType::Identifier && (t.value == "getprocessid" || t.value == "getpid") &&
                       pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LParen) {
                stmts.push_back(parseOsGetProcessIdBareStmt());
            } else if (t.type == TokenType::Identifier && t.value == "ui") {
                throw std::runtime_error("std/ui has been removed at line " + std::to_string(t.line));
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       tokens_[pos_ + 1].type == TokenType::LParen) {
                stmts.push_back(parseFnCall());
            } else if (t.type == TokenType::Goto) {
                stmts.push_back(parseGoto());
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       tokens_[pos_ + 1].type == TokenType::Colon) {
                stmts.push_back(parseLabel());
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size()) {
                TokenType next = tokens_[pos_ + 1].type;
                if (next == TokenType::LBracket) {
                    stmts.push_back(parseIndexedAssignment());
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
            if (singleStatement && stmts.size() > before) break;
        }
        return stmts;
    }

    // Parse a braced block { ... } OR a single braceless statement (C-style),
    // returning a Block node either way.
    AstNode parseBody(AstNode* parentIf = nullptr) {
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

    AstNode parseFnCallExpr() {
        const Token& nameTok = peek();
        advance();
        std::string name = nameTok.value;
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        std::vector<AstNode> args;
        if (peek().type != TokenType::RParen) {
            for (;;) {
                args.push_back(parseExpression());
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
        AstNode expr = parseExpression();
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
        if (!match(TokenType::LBracket)) {
            throw std::runtime_error("Expected '[' at line " + std::to_string(peek().line));
        }
        AstNode idx = parseExpression();
        if (!match(TokenType::RBracket)) {
            throw std::runtime_error("Expected ']' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Assign)) {
            throw std::runtime_error("Expected '=' at line " + std::to_string(peek().line));
        }
        AstNode expr = parseExpression();
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::AssnIndex, name, {idx, expr}};
        return node;
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

    AstNode parseCondition() {
        AstNode left = parseExpression();
        if (match(TokenType::Equals)) {
            AstNode n{AstNode::Type::CondEq, "", {left, parseExpression()}};
            return n;
        }
        if (match(TokenType::NotEquals)) {
            AstNode n{AstNode::Type::CondNe, "", {left, parseExpression()}};
            return n;
        }
        if (match(TokenType::Less)) {
            AstNode n{AstNode::Type::CondLt, "", {left, parseExpression()}};
            return n;
        }
        if (match(TokenType::LessEq)) {
            AstNode n{AstNode::Type::CondLe, "", {left, parseExpression()}};
            return n;
        }
        if (match(TokenType::Greater)) {
            AstNode n{AstNode::Type::CondGt, "", {left, parseExpression()}};
            return n;
        }
        if (match(TokenType::GreaterEq)) {
            AstNode n{AstNode::Type::CondGe, "", {left, parseExpression()}};
            return n;
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
        AstNode cond = parseLogicalOr();
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
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       tokens_[pos_ + 1].type == TokenType::Dot) {
                const Token& id2 = tokens_[pos_ + 2];
                bool isPathFile = id2.type == TokenType::Identifier && (id2.value == "Write" || id2.value == "Append") &&
                    pos_ + 3 < tokens_.size() && tokens_[pos_ + 3].type == TokenType::LParen;
                if (isPathFile) {
                    stmts.push_back(parsePathVarFileCall());
                } else {
                    stmts.push_back(parseMemberAssignment());
                }
            } else if (t.type == TokenType::Identifier && (t.value == "getprocessid" || t.value == "getpid") &&
                       pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LParen) {
                stmts.push_back(parseOsGetProcessIdBareStmt());
            } else if (t.type == TokenType::Identifier && t.value == "ui") {
                throw std::runtime_error("std/ui has been removed at line " + std::to_string(t.line));
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       tokens_[pos_ + 1].type == TokenType::LParen) {
                stmts.push_back(parseFnCall());
            } else if (t.type == TokenType::Goto) {
                stmts.push_back(parseGoto());
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
                       tokens_[pos_ + 1].type == TokenType::Colon) {
                stmts.push_back(parseLabel());
            } else if (t.type == TokenType::Identifier && pos_ + 1 < tokens_.size()) {
                TokenType next = tokens_[pos_ + 1].type;
                if (next == TokenType::LBracket) {
                    stmts.push_back(parseIndexedAssignment());
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
        AstNode cond = parseLogicalOr();
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
        if (!match(TokenType::Comma)) {
            throw std::runtime_error("Expected ',' in for loop at line " + std::to_string(peek().line));
        }
        AstNode countExpr = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after for loop count at line " + std::to_string(peek().line));
        }
        AstNode block = parseBody();
        AstNode forNode{AstNode::Type::For, varName, {countExpr, block}};
        return forNode;
    }

    // Parses a chain of `.field` accesses and `.method(args)` calls applied to a base expression.
    AstNode parseDotChain(AstNode cur) {
        while (peek().type == TokenType::Dot) {
            advance();
            const Token& ftok = peek();
            if (ftok.type != TokenType::Identifier) {
                throw std::runtime_error("Expected field name after '.' at line " + std::to_string(ftok.line));
            }
            advance();
            // value.method(args): core string methods callable on any string value.
            if (peek().type == TokenType::LParen) {
                std::string m = ftok.value;
                // method name -> argument count
                static const std::map<std::string, int> strMethods = {
                    {"upper", 0}, {"lower", 0}, {"trim", 0}, {"len", 0},
                    {"contains", 1}, {"starts_with", 1}, {"ends_with", 1},
                    {"index_of", 1}, {"repeat", 1}, {"split", 1},
                    {"replace", 2}, {"substring", 2}
                };
                auto mit = strMethods.find(m);
                if (mit == strMethods.end()) {
                    throw std::runtime_error("Unknown method '." + m + "()' at line " + std::to_string(ftok.line) +
                        " (string methods: upper, lower, trim, len, contains, starts_with, ends_with, index_of, repeat, split, replace, substring)");
                }
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
            AstNode mem{AstNode::Type::ExprMember, ftok.value, {std::move(cur)}};
            cur = std::move(mem);
        }
        return cur;
    }

    AstNode parseExpression() {
        return parseBitOr();
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
        return parseFactor();
    }

    AstNode parseArrayLiteral() {
        if (!match(TokenType::LBracket)) {
            throw std::runtime_error("Expected '[' at line " + std::to_string(peek().line));
        }
        AstNode node{AstNode::Type::ExprArrayLiteral, "", {}};
        if (peek().type != TokenType::RBracket) {
            for (;;) {
                node.children.push_back(parseExpression());
                if (!match(TokenType::Comma)) break;
            }
        }
        if (!match(TokenType::RBracket)) {
            throw std::runtime_error("Expected ']' at line " + std::to_string(peek().line));
        }
        return node;
    }

    AstNode parseFactor() {
        if (peek().type == TokenType::LBracket) {
            return parseArrayLiteral();
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
        if (match(TokenType::String)) {
            AstNode lit{AstNode::Type::ExprStringLiteral, tokens_[pos_ - 1].value, {}};
            if (peek().type == TokenType::Dot) return parseDotChain(std::move(lit));
            return lit;
        }
        if (peek().type == TokenType::Identifier && peek().value == "io" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            std::string method = tokens_[pos_ + 2].value;
            if (method == "readln") return parseIoReadlnExpr();
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
            if (method == "getprocessid" || method == "getpid" || method == "GetProcessID") return parseOsGetProcessId();
            if (method == "grepkeys" || method == "getkey") return parseOsGrepKeys();
            if (method == "keypressed") return parseOsKeyPressed();
            if (method == "get_volume") return parseOsGetVolume();
            if (method == "get_brightness") return parseOsGetBrightness();
            if (method == "clip_get") return parseOsClipGet();
        }
        if (peek().type == TokenType::Identifier && (peek().value == "getprocessid" || peek().value == "getpid") &&
            pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LParen) {
            return parseOsGetProcessIdBareExpr();
        }
        if (peek().type == TokenType::Identifier && peek().value == "file" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            std::string method = tokens_[pos_ + 2].value;
            if (method == "read" || method == "exists") {
                return parseFileReadOrExists(method == "read");
            }
            if (method == "mkdir") {
                return parseFileMkdirExpr();
            }
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
        if (peek().type == TokenType::Identifier && peek().value == "time" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier) {
            std::string method = tokens_[pos_ + 2].value;
            if (method == "seconds") return parseTimeSeconds();
            if (method == "milliseconds") return parseTimeMilliseconds();
            if (method == "now_ms") return parseTimeNowMs();
        }
        if (peek().type == TokenType::Identifier && peek().value == "thread" && pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::Dot && tokens_[pos_ + 2].type == TokenType::Identifier &&
            tokens_[pos_ + 2].value == "spawn") {
            return parseThreadSpawnExpr();
        }
        if (peek().type == TokenType::Identifier && peek().value == "ui") {
            throw std::runtime_error("std/ui has been removed at line " + std::to_string(peek().line));
        }
        if (peek().type == TokenType::Identifier && peek().value == "len" && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::LParen) {
            return parseLenExpr();
        }
        if (peek().type == TokenType::Identifier && peek().value == "trim" && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::LParen) {
            return parseTrimExpr();
        }
        if (peek().type == TokenType::Identifier && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::LParen) {
            return parseFnCallExpr();
        }
        if (match(TokenType::Identifier)) {
            std::string name = tokens_[pos_ - 1].value;
            AstNode cur = parseDotChain({AstNode::Type::ExprVarRef, name, {}});
            if (match(TokenType::LBracket)) {
                if (cur.type != AstNode::Type::ExprVarRef) {
                    throw std::runtime_error("Only simple arrays support [] indexing at line " + std::to_string(peek().line));
                }
                AstNode idx = parseExpression();
                if (!match(TokenType::RBracket)) {
                    throw std::runtime_error("Expected ']' at line " + std::to_string(peek().line));
                }
                AstNode indexed{AstNode::Type::ExprArrayIndex, cur.value, {idx}};
                // Allow methods on an indexed element, e.g. parts[i].split("=").
                if (peek().type == TokenType::Dot) return parseDotChain(std::move(indexed));
                return indexed;
            }
            return cur;
        }
        if (match(TokenType::LParen)) {
            // Use the full top-level (parseLogicalOr) so parenthesized boolean expressions
            // such as (a && b) parse correctly even when used in arithmetic contexts.
            AstNode e = parseLogicalOr();
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
            }
            if (peek().type == TokenType::Dot) return parseDotChain(std::move(e));
            return e;
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
            argTok.type == TokenType::Float || argTok.type == TokenType::Char) {
            result.children.push_back(parseExpression());
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

    AstNode parseOsCall() {
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
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
            }
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
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after os." + method + "() at line " + std::to_string(peek().line));
            }
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
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after os." + method + "(...) at line " + std::to_string(peek().line));
            }
            AstNode::Type nodeType = (method == "set_brightness") ? AstNode::Type::OsSetBrightness : AstNode::Type::OsSetVolume;
            return {nodeType, "", {arg}};
        }
        if (method == "get_volume" || method == "get_brightness") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os." + method + " at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after os." + method + "() at line " + std::to_string(peek().line));
            }
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
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after os.clip_set(...) at line " + std::to_string(peek().line));
            }
            return {AstNode::Type::OsClipSet, "", {arg}};
        }
        if (method == "clip_get") {
            if (!match(TokenType::LParen) || !match(TokenType::RParen)) {
                throw std::runtime_error("Expected '()' after os.clip_get at line " + std::to_string(peek().line));
            }
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after os.clip_get() at line " + std::to_string(peek().line));
            }
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
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after os.notify(...) at line " + std::to_string(peek().line));
            }
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
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' after os.open(...) at line " + std::to_string(peek().line));
            }
            return {AstNode::Type::OsOpen, "", {arg}};
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
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
            }
            AstNode node{AstNode::Type::OsMessageBox, "", {textArg, titleArg}};
            return node;
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
            if (!match(TokenType::Semicolon)) {
                throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
            }
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
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        return result;
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

    AstNode parseFileReadOrExists(bool isRead) {
        if (!modules_.hasFile()) {
            throw std::runtime_error("file.read/file.exists requires #include <std/file> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "file") {
            throw std::runtime_error("Expected 'file' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != (isRead ? "read" : "exists")) {
            throw std::runtime_error(std::string("Expected file.") + (isRead ? "read" : "exists") + " at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode pathArg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {isRead ? AstNode::Type::FileRead : AstNode::Type::FileExists, "", {pathArg}};
    }

    AstNode parseFileMkdirExpr() {
        if (!modules_.hasFile()) {
            throw std::runtime_error("file.mkdir requires #include <std/file> at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "file") {
            throw std::runtime_error("Expected 'file' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Dot)) {
            throw std::runtime_error("Expected '.' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Identifier) || tokens_[pos_ - 1].value != "mkdir") {
            throw std::runtime_error("Expected 'mkdir' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode pathArg = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::FileMkdir, "", {pathArg}};
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
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        AstNode result{AstNode::Type::DllCall, symTok.value, {}};
        result.children.push_back({AstNode::Type::ExprVarRef, handleVar, {}});
        return result;
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
            throw std::runtime_error("Expected file method (read, write, append, exists, mkdir) at line " + std::to_string(methodTok.line));
        }
        std::string method = methodTok.value;
        if (method != "read" && method != "write" && method != "append" && method != "exists" && method != "mkdir") {
            throw std::runtime_error("Expected file.read, file.write, file.append, file.exists, or file.mkdir at line " + std::to_string(methodTok.line));
        }
        advance();
        if (!match(TokenType::LParen)) {
            throw std::runtime_error("Expected '(' at line " + std::to_string(peek().line));
        }
        AstNode pathArg = parseExpression();
        AstNode node{method == "read" ? AstNode::Type::FileRead : (method == "write" ? AstNode::Type::FileWrite :
            (method == "append" ? AstNode::Type::FileAppend : (method == "mkdir" ? AstNode::Type::FileMkdir : AstNode::Type::FileExists))), "", {}};
        node.children.push_back(pathArg);
        if (method == "write" || method == "append") {
            if (!match(TokenType::Comma)) {
                throw std::runtime_error("Expected ',' before content at line " + std::to_string(peek().line));
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
        const Token& fnTok = peek();
        if (fnTok.type != TokenType::Identifier) {
            throw std::runtime_error("thread.spawn expects a function name at line " + std::to_string(fnTok.line));
        }
        advance();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' at line " + std::to_string(peek().line));
        }
        return {AstNode::Type::ThreadSpawn, fnTok.value, {}};
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
        if (methodTok.type != TokenType::Identifier || methodTok.value != "join") {
            throw std::runtime_error("Expected thread.join at line " + std::to_string(methodTok.line));
        }
        advance();
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
                    (declType.size() >= 7 && declType.compare(0, 7, "struct:") == 0) ||
                    (declType.size() >= 5 && declType.compare(0, 5, "enum:") == 0)) {
                    throw std::runtime_error("Fixed array only supports int, char, or unsigned char at line " + std::to_string(peek().line));
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
            node.initIsInt = (declType == "int");
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
            // parseLogicalOr() also covers comparisons and arithmetic, so bool initializers
            // like `let pressed: bool = hover && mouse_down(MOUSE_LEFT);` parse correctly.
            node.children.push_back(parseLogicalOr());
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
            } else {
                node.initIsInt = !exprProducesString(b);
            }
        }
        if (declType.empty() && !node.children.empty() && astHasMemberAccess(node.children.back())) {
            throw std::runtime_error("let with '.' access requires an explicit type (e.g. let x: int = s.field or let x: enum E = E.A) at line " + std::to_string(line));
        }
        if (!declType.empty()) {
            node.declType = declType;
            node.initIsInt = (declType == "int");
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
        }
        if (!match(TokenType::Semicolon)) {
            throw std::runtime_error("Expected ';' at line " + std::to_string(peek().line));
        }
        node.isConst = isConstVar;
        return node;
    }
};

}  // namespace nexa
