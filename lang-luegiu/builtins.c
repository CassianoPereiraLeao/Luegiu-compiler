#include "builtins.h"

#define REGISTER_BUILTIN(name_str, ret_type, param_count) \
    do { \
        View name = { (name_str), strlen(name_str) }; \
        Symbol* sym = insert_table(table, name, SYMBOL_FUNC, 0, (ret_type)); \
        sym->params_count = (param_count); \
        if((param_count) > 0) { \
            sym->params = (ParamInfo*)arena_alloc(arena, sizeof(ParamInfo) * (param_count)); \
        } \
    } while(0)

void register_builtins(SymbolTable *table, Arena *arena) {
    // char* buffer, big out_len
    {
        REGISTER_BUILTIN("__write_stdout", TOKEN_KEYWORD_VOID, 2);
        Symbol* sym = lookup_table(table, (View){ "__write_stdout", 14 });
        sym->params[0].type = TOKEN_KEYWORD_CHAR;
        sym->params[0].pointer_lvl = 1;
        sym->params[1].type = TOKEN_KEYWORD_BIG;
        sym->params[1].pointer_lvl = 0;
    }

    // link handle, char* buffer, big len
    {
        REGISTER_BUILTIN("__write_file", TOKEN_KEYWORD_UINT64, 3);
        Symbol* sym = lookup_table(table, (View){ "__write_file", 12 });
        sym->params[0].type = TOKEN_KEYWORD_LINK;
        sym->params[0].pointer_lvl = 0;
        sym->params[1].type = TOKEN_KEYWORD_CHAR;
        sym->params[1].pointer_lvl = 1;
        sym->params[2].type = TOKEN_KEYWORD_BIG;
        sym->params[2].pointer_lvl = 0;
    }

    // link handle, char* buffer, big len
    {
        REGISTER_BUILTIN("__read_file", TOKEN_KEYWORD_UINT64, 3);
        Symbol* sym = lookup_table(table, (View){ "__read_file", 11 });
        sym->params[0].type = TOKEN_KEYWORD_LINK;
        sym->params[0].pointer_lvl = 0;
        sym->params[1].type = TOKEN_KEYWORD_CHAR;
        sym->params[1].pointer_lvl = 1;
        sym->params[2].type = TOKEN_KEYWORD_BIG;
        sym->params[2].pointer_lvl = 0;
    }

    // char* buffer, int mode
    {
        REGISTER_BUILTIN("__open_file", TOKEN_KEYWORD_LINK, 2);
        Symbol* sym = lookup_table(table, (View){ "__open_file", 11 });
        sym->params[0].type = TOKEN_KEYWORD_CHAR;
        sym->params[0].pointer_lvl = 1;
        sym->params[1].type = TOKEN_KEYWORD_INT;
        sym->params[1].pointer_lvl = 0;
    }

    {
        REGISTER_BUILTIN("__open_file_w", TOKEN_KEYWORD_LINK, 2);
        Symbol* sym = lookup_table(table, (View){ "__open_file_w", 13 });
        sym->params[0].type = TOKEN_KEYWORD_UTF16CHAR;
        sym->params[0].pointer_lvl = 1;
        sym->params[1].type = TOKEN_KEYWORD_INT;
        sym->params[1].pointer_lvl = 0;
    }

    // link handle
    {
        REGISTER_BUILTIN("__close_file", TOKEN_KEYWORD_VOID, 1);
        Symbol* sym = lookup_table(table, (View){"__close_file", 12});
        sym->params[0].type = TOKEN_KEYWORD_LINK;
        sym->params[0].pointer_lvl = 0;
    }

    // big value
    {
        REGISTER_BUILTIN("__write_int", TOKEN_KEYWORD_VOID, 1);
        Symbol* sym = lookup_table(table, (View){"__write_int", 11});
        sym->params[0].type = TOKEN_KEYWORD_BIG;
        sym->params[0].pointer_lvl = 0;
    }

    // utf16char* buffer, big len
    {
        REGISTER_BUILTIN("__write_stdout_w", TOKEN_KEYWORD_VOID, 2);
        Symbol* sym = lookup_table(table, (View){"__write_stdout_w", 16});
        sym->params[0].type = TOKEN_KEYWORD_UTF16CHAR;
        sym->params[0].pointer_lvl = 1;
        sym->params[1].type = TOKEN_KEYWORD_BIG;
        sym->params[1].pointer_lvl = 0;
    }

    // big size
    {
        REGISTER_BUILTIN("__malloc", TOKEN_KEYWORD_LINK, 1);
        Symbol* sym = lookup_table(table, (View){"__malloc", 8});
        sym->params[0].type = TOKEN_KEYWORD_BIG;
        sym->params[0].pointer_lvl = 0;
    }

    // link pointer
    {
        REGISTER_BUILTIN("__free", TOKEN_KEYWORD_VOID, 1);
        Symbol* sym = lookup_table(table, (View){"__free", 6});
        sym->params[0].type = TOKEN_KEYWORD_LINK;
        sym->params[0].pointer_lvl = 0;
    }

    // char** argc (start function)
    {
        REGISTER_BUILTIN("__parse_args", TOKEN_KEYWORD_INT, 1);
        Symbol* sym = lookup_table(table, (View){"__parse_args", 12});
        sym->params[0].type = TOKEN_KEYWORD_CHAR;
        sym->params[0].pointer_lvl = 2;
    }

    // utf16char** argc (start function)
    {
        REGISTER_BUILTIN("__parse_args_w", TOKEN_KEYWORD_INT, 1);
        Symbol* sym = lookup_table(table, (View){"__parse_args_w", 14});
        sym->params[0].type = TOKEN_KEYWORD_UTF16CHAR;
        sym->params[0].pointer_lvl = 2;
    }

    // int code
    {
        REGISTER_BUILTIN("__exit", TOKEN_KEYWORD_VOID, 1);
        Symbol* sym = lookup_table(table, (View){"__exit", 6});
        sym->params[0].type = TOKEN_KEYWORD_INT;
        sym->params[0].pointer_lvl = 0;
    }

    // link args (ponteiro pro slot atual)
    {
        REGISTER_BUILTIN("__va_start", TOKEN_KEYWORD_LINK, 0);
    }

    // link args
    {
        REGISTER_BUILTIN("__va_next", TOKEN_KEYWORD_LINK, 1);
        Symbol* sym = lookup_table(table, (View){"__va_next", 9});
        sym->params[0].type = TOKEN_KEYWORD_LINK;
        sym->params[0].pointer_lvl = 0;
    }
}
