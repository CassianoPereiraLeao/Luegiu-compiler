#include "lexer.h"

static char peek(Lexer *lexer) {
    return *lexer->cursor;
}

static char advance(Lexer *lexer) {
    char c = *lexer->cursor++;
    lexer->col++;
    return c;
}

static bool strcompare(View view, const char* string) {
    size_t i = 0;
    while(i < view.len && string[i] != '\0') {
        if(view.start[i] != string[i]) return false;
        i++;
    }

    return (i == view.len && string[i] == '\0');
}

static void skip(Lexer *lexer) {
    while(1) {
        while(peek(lexer) == ' ' || peek(lexer) == '\r' || peek(lexer) == '\t' || peek(lexer) == '\n') {
            if(peek(lexer) == '\n') {
                lexer->col = 1;
                lexer->line++;
            }

            if(peek(lexer) == '\t') lexer->col += 4;

            advance(lexer);
        }

        if(peek(lexer) == '/' && lexer->cursor[1] == '/') {
            while(peek(lexer) != '\n' && peek(lexer) != '\0')
                advance(lexer);
            continue;
        }

        if(peek(lexer) == '/' && lexer->cursor[1] == '*') {
            size_t start_line = lexer->line;
            size_t start_col  = lexer->col;
            advance(lexer); advance(lexer);
            while(!(peek(lexer) == '*' && lexer->cursor[1] == '/')) {
                if(peek(lexer) == '\0') {
                    if(lexer->diag)
                        diag_error(lexer->diag, lexer->file_name, start_line, start_col,
                            "comentario de bloco nao fechado (iniciado aqui)");
                    else
                        fprintf(stderr, "Erro Lexico linha %zu: comentario de bloco nao fechado\n",
                                    start_line);
                    return;
                }
                if(peek(lexer) == '\n') {
                    lexer->line++;
                    lexer->col = 1;
                }
                advance(lexer);
            }
            advance(lexer); advance(lexer);
            continue;
        }

        break;
    }
}

static Token create_token(Lexer *lexer, TokenType type) {
    Token token;
    token.type = type;
    token.view.start = lexer->start;
    token.view.len = (size_t)(lexer->cursor - lexer->start);
    token.line = lexer->line;
    token.col = lexer->col;
    token.file_name = lexer->file_name;

    if(token.view.start == NULL || token.view.len > 4096) {
        fprintf(stderr, "TOKEN INVALIDO: type=%d len=%zu start=%p line=%d\n",
            type, token.view.len, (void*)token.view.start, token.line);
        abort();
    }
    
    return token;
}

static TokenType check_keyword(View view) {
    if(strcompare(view, "int")) return TOKEN_KEYWORD_INT;
    if(strcompare(view, "int8")) return TOKEN_KEYWORD_INT8;
    if(strcompare(view, "int16")) return TOKEN_KEYWORD_INT16;
    if(strcompare(view, "int32")) return TOKEN_KEYWORD_INT32;
    if(strcompare(view, "int64")) return TOKEN_KEYWORD_INT64;
    if(strcompare(view, "uint8")) return TOKEN_KEYWORD_UINT8;
    if(strcompare(view, "uint16")) return TOKEN_KEYWORD_UINT16;
    if(strcompare(view, "uint32")) return TOKEN_KEYWORD_UINT32;
    if(strcompare(view, "uint64")) return TOKEN_KEYWORD_UINT64;
    if(strcompare(view, "bool")) return TOKEN_KEYWORD_BOOLEAN;
    if(strcompare(view, "true")) return TOKEN_KEYWORD_TRUE;
    if(strcompare(view, "false")) return TOKEN_KEYWORD_FALSE;
    if(strcompare(view, "float")) return TOKEN_KEYWORD_FLOAT;
    if(strcompare(view, "double")) return TOKEN_KEYWORD_DOUBLE;
    if(strcompare(view, "char")) return TOKEN_KEYWORD_CHAR;
    if(strcompare(view, "utf8char")) return TOKEN_KEYWORD_UTF8CHAR;
    if(strcompare(view, "utf16char")) return TOKEN_KEYWORD_UTF16CHAR;
    if(strcompare(view, "void")) return TOKEN_KEYWORD_VOID;
    if(strcompare(view, "if")) return TOKEN_KEYWORD_IF;
    if(strcompare(view, "do")) return TOKEN_KEYWORD_DO;
    if(strcompare(view, "for")) return TOKEN_KEYWORD_FOR;
    if(strcompare(view, "big")) return TOKEN_KEYWORD_BIG;
    if(strcompare(view, "continue")) return TOKEN_KEYWORD_CONTINUE;
    if(strcompare(view, "return")) return TOKEN_KEYWORD_RETURN;
    if(strcompare(view, "break")) return TOKEN_KEYWORD_BREAK;
    if(strcompare(view, "while")) return TOKEN_KEYWORD_WHILE;
    if(strcompare(view, "switch")) return TOKEN_KEYWORD_SWITCH;
    if(strcompare(view, "case")) return TOKEN_KEYWORD_CASE;
    if(strcompare(view, "small")) return TOKEN_KEYWORD_SMALL;
    if(strcompare(view, "enum")) return TOKEN_KEYWORD_ENUM;
    if(strcompare(view, "typedefinition")) return TOKEN_KEYWORD_TYPEDEFINITION;
    if(strcompare(view, "else")) return TOKEN_KEYWORD_ELSE;
    if(strcompare(view, "struct")) return TOKEN_KEYWORD_STRUCT;
    if(strcompare(view, "static")) return TOKEN_KEYWORD_STATIC;
    if(strcompare(view, "link")) return TOKEN_KEYWORD_LINK;
    if(strcompare(view, "const"))  return TOKEN_KEYWORD_CONST;
    if(strcompare(view, "union")) return TOKEN_KEYWORD_UNION;

    return TOKEN_IDENTIFIER;
}

