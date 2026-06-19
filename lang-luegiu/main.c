#include "common.h"
#include "lexer.h"
#include "arena.h"
#include "parser.h"
#include "preprocessor.h"
#include "codegen.h"
#include "diag.h"
#include "builtins.h"

static void write_output_bootstrap(const char *path, CodeGenBuffer *buf) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(buf->data, 1, buf->len, f);
    fclose(f);
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
    fprintf(stderr, "=== PROCESSED ===\n%s\n=== END ===\n", processed);
    diag_register_source(&context, input_path, processed);
    if (!processed) {
        fprintf(stderr, "Erro: preprocessador retornou NULL\n");
        free(source);
        arena_free(&arena);
        return 1;
    }
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
