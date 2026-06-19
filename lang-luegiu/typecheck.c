#include "typecheck.h"
#include "parser.h"

static void format_type(char *buf, size_t bufsize, TokenType type, size_t ptr_lvl) {
    const char *base = token_to_str(type);
    int written = snprintf(buf, bufsize, "%s", base);
    for (size_t i = 0; i < ptr_lvl && written < (int)bufsize - 2; i++)
        buf[written++] = '*';
    buf[written] = '\0';
}

const char* token_to_str(TokenType type) {
    switch(type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_UNDEFINED: return "undef";
        case TOKEN_IDENTIFIER: return "ident";
        case TOKEN_DOUBLE_LIT: return "double";
        case TOKEN_INT_LIT: return "int";
        case TOKEN_STRING_LITERAL: return "string";
        case TOKEN_CHAR_LITERAL: return "char";
        case TOKEN_HEXA: return "hexa";
        case TOKEN_KEYWORD_INT: return "int";
        case TOKEN_KEYWORD_FLOAT: return "float";
        case TOKEN_KEYWORD_UINT8: return "uint8";
        case TOKEN_KEYWORD_UINT16: return "uint16";
        case TOKEN_KEYWORD_UINT32: return "uint32";
        case TOKEN_KEYWORD_UINT64: return "uint64";
        case TOKEN_KEYWORD_INT16: return "int16";
        case TOKEN_KEYWORD_INT8: return "int8";
        case TOKEN_KEYWORD_INT32: return "int32";
        case TOKEN_KEYWORD_INT64: return "int64";
        case TOKEN_KEYWORD_DOUBLE: return "double";
        case TOKEN_KEYWORD_VOID: return "void";
        case TOKEN_KEYWORD_CHAR: return "char type";
        case TOKEN_KEYWORD_UTF8CHAR: return "utf8";
        case TOKEN_KEYWORD_UTF16CHAR: return "utf16";
        case TOKEN_KEYWORD_SMALL: return "small";
        case TOKEN_KEYWORD_LINK: return "link";
        case TOKEN_KEYWORD_BIG: return "big";
        case TOKEN_KEYWORD_RETURN: return "return";
        case TOKEN_KEYWORD_STATIC: return "static";
        case TOKEN_KEYWORD_FOR: return "for";
        case TOKEN_KEYWORD_WHILE: return "while";
        case TOKEN_KEYWORD_DO: return "do";
        case TOKEN_KEYWORD_IF: return "if";
        case TOKEN_KEYWORD_ELSE: return "else";
        case TOKEN_KEYWORD_BREAK: return "break";
        case TOKEN_KEYWORD_CONTINUE: return "continue";
        case TOKEN_KEYWORD_BOOLEAN: return "bool";
        case TOKEN_KEYWORD_TRUE: return "true";
        case TOKEN_KEYWORD_FALSE: return "false";
        case TOKEN_KEYWORD_STRUCT: return "struct";
        case TOKEN_KEYWORD_UNION: return "union";
        case TOKEN_KEYWORD_SWITCH: return "switch";
        case TOKEN_KEYWORD_CASE: return "case";
        case TOKEN_KEYWORD_ENUM: return "enum";
        case TOKEN_KEYWORD_TYPEDEFINITION: return "typedefinition";
        case TOKEN_OP_PLUS: return "+";
        case TOKEN_OP_PLUS_PLUS: return "++";
        case TOKEN_OP_PLUS_EQ: return "+=";
        case TOKEN_OP_ASSIGN: return "=";
        case TOKEN_OP_EQ_EQ: return "==";
        case TOKEN_OP_MINUS: return "-";
        case TOKEN_OP_MINUS_MINUS: return "--";
        case TOKEN_OP_MINUS_EQ: return "-=";
        case TOKEN_OP_SLASH: return "/";
        case TOKEN_OP_SLASH_EQ: return "/=";
        case TOKEN_OP_STAR: return "*";
        case TOKEN_OP_STAR_EQ: return "*=";
        case TOKEN_OP_TERN: return "?";
        case TOKEN_OP_MOD: return "%";
        case TOKEN_OP_MOD_EQ: return "%=";
        case TOKEN_OP_BANG: return "!";
        case TOKEN_OP_BANG_EQ: return "!=";
        case TOKEN_OP_LT: return "<";
        case TOKEN_OP_GT: return ">";
        case TOKEN_OP_GT_EQ: return ">=";
        case TOKEN_OP_LT_EQ: return "<=";
        case TOKEN_OP_OR: return "||";
        case TOKEN_OP_AND: return "&&";
        case TOKEN_OP_XOR: return "^";
        case TOKEN_OP_DESC: return "~";
        case TOKEN_OP_INF_ARGS: return "...";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "]";
        case TOKEN_RBRACKET: return "[";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_DOT: return ".";
        case TOKEN_COMMA: return ",";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_COLON: return ":";
        case TOKEN_BIT_LSHIFT: return "<<";
        case TOKEN_BIT_LSHIFT_EQ: return "<<=";
        case TOKEN_BIT_RSHIFT: return ">>";
        case TOKEN_BIT_RSHIFT_EQ: return ">>=";
        case TOKEN_BIT_AND: return "&";
        case TOKEN_BIT_AND_EQ: return "&=";
        case TOKEN_BIT_OR: return "|";
        case TOKEN_BIT_OR_EQ: return "|=";
        default: return "Non Key";
    }
}

