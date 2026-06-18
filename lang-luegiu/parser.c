#include "parser.h"

static Node* parse_expr(Parser *parser);
static Node* parse_block(Parser *parser);
static Node* parse_decl(Parser* parser);
static Node* parse_statement(Parser *parser);
static Node* parse_struct_stmt(Parser *parser);

static NodeList* list_create(Arena *arena) {
    NodeList* list = (NodeList*)arena_alloc(arena, sizeof(NodeList));
    list->capacity = 4;
    list->count = 0;
    list->items = (Node**)arena_alloc(arena, sizeof(Node*) * list->capacity);
    return list;
}

static void list_push(Arena *arena, NodeList *list, Node *node) {
    if(list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        Node** new_items = (Node**)arena_alloc(arena, sizeof(Node*) * new_cap);

        for(size_t i = 0; i < list->count; ++i)
            new_items[i] = list->items[i];

        list->items = new_items;
        list->capacity = new_cap;
    }

    list->items[list->count++] = node;
}

static Token peek(Parser *parser) {
    return parser->current;
}

static Token advance(Parser *parser) {
    parser->prev = parser->current;
    parser->current = next_token(parser->lexer);
    return parser->current;
}

static bool is_typedef(SymbolTable *table, View name) {
    Symbol* sym = lookup_table(table, name);

    return (sym != NULL && sym->category == SYMBOL_TYPEDEF);
}

static bool match(Parser *parser, TokenType token) {
    if(parser->current.type == token) {
        advance(parser); 
        return true;
    }
    return false;
}

static Token expected(Parser *parser, TokenType expected, const char* err) {
    if(parser->current.type != expected) {
        diag_fatal(parser->diag, parser->lexer->file_name,
            (size_t)parser->lexer->line, (size_t)parser->lexer->col,
            "%s", err);
    }

    return advance(parser);
}

static Node* create_node(Parser* parser, NodeType type) {
    Node* node = (Node*)arena_alloc(parser->arena, sizeof(Node));
    memset(node, 0, sizeof(Node));
    node->type = type;
    node->file = parser->current.file_name;
    node->line = parser->current.line;
    node->col = parser->current.col;
    return node;
}

static Node* create_node_at(Parser* parser, NodeType type, Token token) {
    Node* node = (Node*)arena_alloc(parser->arena, sizeof(Node));
    memset(node, 0, sizeof(Node));
    node->type = type;
    node->file = token.file_name;
    node->line = token.line;
    node->col = token.col;
    return node;
}

Parser create_parser(Lexer *lexer, Arena *arena, SymbolTable *table, DiagContext *diag) {
    Parser parser;
    parser.current = next_token(lexer);
    parser.lexer = lexer;
    parser.arena = arena;
    parser.table = table;
    parser.diag = diag;
    return parser;
}

static bool is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static bool is_type(Parser *parser, Token token) {
    if(token.type >= TOKEN_KEYWORD_INT && token.type <= TOKEN_KEYWORD_BIG)
        return true;

    if(token.type == TOKEN_KEYWORD_STRUCT) return true;
    if(token.type == TOKEN_KEYWORD_UNION) return true;
    if(token.type == TOKEN_KEYWORD_ENUM) return true;

    if(token.type == TOKEN_IDENTIFIER)
        return is_typedef(parser->table, token.view);
    
    return false;
}

