#include "lexer.h"

Lexer create_lexer(const char* src, const char* filename, DiagContext *context) {
    return (Lexer) {
        src, src, filename,
        1, 1, src,
        context
    };
}

static bool is_end(Lexer *lexer) {
    return *lexer->current == '\0';
}

static char peek(Lexer *lexer) {
    return *lexer->current;
}

static char advance(Lexer *lexer) {
    lexer->col++;
    return *lexer->current++;
}

size_t strlength(const char* string) {
    size_t n = 0;
    while(string[n] != '\0') {
        n++;
    }
    return n;
}

bool strlcompare(View view, const char* string, size_t len) {
    if(view.len != len) return false;
    size_t i = 0;
    while(i < view.len && string[i] != '\0') {
        if(view.start[i] != string[i]) {
            return false;
        }
        i++;
    }

    return (i == view.len && string[i] == '\0');
}

bool strcompare(View view, const char* string) {
    return strlcompare(view, string, strlength(string));
}

bool is_digit(char c) {
    return (c >= '0' && c <= '9');
}

bool is_hexa(char c) {
    return (is_digit(c) || (c >= 'A' && c <= 'F'));
}

bool is_alpha(char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

bool is_alnum(char c) {
    return (is_digit(c) || is_alpha(c));
}

static bool isidentifier(char c) {
    return (is_alnum(c) || c == '_');
}

static void skip(Lexer *lexer) {
    while(peek(lexer) == ' ' || peek(lexer) == '\r' || peek(lexer) == '\t' || peek(lexer) == '\n') {
        if(peek(lexer) == '\n') {
            lexer->col = 0;
            lexer->line++;
        }

        if(peek(lexer) == '\t') {
            lexer->col += 4;
            continue;
        }

        advance(lexer);
    }
}

static Token create_token(Lexer *lexer, TokenType type, size_t col, size_t line) {
    Token token;
    token.type = type;
    token.col = col;
    token.line = line;
    token.filename = lexer->filename;
    token.view.start = lexer->start;
    token.view.len = (size_t)(lexer->current - lexer->start);
    return token;
}

static TokenType check_keyword(View view) {
    switch (view.len)
    {
    case 2:
        if(strcompare(view, "if")) {
            return TOKEN_IF;
        } else if(strcompare(view, "do")) {
            return TOKEN_DO;
        }
        break;
    case 3:
        if(strcompare(view, "for")) {
            return TOKEN_FOR;
        } else if(strcompare(view, "big")) {
            return TOKEN_BIG;
        }
        break;
    case 4: 
        if(strcompare(view, "void")) {
            return TOKEN_VOID;
        } else if(strcompare(view, "char")) {
            return TOKEN_CHAR;
        } else if(strcompare(view, "int8")) {
            return TOKEN_INT8;
        } else if(strcompare(view, "else")) {
            return TOKEN_ELSE;
        } else if(strcompare(view, "data")) {
            return TOKEN_DATA;
        } else if(strcompare(view, "case")) {
            return TOKEN_CASE;
        } else if(strcompare(view, "link")) {
            return TOKEN_LINK;
        } else if(strcompare(view, "enum")) {
            return TOKEN_ENUM;
        }
        break;
    case 5: 
        if(strcompare(view, "while")) {
            return TOKEN_WHILE;
        } else if(strcompare(view, "const")) {
            return TOKEN_CONST;
        } else if(strcompare(view, "int16")) {
            return TOKEN_INT16;
        } else if(strcompare(view, "int32")) {
            return TOKEN_INT32;
        } else if(strcompare(view, "int64")) {
            return TOKEN_INT64;
        } else if(strcompare(view, "uint8")) {
            return TOKEN_UINT8;
        } else if(strcompare(view, "break")) {
            return TOKEN_BREAK;
        } else if(strcompare(view, "small")) {
            return TOKEN_SMALL;
        }
        break;
    case 6:
        if(strcompare(view, "uint16")) {
            return TOKEN_UINT16;
        } else if(strcompare(view, "uint32")) {
            return TOKEN_UINT32;
        } else if(strcompare(view, "uint64")) {
            return TOKEN_UINT64;
        } else if(strcompare(view, "double")) {
            return TOKEN_DOUBLE;
        } else if(strcompare(view, "static")) {
            return TOKEN_STATIC;
        } else if(strcompare(view, "switch")) {
            return TOKEN_SWITCH;
        } else if(strcompare(view, "return")) {
            return TOKEN_RETURN;
        }
        break;
    case 8:
        if(strcompare(view, "continue")) {
            return TOKEN_CONTINUE;
        } else if(strcompare(view, "coperate")) {
            return TOKEN_COPERATE;
        } else if(strcompare(view, "positive")) {
            return TOKEN_POSITIVE;
        }
        break;
    case 9:
        if(strcompare(view, "utf16char")) {
            return TOKEN_UTF16CHAR;
        }
        break;
    case 10:
        if(strcompare(view, "createtype")) {
            return TOKEN_CREATETYPE;
        }
        break;
    }
    
    return TOKEN_IDENTIFIER;
}

static Token hexa_value(Lexer *lexer, size_t col, size_t line) {
    advance(lexer);
    while(is_hexa(peek(lexer))) {
        advance(lexer);
    }
    return create_token(lexer, TOKEN_HEXA_LIT, col, line);
}

static Token double_value(Lexer *lexer, size_t col, size_t line) {
    advance(lexer);
    while(is_digit(peek(lexer))) {
        advance(lexer);
    }
    if(peek(lexer) == 'f') {
        advance(lexer);
    }
    return create_token(lexer, TOKEN_DOUBLE_LIT, col, line);
}

static Token value(Lexer *lexer, size_t col, size_t line) {
    if(peek(lexer) == '0') {
        advance(lexer);
        if(peek(lexer) == 'x') {
            return hexa_value(lexer, col, line);
        }
    }
    while(is_digit(peek(lexer))) {
        advance(lexer);
    }

    if(peek(lexer) == '.') {
        return double_value(lexer, col, line);
    }
    return create_token(lexer, TOKEN_INT_LIT, col, line);
}

static Token identifier(Lexer *lexer, size_t col, size_t line) {
    while(isidentifier(peek(lexer))) {
        advance(lexer);
    }
    View view = {
        lexer->start,
        (size_t)(lexer->current - lexer->start)
    };

    return create_token(lexer, check_keyword(view), col, line);
}

Token next_token(Lexer *lexer) {
    skip(lexer);
    lexer->start = lexer->current;
    size_t col = lexer->col;
    size_t line = lexer->line;

    char current = peek(lexer);

    if(current == '\0') {
        return create_token(lexer, TOKEN_EOF, col, line);
    }

    if(is_digit(current)) {
        return value(lexer, col, line);
    }
    if(is_alnum(current) || current == '_') {
        return identifier(lexer, col, line);
    }

    switch (current)
    {
    case ';': advance(lexer); return create_token(lexer, TOKEN_SEMICOLON, col, line);
    case ':': advance(lexer); return create_token(lexer, TOKEN_COLON, col, line);
    case ',': advance(lexer); return create_token(lexer, TOKEN_COMMA, col, line);
    case '(': advance(lexer); return create_token(lexer, TOKEN_RPAREN, col, line);
    case ')': advance(lexer); return create_token(lexer, TOKEN_LPAREN, col, line);
    case '{': advance(lexer); return create_token(lexer, TOKEN_RBRACE, col, line);
    case '}': advance(lexer); return create_token(lexer, TOKEN_LBRACE, col, line);
    case '[': advance(lexer); return create_token(lexer, TOKEN_RBRACKET, col, line);
    case ']': advance(lexer); return create_token(lexer, TOKEN_LBRACKET, col, line);
    case '?': advance(lexer); return create_token(lexer, TOKEN_TERN, col, line);
    case '~': advance(lexer); return create_token(lexer, TOKEN_DESC, col, line);
    case '"': {
        advance(lexer);

        while(peek(lexer) != '"' && !is_end(lexer)) {
            if(peek(lexer) == '\\') {
                advance(lexer);
                advance(lexer);
                continue;
            }
            advance(lexer);
        }

        if(is_end(lexer)) {
            diag_error(lexer->diag, lexer->filename, line, col,
                    "string literal nao fechada");
            return create_token(lexer, TOKEN_UNDEFINED, col, line);
        }

        advance(lexer);
        return create_token(lexer, TOKEN_STRING_LIT, col, line);
    }
    case '\'': {
        advance(lexer);
        
        if(peek(lexer) == '\\') {
            advance(lexer);
            advance(lexer);
        } else {
            advance(lexer);
        }

        if(peek(lexer) != '\'') {
            diag_error(lexer->diag, lexer->filename, line, col,
                        "literal de caractere invalido ou nao fechado");
            return create_token(lexer, TOKEN_UNDEFINED, col, line);
        }

        return create_token(lexer, TOKEN_CHAR_LIT, col, line);
    }
    case '=': {
        advance(lexer);

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_EQEQ, col, line);
        }

        return create_token(lexer, TOKEN_EQ, col, line);
    }
    case '+': {
        advance(lexer);

        if(peek(lexer) == '+') {
            advance(lexer);
            return create_token(lexer, TOKEN_PP, col, line);
        }

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_PEQ, col, line);
        }

        return create_token(lexer, TOKEN_P, col, line);
    }
    case '-': {
        advance(lexer);

        if(peek(lexer) == '-') {
            advance(lexer);
            return create_token(lexer, TOKEN_MM, col, line);
        }
        
        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_MEQ, col, line);
        }

        return create_token(lexer, TOKEN_M, col, line);
    }
    case '/': {
        advance(lexer);

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_SLASHEQ, col, line);
        }

        return create_token(lexer, TOKEN_SLASH, col, line);
    }
    case '*': {
        advance(lexer);

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_STAREQ, col, line);
        }

        return create_token(lexer, TOKEN_STAR, col, line);
    }
    case '%': {
        advance(lexer);

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_MODEQ, col, line);
        }

        return create_token(lexer, TOKEN_MOD, col, line);
    }
    case '<': {
        advance(lexer);

        if(peek(lexer) == '<') {
            advance(lexer);
            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_LSHIFTEQ, col, line);
            }
            return create_token(lexer, TOKEN_LSHIFT, col, line);
        }

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_LE, col, line);
        }

        return create_token(lexer, TOKEN_LT, col, line);
    }
    case '>': {
        advance(lexer);

        if(peek(lexer) == '>') {
            advance(lexer);
            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_RSHIFTEQ, col, line);
            }
            return create_token(lexer, TOKEN_RSHIFT, col, line);
        }

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_GE, col, line);
        }

        return create_token(lexer, TOKEN_GE, col, line);
    }
    case '|': {
        advance(lexer);

        if(peek(lexer) == '|') {
            advance(lexer);
            return create_token(lexer, TOKEN_LOGOR, col, line);
        }

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_OREQ, col, line);
        }

        return create_token(lexer, TOKEN_OR, col, line);
    }
    case '&': {
        advance(lexer);

        if(peek(lexer) == '&') {
            advance(lexer);
            return create_token(lexer, TOKEN_LOGAND, col, line);
        }

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_ANDEQ, col, line);
        }

        return create_token(lexer, TOKEN_AND, col, line);
    }
    case '!': {
        advance(lexer);

        if(peek(lexer) == '=') {
            advance(lexer);
            return create_token(lexer, TOKEN_BANGEQ, col, line);
        }

        return create_token(lexer, TOKEN_BANG, col, line);
    }
    case '.': {
        advance(lexer);

        if(peek(lexer) == '.') {
            advance(lexer);
            if(peek(lexer) == '.') {
                advance(lexer);
                return create_token(lexer, TOKEN_INF_ARGS, col, line);
            }
            return create_token(lexer, TOKEN_UNDEFINED, col, line);
        }

        return create_token(lexer, TOKEN_DOT, col, line);
    }
    default:
        char err = current;
        advance(lexer);
        if(lexer->diag) {
            diag_error(lexer->diag, lexer->filename, line, col,
                    "caractere inesperado '%c' (ASCII %d)", err, (int)(unsigned char)err);
        }
        return create_token(lexer, TOKEN_UNDEFINED, col, line);
    }
}
