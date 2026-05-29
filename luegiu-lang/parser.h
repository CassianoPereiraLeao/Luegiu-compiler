#pragma once

#include "common.h"
#include "tokenizer.h"
#include "arena.h"

typedef struct ASTNode ASTNode;

typedef enum {
    NODE_NUMERIC_LITERAL,
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
    NODE_FOR_STMT
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
        } var_decl;

        struct {
            struct ASTNode** statements;
            size_t count;
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
        } for_stmt;
    } ast;
};

typedef struct {
    Token_Array tokens;
    size_t cursor;
    Arena* arena;
} Parser;

ASTNode* parse_expr(Parser *parser);
ASTNode* parse_statement(Parser *parser);