static bool view_equals(View a, View b) {
    if (a.len != b.len) return false;
    return memcmp(a.start, b.start, a.len) == 0;
}

static bool is_lvalue(Node *node) {
    if(!node) return false;
    if(node->type == NODE_VAR_ACCESS) return true;
    if(node->type == NODE_UNARY_OP && node->ast.unary_op.op == TOKEN_OP_STAR) return true;
    if(node->type == NODE_BINARY_OP &&
        (node->ast.binary_op.op == TOKEN_DOT || node->ast.binary_op.op == TOKEN_OP_ARROW))
        return true;
    if(node->type == NODE_ARRAY) return true;
    return false;
}
void push_scope(SymbolTable *table) {
    Scope* scope = (Scope*)arena_alloc(table->arena, sizeof(Scope));
    scope->symbols = NULL;
    scope->parent = table->current_scope;
    table->current_scope = scope;
}

void pop_scope(SymbolTable *table) {
    if(table->current_scope->parent)
        table->current_scope = table->current_scope->parent;
}

Symbol* insert_table(SymbolTable *table, View name, SymbolCategory category, size_t pointer_lvl, TokenType type) {
    Symbol* sym = (Symbol*)arena_alloc(table->arena, sizeof(Symbol));
    sym->name = name;
    sym->category = category;
    sym->pointer_lvl = pointer_lvl;
    sym->type = type;
    sym->params = NULL;
    sym->params_count = 0;
    sym->next = table->current_scope->symbols;
    table->current_scope->symbols = sym;
    return sym;
}

Symbol* lookup_table(SymbolTable *table, View name) {
    Scope* current = table->current_scope;

    while(current != NULL) {
        Symbol* sym = current->symbols;
        while(sym != NULL) {
            if(view_equals(sym->name, name)) return sym;
            sym = sym->next;
        }
        current = current->parent;
    }

    return NULL;
}

static Symbol* lookup_current_table(SymbolTable *table, View name) {
    Symbol* sym = table->current_scope->symbols;
    while(sym) {
        if(view_equals(sym->name, name)) return sym;
        sym = sym->next;
    }

    return NULL;
}

static void typecheck_error(TypeChecker *typecheck, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Erro semantico: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    typecheck->error_count++;
}

static void typecheck_error_at(TypeChecker *typecheck, Node *node, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buf[512];

    if(node && node->type == NODE_VAR_ACCESS) {
        size_t len = node->ast.var_access.name.len;
        const char* start = node->ast.var_access.name.start;
        if(len > 256 || start == NULL) {
            fprintf(stderr, "NODE CORROMPIDO: type=%d len=%zu start=%p line=%d col=%d file=%s\n",
                node->type, len, (void*)start, node->line, node->col,
                node->file ? node->file : "<null>");
            va_end(args);
            typecheck->error_count++;
            return;
        }
    }

    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    const char* file = node ? node->file : NULL;
    size_t line = node ? (size_t)node->line : 0;
    size_t col  = node ? (size_t)node->col  : 0;

    diag_error(typecheck->diag, file, line, col, "%s", buf);
    typecheck->error_count++;
}

static bool is_numeric_type(TokenType token) {
    switch (token)
    {
    case TOKEN_KEYWORD_INT:
    case TOKEN_KEYWORD_INT8:
    case TOKEN_KEYWORD_INT16:
    case TOKEN_KEYWORD_INT32:
    case TOKEN_KEYWORD_INT64:
    case TOKEN_KEYWORD_DOUBLE:
    case TOKEN_KEYWORD_FLOAT:
    case TOKEN_KEYWORD_UINT8:
    case TOKEN_KEYWORD_UINT16:
    case TOKEN_KEYWORD_UINT32:
    case TOKEN_KEYWORD_UINT64:
    case TOKEN_KEYWORD_SMALL:
    case TOKEN_KEYWORD_BIG:
        return true;
    
    default:
        return false;
    }
}

static bool is_integer_type(TokenType token) {
    switch (token)
    {
    case TOKEN_KEYWORD_INT:
    case TOKEN_KEYWORD_INT8:
    case TOKEN_KEYWORD_INT16:
    case TOKEN_KEYWORD_INT32:
    case TOKEN_KEYWORD_INT64:
    case TOKEN_KEYWORD_UINT8:
    case TOKEN_KEYWORD_UINT16:
    case TOKEN_KEYWORD_UINT32:
    case TOKEN_KEYWORD_UINT64:
    case TOKEN_KEYWORD_BIG:
        return true;
    
    default:
        return false;
    }
}

