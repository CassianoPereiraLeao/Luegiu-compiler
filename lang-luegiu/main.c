#include "common.h"
#include "lexer.h"
#include "arena.h"
#include "parser.h"
#include "preprocessor.h"
#include "codegen.h"
#include "diag.h"

static void write_output_bootstrap(const char *path, CodeGenBuffer *buf) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(buf->data, 1, buf->len, f);
    fclose(f);
}

void print_list(NodeList* list, int depth);

const char* view_to_str(View v) {
    static char buffer[1024]; 
    size_t len = v.len;
    if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
    memcpy(buffer, v.start, len);
    buffer[len] = '\0';
    
    return buffer;
}

void print_ast(Node* node, int depth) {
    if (node == NULL) return;

    for (int i = 0; i < depth; i++) printf("  ");

    switch (node->type) {
        case NODE_DO_WHILE_STMT:
            printf("DO_WHILE_STMT\n");
            print_ast(node->ast.while_stmt.body, depth + 1);
            print_ast(node->ast.while_stmt.condition, depth + 1);
            break;
        case NODE_UNION:
            printf("UNION: %s\n", view_to_str(node->ast.struct_decl.name));
            print_list(node->ast.struct_decl.fields, depth + 1);
            break;
        case NODE_HEXA_LIT:
        case NODE_INT_LIT:
        case NODE_DOUBLE_LIT:
            printf("LITERAL: %lld\n", node->ast.numeric_literal.value);
            break;
        case NODE_BINARY_OP:
            printf("BIN_OP: %s\n", token_to_str(node->ast.binary_op.op));
            print_ast(node->ast.binary_op.left, depth + 1);
            print_ast(node->ast.binary_op.right, depth + 1);
            break;
        case NODE_UNARY_OP:
            printf("UNARY_OP: %s\n", token_to_str(node->ast.unary_op.op));
            print_ast(node->ast.unary_op.operand, depth + 1);
            break;
        case NODE_VAR_DECL:
            printf("VAR_DECL: %s (type: %s, ptr: %zu)\n", 
                    view_to_str(node->ast.var_decl.name), 
                    token_to_str(node->ast.var_decl.data_type),
                    node->ast.var_decl.pointer_lvl);
            if (node->ast.var_decl.init) print_ast(node->ast.var_decl.init, depth + 1);
            break;
        case NODE_PROGRAM:
            printf("PROGRAM\n");
            print_list(node->ast.program.declarations, depth + 1);
            break;
        case NODE_STRUCT:
            printf("STRUCT: %s\n", view_to_str(node->ast.struct_decl.name));
            print_list(node->ast.struct_decl.fields, depth + 1);
            break;
        case NODE_IF_STMT:
            printf("IF_STMT\n");
            print_ast(node->ast.if_stmt.condition, depth + 1);
            print_ast(node->ast.if_stmt.then, depth + 1);
            if(node->ast.if_stmt.otherwise) print_ast(node->ast.if_stmt.otherwise, depth + 1);
            break;
        case NODE_WHILE_STMT:
            printf("WHILE_STMT\n");
            print_ast(node->ast.while_stmt.condition, depth + 1);
            print_ast(node->ast.while_stmt.body, depth + 1);
            break;
        case NODE_FOR_STMT:
            printf("FOR_STMT\n");
            if (node->ast.for_stmt.init)      print_ast(node->ast.for_stmt.init,      depth + 1);
            if (node->ast.for_stmt.condition) print_ast(node->ast.for_stmt.condition,  depth + 1);
            if (node->ast.for_stmt.increment)    print_ast(node->ast.for_stmt.increment,     depth + 1);
            print_ast(node->ast.for_stmt.body, depth + 1);
            break;
        case NODE_TYPEDEF_DECL:
            printf("TYPEDEF: %s\n", view_to_str(node->ast.typedef_decl.name));
            break;
        case NODE_BLOCK:
            printf("BLOCK\n");
            print_list(node->ast.program.declarations, depth + 1);
            break;
        case NODE_VAR_ACCESS:
            printf("VAR_ACCESS: %s\n", view_to_str(node->ast.var_access.name));
            break;
        case NODE_FUNC_CALL:
            printf("FUNC_CALL: %s (args: %zu)\n",
                view_to_str(node->ast.func_call.name),
                node->ast.func_call.args->count);
            for(size_t i = 0; i < node->ast.func_call.args->count; i++)
                print_ast(node->ast.func_call.args->items[i], depth + 1);
            break;
        case NODE_FUNC_DECL:
            printf("FUNC_DECL: %s (type: %s, ptr: %zu)\n", 
                view_to_str(node->ast.func_decl.name), 
                token_to_str(node->ast.func_decl.return_type),
                node->ast.func_decl.pointer_lvl);
            if(node->ast.func_decl.params->count > 0) {
                for(int i = 0; i < depth + 1; i++) printf("  ");
                printf("PARAMS\n");
                print_list(node->ast.func_decl.params, depth + 2);
            }
            print_ast(node->ast.func_decl.body, depth + 1);
            break;
        case NODE_RETURN_STMT:
            printf("RETURN\n");
            if(node->ast.return_stmt.value)
                print_ast(node->ast.return_stmt.value, depth + 1);
            break;

        default:
            printf("UNKNOWN NODE (%d)\n", node->type);
    }
}

void print_list(NodeList* list, int depth) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        print_ast(list->items[i], depth);
    }
}

