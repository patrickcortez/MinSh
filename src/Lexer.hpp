#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <vector>

enum class LexerTokenType {
    WORD,
    STRING,
    UNKNOWN
};

struct Token {
    LexerTokenType type;
    std::string value;
};

class Lexer {
public:
    static std::vector<Token> tokenize(const std::string& input);
};

#endif // LEXER_HPP