static bool type_compatible(TokenType decl, size_t decl_ptr, TokenType actual, size_t actual_ptr) {
    if(actual == TOKEN_STRING_LITERAL && actual_ptr == 1
        && decl == TOKEN_KEYWORD_CHAR && decl_ptr == 1) {
            return true;
        }

    if(decl_ptr > 0 || actual_ptr > 0) 
        return (decl == actual && decl_ptr == actual_ptr);

    if(decl == actual) return true;

    if(is_numeric_type(decl) && is_numeric_type(actual)) return true;

    if(decl == TOKEN_KEYWORD_CHAR && actual == TOKEN_KEYWORD_CHAR) return true;

    if (actual == TOKEN_DOUBLE_LIT && (decl == TOKEN_KEYWORD_FLOAT || decl == TOKEN_KEYWORD_DOUBLE))
        return true;

    if (decl == TOKEN_DOUBLE_LIT && actual == TOKEN_DOUBLE_LIT)
        return true;

    if (actual == TOKEN_HEXA) {
        if (decl == TOKEN_KEYWORD_LINK) return true;
        if (is_integer_type(decl))            return true;
        return false;
    }

    if(actual == TOKEN_KEYWORD_CHAR ||
        actual == TOKEN_KEYWORD_UTF8CHAR ||
        actual == TOKEN_KEYWORD_UTF16CHAR) {
        
        if(decl == TOKEN_KEYWORD_UTF16CHAR) return true;
        if(decl == TOKEN_KEYWORD_UTF8CHAR)
            return (actual == TOKEN_KEYWORD_UTF8CHAR || actual == TOKEN_KEYWORD_CHAR);
        if(decl == TOKEN_KEYWORD_CHAR)
            return (actual == TOKEN_KEYWORD_CHAR);
    }

    if(actual == TOKEN_CHAR_LITERAL) {
        if(decl_ptr > 0) return false;
        return (decl == TOKEN_KEYWORD_CHAR || decl == TOKEN_KEYWORD_UTF16CHAR || decl == TOKEN_KEYWORD_UTF8CHAR);
    }

    if (decl == TOKEN_KEYWORD_LINK)
        return (actual == TOKEN_KEYWORD_LINK);

    return false;
}

static size_t infer_ptr_lvl(SymbolTable *table, Node *expr) {
    switch (expr->type)
    {
    case NODE_VAR_ACCESS: {
        Symbol *sym = lookup_table(table, expr->ast.var_access.name);
        return sym ? sym->pointer_lvl : 0;
    }
    case NODE_FUNC_CALL: {
        Symbol *sym = lookup_table(table, expr->ast.func_call.name);
        return sym ? sym->pointer_lvl : 0;
    }
    case NODE_UNARY_OP: {
        if(expr->ast.unary_op.op == TOKEN_BIT_AND) 
            return infer_ptr_lvl(table, expr->ast.unary_op.operand) + 1;

        if(expr->ast.unary_op.op == TOKEN_OP_STAR) {
            size_t lvl = infer_ptr_lvl(table, expr->ast.unary_op.operand);
            return lvl > 0 ? lvl - 1 : 0;
        }
        return 0;
    }
    case NODE_CAST:
        return expr->ast.cast_expr.ptr_lvl;
    case NODE_BINARY_OP:
        return 0;
    case NODE_STRING_LIT:
        return 1;
    default: return 0;
    }
}

