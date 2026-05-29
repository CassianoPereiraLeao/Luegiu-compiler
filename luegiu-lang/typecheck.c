#include "typecheck.h"
#include "parser.h"

typedef struct {
    long long min, max;
} TypesBounds;

static uint32_t hash(const char* name, size_t len) {
    uint32_t h = 2166136261u;
    for(size_t i = 0; i < len; i++) {
        h ^= name[i];
        h *= 16777619u;
    }

    return h % HASH_TABLE_SIZE;
}

static bool is_primitive_numeric(const char* type_name, size_t type_len) {
    if (strncmp(type_name, "int", 3) == 0 && type_len == 3) return true;
    if (strncmp(type_name, "int8", 4) == 0 && type_len == 4) return true;
    if (strncmp(type_name, "int16", 5) == 0 && type_len == 5) return true;
    if (strncmp(type_name, "int32", 5) == 0 && type_len == 5) return true;
    if (strncmp(type_name, "int64", 5) == 0 && type_len == 5) return true;
    if (strncmp(type_name, "uint8", 5) == 0 && type_len == 5) return true;
    if (strncmp(type_name, "uint16", 6) == 0 && type_len == 6) return true;
    if (strncmp(type_name, "uint32", 6) == 0 && type_len == 6) return true;
    if (strncmp(type_name, "uint64", 6) == 0 && type_len == 6) return true;
    if (strncmp(type_name, "small", 5) == 0 && type_len == 5) return true;
    if (strncmp(type_name, "big", 3) == 0 && type_len == 3) return true;
    if (strncmp(type_name, "size_t", 6) == 0 && type_len == 6) return true;
    if (strncmp(type_name, "char", 4) == 0 && type_len == 4) return true;
    if (strncmp(type_name, "utf8", 4) == 0 && type_len == 4) return true;
    if (strncmp(type_name, "utf16", 5) == 0 && type_len == 5) return true;

    return false;
}

static bool is_unsigned_integer(const char* type_name, size_t type_len) {
    if (strncmp(type_name, "uint8", 5) == 0 && type_len == 5) return true;
    if (strncmp(type_name, "uint16", 6) == 0 && type_len == 6) return true;
    if (strncmp(type_name, "uint32", 6) == 0 && type_len == 6) return true;
    if (strncmp(type_name, "uint64", 6) == 0 && type_len == 6) return true;
    if (strncmp(type_name, "size_t", 6) == 0 && type_len == 6) return true;
    return false;
}

static bool is_character_type(const char* type_name, size_t type_len) {
    if (strncmp(type_name, "char", 4) == 0 && type_len == 4) return true;
    if (strncmp(type_name, "utf8", 4) == 0 && type_len == 4) return true;
    if (strncmp(type_name, "utf16", 5) == 0 && type_len == 5) return true;
    return false;
}

static bool types_are_equal(Symbol* sym, const char* right_type) {
    if (sym->type_len != strlen(right_type)) {
        return false;
    }
    return strncmp(sym->type_name, right_type, sym->type_len) == 0;
}

SymbolTable* create_table(SymbolTable *enclosing, Arena *arena) {
    SymbolTable* table = (SymbolTable*)arena_alloc(arena, sizeof(SymbolTable));
    memset(table->buckets, 0, sizeof(table->buckets));
    table->enclosing = enclosing;
    return table;
}

bool insert_symbol(SymbolTable *table, Symbol *sym) {
    uint32_t slot = hash(sym->name, sym->len);

    Symbol* current = table->buckets[slot];
    while(current) {
        if(current->len = sym->len && strncmp(current->name, sym->name, sym->len) == 0) return false;
        current->next;
    }

    sym->next = table->buckets[slot];
    table->buckets[slot] = sym;
    return true;
}

Symbol* lookup_symbol(SymbolTable *table, const char* name, size_t len) {
    SymbolTable* current = table;

    while(current != NULL) {
        uint32_t slot = hash(name, len);
        Symbol* sym = current->buckets[slot];

        while(sym) {
            if(sym->len == len && strncmp(sym->name, name, len) == 0) return sym;
            sym = sym->next;
        }

        current = current->enclosing;
    }

    return NULL;
}

