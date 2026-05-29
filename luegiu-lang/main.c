#include "common.h"
#include "tokenizer.h"
#include "arena.h"
#include "parser.h"
#include "typecheck.h"

void print_semantic_tree(ASTNode* node, SymbolTable* table, int depth) {
    if (node == NULL) return;

    for (int i = 0; i < depth; i++) printf("| ");

    switch (node->type) {
        case NODE_PROGRAM:
            for (size_t i = 0; i < node->ast.program.count; i++) {
                print_semantic_tree(node->ast.program.statements[i], table, depth);
            }
            break;

        case NODE_VAR_DECL: {
            Token id = node->ast.var_decl.identifier;
            Symbol* sym = lookup_symbol(table, id.view.data, id.view.len);
            
            if (sym) {
                printf("|-- [VARIAVEL] '%.*s' de tipo '%.*s'\n", 
                        (int)sym->len, sym->name, (int)sym->type_len, sym->type_name);
                
                if (node->ast.var_decl.value) {
                    for (int i = 0; i < depth + 1; i++) printf("  | ");
                    const char* expr_type = type_of_expr(node->ast.var_decl.value, table);
                    printf("+-- Inicializada com expressao do tipo: '%s'\n", expr_type);
                    
                    if (node->ast.var_decl.value->type == NODE_BINARY_EXPR) {
                        print_semantic_tree(node->ast.var_decl.value, table, depth + 1);
                    }
                }
            }
            break;
        }

        case NODE_FUNC_DECL: {
            Token id = node->ast.func_decl.name;
            Symbol* sym = lookup_symbol(table, id.view.data, id.view.len);
            
            if (sym) {
                printf("|-- [FUNCAO] '%.*s' (Retorno: '%.*s') com %zu parametros\n", 
                        (int)sym->len, sym->name, (int)sym->type_len, sym->type_name, sym->param_count);
            }
            print_semantic_tree(node->ast.func_decl.body, table, depth + 1);
            break;
        }

        case NODE_BLOCK: {
            printf("|-- [BLOCO ESCOPO] (%zu instrucoes semanticas)\n", node->ast.block.count);
            SymbolTable* local_table = node->ast.block.scope ? node->ast.block.scope : table;

            for (size_t i = 0; i < node->ast.block.count; i++) {
                print_semantic_tree(node->ast.block.statements[i], local_table, depth + 1);
            }
            break;
        }

        case NODE_ASSIGN_EXPR: {
            if (node->ast.binary.left->type == NODE_IDENTIFIER) {
                Token id = node->ast.binary.left->ast.token;
                const char* right_type = type_of_expr(node->ast.binary.right, table);
                
                printf("|-- [ATRIBUICAO] Variavel '%.*s' recebendo expressao de tipo '%s'\n", 
                        (int)id.view.len, id.view.data, right_type);
                
                print_semantic_tree(node->ast.binary.right, table, depth + 1);
            }
            break;
        }

        case NODE_BINARY_EXPR: {
            const char* left = type_of_expr(node->ast.binary.left, table);
            const char* right = type_of_expr(node->ast.binary.right, table);
            const char* res = type_of_expr(node, table);
            
            printf("|-- [AVALIACAO BINARIA] Op '%.*s' entre '%s' e '%s' => Resulta em '%s'\n",
                    (int)node->ast.binary.op.view.len, node->ast.binary.op.view.data, left, right, res);
            break;
        }

        case NODE_FUNC_CALL: {
            Token id = node->ast.func_call.name;
            const char* ret = type_of_expr(node, table);
            printf("|-- [CHAMADA] '%.*s' invocada. Retorno esperado: '%s'\n", 
                    (int)id.view.len, id.view.data, ret);
            break;
        }

        case NODE_IF_STMT: {
            const char* cond_type = type_of_expr(node->ast.if_stmt.condition, table);
            printf("|-- [CONDICIONAL IF] (Condicao avaliada como tipo '%s')\n", cond_type);
            
            print_semantic_tree(node->ast.if_stmt.then, table, depth + 1);
            if (node->ast.if_stmt.otherwise) {
                for (int i = 0; i < depth; i++) printf("| ");
                printf("|-- [RAMO ELSE]\n");
                print_semantic_tree(node->ast.if_stmt.otherwise, table, depth + 1);
            }
            break;
        }

        case NODE_WHILE_STMT: {
            printf("|-- [LOOP WHILE]\n");
            print_semantic_tree(node->ast.loop_stmt.body, table, depth + 1);
            break;
        }

        case NODE_DO_WHILE_STMT: {
            printf("|-- [LOOP DO-WHILE]\n");
            print_semantic_tree(node->ast.loop_stmt.body, table, depth + 1);
            break;
        }

        case NODE_FOR_STMT: {
            printf("|-- [LOOP FOR]\n");
            SymbolTable* local_table = node->ast.for_stmt.scope ? node->ast.for_stmt.scope : table;
            
            if (node->ast.for_stmt.var) {
                for (int i = 0; i < depth + 1; i++) printf("| ");
                printf("|-- [FOR INIT]\n");
                print_semantic_tree(node->ast.for_stmt.var, local_table, depth + 2);
            }
            
            if (node->ast.for_stmt.condition) {
                const char* cond_type = type_of_expr(node->ast.for_stmt.condition, local_table);
                for (int i = 0; i < depth + 1; i++) printf("| ");
                printf("|-- [FOR CONDICAO] Tipo: '%s'\n", cond_type);
            }

            if (node->ast.for_stmt.body) {
                for (int i = 0; i < depth + 1; i++) printf("| ");
                printf("|-- [FOR CORPO]\n");
                print_semantic_tree(node->ast.for_stmt.body, local_table, depth + 1);
            }
            break;
        }

        default:
            break;
    }
}

