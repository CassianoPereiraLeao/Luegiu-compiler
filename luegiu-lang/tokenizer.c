#include "tokenizer.h"

static char peek(Lexer *lexer) {
    if(lexer->cursor >= lexer->src_len) return '\0';
    return lexer->source[lexer->cursor];
}

static char advance(Lexer *lexer) {
    if(lexer->cursor >= lexer->src_len) return '\0';
    char c = lexer->source[lexer->cursor];
    lexer->cursor++;
    lexer->col++;
    return c;
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alphanumeric(char c) {
    return is_alpha(c) || is_digit(c);
}

Lexer create_lexer(const char* src, size_t len) {
    Lexer lexer = {0};
    lexer.source = src;
    lexer.src_len = len;
    lexer.line = 1;
    lexer.col = 1;
    return lexer;
}

static bool strcompare(View view, const char* string) {
    size_t i = 0;
    while(i < view.len && string[i] != '\0') {
        if(view.data[i] != string[i]) return false;
        i++;
    }

    return (i == view.len && string[i] == '\0');
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

    return TOKEN_IDENTIFIER;
}

Token_Array tokenize(Lexer *lexer, Arena* arena) {
    Token_Array array = { 0 };
    array.capacity = 512;
    array.count = 0;
    array.data = (Token*)arena_alloc(arena, array.capacity * sizeof(Token));

    if (array.data == NULL) {
        printf("Erro crítico: Arena falhou em alocar memória inicial para tokens.\n");
        exit(1);
    }

    while(lexer->cursor < lexer->src_len) {
        char current = peek(lexer);

        if(current == ' ' || current == '\t' || current == '\r') {
            advance(lexer);
            continue;
        }

        if(current == '\n') {
            advance(lexer);
            lexer->line++;
            lexer->col = 1;
            continue;
        }

        if(array.count >= array.capacity) {
            size_t old_cap = array.capacity;
            array.capacity *= 2;
            Token* new_data = (Token*)arena_alloc(arena, array.capacity * sizeof(Token));

            for(size_t i = 0; i < old_cap; i++)
                new_data[i] = array.data[i];

            array.data = new_data;
        }

        const char* start_ptr = &lexer->source[lexer->cursor];
        uint32_t token_line = lexer->line;
        uint32_t token_col = lexer->col;

        if(is_alpha(current)) {
            size_t start_pos = lexer->cursor;
            while(is_alphanumeric(peek(lexer))) advance(lexer);

            Token token;
            token.type = TOKEN_UNDEFINED;
            token.line = token_line;
            token.col = token_col;
            token.view.data = start_ptr;
            token.view.len = lexer->cursor - start_pos;
            token.type = check_keyword(token.view);
            array.data[array.count++] = token;
            continue;
        }

        if(is_digit(current)) {
            size_t start_pos = lexer->cursor;
            bool has_dot = false;

            while(is_digit(peek(lexer)) || peek(lexer) == '.') {
                if(peek(lexer) == '.') {
                    if(has_dot) break;
                    has_dot = true;
                }
                advance(lexer);
            }

            if(peek(lexer) == 'f')
                advance(lexer);

            Token token;
            token.type = TOKEN_NUMERIC_LITERAL;
            token.line = token_line;
            token.col = token_col;
            token.view.data = start_ptr;
            token.view.len = lexer->cursor - start_pos;

            array.data[array.count++] = token;
            continue;
        }

        Token token;
        token.line = token_line;
        token.col = token_col;
        token.view.data = start_ptr;
        token.view.len = 1;

        switch(current) {
        case ';': advance(lexer); token.type = TOKEN_SEMICOLON; break;
        case '(': advance(lexer); token.type = TOKEN_LPAREN; break;
        case ')': advance(lexer); token.type = TOKEN_RPAREN; break;
        case '{': advance(lexer); token.type = TOKEN_LBRACE; break;
        case '}': advance(lexer); token.type = TOKEN_RBRACE; break;
        case '[': advance(lexer); token.type = TOKEN_LBRACKET; break;
        case ']': advance(lexer); token.type = TOKEN_RBRACKET; break;
        case ':': advance(lexer); token.type = TOKEN_COLON; break;
        case '.': advance(lexer); token.type = TOKEN_DOT; break;
        case '?': advance(lexer); token.type = TOKEN_OP_TERN; break;
        case ',': advance(lexer); token.type = TOKEN_COMMA; break;
        case '=':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_OP_EQ_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_OP_ASSIGN;
            break;
        case '+':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_OP_PLUS_EQ;
                token.view.len = 2;
                break;
            }

            if(peek(lexer) == '+') {
                advance(lexer);
                token.type = TOKEN_OP_PLUS_PLUS;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_OP_PLUS;
            break;
        case '-':
            advance(lexer);

            if(peek(lexer) == '-') {
                advance(lexer);
                token.type = TOKEN_OP_MINUS_MINUS;
                token.view.len = 2;
                break;
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_OP_MINUS_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_OP_MINUS;
            break;
        case '>':
            advance(lexer);

            if(peek(lexer) == '>') {
                advance(lexer);
                if(peek(lexer) == '=') {
                    advance(lexer);
                    token.type = TOKEN_BIT_RSHIFT_EQ;
                    token.view.len = 3;
                    break;
                }
                token.type = TOKEN_BIT_RSHIFT;
                token.view.len = 2;
                break;
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_OP_GT_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_OP_GT;
            break;
        case '<':
            advance(lexer);

            if(peek(lexer) == '<') {
                advance(lexer);
                if(peek(lexer) == '=') {
                    advance(lexer);
                    token.type = TOKEN_BIT_LSHIFT_EQ;
                    token.view.len = 3;
                    break;
                }
                token.type = TOKEN_BIT_LSHIFT;
                token.view.len = 2;
                break;
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_OP_LT_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_OP_LT;
            break;
        case '*':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_OP_STAR_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_OP_STAR;
            break;
        case '/':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_OP_SLASH_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_OP_SLASH;
            break;
        case '&':
            advance(lexer);

            if(peek(lexer) == '&') {
                advance(lexer);
                token.type = TOKEN_OP_AND;
                token.view.len = 2;
                break;
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_BIT_AND_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_BIT_AND;
            break;
        case '|':
            advance(lexer);

            if(peek(lexer) == '|') {
                advance(lexer);
                token.type = TOKEN_OP_OR;
                token.view.len = 2;
                break;
            }

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_BIT_OR_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_BIT_OR;
            break;
        case '%':
            advance(lexer);

            if(peek(lexer) == '=') {
                advance(lexer);
                token.type = TOKEN_OP_MOD_EQ;
                token.view.len = 2;
                break;
            }

            token.type = TOKEN_OP_MOD;
            break;
        default:
            advance(lexer);
            token.type = TOKEN_UNDEFINED;
            break;
        }

        array.data[array.count++] = token;
    }

    if(array.count >= array.capacity) {
        size_t old_cap = array.capacity;
        array.capacity *= 2;
        Token* new_data = (Token*)arena_alloc(arena, array.capacity * sizeof(Token));
        for(size_t i = 0; i < old_cap; i++) new_data[i] = array.data[i];
        array.data = new_data;
    }

    Token eof = {
        .type = TOKEN_EOF,
        .view = { .data = "EOF", .len = 3 },
        .line = lexer->line,
        .col = lexer->col
    };

    array.data[array.count++] = eof;
    return array;
}
