#include "common.h"
#include "tokenizer.h"
#include "arena.h"
#include "parser.h"

void print_ast(ASTNode* node, int depth) {
    if (node == NULL) return; for (int i = 0; i < depth; i++) printf("  | ");

    switch (node->type) {
        case NODE_VAR_DECL:
            printf("|-- [DECLARACAO DE VARIAVEL]\n");
            for (int i = 0; i < depth + 1; i++) printf("  | ");
            printf("|-- Tipo: %.*s\n", (int)node->ast.var_decl.type.view.len, node->ast.var_decl.type.view.data);
            for (int i = 0; i < depth + 1; i++) printf("  | ");
            printf("|-- Nome: %.*s\n", (int)node->ast.var_decl.identifier.view.len, node->ast.var_decl.identifier.view.data);
            if (node->ast.var_decl.value) {
                for (int i = 0; i < depth + 1; i++) printf("  | ");
                printf("--- Valor Inicial:\n");
                print_ast(node->ast.var_decl.value, depth + 2);
            }
            break;

        case NODE_ASSIGN_EXPR:
            printf("|-- [ATRIBUICAO] (%.*s)\n", (int)node->ast.binary.op.view.len, node->ast.binary.op.view.data);
            print_ast(node->ast.binary.left, depth + 1);
            print_ast(node->ast.binary.right, depth + 1);
            break;

        case NODE_BINARY_EXPR:
            printf("|-- [OP BINARIA] (%.*s)\n", (int)node->ast.binary.op.view.len, node->ast.binary.op.view.data);
            print_ast(node->ast.binary.left, depth + 1);
            print_ast(node->ast.binary.right, depth + 1);
            break;

        case NODE_UNARY_PREFIX_EXPR:
            printf("|-- [UNARIO PREFIX] (%.*s)\n", (int)node->ast.unary.op.view.len, node->ast.unary.op.view.data);
            print_ast(node->ast.unary.operand, depth + 1);
            break;

        case NODE_UNARY_POSTFIX_EXPR:
            printf("|-- [UNARIO POSTFIX] (%.*s)\n", (int)node->ast.unary.op.view.len, node->ast.unary.op.view.data);
            print_ast(node->ast.unary.operand, depth + 1);
            break;

        case NODE_IDENTIFIER:
            printf("--- [IDENTIFICADOR] (%.*s)\n", (int)node->ast.token.view.len, node->ast.token.view.data);
            break;

        case NODE_NUMERIC_LITERAL:
            printf("--- [LITERAL INT] (%.*s)\n", (int)node->ast.token.view.len, node->ast.token.view.data);
            break;

        case NODE_BLOCK:
            printf("|-- [BLOCO ESCOPO] (%zu instrucoes)\n", node->ast.block.count);
            for (size_t i = 0; i < node->ast.block.count; i++) {
                print_ast(node->ast.block.statements[i], depth + 1);
            }
            break;
        case NODE_FUNC_CALL:
            printf("|-- [CHAMADA DE FUNCAO] (%.*s) com %zu argumentos\n", 
                (int)node->ast.func_call.name.view.len, node->ast.func_call.name.view.data, node->ast.func_call.args_count);
            for(size_t i = 0; i < node->ast.func_call.args_count; i++) {
                print_ast(node->ast.func_call.args[i], depth + 1);
            }
            break;

        case NODE_FUNC_DECL:
            printf("|-- [DECLARACAO DE FUNCAO] %.*s (Retorno: %.*s)\n", 
                (int)node->ast.func_decl.name.view.len, node->ast.func_decl.name.view.data,
                (int)node->ast.func_decl.type.view.len, node->ast.func_decl.type.view.data);
            for(size_t i = 0; i < node->ast.func_decl.count; i++) {
                print_ast(node->ast.func_decl.params[i], depth + 1);
            }
            print_ast(node->ast.func_decl.body, depth + 1);
            break;
        case NODE_RETURN_STMT:
            printf("|-- [STMT RETURN]\n");
            if (node->ast.return_stmt.expr) {
                print_ast(node->ast.return_stmt.expr, depth + 1);
            } else {
                for (int i = 0; i < depth + 1; i++) printf("  │ ");
                printf("--- (void/vazio)\n");
            }
            break;

        case NODE_BREAK_STMT:
            printf("--- [STMT BREAK]\n");
            break;

        case NODE_CONTINUE_STMT:
            printf("--- [STMT CONTINUE]\n");
            break;
        case NODE_IF_STMT:
            printf("|-- [STMT IF]\n");
            print_ast(node->ast.if_stmt.condition, depth + 1);
            print_ast(node->ast.if_stmt.then, depth + 1);
            if (node->ast.if_stmt.otherwise) {
                for (int i = 0; i < depth; i++) printf("  │ ");
                printf("|-- [RAMO ELSE]\n");
                print_ast(node->ast.if_stmt.otherwise, depth + 1);
            }
            break;

        case NODE_WHILE_STMT:
            printf("|-- [LOOP WHILE]\n");
            print_ast(node->ast.loop_stmt.condition, depth + 1);
            print_ast(node->ast.loop_stmt.body, depth + 1);
            break;

        case NODE_DO_WHILE_STMT:
            printf("|-- [LOOP DO-WHILE]\n");
            print_ast(node->ast.loop_stmt.body, depth + 1);
            print_ast(node->ast.loop_stmt.condition, depth + 1);
            break;

        case NODE_FOR_STMT:
            printf("|-- [LOOP FOR]\n");
            if (node->ast.for_stmt.var) print_ast(node->ast.for_stmt.var, depth + 1);
            if (node->ast.for_stmt.condition) print_ast(node->ast.for_stmt.condition, depth + 1);
            if (node->ast.for_stmt.increment) print_ast(node->ast.for_stmt.increment, depth + 1);
            print_ast(node->ast.for_stmt.body, depth + 1);
            break;
    }
}

void rodar_teste(const char* c_codigo, const char* descricao) {
    printf("\n==================================================\n");
    printf("TESTE: %s\n", descricao);
    printf("CODIGO: %s\n", c_codigo);
    printf("==================================================\n");

    Arena arena;
    arena_init(&arena);

    Lexer lexer = create_lexer(c_codigo, strlen(c_codigo));
    Token_Array tokens = tokenize(&lexer, &arena);

    Parser parser = {
        .tokens = tokens,
        .cursor = 0,
        .arena = &arena
    };

    ASTNode* root = parse_statement(&parser);

    if (root) {
        print_ast(root, 0);
    } else {
        printf("[ERRO] Falha ao gerar a AST.\n");
    }

    arena_free(&arena);
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Arquivo não passado para o parâmetro");
        exit(1);
    }

    FILE* f = fopen(argv[1], "r");
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(len + 1);
    fread(content, sizeof(char), len, f);
    content[len] = '\0';
    fclose(f);

    rodar_teste(content, "Controle de Fluxo Complexo com Loops e Condicionais Aninhados");

    free(content);
    fflush(stdout);
    return 0;
}