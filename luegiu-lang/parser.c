#include "parser.h"

static Token peek(Parser *parser) {
    if(parser->cursor >= parser->tokens.count) return parser->tokens.data[parser->tokens.count - 1];
    return parser->tokens.data[parser->cursor];
}

static Token seeforeward(Parser *parser, uint8_t distance) {
    if(parser->cursor >= parser->tokens.count) return parser->tokens.data[parser->tokens.count - 1];
    return parser->tokens.data[parser->cursor + distance];
}

static Token advance(Parser *parser) {
    if(parser->cursor < parser->tokens.count) 
        return parser->tokens.data[parser->cursor++];
    return parser->tokens.data[parser->tokens.count - 1];
}

static Token consume(Parser *parser, uint8_t expected_type, const char* err) {
    Token current = peek(parser);
    if (current.type != expected_type) {
        fprintf(stderr, "Erro de Sintaxe na linha %u, col %u: %s (Encontrou '%.*s')\n",
                current.line, current.col, err, (int)current.view.len, current.view.data);
        exit(1);
    }
    return advance(parser);
}

static ASTNode* create_node(Parser *parser, NodeTypes type) {
    ASTNode* node = (ASTNode*)arena_alloc(parser->arena, sizeof(ASTNode));
    if(node == NULL) {
        fprintf(stderr, "[Erro Crítico] Arena sem memória para nós da AST.\n");
        exit(1);
    }
    node->type = type;
    return node;
}

static ASTNode* parse_block(Parser *parser);
static ASTNode* parse_var(Parser *parser);
static ASTNode* parse_if_statement(Parser *parser);
static ASTNode* parse_for_statement(Parser *parser);
static ASTNode* parse_while_statement(Parser *parser);
static ASTNode* parse_do_while_statement(Parser *parser);
static ASTNode* parse_statement(Parser *parser);
static ASTNode* parse_typedef(Parser *parser);

static ASTNode* parse_primary(Parser *parser) {
    Token current = peek(parser);

    if(current.type == TOKEN_NUMERIC_LITERAL) {
        advance(parser);
        ASTNode* node = create_node(parser, NODE_NUMERIC_LITERAL);
        node->ast.token = current;
        return node;
    }

    if(current.type == TOKEN_IDENTIFIER) {
        advance(parser);
        ASTNode* node = create_node(parser, NODE_IDENTIFIER);
        node->ast.token = current;
        return node;
    }

    if(current.type == TOKEN_CHAR_LITERAL) {
        advance(parser);
        ASTNode* node = create_node(parser, NODE_CHAR_LITERAL);
        node->ast.token = current;
        return node;
    }

    if(current.type == TOKEN_STRING_LITERAL) {
        advance(parser);
        ASTNode* node = create_node(parser, NODE_STRING_LITERAL);
        node->ast.token = current;
        return node;
    }

    if(current.type == TOKEN_HEXA) {
        advance(parser);
        ASTNode* node = create_node(parser, NODE_HEXA);
        node->ast.token = current;
        return node;
    }

    fprintf(stderr, "Erro de Sintaxe na linha %u, col %u: Esperado um número ou identificador, mas encontrou '%.*s'\n",
            current.line, current.col, (int)current.view.len, current.view.data);
    exit(1);
}

