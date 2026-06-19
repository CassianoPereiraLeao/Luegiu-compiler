#pragma once

#include "common.h"
#include "lexer.h"
#include "arena.h"
#include "typecheck.h"
#include "diag.h"

typedef struct NodeList NodeList;

typedef enum {
    NODE_VAR_DECL,
    NODE_CAST,
    NODE_BINARY_OP,
    NODE_FUNC_CALL,
    NODE_FUNC_DECL,
    NODE_UNARY_OP,
    NODE_INT_LIT,
    NODE_DOUBLE_LIT,
    NODE_CHAR_LIT,
    NODE_STRING_LIT,
    NODE_HEXA_LIT,
    NODE_POSTFIX_OP,
    NODE_ARRAY,
    NODE_TYPEDEF_DECL,
    NODE_STRUCT,
    NODE_UNION,
    NODE_ENUM,
    NODE_ENUM_MEMBER,
    NODE_BLOCK,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_FOR_STMT,
    NODE_DO_WHILE_STMT,
    NODE_BREAK_STMT,
    NODE_CONTINUE_STMT,
    NODE_VAR_ACCESS,
    NODE_RETURN_STMT,
    NODE_PROGRAM
} NodeType;

typedef enum {
    PRIMITIVE,
    STRUCT,
    UNION,
    ENUM
} TypedefKind;

typedef struct Node {
    NodeType type;
    struct Node* next;

    const char* file;
    size_t line;
    size_t col;
    union {
        struct {
            TokenType data_type;
            size_t pointer_lvl;
            View name;
            View typename;
            bool is_const;
            bool is_static;
            struct Node* init;
        } var_decl;

        struct {
            TokenType op;
            struct Node* right;
            struct Node* left;
        } binary_op;

        struct {
            TokenType op;
            struct Node* operand;
        } unary_op;
        
        struct {
            long long value;
        } numeric_literal;

        struct {
            NodeList* declarations;
        } program;

        struct {
            TypedefKind type;
            View name;
            struct Node* struct_node;
            struct Node* enum_node;
            Token primitive;
            size_t primitive_ptr_lvl;
        } typedef_decl;

        struct {
            View name;
            NodeList* fields;
        } struct_decl;

        struct {
            struct Node* condition;
            struct Node* then;
            struct Node* otherwise;
        } if_stmt;

        struct {
            struct Node* condition;
            struct Node* body;
        } while_stmt;

        struct {
            struct Node* init;
            struct Node* condition;
            struct Node* increment;
            struct Node* body;
        } for_stmt;

        struct {
            View name;
        } var_access;

        struct {
            View name;
            bool has_name;
            NodeList* members;
        } enum_decl;

        struct {
            View name;
            struct Node* expr;
        } enum_member;

        struct {
            TokenType target;
            size_t ptr_lvl;
            View typename;
            struct Node* expr;
        } cast_expr;

        struct {
            TokenType return_type;
            size_t pointer_lvl;
            View name;
            NodeList* params;
            bool is_static;
            struct Node* body;
        } func_decl;

        struct {
            View name;
            NodeList* args;
        } func_call;

        struct {
            struct Node* value;
        } return_stmt;

        struct {
            View text;
        } string_literal;
    } ast;
} Node;

struct NodeList {
    Node** items;
    size_t count;
    size_t capacity;
};

typedef struct {
    Lexer* lexer;
    Token current;
    Token prev;
    Arena* arena;
    SymbolTable* table;
    DiagContext *diag;
} Parser;

Parser create_parser(Lexer* lexer, Arena *arena, SymbolTable *table, DiagContext *diag);
Node* parse_program(Parser *parser);
