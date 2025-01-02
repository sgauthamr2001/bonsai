#include "Parser/Parser.h"

#include "Parser/Lexer.h"
#include "Parser/Token.h"

#include "IR/Analysis.h"
#include "IR/IREquality.h"
#include "IR/IRPrinter.h"
#include "IR/Type.h"
#include "IR/TypeEnforcement.h"

#include <iostream>
#include <regex>
#include <sstream>

#include "Error.h"

namespace bonsai {
namespace parser {

namespace {

/*
program := program_element*
program_element := import_stmt | element_decl | interface_decl | extern_decl | func_decl
// TODO: support methods in elements? and interfaces / inheritance?
import_stmt := import name(/name)*;
element_decl := element IDENTIFIER ('{' (IDENTIFIER : type)+ '}') | '=' type ';'
interface_decl := interface IDENTIFIER {' func_decl+ '}'
extern_decl := extern IDENTIFIER : type;
func_decl := func IDENTIFIER '( (IDENTIFIER : type)+ ')'
             (-> type)?
             ('{' stmt '}' | '=' expr ';')

*/

struct Parser {
private:
    TokenStream tokens;
    ir::Program program;
    std::list<std::map<std::string, std::pair<ir::Type, bool>>> frames;

    ir::Type get_type_from_frame(const std::string &name) const {
        for (auto it = frames.rbegin(); it != frames.rend(); it++) {
            const auto &frame = *it;
            const auto &found = frame.find(name);
            if (found != frame.cend()) {
                return found->second.first;
            }
        }
        // internal_error << "Cannot check type of unknown var: " << name;
        return ir::Type();
    }

    bool name_in_scope(const std::string &name) const {
        for (auto it = frames.rbegin(); it != frames.rend(); it++) {
            const auto &frame = *it;
            const auto &found = frame.find(name);
            if (found != frame.cend()) {
                return true;
            }
        }
        return false;
    }

    bool is_mutable(const std::string &name) const {
        for (auto it = frames.rbegin(); it != frames.rend(); it++) {
            const auto &frame = *it;
            const auto &found = frame.find(name);
            if (found != frame.cend()) {
                return found->second.second;
            }
        }
        internal_error << "Cannot check mutability of unknown var: " << name;
        return false;
    }

    void add_type_to_frame(const std::string &name, ir::Type type, bool mut) {
        for (auto it = frames.rbegin(); it != frames.rend(); it++) {
            const auto &frame = *it;
            const auto &found = frame.find(name);
            internal_assert(found == frame.cend()) << name << " shadows another variable (of the same name)";
        }
        frames.back()[name] = {std::move(type), mut};
    }

    void modify_type_in_frame(const std::string &name, ir::Type type) {
        for (auto it = frames.rbegin(); it != frames.rend(); it++) {
            auto &frame = *it;
            auto found = frame.find(name);
            if (found != frame.end()) {
                internal_assert(!found->second.first.defined()) << "Attempt to modify defined type for name: " << name;
                found->second.first = std::move(type);
                return;
            }
        }
        internal_error << "Cannot modify_type of unknown var: " << name << " to type " << type;
    }

    void new_frame() {
        frames.emplace_back();
    }

    void end_frame() {
        frames.pop_back();
    }

public:
    Parser(TokenStream _tokens) : tokens(std::move(_tokens)) {
        // Add Intrinsic types!

    }

    ir::Program parseProgram() {
        internal_assert(frames.empty());
        new_frame();
        while (!tokens.empty()) {
            parseProgramElement();
        }
        end_frame();
        internal_assert(frames.empty());
        return std::move(program);
    }

private:
    Token peek(uint32_t k = 0) const { return tokens.peek(k); }

    std::optional<Token> consume(Token::Type type) {
        const Token token = peek();
  
        if (!tokens.consume(type)) {
            return {};
        }

        return token;
    }

    Token expect(Token::Type type) {
        if (auto token = consume(type)) {
            return *token;
        } else {
            internal_error << "Expected " << Token::tokenTypeString(type) << ", instead received: " + peek().toString() << " at line: " << peek().lineBegin;
            return Token{};
        }
    }

    void parseProgramElement() {
        switch (peek().type) {
            case Token::Type::IMPORT: return parseImport();
            case Token::Type::ELEMENT: return parseElement();
            case Token::Type::INTERFACE: return parseInterface();
            case Token::Type::EXTERN: return parseExtern();
            case Token::Type::FUNC: return parseFunction();
            default: {
                internal_error << "Failure in parseProgramElement: " + peek().toString();
            }
        }
    }

