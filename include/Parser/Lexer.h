#pragma once

#include <sstream>
#include <string>

#include "Token.h"

namespace bonsai {
namespace parser {

class Lexer {
public:
    Lexer() {}
    // TODO: support better error handling?

    TokenStream lex(std::istream &);

private:
    enum class ScanState {INITIAL, SLTEST, MLTEST};

    private:
    static Token::Type getTokenType(const std::string);
    
    void reportError(std::string msg, uint32_t line, uint32_t col) {
        throw std::runtime_error("Parser error: " + msg + "\n  on line " + std::to_string(line) + " and column " + std::to_string(col));
        // errors->push_back(ParseError(line, col, line, col, msg));
    }

    char handleEscapedChar(std::istream &programStream, uint32_t line, uint32_t col);

private:
    // std::vector<ParseError> *errors;
};


}  // namespace parser
}  // namespace bonsai
