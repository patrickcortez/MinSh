// Compile: Use CMake (mkdir build && cd build && cmake .. && cmake --build .)
#include "Lexer.hpp"
#include <sstream>

std::vector<Token> Lexer::tokenize(const std::string& input) {
    std::vector<Token> tokens;
    std::string currentToken;
    bool inQuotes = false;
    char quoteChar = 0;

    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];

        if (inQuotes) {
            if (c == quoteChar) {
                // End of quote
                inQuotes = false;
                tokens.push_back({LexerTokenType::STRING, currentToken});
                currentToken.clear();
            } else {
                currentToken += c;
            }
        } else {
            if (c == '"' || c == '\'') {
                // Start of quote
                if (!currentToken.empty()) {
                    tokens.push_back({LexerTokenType::WORD, currentToken});
                    currentToken.clear();
                }
                inQuotes = true;
                quoteChar = c;
            } else if (std::isspace(c)) {
                if (!currentToken.empty()) {
                    tokens.push_back({LexerTokenType::WORD, currentToken});
                    currentToken.clear();
                }
            } else {
                currentToken += c;
            }
        }
    }

    if (!currentToken.empty()) {
        tokens.push_back({inQuotes ? LexerTokenType::STRING : LexerTokenType::WORD, currentToken});
    }

    return tokens;
}