const char* type_of_expr(ASTNode *node, SymbolTable *table) {
    if(node == NULL) return "void";

    switch (node->type)
    {
    case NODE_NUMERIC_LITERAL:
        Token token = node->ast.token;

        for(size_t i = 0; i < token.view.len; i++) {
            if(token.view.data[i] == '.') {
                if(token.view.data[token.view.len - 1] == 'f') return "float";
                return "double";
            }
        }
        return "int";
    case NODE_STRING_LITERAL:
        if(node->ast.token.type == TOKEN_KEYWORD_UTF16CHAR) return "utf16char*";
        if(node->ast.token.type == TOKEN_KEYWORD_UTF8CHAR) return "utf8char*";
        return "char*";
    case NODE_CHAR_LITERAL:
        if(node->ast.token.type == TOKEN_KEYWORD_UTF16CHAR) return "utf16char";
        if(node->ast.token.type == TOKEN_KEYWORD_UTF8CHAR) return "utf8char";
        return "char";
    case NODE_IDENTIFIER: {
        Symbol* sym = lookup_symbol(table, node->ast.token.view.data, node->ast.token.view.len);
        if(sym == NULL) {
            fprintf(stderr, "Erro Semantico: Variavel '%.*s' nao declarada.\n",
                    (int)node->ast.token.view.len, node->ast.token.view.data);
            exit(1);
        }

        if (strncmp(sym->type_name, "uint8", 5) == 0 && sym->type_len == 5)   return "uint8";
        if (strncmp(sym->type_name, "uint16", 6) == 0 && sym->type_len == 6)  return "uint16";
        if (strncmp(sym->type_name, "uint32", 6) == 0 && sym->type_len == 6)  return "uint32";
        if (strncmp(sym->type_name, "uint64", 6) == 0 && sym->type_len == 6)  return "uint64";
        if (strncmp(sym->type_name, "size_t", 6) == 0 && sym->type_len == 6)  return "size_t";
        if (strncmp(sym->type_name, "int8", 4) == 0 && sym->type_len == 4)    return "int8";
        if (strncmp(sym->type_name, "int16", 5) == 0 && sym->type_len == 5)   return "int16";
        if (strncmp(sym->type_name, "int32", 5) == 0 && sym->type_len == 5)   return "int32";
        if (strncmp(sym->type_name, "int64", 5) == 0 && sym->type_len == 5)   return "int64";
        if (strncmp(sym->type_name, "int", 3) == 0 && sym->type_len == 3)     return "int";
        if (strncmp(sym->type_name, "small", 5) == 0 && sym->type_len == 5)   return "small";
        if (strncmp(sym->type_name, "big", 3) == 0 && sym->type_len == 3)     return "big";
        if (strncmp(sym->type_name, "char", 4) == 0 && sym->type_len == 4)    return "char";
        if (strncmp(sym->type_name, "utf8", 4) == 0 && sym->type_len == 4)    return "utf8";
        if (strncmp(sym->type_name, "utf16", 5) == 0 && sym->type_len == 5)   return "utf16";

        return sym->type_name;
    }
    case NODE_BINARY_EXPR: {
        const char* left = type_of_expr(node->ast.binary.left, table);
        const char* right = type_of_expr(node->ast.binary.right, table);
        size_t left_len = strlen(left);
        size_t right_len = strlen(right);

        if (strcmp(left, right) == 0) {
            return left;
        }

        if(is_character_type(left, left_len) || is_character_type(right, right_len)) {
            bool left_valid = is_character_type(left, left_len) || is_unsigned_integer(left, left_len);
            bool right_valid = is_character_type(right, right_len) || is_unsigned_integer(right, right_len);

            if(left_valid && right_valid)
                return is_character_type(left, left_len) ? left : right;
            
            fprintf(stderr, "Erro de Tipo: Operacao invalida. Caracteres so aceitam interacoes com tipos nao-assinados (uint).\n");
            exit(1);
        }

        Token op_tok = node->ast.binary.op;
        if (strncmp(op_tok.view.data, "<", 1) == 0  || 
            strncmp(op_tok.view.data, ">", 1) == 0  || 
            strncmp(op_tok.view.data, "==", 2) == 0 || 
            strncmp(op_tok.view.data, "!=", 2) == 0 ||
            strncmp(op_tok.view.data, "<=", 2) == 0 ||
            strncmp(op_tok.view.data, ">=", 2)) {
            
            bool left_num = is_unsigned_integer(left, left_len) || 
                                (strncmp(left, "int", 3) == 0) || 
                                (strncmp(left, "small", 5) == 0) || 
                                (strncmp(left, "big", 3) == 0);
            
            bool right_num = is_unsigned_integer(left, left_len) || 
                                (strncmp(left, "int", 3) == 0) || 
                                (strncmp(left, "small", 5) == 0) || 
                                (strncmp(left, "big", 3) == 0);
            
            if(left_num && right_num) return "bool";
        }

        fprintf(stderr, "Erro de Tipo: Operacao binaria invalida entre '%s' e '%s'.\n", left, right);
        exit(1);
    }
    case NODE_FUNC_CALL: {
        Token token = node->ast.func_call.name;
        if(strncmp(node->ast.func_call.name.view.data, "fail", 4) == 0 ||
            strncmp(node->ast.func_call.name.view.data, "exit", 4) == 0) {
            if(node->ast.func_call.args_count == 1) {
                const char* arg_type = type_of_expr(node, table);
                if(is_unsigned_integer(arg_type, strlen(arg_type)) || is_primitive_numeric(arg_type, strlen(arg_type))) {
                    fprintf(stderr, "Erro Semantico: Os comandos fail() e end() exigem um codigo inteiro como argumento.\n");
                    exit(1);
                }
            } else {
                fprintf(stderr, "Erro Semantico: fail() e end() esperam exatamente 1 argumento.\n");
                exit(1);
            }
            return "link";
        }

        Symbol* sym = lookup_symbol(table, node->ast.func_call.name.view.data, node->ast.func_call.name.view.len);

        if (sym == NULL) {
            fprintf(stderr, "Erro Semantico: Funcao '%.*s' nao foi declarada.\n",
                    (int)token.view.len, token.view.data);
            exit(1);
        }

        if(sym->type != SYMBOL_FUNC) {
            fprintf(stderr, "Erro Semantico: '%.*s' nao é uma funcao.\n",
                (int)token.view.len, token.view.data);
            exit(1);
        }

        if(node->ast.func_call.args_count != sym->param_count) {
            fprintf(stderr, "Erro de Tipo: Funcao '%.*s' esperava %zu argumentos, mas recebeu %zu.\n",
                (int)sym->len, sym->name, sym->param_count, node->ast.func_call.args_count);
            exit(1);
        }

        return sym->type_name;
    }
    case NODE_HEXA:
        return "hexa";
        
    default:
        return "void";
    }
}