    void parseImport() {
        // Imports are essentially code inlining. Just load and parse the file.
        // TODO: support source/header separations?
        expect(Token::Type::IMPORT);
        const Token id = expect(Token::Type::IDENTIFIER);
        std::string name = std::get<std::string>(id.value);

        // Handle (/name)*
        while (!consume(Token::Type::SEMICOL)) {
            expect(Token::Type::SLASH);
            const Token path = expect(Token::Type::IDENTIFIER);
            name += "/" + std::get<std::string>(path.value);
        }

        name += ".bonsai";

        ir::Program imported;
        try {
            // TODO: recursive imports break everything. Fix this somehow?
             imported = parse(name);
        } catch (const std::exception& e) {
            internal_error << "Failure to parse imported file: " << name << " with message: " << e.what();
        }

        // TODO: disregard like Python does?
        internal_assert(!imported.main_body.defined()) << "Imported file: " << name << " contains main() function.";
        // Can't import externs, those are function arguments to the generated stub.
        internal_assert(imported.externs.empty()) << "Imported file: " << name << " contains externs.";

        for (auto& [fname, func] : imported.funcs) {
            internal_assert(!program.funcs.contains(fname)) << "Redefinition of function: " << fname << " from imported file: " << name;
            program.funcs[fname] = std::move(func);
        }

        for (auto& [tname, type] : imported.types) {
            internal_assert(!program.types.contains(tname)) << "Redefinition of type: " << tname << " from imported file: " << name;
            program.types[tname] = std::move(type);
        }        
    }

    void parseElement() {
        expect(Token::Type::ELEMENT);
        
        // TODO: support methods as well.
        // TODO: figure out overloading policy for that.
        // TODO: for error handling, should we have beginLoc/endLoc like Simit?
        const Token id = expect(Token::Type::IDENTIFIER);
        const std::string name = std::get<std::string>(id.value);

        internal_assert(!program.types.contains(name)) << "Redefinition of type: " << name << " on line " << id.lineBegin;

        // Support inline aliasing.
        if (consume(Token::Type::ASSIGN)) {
            ir::Type alias = parseType();
            expect(Token::Type::SEMICOL);
            program.types[name] = alias;
            return;
        }

        // Regular type declaration.
        ir::Struct_t::Map fields;
        expect(Token::Type::LSQUIGGLE);
        do {
            // Handle multiple names with a single type.
            std::vector<std::string> names;
            do {
                const Token field = expect(Token::Type::IDENTIFIER);
                const std::string field_name = std::get<std::string>(field.value);
                names.push_back(field_name);
            } while (consume(Token::Type::COMMA));

            expect(Token::Type::COL);
            ir::Type type = parseType();
            for (const auto& field_name : names) {
                internal_assert(fields.cend() == std::find_if(fields.cbegin(), fields.cend(), [&field_name](const auto &p) { return p.first == field_name; }))
                    << "Duplicate field name: " << field_name << " in element definition: " << name;
                fields.emplace_back(field_name, type);
            }
        } while (!consume(Token::Type::RSQUIGGLE));

        ir::Type element = ir::Struct_t::make(name, fields);
        program.types[name] = element;
    }

    void parseInterface() {
        internal_error << "TODO: implement parseInterface";
    }

    void parseExtern() {
        // TODO: what if an extern name-conflicts with a type or something? probably should check conflicts for all symbols?
        expect(Token::Type::EXTERN);
        const Token id = expect(Token::Type::IDENTIFIER);
        const std::string name = std::get<std::string>(id.value);
        internal_assert(program.externs.cend() == std::find_if(program.externs.cbegin(), program.externs.cend(), [&name](const auto &p) { return p.first == name; }))
            << "Redefinition of extern: " << name << " on line " << id.lineBegin;
        // TODO: should we support defaults? that makes passing in args harder.
        expect(Token::Type::COL);
        ir::Type type = parseType();
        expect(Token::Type::SEMICOL);
        add_type_to_frame(name, type, /* mutable */ false);
        program.externs.emplace_back(name, std::move(type));
    }