static TokenType infer_type(TypeChecker *typecheck, SymbolTable *table, Node *expr) {
    if(!expr) return TOKEN_UNDEFINED;

    switch (expr->type)
    {
    case NODE_INT_LIT: return TOKEN_KEYWORD_INT;
    case NODE_DOUBLE_LIT: return TOKEN_DOUBLE_LIT;
    case NODE_CHAR_LIT: return TOKEN_CHAR_LITERAL;
    case NODE_STRING_LIT: return TOKEN_STRING_LITERAL;
    case NODE_HEXA_LIT: return TOKEN_HEXA;
    case NODE_VAR_ACCESS: {
        Symbol* sym = lookup_table(table, expr->ast.var_access.name);
        if(!sym) {
            typecheck_error_at(typecheck, expr, "'%.*s' nao foi declarado",
                (int)expr->ast.var_access.name.len, expr->ast.var_access.name.start);
            return TOKEN_UNDEFINED;
        }
        return sym->type;
    }
    case NODE_FUNC_CALL: {
        View name = expr->ast.func_call.name;
        Symbol* sym = lookup_table(table, name);

        if(!sym) {
            typecheck_error_at(typecheck, expr, "'%.*s' nao foi declarado",
                (int)name.len, name.start);
            return TOKEN_UNDEFINED;
        }

        if(sym->category != SYMBOL_FUNC) {
            typecheck_error_at(typecheck, expr, "'%.*s' nao e uma funcao",
                (int)name.len, name.start);
            return TOKEN_UNDEFINED;
        }

        NodeList* args = expr->ast.func_call.args;
        size_t arg_count = args ? args->count : 0;

        if(arg_count != sym->params_count) {
            typecheck_error(typecheck, "'%.*s' espera %zu argumento(s), recebeu %zu",
                (int)name.len, name.start,
                sym->params_count, arg_count);
            return sym->type;
        }

        for(size_t i = 0; i < arg_count; ++i) {
            TokenType arg_type = infer_type(typecheck, table, args->items[i]);
            size_t arg_ptr = infer_ptr_lvl(table, args->items[i]);
            if(arg_type == TOKEN_UNDEFINED) continue;

            if(!type_compatible(sym->params[i].type, sym->params[i].pointer_lvl, arg_type, arg_ptr)) {
                char exp_buf[64], got_buf[64];
                format_type(exp_buf, sizeof(exp_buf), sym->params[i].type, sym->params[i].pointer_lvl);
                format_type(got_buf, sizeof(got_buf), arg_type, arg_ptr);
                typecheck_error_at(typecheck, args->items[i],
                    "'%.*s': argumento %zu incompativel - esperado '%s', recebeu '%s'",
                    (int)name.len, name.start, i + 1, exp_buf, got_buf);
            }
        }

        return sym->type;
    }
    case NODE_UNARY_OP:
    case NODE_POSTFIX_OP: 
        return infer_type(typecheck, table, expr->ast.unary_op.operand);
    case NODE_CAST: {
        TokenType target = expr->ast.cast_expr.target;
        size_t ptr_lvl = expr->ast.cast_expr.ptr_lvl;

        TokenType source = infer_type(typecheck, table, expr->ast.cast_expr.expr);
        size_t source_ptr = infer_ptr_lvl(table, expr->ast.cast_expr.expr);

        if(target == TOKEN_KEYWORD_LINK) {
            typecheck_error_at(typecheck, expr,
                "cast para 'link' nao e permitido - 'link' so pode vir de "
                "literal hexadecimal ou de outro 'link'");
            return TOKEN_UNDEFINED;
        }

        if(source == TOKEN_KEYWORD_LINK) {
            return target;
        }

        bool target_is_aggregate = (target == TOKEN_KEYWORD_STRUCT || target == TOKEN_KEYWORD_UNION);
        bool source_is_aggregate = (target == TOKEN_KEYWORD_STRUCT || target == TOKEN_KEYWORD_UNION);

        if(target_is_aggregate && ptr_lvl == 0) {
            typecheck_error_at(typecheck, expr,
                "cast invalido: nao e possivel fazer cast para '%s' por valor "
                "(apenas ponteiro para struct/union pode ter cast)",
                token_to_str(target));
            return TOKEN_UNDEFINED;
        }

        if(source_is_aggregate && source_ptr == 0) {
            typecheck_error_at(typecheck, expr,
                "cast invalido: nao e possivel fazer cast de '%s' por valor "
                "(apenas ponteiro para struct/union pode ter cast)",
                token_to_str(source));
            return TOKEN_UNDEFINED;
        }

        return target;
    }
    case NODE_BINARY_OP: {
        TokenType op = expr->ast.binary_op.op;

        if(op == TOKEN_DOT || op == TOKEN_OP_ARROW) {
            Node* lhs = expr->ast.binary_op.left;
            Node* rhs = expr->ast.binary_op.right;

            size_t lhs_ptr_lvl = infer_ptr_lvl(table, lhs);

            if(op == TOKEN_OP_ARROW && lhs_ptr_lvl == 0) {
                typecheck_error_at(typecheck, expr, "'->': operando esquerdo nao e um ponteiro");
                return TOKEN_UNDEFINED;
            }

            if(op == TOKEN_DOT && lhs_ptr_lvl > 0) {
                typecheck_error_at(typecheck, expr, "'.': operando esquerdo e um ponteiro, use '->'");
                return TOKEN_UNDEFINED;
            }

            View struct_type_name = { 0 };
            if(lhs->type == NODE_VAR_ACCESS) {
                Symbol* lhs_sym = lookup_table(table, lhs->ast.var_access.name);
                if(lhs_sym) struct_type_name = lhs_sym->typename;
            }

            if(struct_type_name.len == 0) return TOKEN_UNDEFINED;

            Symbol* struct_sym = lookup_table(table, struct_type_name);
            if(!struct_sym || struct_sym->category != SYMBOL_STRUCT) {
                typecheck_error_at(typecheck, expr, "'%.*s' nao e uma struct conhecida",
                        (int)struct_type_name.len, struct_type_name.start);
                return TOKEN_UNDEFINED;
            }

            View field_name = rhs->ast.var_access.name;
            for(size_t i = 0; i < struct_sym->fields_count; ++i) {
                if(struct_sym->fields[i].name.len == field_name.len && 
                    memcmp(struct_sym->fields[i].name.start, field_name.start, field_name.len) == 0)
                    return struct_sym->fields[i].type;
            }

            typecheck_error_at(typecheck, expr, "struct '%.*s' nao tem campo '%.*s'",
                    (int)struct_type_name.len, struct_type_name.start,
                    (int)field_name.len, field_name.start);
            return TOKEN_UNDEFINED;
        }

        if(op == TOKEN_OP_ASSIGN) {
            Node* lhs = expr->ast.binary_op.left;
            Node* rhs = expr->ast.binary_op.right;

            if(!is_lvalue(lhs)) {
                typecheck_error_at(typecheck, expr, "lado esquerdo de '=' nao e um lvalue valido");
                return TOKEN_UNDEFINED;
            }

            Symbol* sym = lookup_table(table, lhs->ast.var_access.name);
            if(!sym) {
                typecheck_error_at(typecheck, lhs, "'%.*s' nao foi declarado",
                    (int)lhs->ast.var_access.name.len,
                    lhs->ast.var_access.name.start);
                return TOKEN_UNDEFINED;
            }

            if(sym->is_const) {
                typecheck_error_at(typecheck, lhs, "atribuicao a variavel const '%.*s' nao e permitida",
                    (int)lhs->ast.var_access.name.len,
                    lhs->ast.var_access.name.start);
                return sym->type;
            }

            TokenType rhs_type = infer_type(typecheck, table, rhs);
            size_t rhs_ptr = infer_ptr_lvl(table, rhs);
            if(rhs_type != TOKEN_UNDEFINED && !type_compatible(sym->type, sym->pointer_lvl, rhs_type, rhs_ptr)) {
                typecheck_error_at(typecheck, expr, "atribuicao incompativel em '%.*s': "
                        "variavel e '%s', expressao e '%s'",
                    (int)lhs->ast.var_access.name.len,
                    lhs->ast.var_access.name.start,
                    token_to_str(sym->type),
                    token_to_str(rhs_type));
            }

            return sym->type;
        }

        if (op == TOKEN_OP_PLUS_EQ  || op == TOKEN_OP_MINUS_EQ  ||
        op == TOKEN_OP_STAR_EQ  || op == TOKEN_OP_SLASH_EQ  ||
        op == TOKEN_OP_MOD_EQ   || op == TOKEN_BIT_AND_EQ   ||
        op == TOKEN_BIT_OR_EQ   || op == TOKEN_BIT_LSHIFT_EQ||
        op == TOKEN_BIT_RSHIFT_EQ) {
            Node* lhs = expr->ast.binary_op.left;
            Node* rhs = expr->ast.binary_op.right;

            if(!is_lvalue(lhs)) {
                typecheck_error_at(typecheck, expr, "lado esquerdo de '%s' nao e um lvalue valido",
                    token_to_str(op));
                return TOKEN_UNDEFINED;
            }

            Symbol* sym = lookup_table(table, lhs->ast.var_access.name);
            if(!sym) {
                typecheck_error_at(typecheck, lhs, "'%.*s' nao foi declarado",
                    (int)lhs->ast.var_access.name.len,
                    lhs->ast.var_access.name.start);
                return TOKEN_UNDEFINED;
            }

            if(sym->is_const) {
                typecheck_error_at(typecheck, expr, "'%s' requer operandos numericos, mas '%.*s' e '%s'",
                    token_to_str(op),
                    (int)lhs->ast.var_access.name.len,
                    lhs->ast.var_access.name.start,
                    token_to_str(sym->type));
                return TOKEN_UNDEFINED;
            }

            if(!is_numeric_type(sym->type)) {
                typecheck_error_at(typecheck, lhs, "atribuicao a variavel const '%.*s' nao e permitida",
                    (int)lhs->ast.var_access.name.len,
                    lhs->ast.var_access.name.start);
                return sym->type;
            }

            TokenType rhs_type = infer_type(typecheck, table, rhs);
            if(rhs_type != TOKEN_UNDEFINED && !is_numeric_type(rhs_type)) {
                typecheck_error_at(typecheck, expr, "'%s' requer operandos numericos, lado direito e '%s'",
                    token_to_str(op),
                    token_to_str(rhs_type));
            }

            return sym->type;
        }

        switch (expr->ast.binary_op.op)
        {
        case TOKEN_OP_EQ_EQ:
        case TOKEN_OP_BANG_EQ:
        case TOKEN_OP_LT:
        case TOKEN_OP_LT_EQ:
        case TOKEN_OP_GT:
        case TOKEN_OP_GT_EQ:
        case TOKEN_OP_AND:
        case TOKEN_OP_OR:
            return TOKEN_KEYWORD_BOOLEAN;
        default:
            break;
        }

        TokenType left = infer_type(typecheck, table, expr->ast.binary_op.left);
        TokenType right = infer_type(typecheck, table, expr->ast.binary_op.right);

        if(left == TOKEN_UNDEFINED || right == TOKEN_UNDEFINED) return TOKEN_UNDEFINED;
        if(left == TOKEN_KEYWORD_DOUBLE || right == TOKEN_KEYWORD_DOUBLE) return TOKEN_KEYWORD_DOUBLE;
        if(left == TOKEN_KEYWORD_FLOAT || right == TOKEN_KEYWORD_FLOAT) return TOKEN_KEYWORD_FLOAT;
        if (left == TOKEN_DOUBLE_LIT || right == TOKEN_DOUBLE_LIT) return TOKEN_DOUBLE_LIT;
        if(is_integer_type(left) && is_integer_type(right)) return left;
        if(!type_compatible(left, 0, right, 0)) {
            typecheck_error_at(typecheck, expr, "tipos incompativeis na expressao binaria '%s'",
                        token_to_str(expr->ast.binary_op.op));
        }

        return left;
    }
    default:
        return TOKEN_UNDEFINED;
    }
}