static bool check_bounds(Symbol *sym, long long value) {
    TypesBounds bounds;
    bounds.min = 0;
    bounds.max = 0;

    if(strncmp(sym->type_name, "uint8", 5) == 0 && sym->type_len == 5)
        bounds.max = (1ULL << 8) - 1;
    else if(strncmp(sym->type_name, "uint16", 6) == 0 && sym->type_len == 6)
        bounds.max = (1ULL << 16) - 1;
    else if(strncmp(sym->type_name, "uint32", 6) == 0 && sym->type_len == 6)
        bounds.max = (1ULL << 32) - 1;
    else if(strncmp(sym->type_name, "uint64", 6) == 0 && sym->type == 6)
        bounds.max = 0x7FFFFFFFFFFFFFFFLL;
    else if(strncmp(sym->type_name, "char", 4) == 0 && sym->type_len == 4) 
        bounds.max = (1ULL << 8) - 1;
    else if(strncmp(sym->type_name, "utf8char", 8) == 0 && sym->type_len == 8) 
        bounds.max = (1ULL << 16) - 1;
    else if(strncmp(sym->type_name, "utf16char", 9) == 0 && sym->type_len == 9) 
        bounds.max = (1ULL << 16) - 1;
    else if(strncmp(sym->type_name, "int", 3) == 0 && sym->type_len == 3) {
        bounds.max = (1LL << (16 - 1)) - 1; 
        bounds.min = -(1LL << (16 - 1));
    } else if(strncmp(sym->type_name, "int8", 4) == 0 && sym->type_len == 4) {
        bounds.max = (1LL << (8 - 1)) - 1;
        bounds.min = -(1LL << (8 - 1));
    } else if(strncmp(sym->type_name, "int16", 5) == 0 && sym->type_len == 5) {
        bounds.max = (1LL << (16 - 1)) - 1;
        bounds.min = -(1LL << (16 - 1));
    } else if(strncmp(sym->type_name, "int32", 5) == 0 && sym->type_len == 5) {
        bounds.max = (1LL << (32 - 1)) - 1;
        bounds.max = -(1LL << (32 - 1));
    } else if(strncmp(sym->type_name, "int64", 5) == 0 && sym->type_len == 5) {
        bounds.max = 0x7FFFFFFFFFFFFFFFLL;
        bounds.min = -9223372036854775807LL - 1LL;
    } else if(strncmp(sym->type_name, "size_t", 6) == 0 && sym->type_len == 6) {
        bounds.max = 0x7FFFFFFFFFFFFFFFLL;
        bounds.min = -9223372036854775807LL - 1LL;
    } else if(strncmp(sym->type_name, "big", 3) == 0 && sym->type_len == 3) {
        bounds.max = 0x7FFFFFFFFFFFFFFFLL;
        bounds.min = -9223372036854775807LL - 1LL;
    } else if(strncmp(sym->type_name, "small", 5) == 0 && sym->type_len == 5) {
        bounds.max = (1LL << (8 - 1)) - 1;
        bounds.min = -(1LL << (8 - 1));
    }

    if(value < bounds.min || value > bounds.max) {
        fprintf(stderr, "Erro de Compilacao: O valor %lld estoura o limite do tipo '%.*s' (%lld a %lld).\n", value, 
            (int)sym->type_len, sym->type_name, bounds.min, bounds.max);
        exit(1);
    }

    return true;
}

