#pragma once

#include "common.h"
#include "tokenizer.h"
#include "arena.h"
#include "typecheck.h"

typedef enum {
    NODE_NUMERIC_LITERAL,
    NODE_CHAR_LITERAL,
    NODE_STRING_LITERAL,
    NODE_HEXA,
    NODE_IDENTIFIER,
    NODE_BINARY_EXPR,
    NODE_UNARY_EXPR,
    NODE_ASSIGN_EXPR,
    NODE_UNARY_PREFIX_EXPR,
    NODE_UNARY_POSTFIX_EXPR,
    NODE_VAR_DECL,
    NODE_BLOCK,
    NODE_FUNC_DECL,
    NODE_FUNC_CALL,
    NODE_RETURN_STMT,
    NODE_BREAK_STMT,
    NODE_CONTINUE_STMT,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_DO_WHILE_STMT,
    NODE_FOR_STMT,
    NODE_INDEX_EXPR,
    NODE_MEMBER_ACCESS,
    NODE_STRUCT_DECL,
    NODE_TYPEDEF,
    NODE_PROGRAM
} NodeTypes;

struct ASTNode {
    NodeTypes type;
    union {
        Token token;

        struct {
            Token op;
            ASTNode* left;
            ASTNode* right;
        } binary;

        struct {
            Token op;
            ASTNode* operand;
        } unary;

        struct {
            Token type;
            Token identifier;
            ASTNode* value;
            size_t pointer_lvl;
            ASTNode* size;
        } var_decl;

        struct {
            struct ASTNode** statements;
            size_t count;
            struct SymbolTable* scope;
        } block;

        struct {
            Token type;
            Token name;
            struct ASTNode** params;
            size_t count;
            struct ASTNode* body;
        } func_decl;

        struct {
            Token name;
            struct ASTNode** args;
            size_t args_count;
        } func_call;

        struct {
            struct ASTNode* expr;
        } return_stmt;

        struct {
            struct ASTNode* condition;
            struct ASTNode* then;
            struct ASTNode* otherwise;
        } if_stmt;

        struct {
            struct ASTNode* condition;
            struct ASTNode* body;
        } loop_stmt;

        struct {
            struct ASTNode* var;
            struct ASTNode* condition;
            struct ASTNode* increment;
            struct ASTNode* body;
            struct SymbolTable* scope;
        } for_stmt;

        struct {
            Token name;
            size_t fields_count;
            struct ASTNode** fields;
        } struct_decl;

        struct {
            Token member;
            Token op;
            ASTNode* left;
        } access_member;

        struct {
            size_t count;
            struct ASTNode** statements;
        } program;

        struct {
            Token type;
            Token name;
            size_t pointer_lvl;
        } typedef_decl;

        struct {
            long long value;
        } hexa;
    } ast;
};

typedef struct {
    Token_Array tokens;
    size_t cursor;
    Arena* arena;
} Parser;

ASTNode* parse_expr(Parser *parser);
ASTNode* parse_program(Parser *parser);
