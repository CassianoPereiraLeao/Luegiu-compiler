#pragma once

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _MSC_VER
  typedef unsigned __int8  uint8_t;
  typedef unsigned __int16 uint16_t;
  typedef unsigned __int32 uint32_t;
  typedef unsigned __int64 uint64_t;
  typedef __int8           int8_t;
  typedef __int16          int16_t;
  typedef __int32          int32_t;
  typedef __int64          int64_t;
  typedef unsigned __int64 size_t;
  typedef __int64          ptrdiff_t;
#else
  typedef unsigned char      uint8_t;
  typedef unsigned short     uint16_t;
  typedef unsigned int       uint32_t;
  typedef unsigned long long uint64_t;
  typedef signed char        int8_t;
  typedef short              int16_t;
  typedef int                int32_t;
  typedef long long          int64_t;
  typedef unsigned long long size_t;
  typedef long long          ptrdiff_t;
#endif

typedef struct DiagContext DiagContext;

typedef enum {
    TOKEN_EOF,
    TOKEN_UNDEFINED,

    TOKEN_IDENTIFIER,
    TOKEN_DOUBLE_LIT,
    TOKEN_INT_LIT,
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
    TOKEN_KEYWORD_BOOLEAN,
    TOKEN_KEYWORD_BIG,

    TOKEN_KEYWORD_RETURN,
    TOKEN_KEYWORD_STATIC,
    TOKEN_KEYWORD_CONST,
    TOKEN_KEYWORD_FOR,
    TOKEN_KEYWORD_WHILE,
    TOKEN_KEYWORD_DO,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_BREAK,
    TOKEN_KEYWORD_CONTINUE,
    TOKEN_KEYWORD_TRUE,
    TOKEN_KEYWORD_FALSE,
    TOKEN_KEYWORD_STRUCT,
    TOKEN_KEYWORD_UNION,
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
    TOKEN_OP_XOR,
    TOKEN_OP_DESC,
    TOKEN_OP_INF_ARGS,
    TOKEN_OP_ARROW,

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
    const char* start;
    size_t len;
} View;

typedef struct {
    const char* src;
    const char* cursor;
    size_t line, col;
    const char* start;
    const char* file_name;
    DiagContext* diag;
} Lexer;

typedef struct {
    TokenType type;
    View view;
    int line, col;
    const char* file_name;
} Token;

typedef struct ArenaChunk {
    struct ArenaChunk* next;
    size_t used;
    size_t capacity;
    uint8_t data[];
} ArenaChunk;

typedef struct {
    ArenaChunk* first;
    ArenaChunk* last;
    size_t size;
} Arena;

typedef enum {
    SYMBOL_VAR,
    SYMBOL_FUNC,
    SYMBOL_TYPEDEF,
    SYMBOL_STRUCT,
    SYMBOL_ENUM
} SymbolCategory;

typedef struct {
    TokenType type;
    size_t pointer_lvl;
} ParamInfo;

typedef struct {
    View name;
    View typename;
    TokenType type;
    size_t pointer_lvl;
} FieldInfo;

typedef struct Symbol {
    View name;
    View typename;
    SymbolCategory category;
    TokenType type;
    size_t pointer_lvl;

    ParamInfo* params;
    size_t params_count;

    FieldInfo* fields;
    size_t fields_count;

    bool is_const;
    bool is_static;
    long long const_value;
    bool has_const_value;
    struct Symbol* next;
} Symbol;

typedef struct Scope {
    Symbol* symbols;
    struct Scope* parent;
} Scope;

typedef struct {
    Scope* current_scope;
    Arena* arena;
} SymbolTable;