    void parseFunction() {
        expect(Token::Type::FUNC);
        const Token id = expect(Token::Type::IDENTIFIER);
        const std::string name = std::get<std::string>(id.value);
        internal_assert(!program.funcs.contains(name)) << "Redefinition of func: " << name << " on line "  << id.lineBegin;
        expect(Token::Type::LPAREN);
    
        new_frame();
        std::vector<ir::Function::Argument> args;
        if (peek().type != Token::Type::RPAREN) {
            // parse arg list
            do {
                // TODO: can we accept multiple args with one type here, as in element definitions?
                const Token arg_id = expect(Token::Type::IDENTIFIER);
                const std::string arg_name = std::get<std::string>(arg_id.value);
                expect(Token::Type::COL);
                // TODO: handle `mut`! Use ParseNameDef?
                ir::Type type = parseType();

                ir::Expr default_value;

                if (consume(Token::Type::ASSIGN)) {
                    // Optional default value.
                    // TODO: default value may need to be cast to type.
                    // For now, assume that's done in type inference.
                    // TODO: this should not perform computation!
                    // Can we easily prevent that? Enforce is_constant?
                    default_value = parseExpr();
                    internal_assert(ir::is_constant_expr(default_value))
                        << "Function default values must be constants, received: " << default_value << " for argument " << arg_name << " of func " << name;
                }

                add_type_to_frame(arg_name, type, /* mutable */ false); // TODO: handle mutable args in functions.
                args.push_back(ir::Function::Argument{arg_name, std::move(type), std::move(default_value)});
            } while (consume(Token::Type::COMMA));
        }
        expect(Token::Type::RPAREN);

        // Optional RARROW with return_type: otherwise, requires type inference!
        ir::Type ret_type;
        if (consume(Token::Type::RARROW)) {
            ret_type = parseType();
        }
        const bool ret_type_set = ret_type.defined();

        // Functions can either be inlined: '=' Expr;
        // or with '{' body '}'

        // To support recursive functions, we need to insert at least
        // the type of the function into the program now. Note that this
        // can break if the return type is not defined! We should assert
        // that the return type is defined for recursive calls, or implement
        // really good type unification or something.
        {
            ir::Function func(name, std::move(args), std::move(ret_type), ir::Stmt());
            program.funcs[name] = std::move(func);
        }

        ir::Stmt body;

        // TODO: the syntax of -> type = expr is ugly, maybe disallow, and just do inference?

        if (consume(Token::Type::ASSIGN)) {
            ir::Expr expr = parseExpr();
            if (!ret_type_set && expr.type().defined()) {
                ret_type = expr.type();
            }
            expect(Token::Type::SEMICOL);
            body = ir::Return::make(expr);
        } else {
            body = parseSequence();
            // TODO: do type checking if they don't match?
            if (!ret_type_set) {
                ret_type = ir::get_return_type(body);
            }
        }

        end_frame();

        internal_assert(!program.funcs[name].body.defined()) << "Woah, how did " << name << " get a function body before being parsed?";

        program.funcs[name].body = std::move(body);
        if (!ret_type_set && ret_type.defined()) {
            // we were able to statically infer the return type
            program.funcs[name].ret_type = std::move(ret_type);
        }
    }

    ir::Stmt parseSequence() {
        std::vector<ir::Stmt> stmts;
        expect(Token::Type::LSQUIGGLE);
        // brackets enclose a new frame!
        new_frame();

        while (!consume(Token::Type::RSQUIGGLE)) {
            stmts.push_back(parseStmt());
        }
        // close the frame.
        end_frame();
        internal_assert(!stmts.empty()) << "Failed to parse a Sequence at line: " << peek().lineBegin; // TODO: this fails when the stream is empty
        if (stmts.size() == 1) {
            return std::move(stmts[0]);
        } else {
            return ir::Sequence::make(std::move(stmts));
        }
    }