static void check_condition(TypeChecker *typechecker, SymbolTable *table, Node *condition, const char* context) {
    TokenType type = infer_type(typechecker, table, condition);
    size_t ptr_lvl = infer_ptr_lvl(table, condition);

    if(type == TOKEN_UNDEFINED) return;

    if(type == TOKEN_KEYWORD_LINK && ptr_lvl == 0) {
        typecheck_error_at(typechecker, condition, "'%s': 'link' nunca e NULL, use 'link*' para verificar nulidade", context);
        return;
    }
    
    if(type == TOKEN_KEYWORD_VOID && ptr_lvl == 0) {
        typecheck_error_at(typechecker, condition, "'%s': condicao nao pode ser void", context);
        return;
    }
}

static void check_node(TypeChecker *typechecker, SymbolTable *table, Node *node);

static void check_node_list(TypeChecker *typechecker, SymbolTable *table, NodeList *list) {
    if(!list) return;
    for(size_t i = 0; i < list->count; ++i)
        check_node(typechecker, table, list->items[i]);
}

static void check_var_decl(TypeChecker *typechecker, SymbolTable *table, Node *node) {
    View name = node->ast.var_decl.name;
    TokenType type = node->ast.var_decl.data_type;
    size_t pointer_lvl = node->ast.var_decl.pointer_lvl;
    Node* init = node->ast.var_decl.init;

    if(lookup_current_table(table, name)) {
        typecheck_error_at(typechecker, node, "'%.*s' ja foi declarado nesse escopo",
                (int)name.len, name.start);
    }

    if(node->ast.var_decl.is_const && !init) {
        typecheck_error_at(typechecker, node, "variavel const '%.*s' deve ter inicializador",
                (int)name.len, name.start);
    }

    if(init) {
        TokenType current = infer_type(typechecker, table, init);
        size_t ptr = infer_ptr_lvl(table, init);
        if(current != TOKEN_UNDEFINED && !type_compatible(type, pointer_lvl, current, ptr)) {
            char decl_buf[64], actual_buf[64];
            format_type(decl_buf,   sizeof(decl_buf),   type,    pointer_lvl);
            format_type(actual_buf, sizeof(actual_buf),  current, ptr);
            typecheck_error_at(typechecker, node,
                "tipo incompativel ao declarar '%.*s': declarado como '%s', recebeu '%s'",
                (int)name.len, name.start, decl_buf, actual_buf);
        }
    }

    Symbol* sym = insert_table(table, name, SYMBOL_VAR, pointer_lvl, type);
    sym->typename = node->ast.var_decl.typename;
    sym->is_const = node->ast.var_decl.is_const;
    sym->is_static = node->ast.var_decl.is_static;
}

