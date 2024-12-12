#include "Parser/Parser.h"

#include "Parser/Lexer.h"
#include "Parser/Token.h"

#include "IR/Analysis.h"
#include "IR/IRPrinter.h"
#include "IR/TypeEnforcement.h"
#include "IR/Type.h"

#include <iostream>
#include <regex>
#include <sstream>

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
    std::list<std::map<std::string, ir::Type>> frames;

    ir::Type get_type_from_frame(const std::string &name) const {
        for (auto it = frames.rbegin(); it != frames.rend(); it++) {
            const auto &frame = *it;
            const auto &found = frame.find(name);
            if (found != frame.cend()) {
                return found->second;
            }
        }
        return ir::Type();
    }

    void add_type_to_frame(const std::string &name, ir::Type type) {
        for (auto it = frames.rbegin(); it != frames.rend(); it++) {
            const auto &frame = *it;
            const auto &found = frame.find(name);
            if (found != frame.cend()) {
                throw std::runtime_error("Found variable shadowing in name: " + name);
            }
        }
        frames.back()[name] = std::move(type);
    }

    void new_frame() {
        frames.emplace_back();
    }

    void end_frame() {
        frames.pop_back();
    }

public:
    Parser(TokenStream _tokens) : tokens(std::move(_tokens)) {
    }

    ir::Program parseProgram() {
        assert(frames.empty());
        new_frame();
        while (!tokens.empty()) {
            parseProgramElement();
        }
        end_frame();
        assert(frames.empty());
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
            std::string errMsg = "Expected " + Token::tokenTypeString(type) + ", instead received: " + peek().toString() + "\n";
            throw std::runtime_error(errMsg);
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
                throw std::runtime_error("Failure in parseProgramElement: " + peek().toString());
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
             ir::global_disable_type_enforcement(); // parsing re-eneables type enforcement.
        } catch (const std::exception& e) {
            throw std::runtime_error("Parsing imported file (" + name + ") failed: " + e.what());
        }

        if (imported.main_body.defined()) {
            throw std::runtime_error("Imported file: " + name + " contains main() function.");
        }

        if (!imported.externs.empty()) {
            throw std::runtime_error("Imported file: " + name + " contains externs.");
        }

        for (auto& [name, func] : imported.funcs) {
            if (program.funcs.contains(name)) {
                throw std::runtime_error("Redefinition of function: " + name + " from imported file: " + name);
            }
            program.funcs[name] = std::move(func);
        }

        for (auto& [name, type] : imported.types) {
            if (program.types.contains(name)) {
                throw std::runtime_error("Redefinition of type: " + name + " from imported file: " + name);
            }
            program.types[name] = std::move(type);
        }        
    }

    void parseElement() {
        expect(Token::Type::ELEMENT);
        
        // TODO: support methods as well.
        // TODO: figure out overloading policy for that.
        // TODO: for error handling, should we have beginLoc/endLoc like Simit?
        const Token id = expect(Token::Type::IDENTIFIER);
        const std::string name = std::get<std::string>(id.value);

        if (program.types.contains(name)) {
            throw std::runtime_error("Redefinition of type: " + name + " on line " + std::to_string(id.lineBegin));
        }

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
                if (fields.cend() != std::find_if(fields.cbegin(), fields.cend(), [&field_name](const auto &p) { return p.first == field_name; })) {
                    throw std::runtime_error("Duplicate field name: " + field_name + " in element definition: " + name);
                }
                fields.emplace_back(field_name, type);
            }
        } while (!consume(Token::Type::RSQUIGGLE));

        ir::Type element = ir::Struct_t::make(name, fields);
        program.types[name] = element;
    }

    void parseInterface() {
        throw std::runtime_error("TODO: implement parseInterface");
    }

    void parseExtern() {
        // TODO: what if an extern name-conflicts with a type or something? probably should check conflicts for all symbols?
        expect(Token::Type::EXTERN);
        const Token id = expect(Token::Type::IDENTIFIER);
        const std::string name = std::get<std::string>(id.value);
        if (program.externs.cend() != std::find_if(program.externs.cbegin(), program.externs.cend(), [&name](const auto &p) { return p.first == name; })) {
            throw std::runtime_error("Redefinition of extern: " + name + " on line " + std::to_string(id.lineBegin));
        }
        // TODO: should we support defaults? that makes passing in args harder.
        expect(Token::Type::COL);
        ir::Type type = parseType();
        expect(Token::Type::SEMICOL);
        add_type_to_frame(name, type);
        program.externs.emplace_back(name, std::move(type));
    }

    void parseFunction() {
        expect(Token::Type::FUNC);
        const Token id = expect(Token::Type::IDENTIFIER);
        const std::string name = std::get<std::string>(id.value);
        if (program.funcs.contains(name)) {
            throw std::runtime_error("Redefinition of func: " + name + " on line " + std::to_string(id.lineBegin));
        }
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
                ir::Type type = parseType();

                ir::Expr default_value;

                if (consume(Token::Type::ASSIGN)) {
                    // Optional default value.
                    // TODO: default value may need to be cast to type.
                    // For now, assume that's done in type inference.
                    // TODO: this should not perform computation!
                    // Can we easily prevent that? Enforce is_constant?
                    default_value = parseExpr();
                    if (!ir::is_constant_expr(default_value)) {
                        throw std::runtime_error("Function default values must be constants, received: " + ir::to_string(default_value) + " for argument " + arg_name + " of func " + name);
                    }
                }

                add_type_to_frame(arg_name, type);
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
            expect(Token::Type::LSQUIGGLE);
            body = parseStmt();
            if (!ret_type_set) {
                ret_type = ir::get_return_type(body);
            }
            expect(Token::Type::RSQUIGGLE);
        }

        end_frame();

        if (program.funcs[name].body.defined()) {
            throw std::runtime_error("Woah, how did " + name + " get a function body before being parsed?");
        }
        program.funcs[name].body = std::move(body);
        if (!ret_type_set && ret_type.defined()) {
            // we were able to statically infer the return type
            program.funcs[name].return_t = std::move(ret_type);
        }
    }

    ir::Stmt parseStmt() {
        throw std::runtime_error("TODO: implement parseStmt");
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

    // rel_expr := eq_expr (('<-' | '<') eq_expr)*
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

    // base_expr := '(' expr ')' | name (('.' field) | ('[' index (, index)* ']' | () )
    ir::Expr parseBaseExpr() {
        // TODO: parse lambda
        if (consume(Token::Type::LPAREN)) {
            ir::Expr inner = parseExpr();
            expect(Token::Type::RPAREN);
            return inner;
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
                        // TODO: asserts should always be enabled!
                        assert(args.size() == 1);
                        return ir::Intrinsic::make(ir::Intrinsic::abs, std::move(args[0]));
                    } else if (name == "sqrt") {
                        assert(args.size() == 1);
                        return ir::Intrinsic::make(ir::Intrinsic::sqrt, std::move(args[0]));
                    } else if (name == "sin") {
                        assert(args.size() == 1);
                        return ir::Intrinsic::make(ir::Intrinsic::sin, std::move(args[0]));
                    } else if (name == "cos") {
                        assert(args.size() == 1);
                        return ir::Intrinsic::make(ir::Intrinsic::cos, std::move(args[0]));
                    } else if (name == "argmin") {
                        assert(args.size() == 2);
                        return ir::SetOp::make(ir::SetOp::argmin, std::move(args[0]), std::move(args[1]));
                    } else if (name == "filter") {
                        assert(args.size() == 2);
                        return ir::SetOp::make(ir::SetOp::filter, std::move(args[0]), std::move(args[1]));
                    } else if (name == "map") {
                        assert(args.size() == 2);
                        return ir::SetOp::make(ir::SetOp::map, std::move(args[0]), std::move(args[1]));
                    } else if (name == "filter") {
                        assert(args.size() == 2);
                        return ir::SetOp::make(ir::SetOp::filter, std::move(args[0]), std::move(args[1]));
                    } else if (name == "distance") {
                        assert(args.size() == 2);
                        return ir::GeomOp::make(ir::GeomOp::distance, std::move(args[0]), std::move(args[1]));
                    } else if (name == "intersects") {
                        assert(args.size() == 2);
                        return ir::GeomOp::make(ir::GeomOp::intersects, std::move(args[0]), std::move(args[1]));
                    } else if (name == "contains") {
                        assert(args.size() == 2);
                        return ir::GeomOp::make(ir::GeomOp::contains, std::move(args[0]), std::move(args[1]));
                    } else {
                        // Not intrinsic or set op, this should check the program's function list and produce a Call to that func
                        throw std::runtime_error("TODO: need to implement Function as an Expr for the purpose of parsing: " + name);
                    }
                } else {
                    // method access!
                    // TODO: type inference via interface?
                    return ir::Call::make(ir::Type(), std::move(expr), std::move(args));
                }
            }

            if (consume(Token::Type::LBRACKET)) {
                throw std::runtime_error("TODO: this is probably a vector index, which the IR does not support yet: " + peek().toString());
            }

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
        // TODO: float and uint literals!
        } else if (peek().type == Token::Type::LAMBDA) {
            expect(Token::Type::LAMBDA);
            std::vector<ir::Lambda::Argument> args = parseLambdaArgs();
            new_frame();
            for (const auto &arg : args) {
                add_type_to_frame(arg.name, arg.type);
            }
            ir::Expr expr = parseExpr();
            end_frame();
            return ir::Lambda::make(std::move(args), std::move(expr));
        } else {
            throw std::runtime_error("Error in parseBaseExpr, unknown token: " + peek().toString());
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
            const Token token = expect(Token::Type::IDENTIFIER);
            const std::string name = std::get<std::string>(token.value);
            ir::Type t;
            if (consume(Token::Type::COL)) {
                t = parseType();
            }
            args.push_back({name, std::move(t)});
        } while (!consume(Token::Type::ASSIGN));
        return args;
    }

    // // div_expr := mod_expr ('/' mod_expr)*
    // ir::Expr parseDivExpr() {
    //     return parseBinOp(ir::BinOp::Div, Token::Type::SLASH, [this]() { return parseModExpr(); });
    // }

    // // mod_expr := add_expr ('%' add_expr)*
    // ir::Expr parseModExpr() {
    //     return parseBinOp(ir::BinOp::Mod, Token::Type::MOD, [this]() { return parseAddExpr(); });
    // }

    // // add_expr := sub_expr ('+' sub_expr)*
    // ir::Expr parseAddExpr() {
    //     return parseBinOp(ir::BinOp::Add, Token::Type::ADD, [this]() { return parseSubExpr(); });
    // }

    // // TODO: bit shifts should be next...
    // // sub_expr := leq_expr ('-' leq_expr)*
    // ir::Expr parseAddExpr() {
    //     return parseBinOp(ir::BinOp::Sub, Token::Type::SUB, [this]() { return parseLeqExpr(); });
    // }

    /*
    // TODO: consider how this parsing effects numeric and/or/xor
    // expr := and_expr ('||' and_expr)*
    ir::Expr parseExpr() {
        ir::Expr expr = parseAndExpr();
        while (consume(Token::Type::OR)) {
            ir::Expr b = parseAndExpr();
            expr = ir::BinOp::make(ir::BinOp::Or, std::move(expr), std::move(b));
        }
        return expr;
    }

    // and_expr := xor_expr ('&&' xor_expr)*
    ir::Expr parseAndExpr() {
        ir::Expr expr = parseXorExpr();
        while (consume(Token::Type::OR)) {
            ir::Expr b = parseXorExpr();
            expr = ir::BinOp::make(ir::BinOp::Or, std::move(expr), std::move(b));
        }
        return expr;
    }

    ir::Expr parseXorExpr() {
        throw std::runtime_error("TODO: implement parseXorExpr");
    }
    */

    // type := i[N] | u[N] | f[N] | bool | vector[type, int] | option[type] | declared_type
    ir::Type parseType() {
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
            // TODO: probably enforce some realistic upper bound....?
            if (lanes < 0 || lanes > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
                throw std::runtime_error("Vector lane count is invalid: " + std::to_string(lanes));
            }
            expect(Token::Type::RBRACKET);
            return ir::Vector_t::make(std::move(etype), static_cast<uint32_t>(lanes));
        } else if (name == "option") {
            expect(Token::Type::LBRACKET);
            ir::Type etype = parseType();
            expect(Token::Type::RBRACKET);
            // TODO: assert etype is not bool, option[bool] is invalid semantically, I think...
            if (etype.is_bool()) {
                throw std::runtime_error("Bonsai does not support option[bool] because the semantics are confusing.");
            }
            return ir::Option_t::make(std::move(etype));
        } else if (name == "set") {
            expect(Token::Type::LBRACKET);
            ir::Type etype = parseType();
            expect(Token::Type::RBRACKET);
            // TODO: assert etype is a struct_t? or volume_t?
            return ir::Set_t::make(std::move(etype));
        } else {
            // TODO: support tuples of types! AKA unnamed structs.
            // Must be a user-defined type, or an error.
            if (program.types.contains(name)) {
                return program.types[name];
            } else {
                throw std::runtime_error("Unknown type: " + name);
            }
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
    ir::Program program = Parser(tokens).parseProgram();

    // TODO: remove this!
    if (program.externs.empty() && program.funcs.empty() && !program.main_body.defined()) {
        // Temporary cop-out to get imports of only elements to work.
        return program;
    }

    // Now do type inference and enforcement
    ir::global_enable_type_enforcement();

    program.dump(std::cout);

    // TODO: type inference / enforcement.
    throw std::runtime_error("TODO: type inference!");
    return program;
}

}  // namespace parser
}  // namespace bonsai