    // if expr stmt [elif expr stmt]* [else stmt]?
    // return expr;
    // TODO: allow labels? or inline funcs? of some kind.
    // { stmt* }
    // TODO: allow mut modifier!
    // name [: type]? =  expr;
    // name += expr; ?
    // call? no bc no side effects...
    // for? not while.
    ir::Stmt parseStmt() {
        if (peek().type == Token::Type::LSQUIGGLE) {
            return parseSequence();
        } else if (consume(Token::Type::IF)) {
            ir::Expr cond = parseExpr(); // no required parens
            ir::Stmt then_case = parseStmt();
            internal_assert(!consume(Token::Type::ELIF)) << "TODO: implement elif parsing for line: " << peek().lineBegin;
            if (consume(Token::Type::ELSE)) {
                ir::Stmt else_case = parseStmt();
                return ir::IfElse::make(std::move(cond), std::move(then_case), std::move(else_case));
            } else {
                return ir::IfElse::make(std::move(cond), std::move(then_case));
            }
        } else if (consume(Token::Type::RETURN)) {
            ir::Expr ret = parseExpr();
            expect(Token::Type::SEMICOL);
            return ir::Return::make(std::move(ret));
        } else if (peek().type == Token::Type::IDENTIFIER) {
            // TODO: allow tuple declaration/assignment?
            // TODO: how to do SSA in parsing?
            auto def = parseNameDef<false, false>(true);
            internal_assert(def.value.defined()) << "Expected expr assignment for name: " << def.name << " at line: " << peek().lineBegin;
            expect(Token::Type::SEMICOL);
            // TODO: do type-forcing here!
            if (def.type.defined() && def.value.type().defined()) {
                internal_assert(ir::equals(def.type, def.value.type()))
                    << "Mismatching type: " << def.name << " is labelled with type: " << def.type
                    << " but " << def.value << " has type " << def.value.type();
            }

            bool mutating = false;

            if (name_in_scope(def.name)) {
                internal_assert(is_mutable(def.name)) << "Variable: " << def.name << "cannot be reassigned, it is not mutable.";
                const ir::Type type = get_type_from_frame(def.name);
                if (type.defined() && def.type.defined()) {
                    internal_assert(ir::equals(type, def.type)) << "Mutable reassignment of " << def.name << " cannot change type from " << type << " to " << def.type;
                }
                // TODO: if def.type is not defined but type is, push it for type inference.
                //       Same with the reverse scenario.
                if (!type.defined() && def.type.defined()) {
                    modify_type_in_frame(def.name, def.type);
                } else if (!type.defined() && def.value.type().defined()) {
                    modify_type_in_frame(def.name, def.value.type());
                }
                // TODO: should this be a different IR construct?
                // probably, to make analysis easier...
                // internal_error << "TODO: handle mutable re-assignments: " << def.name;
                mutating = true;
            } else {
                // TODO: what to do if type is currently undefined?
                if (def.type.defined()) {
                    add_type_to_frame(def.name, def.type, def.mut);
                } else {
                    add_type_to_frame(def.name, def.value.type(), def.mut);
                }
            }

            // TODO: allow mut! add mut to state somewhere.
            return ir::LetStmt::make(def.name, std::move(def.value), mutating);
        }
        internal_error << "TODO: implement parseStmt for " << peek().toString();
        return ir::Stmt();
    }

    // We follow C++'s operator precedence.
    // https://en.cppreference.com/w/cpp/language/operator_precedence

    // TODO: precedence 2: member access...
    // precedence 5: mul, div, mod
    // precedence 6: addition/subtraction
    // precedence 7: bit shifts (TODO: support)
    // precedence 9: relational operators
    // precedence 10: equality operators
    // TODO: we parse and/xor/or and treat them all the same for now.
    // precedence 11: bitwise and (TODO: support)
    // precedence 12: bitwise xor (TODO: support)
    // precedence 13: bitwise or (TODO: support)
    // precedence 14: logical and
    // precedence 15: logical or

    // TODO: parse member access first somehow...?
    // expr := muldivmod_expr
    ir::Expr parseExpr() {
        return parseMulDivMod();
    }

    struct BinOperator {
        const ir::BinOp::OpType op;
        const Token::Type token;
        const bool flip = false;
    };

    template<size_t N, typename F>
    ir::Expr parseBinOpWithPrecedence(F parseSubExpr, const std::array<BinOperator, N> &ops) {
        ir::Expr expr = parseSubExpr();
        while (true) {
            // Find the first matching operation
            auto it = std::find_if(ops.begin(), ops.end(), [this](const auto &op) {
                return consume(op.token);
            });

            // If no operation is found, we're done here.
            if (it == ops.end()) {
                return expr;
            }

            // Parse the right-hand side and create the binary operation
            ir::Expr rhs = parseSubExpr();
            if (it->flip) {
                expr = ir::BinOp::make(it->op, std::move(rhs), std::move(expr));
            } else {
                expr = ir::BinOp::make(it->op, std::move(expr), std::move(rhs));
            }
        }
    }