static void register_builtins(SymbolTable *table, Arena *arena) {
    {
        View name = { "__write_stdout", 14 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORD_VOID);
        sym->params_count = 2;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo) * 2);
        sym->params[0].type      = TOKEN_KEYWORD_CHAR;
        sym->params[0].pointer_lvl = 1;
        sym->params[1].type      = TOKEN_KEYWORD_INT;
        sym->params[1].pointer_lvl = 0;
    }

    {
        View name = { "__exit", 6 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORD_VOID);
        sym->params_count = 1;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo) * 1);
        sym->params[0].type      = TOKEN_KEYWORD_INT;
        sym->params[0].pointer_lvl = 0;
    }

    {
        View name = { "__int_to_str", 12 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 1, TOKEN_KEYWORD_CHAR);
        sym->params_count = 1;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo) * 1);
        sym->params[0].type        = TOKEN_KEYWORD_INT;
        sym->params[0].pointer_lvl = 0;
    }

    {
        View name = { "__write_int", 11 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORD_VOID);
        sym->params_count = 1;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo) * 1);
        sym->params[0].type        = TOKEN_KEYWORD_INT;
        sym->params[0].pointer_lvl = 0;
    }

    {
        View name = { "__malloc", 8 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 1, TOKEN_KEYWORKD_LINK);
        sym->params_count = 1;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo));
        sym->params[0].type = TOKEN_KEYWORD_BIG;
        sym->params[0].pointer_lvl = 0;
    }

    {
        View name = { "__free", 6 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORD_VOID);
        sym->params_count = 1;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo));
        sym->params[0].type = TOKEN_KEYWORKD_LINK;
        sym->params[0].pointer_lvl = 0;
    }

    {
        View name = { "__parse_args", 12 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORD_INT);
        sym->params_count = 1;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo));
        sym->params[0].type      = TOKEN_KEYWORD_CHAR;
        sym->params[0].pointer_lvl = 2;
    }

    {
        View name = { "__open_file", 12 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORKD_LINK);
        sym->params_count = 2;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo));
        sym->params[0].type      = TOKEN_KEYWORD_CHAR;
        sym->params[0].pointer_lvl = 1;
        sym->params[1].type = TOKEN_KEYWORD_INT;
        sym->params[1].pointer_lvl = 0;
    }

    {
        View name = { "__open_file_w", 14 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORKD_LINK);
        sym->params_count = 2;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo));
        sym->params[0].type      = TOKEN_KEYWORD_UTF16CHAR;
        sym->params[0].pointer_lvl = 1;
        sym->params[1].type = TOKEN_KEYWORD_INT;
        sym->params[1].pointer_lvl = 0;
    }

    {
        View name = { "__read_file", 12 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORD_UINT64);
        sym->params_count = 3;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo));
        sym->params[0].type      = TOKEN_KEYWORKD_LINK;
        sym->params[0].pointer_lvl = 1;
        sym->params[1].type = TOKEN_KEYWORD_INT;
        sym->params[1].pointer_lvl = 0;
        sym->params[1].type = TOKEN_KEYWORD_INT;
        sym->params[1].pointer_lvl = 0;
    }

    {
        View name = { "__close_file", 13 };
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, TOKEN_KEYWORD_VOID);
        sym->params_count = 1;
        sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo));
        sym->params[0].type      = TOKEN_KEYWORKD_LINK;
        sym->params[0].pointer_lvl = 1;
    }
}

int main(int argc, char* argv[]) {
    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(len + 1);

    size_t bytes = fread(source, sizeof(char), len, f);
    source[bytes] = '\0';
    fclose(f);

    Arena arena;
    arena_init(&arena, 1024 * 1024);

    const char* input_path = argv[1];

    DiagContext context;
    diag_init(&context, &arena);
    diag_set_max_errors(&context, 20);
    diag_register_source(&context, input_path, source);

    char base_dir[512] = ".";
    char *last_sep = strrchr(argv[1], '/');
    if (!last_sep) last_sep = strrchr(argv[1], '\\');
    if (last_sep) {
        size_t dir_len = last_sep - argv[1];
        memcpy(base_dir, argv[1], dir_len);
        base_dir[dir_len] = '\0';
    }

    char *lib_dir = "./stdlib";
    char *processed = preprocess_source(source, base_dir, lib_dir, &arena);
    Lexer lexer = create_lexer(processed, input_path, &context);

    Scope root_scope = { .symbols = NULL, .parent = NULL };
    SymbolTable table = { .current_scope = &root_scope, .arena = &arena };

    register_builtins(&table, &arena);

    Parser parser = create_parser(&lexer, &arena, &table, &context);
    Node* ast = parse_program(&parser);

    if(has_error(&context)) {
        diag_report_all(&context);
        free(source);
        arena_free(&arena);
        return 1;
    }

    TypeChecker typechecker;
    typecheck_program(&typechecker, &table, ast, &context);

    if (has_error(&context)) {
        diag_report_all(&context);
        free(source);
        arena_free(&arena);
        return 1;
    }

    CodeGen cg;
    codegen_init(&cg, &arena, &table);
    codegen_program(&cg, ast);
    CodeGenBuffer output = codegen_finalize(&cg);

    write_output_bootstrap(argv[2], &output);

    free(source);
    arena_free(&arena);

    return 0;
}