static void check_func_decl(TypeChecker *typechecker, SymbolTable *table, Node *node) {
    View name = node->ast.func_decl.name;
    TokenType return_type = node->ast.func_decl.return_type;
    size_t pointer_lvl = node->ast.func_decl.pointer_lvl;

    bool is_entry = (name.len == 5 &&
        name.start[0] == 's' &&
        name.start[1] == 't' &&
        name.start[2] == 'a' &&
        name.start[3] == 'r' &&
        name.start[4] == 't');
    
    if(is_entry) {
        bool valid = (return_type == TOKEN_KEYWORD_VOID && pointer_lvl == 0) ||
                    (return_type == TOKEN_KEYWORD_LINK && pointer_lvl == 0);
        if(!valid) {
            typecheck_error_at(typechecker, node,
                "'start' so pode retornar 'void' ou 'link', recebeu '%s'",
                token_to_str(return_type));
        }
    }

    NodeList* params = node->ast.func_decl.params;

    if (typechecker->in_func) {
        typecheck_error_at(typechecker, node, "declaracao de funcao '%.*s' dentro de outra funcao nao e permitido",
                (int)node->ast.func_decl.name.len,
                node->ast.func_decl.name.start);
        return;
    }


    if(lookup_current_table(table, name)) {
        typecheck_error_at(typechecker, node, "funcao '%.*s' ja foi declarada nesse escopo",
                (int)name.len, name.start);
    }

    Symbol* func_sym = insert_table(table, name, SYMBOL_FUNC, pointer_lvl, return_type);
    func_sym->is_static = node->ast.func_decl.is_static;

    if(params && params->count > 0) {
        func_sym->params_count = params->count;
        func_sym->params = (ParamInfo*)arena_alloc(table->arena, sizeof(ParamInfo) * params->count);

        for(size_t i = 0; i < params->count; ++i) {
            Node* item = params->items[i];
            func_sym->params[i].type = item->ast.var_decl.data_type;
            func_sym ->params[i].pointer_lvl = item->ast.var_decl.pointer_lvl;
        }
    }

    typechecker->in_func = true;
    typechecker->current_return_type = return_type;
    typechecker->current_return_ptr_lvl = pointer_lvl;

    push_scope(table);

    if(params) {
        for(size_t i = 0; i < params->count; ++i) {
            Node* item = params->items[i];
            insert_table(table, item->ast.var_decl.name, SYMBOL_VAR,
                        item->ast.var_decl.pointer_lvl, item->ast.var_decl.data_type);
        }
    }

    if(node->ast.func_decl.body)
            check_node(typechecker, table, node->ast.func_decl.body);
    
    pop_scope(table);

    typechecker->in_func = false;
    typechecker->current_return_type = TOKEN_UNDEFINED;
    typechecker->current_return_ptr_lvl = 0;
}

