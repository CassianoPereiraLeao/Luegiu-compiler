#include "parser.h"

static NodeList* list_create(Arena *arena) {
    NodeList* list = (NodeList*)arena_alloc(arena, sizeof(NodeList));
    list->capacity = 4;
    list->count = 0;
    list->items = (Node**)arena_alloc(arena, sizeof(Node*) * list->capacity);
    list->is_const = false;
    return list;
}

static void list_push(Arena *arena, NodeList *list, Node *node) {
    if(list->count + 1 >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        Node** new_items = (Node**)arena_alloc(arena, sizeof(Node*) * new_cap);
        
        for(size_t i = 0; i < list->count; ++i) {
            new_items[i] = list->items[i];
        }

        list->items = new_items;
        list->capacity = new_cap;
    }

    list->items[list->count++] = node;
}

static Token peek(Parser *parser) {
    return parser->current;
}

static Token advance(Parser *parser) {
    parser->previous = parser->current;
    parser->current = next_token(parser->lexer);
    return parser->current;
}

static Token consume(Parser *parser, TokenType expected, const char* fallback) {
    if(peek(parser).type != expected) {
        diag_fatal(parser->diag, parser->lexer->filename,
            peek(parser).line, peek(parser).col,
            "%s", fallback);
    }

    return advance(parser);
}

static bool iskey(Token token) {
    if(token.type >= TOKEN_POSITIVE && token.type <= TOKEN_CREATETYPE) {
        return true;
    }
    return false;
}

static Node* create_node(Parser *parser, NodeType type) {
    Node* node = (Node*)arena_alloc(parser->arena, sizeof(Node));
    memset(node, 0, sizeof(Node));
    node->type = type;
    node->col = parser->current.col;
    node->line = parser->current.line;
    node->file = parser->current.filename;
    return node;
}

static Node* parse_while_loop(Parser *parser) {
    Node* node = create_node(parser, NODE_WHILE_LOOP);

    consume(parser, TOKEN_WHILE, "Esperado while");
    consume(parser, TOKEN_LPAREN, "Esperado '(' apos o while");

    node->while_stmt.condition = parse_expr(parser);

    consume(parser, TOKEN_RPAREN, "Esperado ')' apos a expressao");
    node->while_stmt.body = parse_statement(parser);

    return node;
}

static Node* parse_for_loop(Parser *parser) {
    Node* node = create_node(parser, NODE_FOR_LOOP);

    consume(parser, TOKEN_FOR, "Esperado for");
    consume(parser, TOKEN_RPAREN, "Esperado '(' apos o for");

    if(peek(parser).type == TOKEN_SEMICOLON) {
        node->for_stmt.init = NULL;
    } else {
        node->for_stmt.init = parse_decl(parser);
    }

    consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos a declaracao");

    if(peek(parser).type == TOKEN_SEMICOLON) {
        node->for_stmt.condition = NULL;
        consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos a condicao");
    } else if (iskey(peek(parser)) || peek(parser).type == TOKEN_CONST || peek(parser).type == TOKEN_STATIC){
        node->for_stmt.condition = parse_decl(parser);
    } else {
        node->for_stmt.condition = parse_expr(parser);
        consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos a condicao");
    }


    if(peek(parser).type == TOKEN_LPAREN) {
        node->for_stmt.increment = NULL;
    } else {
        node->for_stmt.increment = parse_expr(parser);
    }

    node->for_stmt.body = parse_statement(parser);

    return node;
}

static Node* parse_if_stmt(Parser *parser) {
    Node* node = create_node(parser, NODE_IF_STMT);
    consume(parser, TOKEN_IF, "Esperado if");
    consume(parser, TOKEN_RPAREN, "Esperado '(' apos o if");

    node->if_stmt.condition = parse_expr(parser);

    consume(parser, TOKEN_LPAREN, "Esperado ')' apos a expressao");

    node->if_stmt.then = parse_statement(parser);

    if(peek(parser).type == TOKEN_ELSE) {
        node->if_stmt.otherwise = parse_statement(parser);
    } else {
        node->if_stmt.otherwise = NULL;
    }

    return node;
}

static Node* parse_continue(Parser *parser) {
    Node* node = create_node(parser, NODE_CONTINUE);
    consume(parser, TOKEN_CONTINUE, "Esperado continue");
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos o continue");
    return node;
}

static Node* parse_break(Parser *parser) {
    Node* node = create_node(parser, NODE_BREAK);
    consume(parser, TOKEN_BREAK, "Esperado break");
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos o break");
    return node;
}

static Node* parse_return(Parser *parser) {
    Node* node = create_node(parser, NODE_RETURN);
    consume(parser, TOKEN_RETURN, "Esperado return");
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos o return");
    return node;
}

static Node* parse_statement(Parser *parser) {
    switch (peek(parser).type)
    {
    case TOKEN_IF: return parse_if_stmt(parser);
    case TOKEN_BREAK: return parse_break(parser);
    case TOKEN_CONTINUE: return parse_continue(parser);
    case TOKEN_LBRACE: return parse_block(parser);
    case TOKEN_STATIC:
    case TOKEN_CONST: return parse_decl(parser);
    case TOKEN_WHILE: return parse_while_loop(parser);
    case TOKEN_FOR: return parse_for_loop(parser);
    case TOKEN_RETURN: return parse_return(parser);
    case TOKEN_SWITCH: return parse_switch_stmt(parser);
    case TOKEN_DO: return parse_do_while_loop(parser);
    case TOKEN_DATA: return parse_data(parser);
    case TOKEN_CREATETYPE: return parse_createtype(parser);
    case TOKEN_COPERATE: return parse_coperate(parser);
    }

    if(iskey(peek(parser))) {
        return parse_decl(parser);
    }

    Node* expr = parse_expr(parser);
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos a expressao");
    return expr;
}

Node* parse_program(Parser *parser) {
    Node* program = create_node(parser, NODE_PROGRAM);
    program->program.declarations = list_create(parser->arena);

    while (peek(parser).type != TOKEN_EOF) {
        Node* statement = parse_statement(parser);
        if(statement) {
            list_push(parser->arena, program->program.declarations, statement);
        }
    }

    return program;
}