    // muldivmod_expr := addsub_expr (('*' | '/' | '%') addsub_expr)*
    ir::Expr parseMulDivMod() {
        return parseBinOpWithPrecedence<3>(
            [this]() { return parseAddSub(); },
            {{ {ir::BinOp::Mul, Token::Type::STAR},
               {ir::BinOp::Div, Token::Type::SLASH},
               {ir::BinOp::Mod, Token::Type::MOD}, }});
    }

    // addsub_expr := rel_expr (('+' | '-') rel_expr)*
    ir::Expr parseAddSub() {
        return parseBinOpWithPrecedence<2>(
            [this]() { return parseRels(); },
            {{ {ir::BinOp::Add, Token::Type::PLUS},
               {ir::BinOp::Sub, Token::Type::MINUS}, }});
    }

    // rel_expr := eq_expr (('<=' | '<') eq_expr)*
    ir::Expr parseRels() {
        return parseBinOpWithPrecedence<4>(
            [this]() { return parseEqs(); },
            {{ {ir::BinOp::Lt, Token::Type::LT},
               {ir::BinOp::Le, Token::Type::LEQ},
               {ir::BinOp::Lt, Token::Type::GT, /* flip */ true},
               {ir::BinOp::Le, Token::Type::GEQ, /* flip */ true},}});
    }

    // eq_expr := and_expr (('==' | '!=') and_expr)*
    ir::Expr parseEqs() {
        return parseBinOpWithPrecedence<2>(
            [this]() { return parseAnd(); },
            {{ {ir::BinOp::Eq, Token::Type::EQ},
               {ir::BinOp::Neq, Token::Type::NEQ}, }});
    }

    // and_expr := xor_expr ('^' xor_expr)*
    ir::Expr parseAnd() {
        return parseBinOpWithPrecedence<1>(
            [this]() { return parseXor(); },
            {{ {ir::BinOp::And, Token::Type::AND} }});
    }

    // xor_expr := or_expr ('^' or_expr)*
    ir::Expr parseXor() {
        return parseBinOpWithPrecedence<1>(
            [this]() { return parseOr(); },
            {{ {ir::BinOp::Xor, Token::Type::XOR} }});
    }

    // or_expr := base_expr ('||' base_expr)*
    ir::Expr parseOr() {
        return parseBinOpWithPrecedence<1>(
            // TODO: logical and!
            [this]() { return parseBaseExpr(); },
            {{ {ir::BinOp::Or, Token::Type::OR} }});
    }