void print_ast(ASTNode* node, int depth) {
    if (node == NULL) return; for (int i = 0; i < depth; i++) printf("  | ");

    switch (node->type) {
        case NODE_VAR_DECL:
            printf("|-- [DECLARACAO DE VARIAVEL]\n");
            for (int i = 0; i < depth + 1; i++) printf("  | ");
            printf("|-- Tipo: %.*s", (int)node->ast.var_decl.type.view.len, node->ast.var_decl.type.view.data);
            for (size_t i = 0; i < node->ast.var_decl.pointer_lvl; i++) {
                printf("*");
            }
            printf("\n");
            for (int i = 0; i < depth + 1; i++) printf("  | ");
            printf("|-- Nome: %.*s\n", (int)node->ast.var_decl.identifier.view.len, node->ast.var_decl.identifier.view.data);
            if (node->ast.var_decl.value) {
                for (int i = 0; i < depth + 1; i++) printf("  | ");
                printf("--- Valor Inicial:\n");
                print_ast(node->ast.var_decl.value, depth + 2);
            }
            break;
        case NODE_INDEX_EXPR:
            printf("[ACESSO ARRAYS]\n");
            print_ast(node->ast.binary.left, depth + 1);
            print_ast(node->ast.binary.right, depth + 1);
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
        case NODE_STRUCT_DECL: 
            printf("|-- [DEFINICAO DE STRUCT] struct %.*s\n", 
                    (int)node->ast.struct_decl.name.view.len, 
                    node->ast.struct_decl.name.view.data);

            for (size_t i = 0; i < node->ast.struct_decl.fields_count; i++) {
                print_ast(node->ast.struct_decl.fields[i], depth + 1);
            }
            break;
        case NODE_MEMBER_ACCESS:
            const char* op_str = ".";
            
            printf("|-- [ACESSO MEMBRO] (%s)\n", op_str);
            print_ast(node->ast.access_member.left, depth + 1);
            for (int i = 0; i < depth + 1; i++) printf("  | ");
            printf("--- [MEMBRO] (%.*s)\n", 
                    (int)node->ast.access_member.member.view.len, 
                    node->ast.access_member.member.view.data);
            break;
        case NODE_TYPEDEF:
            printf("|-- [TYPEDEF DEFINITION]\n");
            for (int i = 0; i < depth + 1; i++) printf("  | ");
            printf("|-- Tipo Base: %.*s", (int)node->ast.typedef_decl.type.view.len, node->ast.typedef_decl.type.view.data);
            for (size_t i = 0; i < node->ast.typedef_decl.pointer_lvl; i++) printf("*");
            printf("\n");
            
            for (int i = 0; i < depth + 1; i++) printf("  | ");
            printf("|-- Apelido: %.*s\n", (int)node->ast.typedef_decl.name.view.len, node->ast.typedef_decl.name.view.data);
            break;
        case NODE_PROGRAM:
            for (size_t i = 0; i < node->ast.program.count; i++) {
                print_ast(node->ast.program.statements[i], depth);
            }
            break;
        case NODE_STRING_LITERAL:
            printf("--- [LITERAL STRING] (\"%.*s\")\n", (int)node->ast.token.view.len, node->ast.token.view.data);
            break;

        case NODE_CHAR_LITERAL:
            printf("--- [LITERAL CHAR] ('%.*s')\n", (int)node->ast.token.view.len, node->ast.token.view.data);
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

    ASTNode* root = parse_program(&parser);
    print_ast(root, 0);

    SymbolTable* global_scope = create_table(NULL, &arena);

    printf("[SEMANTICO] Iniciando analise...\n");
    semantic_analysis(root, global_scope, &arena, NULL);
    print_semantic_tree(root, global_scope, 0);
    printf("\n[SEMANTICO] Sucesso! Nenhum erro encontrado.\n");

    arena_free(&arena);
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Arquivo não passado para o parâmetro");
        exit(1);
    }

    FILE* f = fopen(argv[1], "rb");
    
    if (!f) {
        fprintf(stderr, "Erro ao abrir o arquivo.\n");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(len + 1);

    size_t bytes_read = fread(content, sizeof(char), len, f);

    content[len] = '\0';
    fclose(f);

    rodar_teste(content, "Controle de Fluxo Complexo com Loops e Condicionais Aninhados");

    free(content);
    fflush(stdout);
    return 0;
}