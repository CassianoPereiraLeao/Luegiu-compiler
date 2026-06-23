#pragma once

#include "common.h"
#include "lexer.h"
#include "arena.h"

typedef struct NodeList NodeList;

typedef enum {
    NODE_VAR_DECL,
    NODE_FUNC_CALL,
    NODE_FUNC_DECl,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_POSTFIX_OP,
    NODE_INT_LIT,
    NODE_CHAR_LIT,
    NODE_STRING_LIT,
    NODE_HEXA_LIT,
    NODE_DOUBLE_LIT,
    NODE_CAST,
    NODE_ARRAY,
    NODE_CREATETYPE_DECL,
    NODE_DATA,
    NODE_ENUM,
    NODE_ENUM_MEMBER,
    NODE_BLOCK,
    NODE_FOR_LOOP,
    NODE_WHILE_LOOP,
    NODE_DO_WHILE_LOOP,
    NODE_IF_STMT,
    NODE_SWITCH_STMT,
    NODE_CASE,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_VAR_ACCESS,
    NODE_RETURN,
    NODE_PROGRAM
} NodeType;

typedef enum {
    PRIMITIVE,
    DATA,
    COPERATE,
    ENUM
} CreatetypeType;

typedef struct {
    NodeType type;
    const char* file;
    size_t line;
    size_t col;
    struct Node* next;
    union {
        struct {
            TokenType type;
            size_t pointer_lvl;
            View name;
            View typename;
            bool is_const;
            bool is_static;
            struct Node* init;
        } var_decl;

        struct {
            View name;
        } var_access;

        struct {
            TokenType op;
            struct Node* left;
            struct Node* right;
        } binary;

        struct {
            TokenType op;
            struct Node* operand;
        } unary;

        struct {
            long long value;
            long double fvalue;
        } numeric;

        struct {
            NodeList* declarations;
        } program;

        struct {
            CreatetypeType type;
            View name;
            struct Node* data_node;
            struct Node* enum_node;
            Token primitive;
            size_t ptr_lvl;
        } createtype;

        struct {
            View name;
            NodeList* fields;
        } data_decl;

        struct {
            View name;
            bool has_name;
            NodeList* members;
        } enum_decl;

        struct {
            struct Node* subject;
            NodeList* cases;
        } switch_stmt;

        struct {
            struct Node* expr;
            struct Node* body;
        } cases;

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
            TokenType target;
            size_t ptr_lvl;
            View typename;
            struct Node* expr;
        } cast;

        struct {
            TokenType type;
            size_t ptr_lvl;
            View name;
            View typename;
            NodeList* params;
            bool is_const;
            bool is_static;
            bool is_variadic;
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
        } string_lit;
    };
    
} Node;

struct NodeList {
    bool is_const;
    Node** items;
    size_t count;
    size_t capacity;
};

typedef struct {
    Token current;
    Token previous;
    Lexer *lexer;
    Arena *arena;
    DiagContext *diag;
} Parser;

Parser create_parser(Lexer *lexer, Arena *arena, DiagContext *context);
Node* parse_program(Parser *parser);
