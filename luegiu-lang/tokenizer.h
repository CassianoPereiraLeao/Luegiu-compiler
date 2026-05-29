#pragma once

#include "common.h"
#include "arena.h"

typedef enum {
    TOKEN_EOF,
    TOKEN_UNDEFINED,

    TOKEN_IDENTIFIER,
    TOKEN_NUMERIC_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,
    TOKEN_HEXA,

    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_FLOAT,
    TOKEN_KEYWORD_UINT8,
    TOKEN_KEYWORD_UINT16,
    TOKEN_KEYWORD_UINT32,
    TOKEN_KEYWORD_UINT64,
    TOKEN_KEYWORD_INT16,
    TOKEN_KEYWORD_INT8,
    TOKEN_KEYWORD_INT32,
    TOKEN_KEYWORD_INT64,
    TOKEN_KEYWORD_DOUBLE,
    TOKEN_KEYWORD_VOID,
    TOKEN_KEYWORD_CHAR,
    TOKEN_KEYWORD_UTF8CHAR,
    TOKEN_KEYWORD_UTF16CHAR,
    TOKEN_KEYWORD_SMALL,
    TOKEN_KEYWORKD_LINK,
    TOKEN_KEYWORD_BIG,

    TOKEN_KEYWORD_RETURN,
    TOKEN_KEYWORD_STATIC,
    TOKEN_KEYWORD_FOR,
    TOKEN_KEYWORD_WHILE,
    TOKEN_KEYWORD_DO,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_BREAK,
    TOKEN_KEYWORD_CONTINUE,
    TOKEN_KEYWORD_BOOLEAN,
    TOKEN_KEYWORD_TRUE,
    TOKEN_KEYWORD_FALSE,
    TOKEN_KEYWORD_STRUCT,
    TOKEN_KEYWORD_SWITCH,
    TOKEN_KEYWORD_CASE,
    TOKEN_KEYWORD_ENUM,
    TOKEN_KEYWORD_TYPEDEFINITION,

    TOKEN_OP_PLUS,
    TOKEN_OP_PLUS_PLUS,
    TOKEN_OP_PLUS_EQ,
    TOKEN_OP_ASSIGN,
    TOKEN_OP_EQ_EQ,
    TOKEN_OP_MINUS,
    TOKEN_OP_MINUS_MINUS,
    TOKEN_OP_MINUS_EQ,
    TOKEN_OP_SLASH,
    TOKEN_OP_SLASH_EQ,
    TOKEN_OP_STAR,
    TOKEN_OP_STAR_EQ,
    TOKEN_OP_TERN,
    TOKEN_OP_MOD,
    TOKEN_OP_MOD_EQ,
    TOKEN_OP_BANG,
    TOKEN_OP_BANG_EQ,
    TOKEN_OP_LT,
    TOKEN_OP_GT,
    TOKEN_OP_GT_EQ,
    TOKEN_OP_LT_EQ,
    TOKEN_OP_OR,
    TOKEN_OP_AND,

    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,

    TOKEN_BIT_LSHIFT,
    TOKEN_BIT_LSHIFT_EQ,
    TOKEN_BIT_RSHIFT,
    TOKEN_BIT_RSHIFT_EQ,
    TOKEN_BIT_AND,
    TOKEN_BIT_AND_EQ,
    TOKEN_BIT_OR,
    TOKEN_BIT_OR_EQ
} TokenType;

typedef struct {
    const char* data;
    size_t len;
} View;

typedef struct {
    TokenType type;
    View view;
    uint32_t line;
    uint32_t col;
} Token;

typedef struct {
    Token* data;
    size_t count;
    size_t capacity;
} Token_Array;

typedef struct {
    const char* source;
    size_t cursor;
    size_t src_len;
    uint32_t line;
    uint32_t col;
} Lexer;

Lexer create_lexer(const char* src, size_t len);
Token_Array tokenize(Lexer *lexer, Arena *arena);
