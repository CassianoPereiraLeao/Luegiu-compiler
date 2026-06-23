#include "common.h"
#include "lexer.h"
#include "diag.h"

void printlexer(const char* header, const char* source, Lexer lexer) {

    FILE* file = fopen("./logs/lexer_output.txt", "w");
    if (file == NULL) {
        perror("Erro ao abrir arquivo");
        return;
    }

    fprintf(file, "%s", header);
    
    Token token;
    while((token = next_token(&lexer)).type != TOKEN_EOF) {
        fprintf(file, "Token: ");
        switch (token.type) {
            case TOKEN_UNDEFINED: fprintf(file, "TOKEN_UNDEFINED"); break;
            case TOKEN_EOF: fprintf(file, "TOKEN_EOF"); break;
            case TOKEN_POSITIVE: fprintf(file, "TOKEN_POSITIVE"); break;
            case TOKEN_BIG: fprintf(file, "TOKEN_BIG"); break;
            case TOKEN_SMALL: fprintf(file, "TOKEN_SMALL"); break;
            case TOKEN_INT32: fprintf(file, "TOKEN_INT32"); break;
            case TOKEN_INT64: fprintf(file, "TOKEN_INT64"); break;
            case TOKEN_INT16: fprintf(file, "TOKEN_INT16"); break;
            case TOKEN_INT8: fprintf(file, "TOKEN_INT8"); break;
            case TOKEN_UINT8: fprintf(file, "TOKEN_UINT8"); break;
            case TOKEN_UINT16: fprintf(file, "TOKEN_UINT16"); break;
            case TOKEN_UINT32: fprintf(file, "TOKEN_UINT32"); break;
            case TOKEN_UINT64: fprintf(file, "TOKEN_UINT64"); break;
            case TOKEN_FLOAT: fprintf(file, "TOKEN_FLOAT"); break;
            case TOKEN_DOUBLE: fprintf(file, "TOKEN_DOUBLE"); break;
            case TOKEN_CHAR: fprintf(file, "TOKEN_CHAR"); break;
            case TOKEN_UTF16CHAR: fprintf(file, "TOKEN_UTF16CHAR"); break;
            case TOKEN_VOID: fprintf(file, "TOKEN_VOID"); break;
            case TOKEN_LINK: fprintf(file, "TOKEN_LINK"); break;
            case TOKEN_RETURN: fprintf(file, "TOKEN_RETURN"); break;
            case TOKEN_STATIC: fprintf(file, "TOKEN_STATIC"); break;
            case TOKEN_CONST: fprintf(file, "TOKEN_CONST"); break;
            case TOKEN_FOR: fprintf(file, "TOKEN_FOR"); break;
            case TOKEN_DO: fprintf(file, "TOKEN_DO"); break;
            case TOKEN_WHILE: fprintf(file, "TOKEN_WHILE"); break;
            case TOKEN_IF: fprintf(file, "TOKEN_IF"); break;
            case TOKEN_ELSE: fprintf(file, "TOKEN_ELSE"); break;
            case TOKEN_BREAK: fprintf(file, "TOKEN_BREAK"); break;
            case TOKEN_CONTINUE: fprintf(file, "TOKEN_CONTINUE"); break;
            case TOKEN_SWITCH: fprintf(file, "TOKEN_SWITCH"); break;
            case TOKEN_CASE: fprintf(file, "TOKEN_CASE"); break;
            case TOKEN_DATA: fprintf(file, "TOKEN_DATA"); break;
            case TOKEN_COPERATE: fprintf(file, "TOKEN_COPERATE"); break;
            case TOKEN_ENUM: fprintf(file, "TOKEN_ENUM"); break;
            case TOKEN_CREATETYPE: fprintf(file, "TOKEN_CREATETYPE"); break;
            case TOKEN_P: fprintf(file, "TOKEN_P"); break;
            case TOKEN_PEQ: fprintf(file, "TOKEN_PEQ"); break;
            case TOKEN_PP: fprintf(file, "TOKEN_PP"); break;
            case TOKEN_M: fprintf(file, "TOKEN_M"); break;
            case TOKEN_MM: fprintf(file, "TOKEN_MM"); break;
            case TOKEN_MEQ: fprintf(file, "TOKEN_MEQ"); break;
            case TOKEN_EQ: fprintf(file, "TOKEN_EQ"); break;
            case TOKEN_EQEQ: fprintf(file, "TOKEN_EQEQ"); break;
            case TOKEN_BANG: fprintf(file, "TOKEN_BANG"); break;
            case TOKEN_BANGEQ: fprintf(file, "TOKEN_BANGEQ"); break;
            case TOKEN_LT: fprintf(file, "TOKEN_LT"); break;
            case TOKEN_LE: fprintf(file, "TOKEN_LE"); break;
            case TOKEN_GT: fprintf(file, "TOKEN_GT"); break;
            case TOKEN_GE: fprintf(file, "TOKEN_GE"); break;
            case TOKEN_STAR: fprintf(file, "TOKEN_STAR"); break;
            case TOKEN_STAREQ: fprintf(file, "TOKEN_STAREQ"); break;
            case TOKEN_SLASH: fprintf(file, "TOKEN_SLASH"); break;
            case TOKEN_SLASHEQ: fprintf(file, "TOKEN_SLASHEQ"); break;
            case TOKEN_OR: fprintf(file, "TOKEN_OR"); break;
            case TOKEN_OREQ: fprintf(file, "TOKEN_OREQ"); break;
            case TOKEN_AND: fprintf(file, "TOKEN_AND"); break;
            case TOKEN_ANDEQ: fprintf(file, "TOKEN_ANDEQ"); break;
            case TOKEN_LOGAND: fprintf(file, "TOKEN_LOGAND"); break;
            case TOKEN_LOGOR: fprintf(file, "TOKEN_LOGOR"); break;
            case TOKEN_XOR: fprintf(file, "TOKEN_XOR"); break;
            case TOKEN_XOREQ: fprintf(file, "TOKEN_XOREQ"); break;
            case TOKEN_DESC: fprintf(file, "TOKEN_DESC"); break;
            case TOKEN_ARROW: fprintf(file, "TOKEN_ARROW"); break;
            case TOKEN_INF_ARGS: fprintf(file, "TOKEN_INF_ARGS"); break;
            case TOKEN_TERN: fprintf(file, "TOKEN_TERN"); break;
            case TOKEN_MOD: fprintf(file, "TOKEN_MOD"); break;
            case TOKEN_MODEQ: fprintf(file, "TOKEN_MODEQ"); break;
            case TOKEN_LBRACE: fprintf(file, "TOKEN_LBRACE"); break;
            case TOKEN_RBRACE: fprintf(file, "TOKEN_RBRACE"); break;
            case TOKEN_LBRACKET: fprintf(file, "TOKEN_LBRACKET"); break;
            case TOKEN_RBRACKET: fprintf(file, "TOKEN_RBRACKER"); break;
            case TOKEN_LPAREN: fprintf(file, "TOKEN_LPAREN"); break;
            case TOKEN_RPAREN: fprintf(file, "TOKEN_RPAREN"); break;
            case TOKEN_COMMA: fprintf(file, "TOKEN_COMMA"); break;
            case TOKEN_DOT: fprintf(file, "TOKEN_DOT"); break;
            case TOKEN_SEMICOLON: fprintf(file, "TOKEN_SEMICOLON"); break;
            case TOKEN_COLON: fprintf(file, "TOKEN_COLON"); break;
            case TOKEN_LSHIFT: fprintf(file, "TOKEN_LSHIFT"); break;
            case TOKEN_RSHIFT: fprintf(file, "TOKEN_RSHIFT"); break;
            case TOKEN_LSHIFTEQ: fprintf(file, "TOKEN_LSHIFTEQ"); break;
            case TOKEN_RSHIFTEQ: fprintf(file, "TOKEN_RSHIFTEQ"); break;
            case TOKEN_IDENTIFIER: fprintf(file, "TOKEN_IDENTIFIER"); break;
            case TOKEN_STRING_LIT: fprintf(file, "TOKEN_STRING_LIT"); break;
            case TOKEN_INT_LIT: fprintf(file, "TOKEN_INT_LIT"); break;
            case TOKEN_DOUBLE_LIT: fprintf(file, "TOKEN_DOUBLE_LIT"); break;
            case TOKEN_CHAR_LIT: fprintf(file, "TOKEN_DOUBLE_LIT"); break;
            default: break;
        }
        fprintf(file, " | Lexema: %.*s\n", (int)token.view.len, token.view.start);
    }

    fclose(file);
    fprintf(stdout, "Lexer log salvo em lexer_output.txt com sucesso!\n");
}

static char* content(const char* file) {
    FILE* f = fopen(file, "rb");
    if(!f) {
        fprintf(stderr, "Erro ao ler o arquivo");
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(len + 1);
    size_t bytes = fread(source, 1, len, f);
    if(bytes < len) {
        fprintf(stderr, "Erro ao ler o arquivo");
        exit(1);
    }

    source[bytes] = '\0';
    fclose(f);
    return source;
}

int main(int argc, char* argv[]) {
    if(argc < 3) {
        fprintf(stderr, "Por favor coloque algo apos o .exe como -L ou -N\n");
        return 1;
    }

    const char* src = content(argv[2]);
    
    Arena arena;
    arena_init(&arena, 1024 * 1024);
    DiagContext context;
    diag_init(&context, &arena);
    diag_set_max_errors(&context, 20);
    diag_register_source(&context, "./main.luegiu", src);
    Lexer lexer = create_lexer(src, "./main.luegiu", &context);
    if(strcmp(argv[1], "-L") == 0) {
        printlexer("=======LEXER=======\n", src, lexer);
    }

    if(has_error(&context)) {
        diag_report_all(&context);
        arena_free(&arena);
        return 1;
    }

    arena_free(&arena);
}