static ASTNode* parse_postfix(Parser *parser) {
    ASTNode* expr = parse_primary(parser);

    while(1) {
        Token op = peek(parser);

        if(op.type == TOKEN_LPAREN) {
            advance(parser);

            if(expr->type != NODE_IDENTIFIER) {
                fprintf(stderr, "Erro de Sintaxe: Apenas identificadores podem ser chamados como funcao.\n");
                exit(1);
            }

            Token name = expr->ast.token;
            ASTNode* args[16];
            size_t args_count = 0;

            if(peek(parser).type != TOKEN_RPAREN) {
                do {
                    args[args_count++] = parse_expr(parser);
                } while(peek(parser).type == TOKEN_COMMA && advance(parser).type);
            }

            consume(parser, TOKEN_RPAREN, "Esperado ')' apos os argumentos da chamada de funcao.");

            ASTNode* node = create_node(parser, NODE_FUNC_CALL);
            node->ast.func_call.name = name;
            node->ast.func_call.args_count = args_count;
            node->ast.func_call.args = (ASTNode**)arena_alloc(parser->arena, sizeof(ASTNode*) * args_count);

            for(size_t i = 0; i < args_count; i++)
                node->ast.func_call.args[i] = args[i];
            
            expr = node;
            continue;
        }

        if(op.type == TOKEN_LBRACKET) {
            advance(parser);

            ASTNode* index = parse_expr(parser);

            consume(parser, TOKEN_RBRACKET, "Esperado ']' apos o indice do vetor.");

            ASTNode* node = create_node(parser, NODE_INDEX_EXPR);
            node->ast.binary.op = op;
            node->ast.binary.left = expr;
            node->ast.binary.right = index;
            expr = node;
            continue;
        }
        
        if(op.type == TOKEN_OP_PLUS_PLUS || op.type == TOKEN_OP_MINUS_MINUS) {
            advance(parser);
            ASTNode* node = create_node(parser, NODE_UNARY_POSTFIX_EXPR);
            node->ast.unary.op = op;
            node->ast.unary.operand = expr;
            expr = node;
            continue;
        }

        if(op.type == TOKEN_DOT) {
            advance(parser);
            Token member = consume(parser, TOKEN_IDENTIFIER, "Esperado o nome do membro apos o operador de acesso.");
            ASTNode* node = create_node(parser, NODE_MEMBER_ACCESS);
            node->ast.access_member.op = op;
            node->ast.access_member.left = expr;
            node->ast.access_member.member = member;

            expr = node;
            continue;
        }

        break;
    }

    return expr;
}

static ASTNode* parse_prefix(Parser *parser) {
    Token op = peek(parser);

    if (op.type == TOKEN_OP_PLUS_PLUS || op.type == TOKEN_OP_MINUS_MINUS) {
        advance(parser);
        ASTNode* node = create_node(parser, NODE_UNARY_PREFIX_EXPR);
        node->ast.unary.op = op;
        node->ast.unary.operand = parse_postfix(parser); 
        return node;
    }

    if(op.type == TOKEN_OP_MINUS || op.type == TOKEN_BIT_AND || op.type == TOKEN_OP_STAR) {
        advance(parser);
        ASTNode* node = create_node(parser, NODE_UNARY_PREFIX_EXPR);
        node->ast.unary.op = op;
        node->ast.unary.operand = parse_prefix(parser);
        return node;
    }

    return parse_postfix(parser);
}