static bool is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static bool is_alpha(char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static bool is_identifier(char c) {
    return is_alpha(c) || c == '_';
}

static bool is_alphanumeric(char c) {
    return (is_digit(c) || is_alpha(c));
}

static bool is_hexa(char c) {
    return ((c >= 'A' && c <= 'F') || is_digit(c));
}

static Token hexa(Lexer *lexer) {
    while(is_hexa((peek(lexer)))) advance(lexer);
    return create_token(lexer, TOKEN_HEXA);
}

static Token double_val(Lexer *lexer) {
    advance(lexer);
    while(is_digit(peek(lexer))) advance(lexer);
    if(peek(lexer) == 'f')
        advance(lexer);
    return create_token(lexer, TOKEN_DOUBLE_LIT);
}

static Token numeric(Lexer *lexer) {
    if(peek(lexer) == '0') {
        advance(lexer);
        if(peek(lexer) == 'x' || peek(lexer) == 'X') {
            advance(lexer);
            return hexa(lexer);
        }
    }
    while(is_digit(peek(lexer))) advance(lexer);
    if(peek(lexer) == '.') return double_val(lexer);
    return create_token(lexer, TOKEN_INT_LIT);
}

static Token identifier(Lexer *lexer) {
    while(is_alphanumeric(peek(lexer)) || peek(lexer) == '_') advance(lexer);
    View view = { lexer->start, (size_t)(lexer->cursor - lexer->start) };
    return create_token(lexer, check_keyword(view));
}

Lexer create_lexer(const char* source, const char* filename, DiagContext *context) {
    Lexer lexer;
    lexer.col = 1;
    lexer.line = 1;
    lexer.cursor = source;
    lexer.src = source;
    lexer.start = source;
    lexer.file_name = filename ? filename : "<entrada>";
    lexer.diag = context;
    return lexer;
}

Token next_token(Lexer *lexer) {
    skip(lexer);
    lexer->start = lexer->cursor;

    char current = peek(lexer);

    if(current == '\0') return create_token(lexer, TOKEN_EOF);

    if(is_digit(current)) return numeric(lexer);
    if(is_identifier(current)) return identifier(lexer);

    switch(current) {
        case ';': advance(lexer); return create_token(lexer, TOKEN_SEMICOLON);
        case '(': advance(lexer); return create_token(lexer, TOKEN_LPAREN);
        case ')': advance(lexer); return create_token(lexer, TOKEN_RPAREN);
        case '{': advance(lexer); return create_token(lexer, TOKEN_LBRACE);
        case '}': advance(lexer); return create_token(lexer, TOKEN_RBRACE);
        case '[': advance(lexer); return create_token(lexer, TOKEN_LBRACKET);
        case ']': advance(lexer); return create_token(lexer, TOKEN_RBRACKET);
        case ':': advance(lexer); return create_token(lexer, TOKEN_COLON);
        case '?': advance(lexer); return create_token(lexer, TOKEN_OP_TERN);
        case ',': advance(lexer); return create_token(lexer, TOKEN_COMMA);
        case '^': advance(lexer); return create_token(lexer, TOKEN_OP_XOR);
        case '~': advance(lexer); return create_token(lexer, TOKEN_OP_DESC);
        case '"': {
            size_t start_line = lexer->line;
            size_t start_col = lexer->col;
            advance(lexer);
            
            while(peek(lexer) != '"' && peek(lexer) != '\0')
                advance(lexer);
            
            if (peek(lexer) == '\0') {
                diag_error(lexer->diag, lexer->file_name, start_line, start_col,
                        "string literal nao fechada");
                return create_token(lexer, TOKEN_UNDEFINED);
            }

            advance(lexer);
            return create_token(lexer, TOKEN_STRING_LITERAL);
        }
        case '\'': {
            size_t start_line = lexer->line;
            size_t start_col = lexer->col;
            advance(lexer);

            if(peek(lexer) == '\\') {
                advance(lexer);
                advance(lexer);
            } else {
                advance(lexer);
            }

            if (peek(lexer) != '\'') {
                diag_error(lexer->diag, lexer->file_name, start_line, start_col,
                        "literal de caractere invalido ou nao fechado");
                return create_token(lexer, TOKEN_UNDEFINED);
            }

            advance(lexer);
            return create_token(lexer, TOKEN_CHAR_LITERAL);
        }
        case '=':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_EQ_EQ);
            }

            return create_token(lexer, TOKEN_OP_ASSIGN);
        case '+':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_PLUS_EQ);
            }

            if(peek(lexer) == '+') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_PLUS_PLUS);
            }

            return create_token(lexer, TOKEN_OP_PLUS);
        case '-':
            advance(lexer);

            if(peek(lexer) == '-') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_MINUS_MINUS);
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_MINUS_EQ);
            }

            if(peek(lexer) == '>') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_ARROW);
            }

            return create_token(lexer, TOKEN_OP_MINUS);
        case '>':
            advance(lexer);

            if(peek(lexer) == '>') {
                advance(lexer);
                if(peek(lexer) == '=') {
                    advance(lexer);
                    return create_token(lexer, TOKEN_BIT_RSHIFT_EQ);
                }
                
                return create_token(lexer, TOKEN_BIT_RSHIFT);
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_GT_EQ);
            }

            return create_token(lexer, TOKEN_OP_GT);
        case '<':
            advance(lexer);

            if(peek(lexer) == '<') {
                advance(lexer);
                if(peek(lexer) == '=') {
                    advance(lexer);
                    return create_token(lexer, TOKEN_BIT_LSHIFT_EQ);
                }
                
                return create_token(lexer, TOKEN_BIT_LSHIFT);
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_LT_EQ);
            }

            return create_token(lexer, TOKEN_OP_LT);
        case '*':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_STAR_EQ);
            }

            return create_token(lexer, TOKEN_OP_STAR);
        case '/':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_SLASH_EQ);
            }

            return create_token(lexer, TOKEN_OP_SLASH);
        case '&':
            advance(lexer);

            if(peek(lexer) == '&') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_AND);
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_BIT_AND_EQ);
            }

            return create_token(lexer, TOKEN_BIT_AND);
        case '|':
            advance(lexer);

            if(peek(lexer) == '|') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_OR);
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_BIT_OR_EQ);
            }

            return create_token(lexer, TOKEN_BIT_OR);
        case '%':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_MOD_EQ);
            }

            return create_token(lexer, TOKEN_OP_MOD);
        case '.': 
            advance(lexer); 
            if(peek(lexer) == '.') {
                advance(lexer);
                if(peek(lexer) == '.') {
                    advance(lexer);
                    return create_token(lexer, TOKEN_OP_INF_ARGS);
                }
            }

            return create_token(lexer, TOKEN_DOT);
        case '!':
            advance(lexer);
            if(peek(lexer) == '=') {
                advance(lexer);
                return create_token(lexer, TOKEN_OP_BANG_EQ);
            }

            return create_token(lexer, TOKEN_OP_BANG);
        default:
            size_t err_line = lexer->line;
            size_t err_col  = lexer->col;
            char bad = current;
            advance(lexer);
            if(lexer->diag)
                diag_error(lexer->diag, lexer->file_name, err_line, err_col,
                    "caractere inesperado '%c' (ASCII %d)", bad, (int)(unsigned char)bad);
            return create_token(lexer, TOKEN_UNDEFINED);
    }
}