    // base_expr := '(' expr ')' | name (('.' field) | ('[' index (, index)* ']' | () ) | lambda args? : expr
    ir::Expr parseBaseExpr() {
        if (consume(Token::Type::LPAREN)) {
            ir::Expr inner = parseExpr();
            expect(Token::Type::RPAREN);
            return inner;
        // TODO: do these have the correct precedence?
        } else if (consume(Token::Type::MINUS)) {
            ir::Expr inner = parseExpr();
            return ir::UnOp::make(ir::UnOp::Neg, std::move(inner));
        } else if (consume(Token::Type::NOT)) {
            ir::Expr inner = parseExpr();
            return ir::UnOp::make(ir::UnOp::Not, std::move(inner));
        } else if (peek().type == Token::Type::IDENTIFIER) {
            const Token token = expect(Token::Type::IDENTIFIER);
            const std::string name = std::get<std::string>(token.value);
            std::vector<std::string> fields; // possibly nested
            while (consume(Token::Type::PERIOD)) {
                // Member access.
                const Token field_token = expect(Token::Type::IDENTIFIER);
                const std::string field_name = std::get<std::string>(field_token.value);
                fields.push_back(field_name);
            }

            // Just a variable, possibly with field accesses.
            // the type might have been provided already, or
            // might need to be inferred later.
            ir::Type var_type = get_type_from_frame(name); // possibly undefined.
            ir::Expr expr = ir::Var::make(var_type, name);

            for (const auto &field : fields) {
                expr = ir::Access::make(field, expr);
            }

            if (consume(Token::Type::LPAREN)) {
                std::vector<ir::Expr> args = parseExprListUntil(Token::Type::RPAREN);

                // Intrinsics/set operations
                if (fields.empty()) {
                    if (name == "abs") {
                        internal_assert(args.size() == 1) << "abs takes a single argument, received: " << args.size();
                        return ir::Intrinsic::make(ir::Intrinsic::abs, std::move(args));
                    } else if (name == "sqrt") {
                        internal_assert(args.size() == 1) << "sqrt takes a single argument, received: " << args.size();
                        return ir::Intrinsic::make(ir::Intrinsic::sqrt, std::move(args));
                    } else if (name == "sin") {
                        internal_assert(args.size() == 1) << "sin takes a single argument, received: " << args.size();
                        return ir::Intrinsic::make(ir::Intrinsic::sin, std::move(args));
                    } else if (name == "cos") {
                        internal_assert(args.size() == 1) << "cos takes a single argument, received: " << args.size();
                        return ir::Intrinsic::make(ir::Intrinsic::cos, std::move(args));
                    } else if (name == "cross") {
                        internal_assert(args.size() == 2) << "cross takes two arguments, received: " << args.size();
                        return ir::Intrinsic::make(ir::Intrinsic::cos, std::move(args));
                    } else if (name == "argmin") {
                        internal_assert(args.size() == 2) << "argmin takes two arguments, received: " << args.size();
                        return ir::SetOp::make(ir::SetOp::argmin, std::move(args[0]), std::move(args[1]));
                    } else if (name == "filter") {
                        internal_assert(args.size() == 2) << "filter takes two arguments, received: " << args.size();
                        return ir::SetOp::make(ir::SetOp::filter, std::move(args[0]), std::move(args[1]));
                    } else if (name == "map") {
                        internal_assert(args.size() == 2) << "map takes two arguments, received: " << args.size();
                        return ir::SetOp::make(ir::SetOp::map, std::move(args[0]), std::move(args[1]));
                    } else if (name == "product") {
                        internal_assert(args.size() == 2) << "product takes two arguments, received: " << args.size();
                        return ir::SetOp::make(ir::SetOp::product, std::move(args[0]), std::move(args[1]));
                    } else if (name == "distance") {
                        internal_assert(args.size() == 2) << "distance takes two arguments, received: " << args.size();
                        return ir::GeomOp::make(ir::GeomOp::distance, std::move(args[0]), std::move(args[1]));
                    } else if (name == "intersects") {
                        internal_assert(args.size() == 2) << "intersects takes two arguments, received: " << args.size();
                        return ir::GeomOp::make(ir::GeomOp::intersects, std::move(args[0]), std::move(args[1]));
                    } else if (name == "contains") {
                        internal_assert(args.size() == 2) << "contains takes two arguments, received: " << args.size();
                        return ir::GeomOp::make(ir::GeomOp::contains, std::move(args[0]), std::move(args[1]));
                    } else if (name == "sum") {
                        internal_assert(args.size() == 1) << "sum takes a single argument, received: " << args.size();
                        return ir::VectorReduce::make(ir::VectorReduce::Add, std::move(args[0]));
                    } else if (name == "idxmin") {
                        internal_assert(args.size() == 1) << "idxmin takes a single argument, received: " << args.size();
                        return ir::VectorReduce::make(ir::VectorReduce::Idxmin, std::move(args[0]));
                    } else if (name == "idxmax") {
                        internal_assert(args.size() == 1) << "idxmax takes a single argument, received: " << args.size();
                        return ir::VectorReduce::make(ir::VectorReduce::Idxmax, std::move(args[0]));
                    } else if (name == "permute") {
                        internal_assert(args.size() == 2) << "permute takes two arguments, received: " << args.size();
                        internal_assert(args[1].is<ir::Build>()) << "permute expects the second argument to be a list of indexes, instead received: " << args[1];
                        return ir::VectorShuffle::make(std::move(args[0]), args[1].as<ir::Build>()->values);
                    } else {
                        if (program.funcs.contains(name)) {
                            ir::Type ftype;
                            const ir::Function &func = program.funcs[name];
                            // TODO: handle default params!
                            internal_assert(args.size() == func.args.size())
                                << "Call to: " << name << " at line " << token.lineBegin << " has incorrect number of arguments.\n"
                                << "Expected: " << func.args.size() << " but parsed " << args.size();

                            if (func.ret_type.defined()) {
                                // Argument types are always required, but the return type could not be.
                                std::vector<ir::Type> arg_types(func.args.size());
                                for (size_t i = 0; i < func.args.size(); i++) {
                                    arg_types[i] = func.args[i].type;
                                    // TODO: we could push types down here, because we know the arg types.
                                    // That mixes type inference with parsing though, not sure we want that.
                                    internal_assert(!args[i].type().defined() || ir::equals(func.args[i].type, args[i].type()))
                                        << "Argument " << i << " of call to function " << name << " on line " << token.lineBegin
                                        << " has incorrect type. Expected " << func.args[i].type << " but parsed: " << args[i].type();
                                }
                                ftype = ir::Function_t::make(func.ret_type, arg_types);
                            }
                            // TODO: if we allowed partially-defined types, we could make: Fn(arg_types) -> (undef)
                            // that would make type inference easier, but we'd have to change a lot of the error
                            // handling in IR/Type.cpp
                            // For now, leave ftype undefined in the else case
                            ir::Expr f = ir::Var::make(ftype, name);
                            return ir::Call::make(std::move(f), std::move(args));
                        }
                        // Not intrinsic or set op, not sure what this is.
                        // TODO: could be a ctor of a type?
                        internal_error << "Unknown function call " << name;
                        return ir::Expr();
                    }
                } else {
                    // method access!
                    // TODO: type inference via interface?
                    return ir::Call::make(std::move(expr), std::move(args));
                }
            }

            internal_assert(!consume(Token::Type::LBRACKET))
                << "TODO: this is probably a vector index, which the IR does not support yet: " << peek().toString();

            return expr;
        // Parse literals.
        // } else if (consume(Token::Type::TRUE)) {
            // return BoolImm::make(true);
        // } else if (consume(Token::Type::FALSE)) {
            // return BoolImm::make(false);
        } else if (peek().type == Token::Type::INT_LITERAL) {
            // can't know concrete type yet, let type inference figure it out.
            const Token token = expect(Token::Type::INT_LITERAL);
            const int64_t value = std::get<int64_t>(token.value);
            return ir::IntImm::make(ir::Type(), value);
        } else if (peek().type == Token::Type::UINT_LITERAL) {
            // can't know concrete type yet, let type inference figure it out.
            const Token token = expect(Token::Type::UINT_LITERAL);
            const uint64_t value = std::get<uint64_t>(token.value);
            return ir::UIntImm::make(ir::Type(), value);
        } else if (peek().type == Token::Type::FLOAT_LITERAL) {
            // can't know concrete type yet, let type inference figure it out.
            const Token token = expect(Token::Type::FLOAT_LITERAL);
            const double value = std::get<double>(token.value);
            std::cout << "making FloatImm of value: " << value;
            return ir::FloatImm::make(ir::Type(), value);
        } else if (consume(Token::Type::LAMBDA)) {
            std::vector<ir::Lambda::Argument> args = parseLambdaArgs();
            new_frame();
            for (const auto &arg : args) {
                add_type_to_frame(arg.name, arg.type, /* mutable */ false);
            }
            ir::Expr expr = parseExpr();
            end_frame();
            return ir::Lambda::make(std::move(args), std::move(expr));
        } else if (consume(Token::Type::LSQUIGGLE)) {
            // Could be a build or an empty
            // TODO: is there an easy way to always know the type?
            if (consume(Token::Type::RSQUIGGLE)) {
                // Empty!
                return ir::Build::make(ir::Type(), {});
            } else {
                std::vector<ir::Expr> args = parseExprListUntil(Token::Type::RSQUIGGLE);
                // TODO: can we know the type?
                return ir::Build::make(ir::Type(), std::move(args));
            }
            // TODO: should also support e.g. Type{} notation.
        } else {
            internal_error << "Unknown token in parseBaseExpr: " << peek().toString() << " at line: " << peek().lineBegin;
            return ir::Expr();
        }
    }