static void validate_assign_or_init(Symbol *sym, ASTNode *node, SymbolTable *table) {
    const char* right = type_of_expr(node, table);
    size_t right_len = strlen(right);

    if(node->type == NODE_NUMERIC_LITERAL) {
        long long value = strtoll(node->ast.token.view.data, NULL, 10);
        check_bounds(sym, value);
        return;
    }

    if(is_character_type(sym->type_name, sym->type_len)|| is_character_type(right, right_len)) {
        bool dest_is_valid = is_character_type(sym->type_name, sym->type_len) || 
            is_unsigned_integer(sym->type_name, sym->type_len);
        bool right_is_valid = is_character_type(right, right_len) || is_unsigned_integer(right, right_len);

        if (!dest_is_valid || !right_is_valid) {
        fprintf(stderr, "Erro de Tipo: Atribuicao invalida. Tipos de caracteres ('%.*s' <- '%.*s') so interagem com outros caracteres ou tipos nao-assinados (uint).\n",
                (int)sym->type_len, sym->type_name, 
                (int)right_len, right);
        exit(1);
    }
        return;
    }

    if (strncmp(sym->type_name, right, sym->type_len) != 0 || sym->type_len != right_len) {
        fprintf(stderr, "Erro de Tipo: Nao e possivel atribuir o tipo '%s' para a variavel '%.*s' do tipo '%.*s'.\n",
                right, (int)sym->len, sym->name, (int)sym->type_len, sym->type_name);
        exit(1);
    }
}