static long long to_hex(Parser *parser, View string) {
    long long result = 0;
    size_t i = 0;

    if(string.len >= 2 && string.start[0] == '0' &&
        (string.start[1] == 'x' || string.start[1] == 'X'))
        i = 2;

    for(; i < string.len; i++) {
        char c = string.start[i];
        int digit;
        if(c >= '0' && c <= '9')      digit = c - '0';
        else if(c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else {
            diag_error(parser->diag, parser->prev.file_name,
                (size_t)parser->prev.line, (size_t)parser->prev.col,
                "caractere hexadecimal invalido: '%c'", c);
            return result;
        }
        result = result * 16 + digit;
    }
    return result;
}

static long long to_int(Parser *parser, View string) {
    long long result = 0;
    for(size_t i = 0; i < string.len; i++) {
        char c = string.start[i];
        if(is_digit(c))
            result = result * 10 + (c - '0');
        else {
            diag_error(parser->diag, parser->prev.file_name,
                (size_t)parser->prev.line, (size_t)parser->prev.col,
                "o caractere '%c' (ASCII %d) nao e numerico", c, (int)(unsigned char)c);
            return result;
        }
    }

    return result;
}

static double to_double(Parser *parser, View string) {
    double result = 0;
    bool pass_dot = false;
    int64_t divisor = 10;
    for(size_t i = 0; i < string.len; i++) {
        char c = string.start[i];
        if(c == '.') {
            pass_dot = true;
            continue;
        }
        if(c == 'f') continue;
        if(is_digit(c) && !pass_dot)
            result = result * 10 + (c - '0');
        else if(is_digit(c) && pass_dot) {
            result = result + ((c - '0') / divisor);
            divisor *= 10;
        }
        else {
            diag_error(parser->diag, parser->prev.file_name,
                (size_t)parser->prev.line, (size_t)parser->prev.col,
                "o caractere '%c' (ASCII %d) nao e numerico", c, (int)(unsigned char)c);
            return result;
        }
    }

    return result;
}

static Node* parse_primary(Parser *parser) {
    if(match(parser, TOKEN_INT_LIT)) {
        Node* node = create_node_at(parser, NODE_INT_LIT, parser->prev);
        node->ast.numeric_literal.value = to_int(parser, parser->prev.view);
        return node;
    }

    if(match(parser, TOKEN_DOUBLE_LIT)) {
        Node* node = create_node_at(parser, NODE_DOUBLE_LIT, parser->prev);
        node->ast.numeric_literal.value = (long long)to_double(parser, parser->prev.view);
        return node;
    }

    if(match(parser, TOKEN_CHAR_LITERAL)) {
        Node* node = create_node_at(parser, NODE_CHAR_LIT, parser->prev);
        node->ast.string_literal.text = parser->prev.view;
        return node;
    }

    if(match(parser, TOKEN_STRING_LITERAL)) {
        Node* node = create_node_at(parser, NODE_STRING_LIT, parser->prev);
        node->ast.string_literal.text = parser->prev.view;
        return node;
    }

    if(match(parser, TOKEN_HEXA)) {
        Node* node = create_node_at(parser, NODE_HEXA_LIT, parser->prev);
        node->ast.numeric_literal.value = to_hex(parser, parser->prev.view);
        return node;
    }

    if(match(parser, TOKEN_LPAREN)) {
        Node* node = parse_expr(parser);
        expected(parser, TOKEN_RPAREN, "Esperava ')' após a expressão");
        return node;
    }

    if(match(parser, TOKEN_IDENTIFIER)) {
        Token ident_tok = parser->prev;
        View name = ident_tok.view;
        if(match(parser, TOKEN_LPAREN)) {
            Node* node = create_node_at(parser, NODE_FUNC_CALL, ident_tok);
            node->ast.func_call.name = name;
            node->ast.func_call.args = list_create(parser->arena);
            if(peek(parser).type != TOKEN_RPAREN) {
                do {
                    Node* arg = parse_expr(parser);
                    list_push(parser->arena, node->ast.func_call.args, arg);
                } while(match(parser, TOKEN_COMMA));
            }
            expected(parser, TOKEN_RPAREN, "Esperava ')' apos argumentos");
            return node;
        }
        Node* node = create_node_at(parser, NODE_VAR_ACCESS, ident_tok);
        node->ast.var_access.name = name;
        return node;
    }

    diag_fatal(parser->diag, parser->lexer->file_name,
        (size_t)parser->lexer->line, (size_t)parser->lexer->col,
        "esperava uma expressao valida");
    return NULL;
}

static Node* parse_postfix(Parser *parser) {
    Node* expr = parse_primary(parser);

    while(1) {
        if(match(parser, TOKEN_OP_PLUS_PLUS) || match(parser, TOKEN_OP_MINUS_MINUS)) {
            Node* node = create_node_at(parser, NODE_POSTFIX_OP, parser->prev);
            node->ast.unary_op.op = parser->prev.type;
            node->ast.unary_op.operand = expr;
            expr = node;
            continue;
        }

        if(match(parser, TOKEN_LBRACKET)) {
            Node* node = create_node_at(parser, NODE_ARRAY, parser->prev);
            node->ast.binary_op.left = expr;
            node->ast.binary_op.right = parse_expr(parser);
            expected(parser, TOKEN_RBRACKET, "Esperava ']' após o índice");
            expr = node;
            continue;
        }

        if(match(parser, TOKEN_DOT) || match(parser, TOKEN_OP_ARROW)) {
            TokenType op = parser->prev.type;
            expected(parser, TOKEN_IDENTIFIER, "Esperava nome do campo apos '.' ou '->'");

            Node* field = create_node(parser, NODE_VAR_ACCESS);
            field->ast.var_access.name = parser->prev.view;

            Node* node = create_node(parser, NODE_BINARY_OP);
            node->ast.binary_op.op = op;
            node->ast.binary_op.left = expr;
            node->ast.binary_op.right = field;
            expr = node;
            continue;
        }

        break;
    }

    return expr;
}

static Node* parse_unary(Parser *parser) {
    if (match(parser, TOKEN_OP_MINUS) || 
        match(parser, TOKEN_OP_BANG)   || 
        match(parser, TOKEN_OP_PLUS_PLUS)   || 
        match(parser, TOKEN_OP_MINUS_MINUS) ||
        match(parser, TOKEN_BIT_AND) ||
        match(parser, TOKEN_OP_STAR)) {
            Node* node = create_node(parser, NODE_UNARY_OP);
            node->ast.unary_op.op = parser->prev.type;
            node->ast.unary_op.operand = parse_unary(parser);
            return node;
        }

    return parse_postfix(parser);
}

static Node* parse_factor(Parser *parser) {
    Node* expr = parse_unary(parser);

    while(match(parser, TOKEN_OP_STAR) || 
        match(parser, TOKEN_OP_SLASH) || 
        match(parser, TOKEN_OP_MOD)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_unary(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_term(Parser *parser) {
    Node* expr = parse_factor(parser);

    while(match(parser, TOKEN_OP_PLUS) || match(parser, TOKEN_OP_MINUS)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_factor(parser);

        expr = node;
    }

    return expr;
}

static Node* parse_shift(Parser *parser) {
    Node* expr = parse_term(parser);

    while(match(parser, TOKEN_BIT_LSHIFT) || match(parser, TOKEN_BIT_RSHIFT)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_term(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_comparison(Parser *parser) {
    Node* expr = parse_shift(parser);

    while(match(parser, TOKEN_OP_LT) || match(parser, TOKEN_OP_GT) ||
        match(parser, TOKEN_OP_LT_EQ) || match(parser, TOKEN_OP_GT_EQ)) {
        
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_shift(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_equality(Parser *parser) {
    Node* expr = parse_comparison(parser);

    while(match(parser, TOKEN_OP_EQ_EQ) || match(parser, TOKEN_OP_BANG_EQ)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_comparison(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_bitwise_and(Parser *parser) {
    Node* expr = parse_equality(parser);

    while(match(parser, TOKEN_BIT_AND)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_equality(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_bitwise_xor(Parser *parser) {
    Node* expr = parse_bitwise_and(parser);

    while(match(parser, TOKEN_OP_XOR)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_bitwise_and(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_bitwise_or(Parser *parser) {
    Node* expr = parse_bitwise_xor(parser);

    while(match(parser, TOKEN_BIT_OR)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_bitwise_xor(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_logical_and(Parser *parser) {
    Node* expr = parse_bitwise_or(parser);

    while(match(parser, TOKEN_OP_AND)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_bitwise_or(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_logical_or(Parser *parser) {
    Node* expr = parse_logical_and(parser);

    while(match(parser, TOKEN_OP_OR)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_logical_and(parser);
        expr = node;
    }

    return expr;
}

static Node* parse_assignment(Parser *parser) {
    Node* expr = parse_logical_or(parser);

    while(match(parser, TOKEN_OP_ASSIGN) || 
        match(parser, TOKEN_OP_PLUS_EQ) || 
        match(parser, TOKEN_OP_MINUS_EQ) ||
        match(parser, TOKEN_OP_STAR_EQ) ||
        match(parser, TOKEN_OP_SLASH_EQ) ||
        match(parser, TOKEN_BIT_AND_EQ) ||
        match(parser, TOKEN_BIT_OR_EQ) ||
        match(parser, TOKEN_BIT_LSHIFT_EQ) ||
        match(parser, TOKEN_BIT_RSHIFT_EQ)) {
        TokenType op = parser->prev.type;
        Node* node = create_node(parser, NODE_BINARY_OP);
        node->ast.binary_op.op = op;
        node->ast.binary_op.left = expr;
        node->ast.binary_op.right = parse_assignment(parser);
        return node;
    }

    return expr;
}

static Node* parse_expr(Parser *parser) { return parse_assignment(parser); }

static Node* parse_block(Parser *parser) {
    Node* block = create_node(parser, NODE_BLOCK);
    block->ast.program.declarations = list_create(parser->arena);

    expected(parser, TOKEN_LBRACE, "Esperado '{'");

    while(peek(parser).type != TOKEN_RBRACE) {
        if(peek(parser).type == TOKEN_EOF) {
            diag_fatal(parser->diag, parser->lexer->file_name,
                (size_t)parser->lexer->line, (size_t)parser->lexer->col,
                "bloco nao fechado, fim de arquivo inesperado (esperava '}')");
        }
        Node* stmt = parse_statement(parser);
        list_push(parser->arena, block->ast.program.declarations, stmt);
    }

    expected(parser, TOKEN_RBRACE, "bloco nao fechado, esperava '}'");
    return block;
}

static Node* parse_var_decl(Parser *parser, Token type, View typename, size_t ptr_lvl, View name, bool is_const, bool is_static) {
    Node* head = NULL;
    Node* tail = NULL;

    Node* node = create_node(parser, NODE_VAR_DECL);
    node->ast.var_decl.name = name;
    node->ast.var_decl.pointer_lvl = ptr_lvl;
    node->ast.var_decl.data_type = type.type;
    node->ast.var_decl.typename = typename;
    node->ast.var_decl.is_const = is_const;
    node->ast.var_decl.is_static = is_static;

    if(match(parser, TOKEN_OP_ASSIGN))
        node->ast.var_decl.init = parse_expr(parser);

    head = tail = node;

    while(match(parser, TOKEN_COMMA)) {
        Node* next = create_node(parser, NODE_VAR_DECL);
        expected(parser, TOKEN_IDENTIFIER, "Esperava identificador");
        next->ast.var_decl.name = parser->prev.view;
        next->ast.var_decl.data_type = type.type;
        next->ast.var_decl.pointer_lvl = ptr_lvl;

        if(match(parser, TOKEN_OP_ASSIGN))
            next->ast.var_decl.init = parse_expr(parser);
    
        tail->next = next;
        tail = next;
    }

    expected(parser, TOKEN_SEMICOLON, "Esperava ';' apos declaracao");
    return head;
}

static Node* parse_return(Parser *parser) {
    expected(parser, TOKEN_KEYWORD_RETURN, "Esperado 'return'");
    Node* node = create_node(parser, NODE_RETURN_STMT);

    if(parser->current.type != TOKEN_SEMICOLON) {
        node->ast.return_stmt.value = parse_expr(parser);
    }
    else {
        node->ast.return_stmt.value = NULL;
    }

    expected(parser, TOKEN_SEMICOLON, "Esperado ';' apos return");
    return node;
}

static Node* parse_func_decl(Parser *parser, Token type, size_t ptr_lvl, View name, bool is_static) {
    Node* node = create_node(parser, NODE_FUNC_DECL);
    node->ast.func_decl.return_type = type.type;
    node->ast.func_decl.pointer_lvl = ptr_lvl;
    node->ast.func_decl.name = name;
    node->ast.func_decl.params = list_create(parser->arena);
    node->ast.func_decl.is_static = is_static;

    if(!match(parser, TOKEN_RPAREN)) {
        do {
            bool param_const = match(parser, TOKEN_KEYWORD_CONST);

            Token param_token = peek(parser);
            advance(parser);

            if(param_token.type == TOKEN_IDENTIFIER) {
                Symbol* sym = lookup_table(parser->table, param_token.view);
                if(sym && sym->category == SYMBOL_TYPEDEF) {
                    param_token.type = sym->type;
                }
            }

            size_t param_ptr = 0;
            while(match(parser, TOKEN_OP_STAR)) param_ptr++;

            if(param_token.type == TOKEN_OP_INF_ARGS) {
                Node* param = create_node_at(parser, NODE_VAR_DECL, param_token);
                param->ast.var_decl.data_type = TOKEN_OP_INF_ARGS;
                param->ast.var_decl.pointer_lvl = 0;
                list_push(parser->arena, node->ast.func_decl.params, param);
                break;
            }

            expected(parser, TOKEN_IDENTIFIER, "Esperava nome do parametro");

            Node* param = create_node(parser, NODE_VAR_DECL);
            param->ast.var_decl.data_type = param_token.type;
            param->ast.var_decl.pointer_lvl = param_ptr;
            param->ast.var_decl.name = parser->prev.view;
            param->ast.var_decl.init = NULL;

            if(match(parser, TOKEN_LBRACKET)) {
                param->ast.var_decl.pointer_lvl += 1;
                expected(parser, TOKEN_RBRACKET, "Esperava ']' apos '['");
            }

            list_push(parser->arena, node->ast.func_decl.params, param);
        } while(match(parser, TOKEN_COMMA));

        expected(parser, TOKEN_RPAREN, "Esperava ')' apos parametros");
    }

    if(parser->current.type == TOKEN_LBRACE) 
        node->ast.func_decl.body = parse_block(parser);
    else {
        expected(parser, TOKEN_SEMICOLON, "Esperava ';' ou '{' apos declaracao de funcao");
        node->ast.func_decl.body = NULL;
    }

    return node;
}

static Node* parse_decl(Parser* parser) {
    bool is_static = match(parser, TOKEN_KEYWORD_STATIC);
    bool is_const = match(parser, TOKEN_KEYWORD_CONST);

    Token type = peek(parser);
    advance(parser);

    if(type.type == TOKEN_IDENTIFIER) {
        Symbol* sym = lookup_table(parser->table, type.view);
        if(sym && sym->category == SYMBOL_TYPEDEF)
            type.type = sym->type;
    }

    View type_name = { 0 };

    if(type.type == TOKEN_KEYWORD_STRUCT) {
        expected(parser, TOKEN_IDENTIFIER, "Esperado nome da struct.");
        type_name = parser->prev.view;
    }

    if(type.type == TOKEN_KEYWORD_UNION) {
        expected(parser, TOKEN_IDENTIFIER, "Esperado nome da union.");
        type_name = parser->prev.view;
    }

    size_t pointer_lvl = 0;
    while(match(parser, TOKEN_OP_STAR)) pointer_lvl++;

    expected(parser, TOKEN_IDENTIFIER, "Esperava identificador");
    View name = parser->prev.view;

    if(match(parser, TOKEN_LPAREN))
        return parse_func_decl(parser, type, pointer_lvl, name, is_static);
    return parse_var_decl(parser, type, type_name, pointer_lvl, name, is_const, is_static);
}

static Node* parse_union(Parser *parser, bool anonymous) {
    Node* node = create_node(parser, NODE_UNION);
    node->ast.struct_decl.fields = list_create(parser->arena);

    expected(parser, TOKEN_KEYWORD_UNION, "Esperado 'union'");

    if(match(parser, TOKEN_IDENTIFIER))
        node->ast.struct_decl.name = parser->prev.view;
    else if(!anonymous) {
        diag_error(parser->diag, parser->lexer->file_name,
                (size_t)parser->lexer->line, (size_t)parser->lexer->col,
                "esperado nome apos 'union'");
    }

    expected(parser, TOKEN_LBRACE, "Esperado '{' para inicializar a union");

    do {
        if(peek(parser).type == TOKEN_RBRACE) break;

        if(!is_type(parser, peek(parser))) {
            diag_fatal(parser->diag, parser->lexer->file_name,
                (size_t)parser->lexer->line, (size_t)parser->lexer->col,
                "esperado tipo no campo da union");
        }

        Token field_type = peek(parser);
        advance(parser);

        if(field_type.type == TOKEN_IDENTIFIER) {
            Symbol* sym = lookup_table(parser->table, field_type.view);
            if(sym && sym->category == SYMBOL_TYPEDEF)
                field_type.type = sym->type;
        }

        size_t ptr_lvl = 0;
        while(match(parser, TOKEN_OP_STAR)) ptr_lvl++;

        do {
            expected(parser, TOKEN_IDENTIFIER, "Esperado nome do campo da union");

            Node* field = create_node(parser, NODE_VAR_DECL);
            field->ast.var_decl.data_type = field_type.type;
            field->ast.var_decl.pointer_lvl = ptr_lvl;
            field->ast.var_decl.name = parser->prev.view;
            field->ast.var_decl.init = NULL;
            list_push(parser->arena, node->ast.struct_decl.fields, field);
        } while(match(parser, TOKEN_COMMA));
    } while(match(parser, TOKEN_SEMICOLON));

    expected(parser, TOKEN_RBRACE, "Esperado '}' para declaracao da union");
    return node;
}

static Node* parse_union_stmt(Parser *parser) {
    Node* node = parse_union(parser, false);
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' para finalizar declaracao da union");
    return node;
}

static Node* parse_struct(Parser *parser, bool anonymous) {
    Node* node = create_node(parser, NODE_STRUCT);
    node->ast.struct_decl.fields = list_create(parser->arena);

    expected(parser, TOKEN_KEYWORD_STRUCT, "Esperado 'struct'");

    if(match(parser, TOKEN_IDENTIFIER))
        node->ast.struct_decl.name = parser->prev.view;
    else if(!anonymous) {
        diag_error(parser->diag, parser->lexer->file_name,
            (size_t)parser->lexer->line, (size_t)parser->lexer->col,
            "esperado nome apos declarar 'struct'");
    }

    expected(parser, TOKEN_LBRACE, "Esperado '{' para inicializar a struct");

    do {
        if(peek(parser).type == TOKEN_RBRACE) break;

        if(peek(parser).type == TOKEN_KEYWORD_UNION || peek(parser).type == TOKEN_KEYWORD_STRUCT) {
            bool is_union = (peek(parser).type == TOKEN_KEYWORD_UNION);
            Node* inner = is_union ? parse_union(parser, true) : parse_struct(parser, true);

            if(peek(parser).type == TOKEN_IDENTIFIER) {
                inner->ast.struct_decl.name = parser->current.view;
                advance(parser);
            }

            list_push(parser->arena, node->ast.struct_decl.fields, inner);
            continue;
        }
        
        if(!is_type(parser, peek(parser))) {
            diag_fatal(parser->diag, parser->lexer->file_name,
                (size_t)parser->lexer->line, (size_t)parser->lexer->col,
                "esperado tipo no campo da struct");
        }

        Token field_type = peek(parser);
        advance(parser);

        if(field_type.type == TOKEN_IDENTIFIER) {
            Symbol* sym = lookup_table(parser->table, field_type.view);
            if(sym && sym->category == SYMBOL_TYPEDEF)
                field_type.type = sym->type;
        }

        size_t ptr_lvl = 0;
        while(match(parser, TOKEN_OP_STAR)) ptr_lvl++;

        do {
            expected(parser, TOKEN_IDENTIFIER, "Esperado nome do campo da struct");
            
            Node* field = create_node(parser, NODE_VAR_DECL);
            field->ast.var_decl.data_type = field_type.type;
            field->ast.var_decl.init = NULL;
            field->ast.var_decl.pointer_lvl = ptr_lvl;
            field->ast.var_decl.name = parser->prev.view;

            list_push(parser->arena, node->ast.struct_decl.fields, field);
        } while(match(parser, TOKEN_COMMA));
    } while(match(parser, TOKEN_SEMICOLON));

    expected(parser, TOKEN_RBRACE, "Esperado '}' para declaracao da struct");

    return node;
}

static Node* parse_struct_stmt(Parser *parser) {
    Node* node = parse_struct(parser, false);
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' para finalizar declaracao da struct");
    return node;
}

static Node* parse_enum(Parser *parser) {
    Node* node = create_node(parser, NODE_ENUM);
    node->ast.enum_decl.members = list_create(parser->arena);
    node->ast.enum_decl.has_name = false;

    expected(parser, TOKEN_KEYWORD_ENUM, "Esperado 'enum'");

    if(peek(parser).type == TOKEN_IDENTIFIER) {
        node->ast.enum_decl.has_name = true;
        node->ast.enum_decl.name = parser->current.view;
        advance(parser);
    }

    expected(parser, TOKEN_LBRACE, "Esperado '{' apos enum");

    do {
        if(peek(parser).type == TOKEN_RBRACE) break;

        expected(parser, TOKEN_IDENTIFIER, "Esperado identificador no enum");

        Node* member = create_node(parser, NODE_ENUM_MEMBER);
        member->ast.enum_member.name = parser->prev.view;

        if(match(parser, TOKEN_OP_ASSIGN))
            member->ast.enum_member.expr = parse_expr(parser);
        else
            member->ast.enum_member.expr = NULL;

        list_push(parser->arena, node->ast.enum_decl.members, member);
    } while(match(parser, TOKEN_COMMA));

    if(node->ast.enum_decl.members->count == 0) {
        diag_fatal(parser->diag, parser->lexer->file_name,
            (size_t)parser->lexer->line, (size_t)parser->lexer->col,
            "enum deve ter pelo menos um membro");
    }

    expected(parser, TOKEN_RBRACE, "Esperado '}' apos membros do enum");

    return node;
}

static Node* parse_enum_stmt(Parser *parser) {
    Node* node = parse_enum(parser);
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' após o enum");
    return node;
}

static Node* parse_typedefinition(Parser *parser) {
    expected(parser, TOKEN_KEYWORD_TYPEDEFINITION, "Esperado 'typedefinition'");

    Node* node = create_node(parser, NODE_TYPEDEF_DECL);
    TokenType typedefined_token = TOKEN_UNDEFINED;

    if(peek(parser).type == TOKEN_KEYWORD_STRUCT) {
        node->ast.typedef_decl.type = STRUCT;
        node->ast.typedef_decl.struct_node = parse_struct(parser, true);
        typedefined_token = TOKEN_KEYWORD_STRUCT;
    } else if(peek(parser).type == TOKEN_KEYWORD_ENUM) {
        node->ast.typedef_decl.type = ENUM;
        node->ast.typedef_decl.enum_node = parse_enum(parser);
        typedefined_token = TOKEN_KEYWORD_ENUM;
    } else if(peek(parser).type == TOKEN_KEYWORD_UNION) {
        node->ast.typedef_decl.type = UNION;
        node->ast.typedef_decl.enum_node = parse_union(parser, true);
        typedefined_token = TOKEN_KEYWORD_UNION;
    } else {
        node->ast.typedef_decl.type = PRIMITIVE;
        if(peek(parser).type >= TOKEN_KEYWORD_INT && peek(parser).type <= TOKEN_KEYWORD_BIG) {
            node->ast.typedef_decl.primitive = peek(parser);
            typedefined_token = peek(parser).type;
            advance(parser);
        }
        else {
            diag_error(parser->diag, parser->lexer->file_name,
                (size_t)parser->lexer->line, (size_t)parser->lexer->col,
                "esperado tipo primitivo ou struct");
        }
    }

    expected(parser, TOKEN_IDENTIFIER, "Esperado identificador apos a expressao");
    View name = parser->prev.view;
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' apos o identificador");
    node->ast.typedef_decl.name = name;

    if(lookup_table(parser->table, name)){
        diag_error(parser->diag, parser->prev.file_name,
            (size_t)parser->prev.line, (size_t)parser->prev.col,
            "'%.*s' ja foi declarado", (int)name.len, name.start);
    } else {
        insert_table(parser->table, name, SYMBOL_TYPEDEF, 0, typedefined_token);
    } 

    return node;
}

static Node* parse_if(Parser *parser) {
    Node* node = create_node(parser, NODE_IF_STMT);

    expected(parser, TOKEN_KEYWORD_IF, "Esperado 'if'");
    expected(parser, TOKEN_LPAREN, "Esperado '(' apos 'if'");

    node->ast.if_stmt.condition = parse_expr(parser);

    expected(parser, TOKEN_RPAREN, "Esperado ')' apos condicao");

    node->ast.if_stmt.then = parse_statement(parser);
    if(match(parser, TOKEN_KEYWORD_ELSE))
        node->ast.if_stmt.otherwise = parse_statement(parser);
    else
        node->ast.if_stmt.otherwise = NULL;
    
    return node;
}

static Node* parse_break(Parser *parser) {
    expected(parser, TOKEN_KEYWORD_BREAK, "Esperado 'break'");
    Node* node = create_node(parser, NODE_BREAK_STMT);
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' apos 'break'");
    return node;
}

static Node* parse_continue(Parser *parser) {
    expected(parser, TOKEN_KEYWORD_CONTINUE, "Esperado 'continue'");
    Node* node = create_node(parser, NODE_CONTINUE_STMT);
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' apos 'continue'");
    return node; 
}

static Node* parse_do_while_loop(Parser *parser) {
    Node* node = create_node(parser, NODE_DO_WHILE_STMT);
    expected(parser, TOKEN_KEYWORD_DO, "Esperado 'do'");

    node->ast.while_stmt.body = parse_statement(parser);

    expected(parser, TOKEN_KEYWORD_WHILE, "Esperado 'while' apos bloco do 'do'");
    expected(parser, TOKEN_LPAREN, "Esperado '(' apos 'while'");

    node->ast.while_stmt.condition = parse_expr(parser);

    expected(parser, TOKEN_RPAREN, "Esperado ')' apos condicao");
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' apos do-while");

    return node;
}

static Node* parse_while_loop(Parser *parser) {
    Node* node = create_node(parser, NODE_WHILE_STMT);

    expected(parser, TOKEN_KEYWORD_WHILE, "Esperado 'while'");
    expected(parser, TOKEN_LPAREN, "Esperado '(' apos 'while'");

    node->ast.while_stmt.condition = parse_expr(parser);

    expected(parser, TOKEN_RPAREN, "Esperado ')' apos a condicao");

    node->ast.while_stmt.body = parse_statement(parser);

    return node;
}

static Node* parse_for_loop(Parser *parser) {
    Node* node = create_node(parser, NODE_FOR_STMT);

    expected(parser, TOKEN_KEYWORD_FOR, "Esperado 'for'");
    expected(parser, TOKEN_LPAREN, "Esperado '(' apos 'for'");

    if(peek(parser).type == TOKEN_SEMICOLON) {
        node->ast.for_stmt.init = NULL;
        advance(parser);
    } else if(is_type(parser, peek(parser)) || peek(parser).type == TOKEN_KEYWORD_CONST || peek(parser).type == TOKEN_KEYWORD_STATIC)
        node->ast.for_stmt.init = parse_decl(parser);
    else {
        node->ast.for_stmt.init = parse_expr(parser);
        expected(parser, TOKEN_SEMICOLON, "Esperado ';' apos init do for");
    }

    if(peek(parser).type == TOKEN_SEMICOLON) 
        node->ast.for_stmt.condition = NULL;
    else
        node->ast.for_stmt.condition = parse_expr(parser);
    
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' apos condicao do for");

    if(peek(parser).type == TOKEN_RPAREN) 
        node->ast.for_stmt.increment = NULL;
    else
        node->ast.for_stmt.increment = parse_expr(parser);

    expected(parser, TOKEN_RPAREN, "Esperado ')' apos update do for");

    node->ast.for_stmt.body = parse_statement(parser);

    return node;
}

static Node* parse_statement(Parser *parser) {
    if(peek(parser).type == TOKEN_LBRACE)
        return parse_block(parser);

    if(peek(parser).type == TOKEN_KEYWORD_STATIC || peek(parser).type == TOKEN_KEYWORD_CONST)
        return parse_decl(parser);

    if(peek(parser).type == TOKEN_KEYWORD_TYPEDEFINITION)
        return parse_typedefinition(parser);

    if(peek(parser).type == TOKEN_KEYWORD_STRUCT)
        return parse_struct_stmt(parser);

    if(peek(parser).type == TOKEN_KEYWORD_UNION)
        return parse_union_stmt(parser);

    if(peek(parser).type == TOKEN_KEYWORD_ENUM)
        return parse_enum_stmt(parser);

    if(peek(parser).type == TOKEN_KEYWORD_CONTINUE)
        return parse_continue(parser);

    if(peek(parser).type == TOKEN_KEYWORD_BREAK)
        return parse_break(parser);

    if(peek(parser).type == TOKEN_KEYWORD_DO)
        return parse_do_while_loop(parser);

    if(peek(parser).type == TOKEN_KEYWORD_WHILE)
        return parse_while_loop(parser);

    if(peek(parser).type == TOKEN_KEYWORD_FOR)
        return parse_for_loop(parser);
    
    if(is_type(parser, peek(parser)))
        return parse_decl(parser);

    if(peek(parser).type == TOKEN_KEYWORD_IF) 
        return parse_if(parser);
        
    if(peek(parser).type == TOKEN_KEYWORD_RETURN)
        return parse_return(parser);

    Node* expr = parse_expr(parser);
    expected(parser, TOKEN_SEMICOLON, "Esperado ';' apos expressao");
    return expr;
}

Node* parse_program(Parser *parser) {
    Node* program = create_node(parser, NODE_PROGRAM);
    program->ast.program.declarations = list_create(parser->arena);

    while(peek(parser).type != TOKEN_EOF) {
        Node* stmt = parse_statement(parser);
        if(stmt)
            list_push(parser->arena, program->ast.program.declarations, stmt);
    }

    return program;
}