static void check_node(TypeChecker *typechecker, SymbolTable *table, Node *node) {
    if(!node) return;

    Node* current = node;
    while(current) {
        switch (current->type)
        {
        case NODE_STRUCT: {
            View struct_name = current->ast.struct_decl.name;
            NodeList* fields = current->ast.struct_decl.fields;

            if(lookup_current_table(table, struct_name)) {
                typecheck_error_at(typechecker, current, "struct '%.*s' ja foi declarada nesse escopo",
                        (int)struct_name.len, struct_name.start);
            }

            Symbol* sym = insert_table(table, struct_name, SYMBOL_STRUCT, 0, TOKEN_KEYWORD_STRUCT);
            if(!fields || fields->count == 0) break;

            size_t total = 0;
            for(size_t i = 0; i < fields->count; ++i) {
                Node* field = fields->items[i];
                if((field->type == NODE_UNION || field->type == NODE_STRUCT) && field->ast.struct_decl.name.len == 0)
                    total += field->ast.struct_decl.fields ? field->ast.struct_decl.fields->count : 0;
                else
                    total++;
            }

            sym->fields_count = total;
            sym->fields = (FieldInfo*)arena_alloc(table->arena, sizeof(FieldInfo) * total);

            size_t fi = 0;
            for(size_t i = 0; i < fields->count; ++i) {
                Node* field = fields->items[i];

                bool is_embedded = (field->type == NODE_UNION || field->type == NODE_STRUCT);

                if(is_embedded && field->ast.struct_decl.name.len == 0) {
                    NodeList* inner = field->ast.struct_decl.fields;
                    if(!inner) continue;
                    for(size_t j = 0; j < inner->count; ++j) {
                        Node* g = inner->items[j];
                        sym->fields[fi].name = g->ast.var_decl.name;
                        sym->fields[fi].type = g->ast.var_decl.data_type;
                        sym->fields[fi].pointer_lvl = g->ast.var_decl.pointer_lvl;
                        sym->fields[fi].typename = g->ast.var_decl.typename;
                        fi++;
                    }
                } else if(is_embedded) {
                    View inner_name = field->ast.struct_decl.name;
                    sym->fields[fi].name = inner_name;
                    sym->fields[fi].type = (field->type == NODE_UNION) ? TOKEN_KEYWORD_UNION : TOKEN_KEYWORD_STRUCT;
                    sym->fields[fi].typename = inner_name;
                    sym->fields[fi].pointer_lvl = 0;
                    fi++;
                } else {
                    sym->fields[fi].name = field->ast.var_decl.name;
                    sym->fields[fi].type = field->ast.var_decl.data_type;
                    sym->fields[fi].typename = field->ast.var_decl.typename;
                    sym->fields[fi].pointer_lvl = field->ast.var_decl.pointer_lvl;
                    fi++;
                }
            }
            break;
        }
        case NODE_ENUM: {
            NodeList* members = current->ast.enum_decl.members;
            long long next_value = 0;

            if(current->ast.enum_decl.has_name) {
                View enum_name = current->ast.enum_decl.name;
                if(lookup_current_table(table, enum_name)) {
                    typecheck_error_at(typechecker, current, "enum '%.*s' ja foi declarado nesse escopo",
                            (int)enum_name.len, enum_name.start);
                }
                insert_table(table, enum_name, SYMBOL_ENUM, 0, TOKEN_KEYWORD_ENUM);
            }

            for(size_t i = 0; i < members->count; ++i) {
                Node* member = members->items[i];
                View member_name = member->ast.enum_member.name;

                if(lookup_current_table(table, member_name)) {
                    typecheck_error_at(typechecker, member, "membro de enum '%.*s' ja foi declarado",
                                    (int)member_name.len, member_name.start);
                }

                Symbol* member_sym = insert_table(table, member_name, SYMBOL_VAR, 0, TOKEN_KEYWORD_INT);
                member_sym->is_const = true;

                if(member->ast.enum_member.expr) {
                    TokenType type = infer_type(typechecker, table, member->ast.enum_member.expr);
                    if(type != TOKEN_UNDEFINED && !is_integer_type(type)) {
                        typecheck_error_at(typechecker, member,
                            "valor do membro de enum '%.*s' deve ser inteiro",
                            (int)member_name.len, member_name.start);
                    }
                    if(member->ast.enum_member.expr->type == NODE_INT_LIT) {
                        next_value = member->ast.enum_member.expr->ast.numeric_literal.value;
                    } else if (member->ast.enum_member.expr->type == NODE_HEXA_LIT){
                        next_value = member->ast.enum_member.expr->ast.numeric_literal.value;
                    }
                }
                member_sym->has_const_value = true;
                member_sym->const_value = next_value;
                next_value++;
            }
            break;
        }
        case NODE_TYPEDEF_DECL: {
            View alias = current->ast.typedef_decl.name;

            if(lookup_current_table(table, alias)) {
                typecheck_error_at(typechecker, current,
                    "'%.*s' ja foi declarado nesse escopo",
                    (int)alias.len, alias.start);
                break;
            }

            switch (current->ast.typedef_decl.type)
            {
            case STRUCT:
            case UNION: 
                check_node(typechecker, table, current->ast.typedef_decl.struct_node);
                insert_table(table, alias, SYMBOL_TYPEDEF, 0, current->ast.typedef_decl.type == STRUCT ? TOKEN_KEYWORD_STRUCT : TOKEN_KEYWORD_UNION);
                break;
            case ENUM: 
                check_node(typechecker, table, current->ast.typedef_decl.enum_node);
                insert_table(table, alias, SYMBOL_TYPEDEF, 0, TOKEN_KEYWORD_ENUM);
                break;
            case PRIMITIVE:
                insert_table(table, alias, SYMBOL_TYPEDEF, 0, current->ast.typedef_decl.primitive.type);
                break;
            default:
                break;
            }
        }
        case NODE_UNION:
            break;
        case NODE_VAR_DECL: {
            check_var_decl(typechecker, table, current);
            break;
        }
        case NODE_FUNC_DECL: {
            check_func_decl(typechecker, table, current);
            break;
        }
        case NODE_BLOCK: {
            push_scope(table);
            check_node_list(typechecker, table, current->ast.program.declarations);
            pop_scope(table);
            break;
        }
        case NODE_PROGRAM: {
            check_node_list(typechecker, table, current->ast.program.declarations);
            break;
        }
        case NODE_IF_STMT: {
            infer_type(typechecker, table, current->ast.if_stmt.condition);
            check_node(typechecker, table, current->ast.if_stmt.then);
            if(current->ast.if_stmt.otherwise)
                check_node(typechecker, table, current->ast.if_stmt.otherwise);
            break;
        }
        case NODE_WHILE_STMT: {
            check_condition(typechecker, table, current->ast.while_stmt.condition, "while");
            bool prev_in_loop = typechecker->in_loop;
            typechecker->in_loop = true;
            check_node(typechecker, table, current->ast.while_stmt.body);
            typechecker->in_loop = prev_in_loop;
            break;
        }
        case NODE_FOR_STMT: {
            push_scope(table);

            if(current->ast.for_stmt.init) 
                check_node(typechecker, table, current->ast.for_stmt.init);

            if(current->ast.for_stmt.condition)
                check_condition(typechecker, table, current->ast.for_stmt.condition, "for");

            if(current->ast.for_stmt.increment)
                infer_type(typechecker, table, current->ast.for_stmt.increment);

            bool prev_in_loop = typechecker->in_loop;
            typechecker->in_loop = true;
            check_node(typechecker, table, current->ast.for_stmt.body);
            typechecker->in_loop = prev_in_loop;

            pop_scope(table);
            break;
        }
        case NODE_DO_WHILE_STMT: {
            bool prev_in_loop = typechecker->in_loop;
            typechecker->in_loop = true;
            check_node(typechecker, table, current->ast.while_stmt.body);
            typechecker->in_loop = prev_in_loop;
            check_condition(typechecker, table, current->ast.while_stmt.condition, "do-while");
            break;
        }
        case NODE_BREAK_STMT:
        case NODE_CONTINUE_STMT: {
            if(!typechecker->in_loop) {
                typecheck_error_at(typechecker, current,
                "'%s' fora de um loop",
                current->type == NODE_BREAK_STMT ? "break" : "continue");
            }
            break;
        }
        case NODE_RETURN_STMT: {
            Node* val = current->ast.return_stmt.value;
            if(!typechecker->in_func) {
                typecheck_error_at(typechecker, current, "'return' fora de uma funcao");
                break;
            }

            bool is_void = (typechecker->current_return_type == TOKEN_KEYWORD_VOID && typechecker->current_return_ptr_lvl == 0);

            if(!val && is_void) break;

            if(val && is_void) {
                typecheck_error_at(typechecker, current, "funcao '%s' deve retornar um valor",
                    token_to_str(typechecker->current_return_type));
                break;
            }

            if(!val && !is_void) {
                typecheck_error(typechecker, "funcao '%s' deve retornar um valor",
                    token_to_str(typechecker->current_return_type));
                break;
            }

            if(typechecker->current_return_type == TOKEN_KEYWORD_LINK) {
                TokenType actual = infer_type(typechecker, table, val);
                if (actual != TOKEN_HEXA && actual != TOKEN_KEYWORD_LINK) {
                    typecheck_error_at(typechecker, current,
                        "funcao 'link' so pode retornar literais hexadecimais ou outro link"
                        " (ex: 0x1A2B), recebeu '%s'",
                        token_to_str(actual));
                }
                break;
            }

            TokenType actual = infer_type(typechecker, table, val);
            size_t ptr = infer_ptr_lvl(table, val);
            if(actual != TOKEN_UNDEFINED && !type_compatible(typechecker->current_return_type, typechecker->current_return_ptr_lvl,
                actual, ptr)) {
                char exp_buf[64], got_buf[64];
                format_type(exp_buf, sizeof(exp_buf), typechecker->current_return_type, typechecker->current_return_ptr_lvl);
                format_type(got_buf, sizeof(got_buf), actual, ptr);
                typecheck_error_at(typechecker, current,
                    "tipo de retorno incompativel: esperado '%s', recebeu '%s'",
                    exp_buf, got_buf);
            }

            break;
        }
        case NODE_FUNC_CALL: {
            infer_type(typechecker, table, current);
            break;
        }
        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP:
        case NODE_VAR_ACCESS:
        case NODE_CAST:
            infer_type(typechecker, table, current);
            break;
        default:
            break;
        }

        current = current->next;
    }
}

void typecheck_program(TypeChecker *typechecker, SymbolTable *table, Node *program, DiagContext *diag) {
    typechecker->error_count = 0;
    typechecker->in_func = false;
    typechecker->in_loop = false;
    typechecker->current_return_type = TOKEN_UNDEFINED;
    typechecker->current_return_ptr_lvl = 0;
    typechecker->diag = diag;
    check_node(typechecker, table, program);
}
