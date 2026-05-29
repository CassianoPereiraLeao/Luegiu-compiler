#pragma once

#include "common.h"
#include "arena.h"

#define HASH_TABLE_SIZE 128

typedef enum {
    SYMBOL_VAR,
    SYMBOL_FUNC,
    SYMBOL_STRUCT,
    SYMBOL_TYPEDEF
} SymbolType;

typedef struct Symbol {
    const char* name;
    size_t len;

    SymbolType type;
    size_t type_len;

    const char* type_name;
    size_t pointer_lvl;
    size_t param_count;
    bool is_array;

    struct Symbol* next;
} Symbol;

typedef struct SymbolTable{
    Symbol* buckets[HASH_TABLE_SIZE];
    struct SymbolTable* enclosing;
} SymbolTable;

SymbolTable* create_table(SymbolTable *enclosing, Arena *arena);
void semantic_analysis(ASTNode *node, SymbolTable *table, Arena *arena, const char* return_type);
const char* type_of_expr(ASTNode *node, SymbolTable *table);
Symbol* lookup_symbol(SymbolTable *table, const char* name, size_t len);