    std::vector<ir::Expr> parseExprListUntil(const Token::Type &token) {
        std::vector<ir::Expr> exprs;
        if (consume(token)) {
            return exprs;
        }
        do {
            ir::Expr expr = parseExpr();
            exprs.emplace_back(std::move(expr));
        } while (consume(Token::Type::COMMA));
        expect(token);
        return exprs;
    }

    std::vector<ir::Lambda::Argument> parseLambdaArgs() {
        // arg := name (':' type)?
        // args := arg (',' arg)*
        // TODO: should we allow no arg lambdas?
        // not sure I want that for now. doesn't
        // that imply some sort of side effects?
        // but maybe we need that for rng?
        std::vector<ir::Lambda::Argument> args;
        do {
            auto def = parseNameDef<false, true>(false);
            internal_assert(!def.value.defined()) << "Lambdas cannot have default function values! " << def.name << " assigned default: " << def.value;
            internal_assert(!def.mut) << "TODO: support mutable arguments in lambdas. Argument: " << def.name << " marked as mutable.";
            args.push_back({std::move(def.name), std::move(def.type)});
        } while (!consume(Token::Type::ASSIGN));
        return args;
    }

    struct NameDef {
        std::string name;
        ir::Type type;    // optional
        ir::Expr value;   // optional
        bool mut = false;
    };

