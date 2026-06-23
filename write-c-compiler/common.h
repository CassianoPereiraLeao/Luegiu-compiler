#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct DiagContext DiagContext;

typedef enum {
    TOKEN_UNDEFINED,
    TOKEN_EOF,
    TOKEN_IDENTIFIER,

    TOKEN_INT_LIT,
    TOKEN_HEXA_LIT,
    TOKEN_DOUBLE_LIT,
    TOKEN_CHAR_LIT,
    TOKEN_STRING_LIT,

    TOKEN_POSITIVE,
    TOKEN_BIG,
    TOKEN_SMALL,
    TOKEN_INT32,
    TOKEN_INT64,
    TOKEN_INT16,
    TOKEN_INT8,
    TOKEN_UINT8,
    TOKEN_UINT16,
    TOKEN_UINT32,
    TOKEN_UINT64,
    TOKEN_FLOAT,
    TOKEN_DOUBLE,
    TOKEN_CHAR,
    TOKEN_UTF16CHAR,
    TOKEN_VOID,
    TOKEN_LINK,

    TOKEN_RETURN,
    TOKEN_STATIC,
    TOKEN_CONST,
    TOKEN_FOR,
    TOKEN_DO,
    TOKEN_WHILE,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DATA,
    TOKEN_COPERATE,
    TOKEN_ENUM,
    TOKEN_CREATETYPE,

    TOKEN_P,
    TOKEN_PEQ,
    TOKEN_PP,
    TOKEN_M,
    TOKEN_MM,
    TOKEN_MEQ,
    TOKEN_EQ,
    TOKEN_EQEQ,
    TOKEN_BANG,
    TOKEN_BANGEQ,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,
    TOKEN_STAR,
    TOKEN_STAREQ,
    TOKEN_SLASH,
    TOKEN_SLASHEQ,
    TOKEN_OR,
    TOKEN_OREQ,
    TOKEN_AND,
    TOKEN_ANDEQ,
    TOKEN_LOGAND,
    TOKEN_LOGOR,
    TOKEN_XOR,
    TOKEN_XOREQ,
    TOKEN_DESC,
    TOKEN_ARROW,
    TOKEN_INF_ARGS,
    TOKEN_TERN,
    TOKEN_MOD,
    TOKEN_MODEQ,

    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_SEMICOLON,
    TOKEN_COLON,

    TOKEN_LSHIFT,
    TOKEN_RSHIFT,
    TOKEN_LSHIFTEQ,
    TOKEN_RSHIFTEQ, 
} TokenType;

typedef struct {
    const char* start;
    size_t len;
} View;

typedef struct {
    TokenType type;
    const char* filename;
    View view;
    size_t line;
    size_t col;
} Token;

typedef struct ArenaChunk {
    struct ArenaChunk* next;
    size_t used;
    size_t capacity;
    uint8_t data[];
} ArenaChunk;

typedef struct {
    ArenaChunk* first;
    ArenaChunk *last;
    size_t size;
} Arena;

typedef struct {
    const char* source;
    const char* current;
    const char* filename;
    size_t line;
    size_t col;
    const char* start;
    DiagContext *diag;
} Lexer;
