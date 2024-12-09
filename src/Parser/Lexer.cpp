#include "Parser/Lexer.h"

#include <sstream>

namespace bonsai {
namespace parser {

Token::Type Lexer::getTokenType(const std::string token) {
    if (token == "element") return  Token::Type::ELEMENT;
    if (token == "interface") return  Token::Type::INTERFACE;
    if (token == "extern") return  Token::Type::EXTERN;
    if (token == "func") return  Token::Type::FUNC;
    if (token == "mut") return  Token::Type::MUT;
    if (token == "lambda") return  Token::Type::LAMBDA;
    if (token == "return") return  Token::Type::RETURN;
    if (token == "for") return  Token::Type::FOR;
    if (token == "if") return  Token::Type::IF;
    if (token == "elif") return  Token::Type::ELIF;
    if (token == "else") return  Token::Type::ELSE;
    if (token == "true") return  Token::Type::TRUE;
    if (token == "false") return  Token::Type::FALSE;

    // If string does not correspond to a keyword, assume it is an identifier.
    return Token::Type::IDENTIFIER;
}

TokenStream Lexer::lex(std::istream &programStream) {
    TokenStream tokens;
    uint32_t line = 1;
    uint32_t col = 1;
    ScanState state = ScanState::INITIAL;

    while (programStream.peek() != EOF) {
        // Try to parse a name of the form [A-Za-z_][A-Za-z0-9_]
        if (programStream.peek() == '_' || std::isalpha(programStream.peek())) {
            std::string tokenString(1, programStream.get());
            while (programStream.peek() == '_' || std::isalnum(programStream.peek())) {
                tokenString += programStream.get();
            }

            Token newToken;
            newToken.type = getTokenType(tokenString);
            newToken.lineBegin = line;
            newToken.colBegin = col;
            newToken.lineEnd = line;
            newToken.colEnd = col + tokenString.length() - 1;
            if (newToken.type == Token::Type::IDENTIFIER) {
                newToken.value = tokenString;
            }
            tokens.addToken(newToken);

            col += tokenString.length();
        } else {
            switch (programStream.peek()) {
                case '(':
                    programStream.get();
                    tokens.addToken(Token::Type::LPAREN, line, col++);
                    break;
                case ')':
                    programStream.get();
                    tokens.addToken(Token::Type::RPAREN, line, col++);
                    break;
                case '[':
                    programStream.get();
                    tokens.addToken(Token::Type::LBRACKET, line, col++);
                    break;
                case ']':
                    programStream.get();
                    tokens.addToken(Token::Type::RBRACKET, line, col++);
                    break;
                case '{':
                    programStream.get();
                    tokens.addToken(Token::Type::LSQUIGGLE, line, col++);
                    break;
                case '}':
                    programStream.get();
                    tokens.addToken(Token::Type::RSQUIGGLE, line, col++);
                    break;
                case ',':
                    programStream.get();
                    tokens.addToken(Token::Type::COMMA, line, col++);
                    break;
                case '.':
                    // NOTE: this means float literals like .0f are illegal!
                    programStream.get();
                    tokens.addToken(Token::Type::PERIOD, line, col++);
                    break;
                case ':':
                    programStream.get();
                    tokens.addToken(Token::Type::COL, line, col++);
                    break;
                case ';':
                    programStream.get();
                    tokens.addToken(Token::Type::SEMICOL, line, col++);
                    break;
                case '=':
                    programStream.get();
                    if (programStream.peek() == '=') {
                        programStream.get();
                        tokens.addToken(Token::Type::EQ, line, col, 2);
                        col += 2;
                    } else {
                        tokens.addToken(Token::Type::ASSIGN, line, col++);
                    }
                    break;
                case '&':
                    programStream.get();
                    if (programStream.peek() == '&') {
                        programStream.get();
                        tokens.addToken(Token::Type::AND, line, col, 2);
                        col += 2;
                    } else {
                        // TODO: what to do?
                        reportError("SINGLE & not supported", line, col);
                        tokens.addToken(Token::Type::ERROR, line, col++);
                    }
                    break;
                case '|':
                    programStream.get();
                    if (programStream.peek() == '|') {
                        programStream.get();
                        tokens.addToken(Token::Type::OR, line, col, 2);
                        col += 2;
                    } else {
                        // TODO: what to do?
                        reportError("SINGLE | not supported", line, col);
                        tokens.addToken(Token::Type::ERROR, line, col++);
                    }
                    break;
                case '!':
                    programStream.get();
                    if (programStream.peek() == '=') {
                        programStream.get();
                        tokens.addToken(Token::Type::NEQ, line, col, 2);
                        col += 2;
                    } else {
                        tokens.addToken(Token::Type::NOT, line, col++);
                    }
                    break;
                case '+':
                    programStream.get();
                    if (programStream.peek() == '+') {
                        programStream.get();
                        tokens.addToken(Token::Type::INC, line, col, 2);
                        col += 2;
                    } else {
                        tokens.addToken(Token::Type::PLUS, line, col++);
                    }
                    break;
                case '-':
                    programStream.get();
                    if (programStream.peek() == '>') {
                        programStream.get();
                        tokens.addToken(Token::Type::RARROW, line, col, 2);
                        col += 2;
                    } else if (programStream.peek() == '-') {
                        programStream.get();
                        tokens.addToken(Token::Type::DEC, line, col, 2);
                        col += 2;
                    } else {
                        tokens.addToken(Token::Type::MINUS, line, col++);
                    }
                    break;
                case '*':
                    programStream.get();
                    tokens.addToken(Token::Type::STAR, line, col++);
                    break;
                case '/':
                    programStream.get();
                    if (programStream.peek() == '/') {
                        while (programStream.peek() != EOF && programStream.peek() != '\n') {
                            programStream.get();
                            col++;
                        }
                        if (programStream.peek() != '\n') {
                            programStream.get();
                            line++;
                            col = 1;
                        }
                        // TODO: emit comment token?
                    } else {
                        tokens.addToken(Token::Type::SLASH, line, col++);
                    }
                    break;
                case '%':
                    programStream.get();
                    tokens.addToken(Token::Type::MOD, line, col++);
                    break;
                // EQ, NEQ already handled
                case '<':
                    programStream.get();
                    if (programStream.peek() == '=') {
                        programStream.get();
                        tokens.addToken(Token::Type::LEQ, line, col, 2);
                        col += 2;
                    } else {
                        tokens.addToken(Token::Type::LT, line, col++);
                    }
                    break;
                case '>':
                    programStream.get();
                    if (programStream.peek() == '=') {
                        programStream.get();
                        tokens.addToken(Token::Type::GEQ, line, col, 2);
                        col += 2;
                    } else {
                        tokens.addToken(Token::Type::GT, line, col++);
                    }
                    break;
                case '"': {
                    programStream.get();
                    Token newToken;
                    newToken.type = Token::Type::STRING_LITERAL;
                    newToken.lineBegin = line;
                    newToken.colBegin = col++;
                    std::string str;

                    while (programStream.peek() != EOF && programStream.peek() != '"') {
                        if (programStream.peek() == '\\') {
                            char escapedChar = handleEscapedChar(programStream, line, col);
                            if (escapedChar != ' ') {
                                str += escapedChar;
                                programStream.get();
                                col += 2;
                            } else {
                                // error case.
                                col++;
                            }
                        } else {
                            str += programStream.get();
                            col++;
                        }
                    }

                    newToken.lineEnd = line;
                    newToken.colEnd = col;
                    newToken.value = str;
                    tokens.addToken(newToken);

                    if (programStream.peek() == '"') {
                        programStream.get();
                        col++;
                    } else {
                        reportError("unclosed string literal", line, col);
                    }
                    break;
                }
                // Whitespace
                case '\v':
                    throw std::runtime_error("what is \\v = \v ???\n");
                case '\f':
                    throw std::runtime_error("what is \\f = \f ???\n");
                case '\r': // ???
                    throw std::runtime_error("what is \\r = \r ???\n");
                case '\n':
                    programStream.get();
                    if (state == ScanState::SLTEST) {
                        state = ScanState::INITIAL;
                    }
                    line++;
                    col = 1;
                    break;
                case ' ':
                case '\t':
                    programStream.get();
                    ++col;
                    break;
                default: {
                    // Try to parse a(n) (int uint, float) literal.
                    // TODO: currently float literals like .0f are illegal due to PERIOD parsing above.
                    // this might be fixable in the parser itself though, .int -> float?
                    if (!std::isdigit(programStream.peek())) {
                        std::stringstream errMsg;
                        errMsg << "unexpected symbol (expected digit) '" << (char)programStream.peek() << "'";
                        reportError(errMsg.str(), line, col);
                        while (programStream.peek() != EOF && !std::isspace(programStream.peek())) {
                            programStream.get();
                            col++;
                        }
                        break;
                    }

                    Token newToken;
                    newToken.lineBegin = line;
                    newToken.colBegin = col;
                    newToken.type = Token::Type::INT_LITERAL;

                    std::string tokenString;
                    while (std::isdigit(programStream.peek())) {
                        tokenString += programStream.get();
                        ++col;
                    }

                    // handle decimal
                    if (programStream.peek() == '.') {
                        newToken.type = Token::Type::FLOAT_LITERAL;
                        tokenString += programStream.get();
                        ++col;

                        if (!std::isdigit(programStream.peek())) {
                            std::stringstream errMsg;
                            errMsg << "unexpected symbol (expected digit for decimal) '" << (char)programStream.peek() << "'";
                            reportError(errMsg.str(), line, col);

                            while (programStream.peek() != EOF && !std::isspace(programStream.peek())) {
                                programStream.get();
                                ++col;
                            }
                            break;
                        }
                        do {
                            tokenString += programStream.get();
                            col++;
                        } while (std::isdigit(programStream.peek()));
                    }

                    // handle exponent
                    if (programStream.peek() == 'e' || programStream.peek() == 'E') {
                        newToken.type = Token::Type::FLOAT_LITERAL;
                        tokenString += programStream.get();
                        ++col;

                        if (programStream.peek() == '+' || programStream.peek() == '-') {
                            tokenString += programStream.get();
                            ++col;
                        }

                        if (!std::isdigit(programStream.peek())) {
                            std::stringstream errMsg;
                            errMsg << "unexpected symbol (expected digit for exponent) '" << (char)programStream.peek() << "'";
                            reportError(errMsg.str(), line, col);
                            
                            while (programStream.peek() != EOF && !std::isspace(programStream.peek())) {
                                programStream.get();
                                ++col;
                            }
                            break;
                        }
                        do {
                            tokenString += programStream.get();
                            col++;
                        } while (std::isdigit(programStream.peek()));
                    }

                    // TODO: handle f (float), h (half) modifiers

                    // handle u (unsigned) modifier.
                    if (programStream.peek() == 'u') {
                        newToken.type = Token::Type::UINT_LITERAL;
                        ++col;
                        newToken.value = (uint64_t)std::stoull(tokenString);
                    } else if (newToken.type == Token::Type::INT_LITERAL) {
                        newToken.value = (int64_t)std::stoll(tokenString);
                    } else {
                        if (newToken.type != Token::Type::FLOAT_LITERAL) {
                            throw std::runtime_error("State error in literal parsing: " + newToken.toString());
                        }
                        newToken.value = (double)std::stold(tokenString);
                    }
                    newToken.lineEnd = line;
                    newToken.colEnd = col - 1;
                    tokens.addToken(newToken);
                    break;
                }
            }
        }
    }
    if (state != ScanState::INITIAL) {
        reportError("unclosed test", line, col);
    }

    // tokens.addToken(Token::Type::END, line, col);
    return tokens;
}

char Lexer::handleEscapedChar(std::istream &programStream, uint32_t line, uint32_t col) {
    switch (programStream.peek()) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '\"';
        case '?': return '\?';
        default:
            reportError("Unrecognized escape sequence", line, col);
            return ' ';
    }
}


}  // namespace parser
}  // namespace bonsai