    // TODO: allow `mut` flag!
    template<bool T_REQUIRED, bool E_CONST>
    NameDef parseNameDef(const bool expr_allowed) {
        NameDef def;

        const Token token = expect(Token::Type::IDENTIFIER);
        def.name = std::get<std::string>(token.value);

        if constexpr (T_REQUIRED) {
            expect(Token::Type::COL);
            if (consume(Token::Type::MUT)) {
                def.mut = true;
            }
            def.type = parseType();
        } else if (consume(Token::Type::COL)) {
            if (consume(Token::Type::MUT)) {
                def.mut = true;
                // might have no type label, just mut
                if (peek().type != Token::Type::ASSIGN) {
                    def.type = parseType();
                }
            } else {
                // no mut label, must be type.
                def.type = parseType();
            }
        }

        if (expr_allowed && consume(Token::Type::ASSIGN)) {
            def.value = parseExpr();
            if constexpr (E_CONST) {
                internal_assert(ir::is_constant_expr(def.value))
                    << "Expected constant value for name: " << def.name << " but instead received: " << def.value;
            }
        }
        return def;
    }

    // type := i[N] | u[N] | f[N] | bool | vector[type, int] | option[type] | declared_type
    ir::Type parseType() {
        // TODO: support tuples of types! AKA unnamed structs.
        const Token id = expect(Token::Type::IDENTIFIER);
        const std::string name = std::get<std::string>(id.value);

        // First look for numeric types
        std::regex int_pattern("^i(\\d+)$");
        std::regex uint_pattern("^u(\\d+)$");
        std::regex float_pattern("^f(\\d+)$");
        std::smatch match;

        if (std::regex_match(name, match, int_pattern)) {
            const uint32_t bits = std::stoul(match[1].str());
            return ir::Int_t::make(bits);
        } else if (std::regex_match(name, match, uint_pattern)) {
            const uint32_t bits = std::stoul(match[1].str());
            return ir::UInt_t::make(bits);
        } else if (std::regex_match(name, match, float_pattern)) {
            const uint32_t bits = std::stoul(match[1].str());
            return ir::Float_t::make(bits);
        } else if (name == "bool") {
            return ir::Bool_t::make();
        }
        // Now look for built-ins
        else if (name == "vector") {
            expect(Token::Type::LBRACKET);
            ir::Type etype = parseType();
            expect(Token::Type::COMMA);
            const int64_t lanes = parseIntLiteral();
            // TODO: this upper bound is arbitrary. Can't imagine needing a larger one though?
            internal_assert(lanes > 0 && lanes < 1025) << "Vector lane count is invalid: " << lanes;
            expect(Token::Type::RBRACKET);
            return ir::Vector_t::make(std::move(etype), static_cast<uint32_t>(lanes));
        } else if (name == "option") {
            expect(Token::Type::LBRACKET);
            ir::Type etype = parseType();
            expect(Token::Type::RBRACKET);
            internal_assert(!etype.is_bool()) << "Bonsai does not support option[bool] because the semantics are confusing.";
            return ir::Option_t::make(std::move(etype));
        } else if (name == "set") {
            expect(Token::Type::LBRACKET);
            ir::Type etype = parseType();
            expect(Token::Type::RBRACKET);
            // TODO: assert etype is a struct_t? or volume_t?
            return ir::Set_t::make(std::move(etype));
        } else {
            // Must be a user-defined type, or an error.
            internal_assert(program.types.contains(name)) << "Unknown type: " << name;
            return program.types[name];
        }
    }

    int64_t parseIntLiteral() {
        // TODO: might need a "tryParseIntLiteral"...
        const Token _int = expect(Token::Type::INT_LITERAL);
        return std::get<int64_t>(_int.value);
    }
};

}



ir::Program parse(const std::string &filename) {
    TokenStream tokens = lex(filename);
    // Don't enforce types when building ASTs, might need to do some inference.
    ir::global_disable_type_enforcement();
    return Parser(tokens).parseProgram();
}

}  // namespace parser
}  // namespace bonsai