void semantic_analysis(ASTNode *node, SymbolTable *table, Arena *arena, const char* return_type) {
    if(node == NULL) return;

    switch(node->type) {
    case NODE_PROGRAM:
        for(size_t i = 0; i < node->ast.program.count; i++)
            semantic_analysis(node->ast.program.statements[i], table, arena, return_type);
        break;
    case NODE_VAR_DECL: {
        Symbol* sym = (Symbol*)arena_alloc(arena, sizeof(Symbol));
        sym->name = node->ast.var_decl.identifier.view.data;
        sym->len = node->ast.var_decl.identifier.view.len;
        sym->type = SYMBOL_VAR;
        sym->type_name = node->ast.var_decl.type.view.data;
        sym->type_len = node->ast.var_decl.type.view.len;
        sym->pointer_lvl = node->ast.var_decl.pointer_lvl;
        sym->next = NULL;

        if(!insert_symbol(table, sym)) {
            fprintf(stderr, "Erro Semantico: Variavel '%.*s' ja declarada neste escopo.\n",
                        (int)sym->len, sym->name);
            exit(1);
        }

        if(node->ast.var_decl.value)
            validate_assign_or_init(sym, node->ast.var_decl.value, table);

        break;
    }
    case NODE_FUNC_DECL: {
        Symbol* sym = (Symbol*)arena_alloc(arena, sizeof(Symbol));
        sym->name = node->ast.func_decl.name.view.data;
        sym->len = node->ast.func_decl.name.view.len;
        sym->type = SYMBOL_FUNC;
        sym->type_name = node->ast.func_decl.type.view.data;
        sym->type_len = node->ast.func_decl.type.view.len;
        sym->param_count = node->ast.func_decl.count;
        sym->next = NULL;

        if(!insert_symbol(table, sym)) {
            fprintf(stderr, "Erro Semantico: Funcao '%.*s' ja definida.\n",
                (int)sym->len, sym->name);
            exit(1);
        }

        SymbolTable* scope = create_table(table, arena);

        char* ret_type_str = (char*)arena_alloc(arena, sym->type_len + 1);
        memcpy(ret_type_str, sym->type_name, sym->type_len);
        ret_type_str[sym->type_len] = '\0';
        
        semantic_analysis(node->ast.func_decl.body, scope, arena, ret_type_str);
        break;
    }
    case NODE_FUNC_CALL: {
        type_of_expr(node, table); 
        break;
    }
    case NODE_BLOCK: {
        SymbolTable* scope = create_table(table, arena);
        node->ast.block.scope = scope;
        for(size_t i = 0; i < node->ast.block.count; i++)
            semantic_analysis(node->ast.block.statements[i], scope, arena, return_type);
        break;
    }
    case NODE_ASSIGN_EXPR: {
        if(node->ast.binary.left->type == NODE_IDENTIFIER) {
            Token id = node->ast.binary.left->ast.token;
            Symbol* sym = lookup_symbol(table, id.view.data, id.view.len);

            if(sym != NULL)
                validate_assign_or_init(sym, node->ast.binary.right, table);
        }
        break;
    }
    case NODE_BINARY_EXPR: {
        semantic_analysis(node->ast.binary.left, table, arena, return_type);
        semantic_analysis(node->ast.binary.right, table, arena, return_type);
        break;
    }
    case NODE_IDENTIFIER: {
        Symbol* sym = lookup_symbol(table, node->ast.token.view.data, node->ast.token.view.len);
        if(sym == NULL) {
            fprintf(stderr, "Erro Semantico: Variavel '%.*s' usada mas nao foi declarada.\n",
                (int)node->ast.token.view.len, node->ast.token.view.data);
            exit(1);
        }

        if(sym->type == SYMBOL_FUNC) {
            fprintf(stderr, "Erro Semantico: Identificador '%.*s' é uma funcao e nao pode ser usado como variavel.\n",
                (int)node->ast.token.view.len, node->ast.token.view.data);
            exit(1);
        }

        if(sym->type == SYMBOL_TYPEDEF) {
            fprintf(stderr, "Erro Semantico: Identificador '%.*s' é um tipo (typedefinition) e nao uma variavel.\n",
                (int)node->ast.token.view.len, node->ast.token.view.data);
            exit(1);
        }

        break;
    }
    case NODE_FOR_STMT: {
        SymbolTable* scope = create_table(table, arena);
        node->ast.for_stmt.scope = scope;

        if(node->ast.for_stmt.var)
            semantic_analysis(node->ast.for_stmt.var, scope, arena, return_type);

        if(node->ast.for_stmt.condition) {
            const char* condition = type_of_expr(node->ast.for_stmt.condition, scope);
            if(strcmp(condition, "void") == 0 || strstr(condition, "*") != NULL) {
                fprintf(stderr, "Erro de Tipo: A condicao do 'for' precisa resultar em um tipo logico ou numerico. Recebeu: '%s'.\n", condition);
                exit(1);
            }
        }

        if(node->ast.for_stmt.increment)
            semantic_analysis(node->ast.for_stmt.increment, scope, arena, return_type);
        
        semantic_analysis(node->ast.for_stmt.body, scope, arena, return_type);
        break;
    }
    case NODE_DO_WHILE_STMT: {
        semantic_analysis(node->ast.loop_stmt.body, table, arena, return_type);

        const char* type = type_of_expr(node->ast.loop_stmt.condition, table);
        if(strcmp(type, "void") == 0 || strstr(type, "*") != NULL) {
            fprintf(stderr, "Erro de Tipo: A condicao do 'do-while' precisa resultar em um tipo logico ou numerico. Recebeu: '%s'.\n", type);
            exit(1);
        }
        break;
    }
    case NODE_WHILE_STMT: {
        const char* type = type_of_expr(node->ast.loop_stmt.condition, table);
        
        if(strcmp(type, "void") == 0 || strstr(type, "*") != NULL) {
            fprintf(stderr, "Erro de Tipo: A condicao do 'while' precisa resultar em um tipo logico ou numerico. Recebeu: '%s'.\n", type);
            exit(1);
        }

        semantic_analysis(node->ast.loop_stmt.body, table, arena, return_type);
        break;
    }
    case NODE_IF_STMT: {
        const char* type = type_of_expr(node->ast.if_stmt.condition, table);
        size_t cond_len = strlen(type);

        if(strcmp(type, "void") == 0 || strstr(type, "*") != NULL) {
            fprintf(stderr, "Erro de Tipo: A condicao do 'if' precisa resultar em um tipo logico ou numerico. Recebeu: '%s'.\n", type);
            exit(1);
        }

        semantic_analysis(node->ast.if_stmt.then, table, arena, return_type);
        if(node->ast.if_stmt.otherwise) 
            semantic_analysis(node->ast.if_stmt.otherwise, table, arena, return_type);
        break;
    }

    case NODE_RETURN_STMT: {
        if(return_type == NULL || strcmp(return_type, "void") == 0) {
            if(node->ast.return_stmt.expr != NULL) {
                fprintf(stderr, "Erro de Tipo: Funcao do tipo 'void' nao deveria retornar um valor.\n");
                exit(1);
            }
            break;
        }

        if(node->ast.return_stmt.expr) {
            const char* expr = type_of_expr(node->ast.return_stmt.expr, table);
            if(strcmp(expr, return_type) != 0) {
                fprintf(stderr, "Erro de Tipo: Tipo de retorno invalido. A funcao esperava '%s', mas recebeu '%s'.\n",
                        return_type, expr);
                exit(1);
            }

            if(strcmp(expr, "link") == 0) {
                if(strcmp(expr, "hexa") != 0) {
                    fprintf(stderr, "Erro de Tipo Estrito: A funcao exige o tipo 'link', portanto o retorno deve ser um valor hexadecimal (ex: 0x010). Recebeu: '%s'.\n", 
                            expr);
                    exit(1);
                }

                if (node->ast.return_stmt.expr->ast.hexa.value > 0x0FF) {
                    fprintf(stderr, "Erro Semantico: O valor 0x%llX excede o limite do tipo 'link' (max: 0x0FF).\n",
                            node->ast.return_stmt.expr->ast.hexa.value);
                    exit(1);
                }
            }
        } else {
            fprintf(stderr, "Erro de Tipo: A funcao exige um retorno do tipo '%s', mas o comando 'return' esta vazio.\n", 
                    return_type);
            exit(1);
        }

        break;
    }

    case NODE_NUMERIC_LITERAL:
    case NODE_STRING_LITERAL:
    case NODE_CHAR_LITERAL:
        break;

    default:
        break;
    }
}