static ASTNode* parse_term(Parser *parser) {
    ASTNode* left = parse_prefix(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_OP_STAR || op.type == TOKEN_OP_SLASH || op.type == TOKEN_OP_MOD) {
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_prefix(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_addictive(Parser *parser) {
    ASTNode* left = parse_term(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_OP_PLUS || op.type == TOKEN_OP_MINUS) {
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_term(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_shift(Parser *parser) {
    ASTNode* left = parse_addictive(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_BIT_LSHIFT || op.type == TOKEN_BIT_RSHIFT) {
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_addictive(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_relational(Parser *parser) {
    ASTNode* left = parse_shift(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_OP_LT || op.type == TOKEN_OP_LT_EQ || 
            op.type == TOKEN_OP_GT || op.type == TOKEN_OP_GT_EQ) {
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_shift(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_equality(Parser *parser) {
    ASTNode* left = parse_relational(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_OP_EQ_EQ || op.type == TOKEN_OP_BANG_EQ) {
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_relational(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_bitwise_and(Parser *parser) {
    ASTNode* left = parse_equality(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_BIT_AND) { 
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_equality(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_bitwise_or(Parser *parser) {
    ASTNode* left = parse_bitwise_and(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_BIT_OR) {
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_bitwise_and(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_logical_and(Parser *parser) {
    ASTNode* left = parse_bitwise_or(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_OP_AND) {
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_bitwise_or(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_logical_or(Parser *parser) {
    ASTNode* left = parse_logical_and(parser);
    Token op = peek(parser);
    while(op.type == TOKEN_OP_OR) {
        advance(parser);
        ASTNode* binary = create_node(parser, NODE_BINARY_EXPR);
        binary->ast.binary.op = op;
        binary->ast.binary.left = left;
        binary->ast.binary.right = parse_logical_and(parser);
        left = binary;
        op = peek(parser);
    }
    return left;
}

static ASTNode* parse_assignment(Parser *parser) {
    ASTNode* expr = parse_logical_or(parser);

    Token op = peek(parser);
    if(op.type == TOKEN_OP_ASSIGN || op.type == TOKEN_OP_PLUS_EQ || op.type == TOKEN_OP_MINUS_EQ ||
        op.type == TOKEN_BIT_AND_EQ || op.type == TOKEN_BIT_OR_EQ || op.type == TOKEN_BIT_LSHIFT_EQ || 
        op.type == TOKEN_BIT_RSHIFT_EQ) {
        advance(parser);

        if(expr->type != NODE_IDENTIFIER && expr->type != NODE_INDEX_EXPR &&
            expr->type != NODE_MEMBER_ACCESS && !(expr->type == NODE_UNARY_PREFIX_EXPR 
                && expr->ast.unary.op.type == TOKEN_OP_STAR)) {
            fprintf(stderr, "Erro de Sintaxe: Lado esquerdo da atribuicao precisa ser uma variavel.\n");
            exit(1);
        }

        ASTNode* node = create_node(parser, NODE_ASSIGN_EXPR);
        node->ast.binary.op = op;
        node->ast.binary.left = expr;
        node->ast.binary.right = parse_logical_or(parser);
        return node;
    }

    return expr;
}

ASTNode* parse_expr(Parser *parser) {
    return parse_assignment(parser);
}

static ASTNode* parse_struct(Parser *parser) {
    consume(parser, TOKEN_KEYWORD_STRUCT, "Esperado 'struct'.");
    Token name = consume(parser, TOKEN_IDENTIFIER, "Esperado nome da struct.");

    consume(parser, TOKEN_LBRACE, "Esperado '{' para iniciar o corpo da struct.");

    ASTNode* fields[32];
    size_t fields_count = 0;

    while(peek(parser).type != TOKEN_RBRACE) fields[fields_count++] = parse_var(parser);

    consume(parser, TOKEN_RBRACE, "Esperado '}' para fechar o corpo da struct.");
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos a definicao da struct.");

    ASTNode* node = create_node(parser, NODE_STRUCT_DECL);
    node->ast.struct_decl.name = name;
    node->ast.struct_decl.fields_count = fields_count;

    node->ast.struct_decl.fields = (ASTNode**)arena_alloc(parser->arena, sizeof(ASTNode*) * fields_count);

    for(size_t i = 0; i < fields_count; i++)
        node->ast.struct_decl.fields[i] = fields[i];

    return node;
}

static ASTNode* parse_var(Parser *parser) {
    Token type = advance(parser);

    if(type.type == TOKEN_KEYWORD_STRUCT) {
        Token name = consume(parser, TOKEN_IDENTIFIER, "Esperado o nome da struct apos a palavra-chave 'struct'.");
        type = name;
    }

    size_t pointer_lvl = 0;
    while(peek(parser).type == TOKEN_OP_STAR) {
        advance(parser);
        pointer_lvl++;
    }

    Token identifier = consume(parser, TOKEN_IDENTIFIER, "Esperado o nome da variavel apos o tipo.");

    if(peek(parser).type == TOKEN_LPAREN) {
        advance(parser);

        ASTNode* params[16];
        size_t param_count = 0;

        if(peek(parser).type != TOKEN_RPAREN) {
            do {
                Token param_type = advance(parser);

                size_t param_pointer_lvl = 0;
                while(peek(parser).type == TOKEN_OP_STAR) {
                    advance(parser);
                    param_pointer_lvl++;
                }

                Token param_id = consume(parser, TOKEN_IDENTIFIER, "Esperado nome do parametro.");

                if(peek(parser).type == TOKEN_LBRACKET) {
                    advance(parser);

                    if(peek(parser).type != TOKEN_RBRACKET)
                        parse_expr(parser);
                    consume(parser, TOKEN_RBRACKET, "Esperado ']' no parametro.");
                }

                ASTNode* node = create_node(parser, NODE_VAR_DECL);
                node->ast.var_decl.type = param_type;
                node->ast.var_decl.identifier = param_id;
                node->ast.var_decl.value = NULL;
                node->ast.var_decl.pointer_lvl = param_pointer_lvl;

                params[param_count++] = node;

                if(peek(parser).type == TOKEN_COMMA)
                    advance(parser);
                else
                    break;
            } while(1);
        }

        consume(parser, TOKEN_RPAREN, "Esperado ')' apos os parametros.");

        ASTNode* body = parse_block(parser);
        ASTNode* node = create_node(parser, NODE_FUNC_DECL);
        node->ast.func_decl.type = type;
        node->ast.func_decl.count = param_count;
        node->ast.func_decl.body = body;
        node->ast.func_decl.name = identifier;
        node->ast.func_decl.params = (ASTNode**)arena_alloc(parser->arena, sizeof(ASTNode*) * param_count);

        for(size_t i = 0; i < param_count; i++)
            node->ast.func_decl.params[i] = params[i];
        
        return node;
    }

    ASTNode* array_size = NULL;
    if(peek(parser).type == TOKEN_LBRACKET) {
        advance(parser);
        array_size = parse_expr(parser);
        consume(parser, TOKEN_RBRACKET, "Esperado ']' apos o tamanho do vetor na declaracao.");
    }

    ASTNode* node = create_node(parser, NODE_VAR_DECL);
    node->ast.var_decl.type = type;
    node->ast.var_decl.identifier = identifier;
    node->ast.var_decl.value = NULL;
    node->ast.var_decl.pointer_lvl = pointer_lvl;
    node->ast.var_decl.size = array_size;

    Token current = peek(parser);
    if(current.type == TOKEN_OP_ASSIGN) {
        advance(parser);
        node->ast.var_decl.value = parse_expr(parser);
    }

    consume(parser, TOKEN_SEMICOLON, "Esperado ';' no final da declaracao de variavel.");
    return node;
}

static ASTNode* parse_typedef(Parser *parser) {
    consume(parser, TOKEN_KEYWORD_TYPEDEFINITION, "Esperado 'typedefinition'.");

    Token base = advance(parser);
    if(base.type == TOKEN_KEYWORD_STRUCT) {
        base = consume(parser, TOKEN_IDENTIFIER, "Esperado nome da struct apos 'struct' no typedef.");
    }

    size_t pointer_lvl = 0;
    while(peek(parser).type == TOKEN_OP_STAR) {
        advance(parser);
        pointer_lvl++;
    }

    Token type = consume(parser, TOKEN_IDENTIFIER, "Esperado o nome do novo tipo.");
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' no final do typedef.");

    ASTNode* node = create_node(parser, NODE_TYPEDEF);
    node->ast.typedef_decl.type = base;
    node->ast.typedef_decl.name = type;
    node->ast.typedef_decl.pointer_lvl = pointer_lvl;
    return node;
}

static ASTNode* parse_if_statement(Parser *parser) {
    advance(parser);

    consume(parser, TOKEN_LPAREN, "Esperado '(' apos o 'if'.");
    ASTNode* condition = parse_expr(parser);
    consume(parser, TOKEN_RPAREN, "Esperado ')' apos a condicao do 'if'.");

    ASTNode* then = parse_statement(parser);
    ASTNode* otherwise = NULL;

    if(peek(parser).type == TOKEN_KEYWORD_ELSE) {
        advance(parser);
        otherwise = parse_statement(parser);
    }

    ASTNode* node = create_node(parser, NODE_IF_STMT);
    node->ast.if_stmt.otherwise = otherwise;
    node->ast.if_stmt.condition = condition;
    node->ast.if_stmt.then = then;
    return node;
}

static ASTNode* parse_for_statement(Parser *parser) {
    advance(parser);
    consume(parser, TOKEN_LPAREN, "Esperado '(' apos o 'for'.");
    
    ASTNode* init = NULL;
    if(peek(parser).type != TOKEN_SEMICOLON) {
        Token current = peek(parser);
        if(current.type >= TOKEN_KEYWORD_INT && current.type <= TOKEN_KEYWORD_BIG)
            init = parse_var(parser);
        else {
            init = parse_expr(parser);
            consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos a inicializacao do 'for'.");
        }
    } else 
        advance(parser);

    ASTNode* condition = NULL;
    if(peek(parser).type != TOKEN_SEMICOLON) 
        condition = parse_expr(parser);
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos a condicao do 'for'.");

    ASTNode* increment = NULL;
    if(peek(parser).type != TOKEN_RPAREN) 
        increment = parse_expr(parser);
    consume(parser, TOKEN_RPAREN, "Esperado ')' apos as instrucoes do 'for'.");

    ASTNode* body = parse_statement(parser);
    ASTNode* node = create_node(parser, NODE_FOR_STMT);
    node->ast.for_stmt.body = body;
    node->ast.for_stmt.condition = condition;
    node->ast.for_stmt.increment = increment;
    node->ast.for_stmt.var = init;
    return node;
}

static ASTNode* parse_while_statement(Parser *parser) {
    advance(parser);

    consume(parser, TOKEN_LPAREN, "Esperado '(' apos o 'while'.");
    ASTNode* condition = parse_expr(parser);
    consume(parser, TOKEN_RPAREN, "Esperado ')' apos a condicao do 'while'.");

    ASTNode* body = parse_statement(parser);
    ASTNode* node = create_node(parser, NODE_WHILE_STMT);
    node->ast.loop_stmt.condition = condition;
    node->ast.loop_stmt.body = body;
    return node;
}

static ASTNode* parse_do_while_statement(Parser *parser) {
    advance(parser);

    ASTNode* body = parse_statement(parser);

    consume(parser, TOKEN_KEYWORD_WHILE, "Esperado 'while' apos o escopo do 'do'.");
    consume(parser, TOKEN_LPAREN, "Esperado '(' apos o 'while'.");
    ASTNode* condition = parse_expr(parser);
    consume(parser, TOKEN_RPAREN, "Esperado ')' apos a condicao do 'while'.");
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' no final da expressao 'do-while'.");

    ASTNode* node = create_node(parser, NODE_DO_WHILE_STMT);
    node->ast.loop_stmt.body = body;
    node->ast.loop_stmt.condition = condition;
    return node;
}

static ASTNode* parse_block(Parser *parser) {
    consume(parser, TOKEN_LBRACE, "Esperado '{' para iniciar o bloco.");

    ASTNode* temp_statements[1024];
    size_t count = 0;

    while(peek(parser).type != TOKEN_RBRACE && parser->cursor < parser->tokens.count) {
        if(count >= 1024) {
            fprintf(stderr, "[Erro Crítico] Estouro de capacidade temporária de bloco.\n");
            exit(1);
        }
        temp_statements[count++] = parse_statement(parser);
    }

    consume(parser, TOKEN_RBRACE, "Esperado '}' para fechar o bloco.");

    ASTNode* node = create_node(parser, NODE_BLOCK);
    node->ast.block.count = count;
    node->ast.block.statements = (ASTNode**)arena_alloc(parser->arena, sizeof(ASTNode*) * count);

    for(size_t i = 0; i < count; i++)
        node->ast.block.statements[i] = temp_statements[i];

    return node;
}

static ASTNode* parse_statement(Parser *parser) {
    Token current = peek(parser);

    printf("[DEBUG PARSER] O token atual e do tipo numerico: %d | Texto: '%.*s'\n", 
        current.type, (int)current.view.len, current.view.data);
    
    if(current.type >= TOKEN_KEYWORD_INT && current.type <= TOKEN_KEYWORD_BIG)
        return parse_var(parser);
        
    if(current.type == TOKEN_LBRACE)
        return parse_block(parser);

    if(current.type == TOKEN_KEYWORD_RETURN) {
        advance(parser);
        ASTNode* node = create_node(parser, NODE_RETURN_STMT);
        node->ast.return_stmt.expr = NULL;

        if(peek(parser).type != TOKEN_SEMICOLON)
            node->ast.return_stmt.expr = parse_expr(parser);
        
        consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos o return.");
        return node;
    }

    if(current.type == TOKEN_KEYWORD_CONTINUE) {
        advance(parser);
        consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos o continue.");
        return create_node(parser, NODE_CONTINUE_STMT);
    }

    if(current.type == TOKEN_KEYWORD_BREAK) {
        advance(parser);
        consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos o break.");
        return create_node(parser, NODE_BREAK_STMT);
    }

    if(current.type == TOKEN_KEYWORD_STRUCT) {
        if(seeforeward(parser, 1).type == TOKEN_IDENTIFIER && seeforeward(parser, 2).type == TOKEN_LBRACE)
            return parse_struct(parser);
        return parse_var(parser);
    }

    if(current.type == TOKEN_KEYWORD_IF)         return parse_if_statement(parser);
    if(current.type == TOKEN_KEYWORD_WHILE)      return parse_while_statement(parser);
    if(current.type == TOKEN_KEYWORD_DO)         return parse_do_while_statement(parser);
    if(current.type == TOKEN_KEYWORD_FOR)        return parse_for_statement(parser);
    if(current.type == TOKEN_KEYWORD_TYPEDEFINITION) return parse_typedef(parser);
    
    ASTNode* expr = parse_expr(parser);
    consume(parser, TOKEN_SEMICOLON, "Esperado ';' apos a expressao.");
    return expr;
}

ASTNode* parse_program(Parser *parser) {
    ASTNode* node = create_node(parser, NODE_PROGRAM);

    ASTNode* statements[1024];
    size_t count = 0;

    while(peek(parser).type != TOKEN_EOF)
        statements[count++] = parse_statement(parser);

    node->ast.program.count = count;
    node->ast.program.statements = (ASTNode**)arena_alloc(parser->arena, sizeof(ASTNode*) * count);

    for(size_t i = 0; i < count; i++)
        node->ast.program.statements[i] = statements[i];
    
    return node;
}
