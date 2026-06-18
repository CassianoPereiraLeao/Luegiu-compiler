#pragma once

#include "common.h"
#include "arena.h"
#include "diag.h"

typedef struct Node Node;
typedef struct NodeList NodeList;

typedef struct {
    int error_count;
    bool in_func;
    bool in_loop;
    TokenType current_return_type;
    size_t current_return_ptr_lvl;
    DiagContext *diag;
} TypeChecker;

Symbol* insert_table(SymbolTable *table, View name, SymbolCategory category, size_t pointer_lvl, TokenType type);
Symbol* lookup_table(SymbolTable *table, View name);
void push_scope(SymbolTable *table);
void pop_scope(SymbolTable *table);
void typecheck_program(TypeChecker *typechecker, SymbolTable *table, Node *program, DiagContext *context);
const char* token_to_str(TokenType type);
