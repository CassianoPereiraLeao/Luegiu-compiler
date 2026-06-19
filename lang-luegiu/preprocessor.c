#include "preprocessor.h"

static IncludedFile* g_included = NULL;
static Arena* g_included_arena = NULL;

static void included_init(Arena *arena) {
    g_included = NULL;
    g_included_arena = arena;
}

static bool included_check(const char* path) {
    for(IncludedFile* file = g_included; file; file = file->next) {
        if(strcmp(file->path, path) == 0) {
            return true;
        }
    }
    return false;
}

static void included_add(const char* path) {
    IncludedFile* file = (IncludedFile*)arena_alloc(g_included_arena, sizeof(IncludedFile));
    size_t len = strlen(path);
    char* copy = (char*)arena_alloc(g_included_arena, len + 1);
    memcpy(copy, path, len + 1);
    file->path = copy;
    file->next = g_included;
    g_included = file;
}

static char* get_dir(const char* filepath, Arena *arena) {
    const char* last_sep = NULL;

    for(const char* peek = filepath; *peek; peek++) {
        if(*peek == '/' || *peek == '\\') last_sep = peek;
    }

    if(!last_sep) {
        char* dot = (char*)arena_alloc(arena, 2);
        dot[0] = '.';
        dot[1] = '\0';
        return dot;
    }

    size_t len = last_sep - filepath;
    char* dir = (char*)arena_alloc(arena, len + 1);
    memcpy(dir, filepath, len);
    dir[len] = '\0';
    return dir;
}

static bool is_identifier_char(char c) {
    return (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_';
}

static char* read_file(const char* path, Arena *arena) {
    FILE* f = fopen(path, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = (char*)arena_alloc(arena, len + 1);
    size_t bytes = fread(buffer, sizeof(char), len, f);
    buffer[bytes] = '\0';
    fclose(f);
    return buffer;
}

static char* join_path(const char* dir, const char* file, Arena *arena) {
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    char* path = (char*)arena_alloc(arena, dir_len + file_len + 2);
    memcpy(path, dir, dir_len);
    if(dir_len > 0 && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\')
        path[dir_len++] = '/';
    memcpy(path + dir_len, file, file_len);
    path[dir_len + file_len] = '\0';
    return path;
}

static char* preprocess_internal(const char* source, const char* base_dir, const char* lib_dir, Arena *arena, IncludePolicy policy);

static IncludePolicy parse_use_directive(const char* source, const char** out) {
    IncludePolicy policy = INCLUDE_MULTIPLE;

    const char* peek = source;

    while(*peek == ' ' || *peek == '\t') peek++;

    if(strncmp(peek, "target", 6) != 0 || is_identifier_char(peek[6])) {
        *out = source;
        return INCLUDE_MULTIPLE;
    }

    peek += 6;

    while(*peek == ' ' || *peek == '\t') peek++;
    if(*peek != '-' || *(peek+1) != '>') {
        *out = source;
        return INCLUDE_MULTIPLE;
    }

    peek += 2;

    while(*peek == ' ' || *peek == '\t') peek++;
    if(strncmp(peek, "single", 6) == 0 && !is_identifier_char(peek[6])) {
        policy = INCLUDE_SINGLE;
        peek += 6;
    } else if(strncmp(peek, "multiple", 8) == 0 && !is_identifier_char(peek[8])) {
        policy = INCLUDE_MULTIPLE;
        peek += 8;
    } else {
        fprintf(stderr, "Preprocessador: argumento invalido em 'use target'. "
                        "Use 'single' ou 'multiple'\n");
        exit(1);
    }

    while(*peek && *peek != '\n') peek++;
    if(*peek == '\n') peek++;

    *out = peek;
    return policy;
}

static char* expand_extracts(const char* src, const char* base_dir, const char* lib_dir, Arena *arena, IncludePolicy policy) {
    // fprintf(stderr, "=== EXPAND_MACROS INPUT ===\n%s\n=== END ===\n", src);
    size_t src_len = strlen(src);
    size_t cap = src_len * 64 + 65536;
    char* out = (char*)arena_alloc(arena, cap);
    char* w = out;

    const char* source = src;
    IncludePolicy file_policy = policy;

    #define WRITE(str, len) do { \
        size_t _len = (len); \
        if((size_t)(w - out) + _len + 1 >= cap) { \
            fprintf(stderr, "Preprocessador: buffer de expansao estourado\n"); \
            exit(1); \
        } \
        memcpy(w, (str), _len); \
        w += _len; \
    } while(0)

    while(*source) {
        const char* line_start = source;
        while(*source == ' ' || *source == '\t') source++;

        if(strncmp(source, "use", 3) == 0 && !is_identifier_char(source[3])) {
            source += 3;
            while (*source == ' ' || *source == '\t') source++;
            
            const char* after = source;
            IncludePolicy p = parse_use_directive(source, &after);
            if(after != source) {
                file_policy = p;
                source = after;
                *w++ = '\n';
                continue;
            }
            source = line_start;
        }

        if(strncmp(source, "extract", 7) == 0 && !is_identifier_char(source[7])) {
            source += 7;
            while(*source == ' ' || *source == '\t') source++;

            if(*source == '-' && *(source + 1) == '>') source += 2;
            while(*source == ' ' || *source == '\t') source++;

            char* resolved_path = NULL;
            char* file_src = NULL;
            const char* new_base_dir = base_dir;

            if(*source == '"') {
                source++;
                const char* start = source;
                while(*source && *source != '"' && *source != '\n') source++;
                size_t len = source - start;
                if(*source == '"') source++;

                char* filename = (char*)arena_alloc(arena, len + 1);
                memcpy(filename, start, len);
                filename[len] = '\0';

                resolved_path = join_path(base_dir, filename, arena);
                file_src = read_file(resolved_path, arena);
                new_base_dir = get_dir(resolved_path, arena);

                if(!file_src) {
                    fprintf(stderr, "Preprocessador: arquivo nao encontrado: '%s'\n", resolved_path);
                    exit(1);
                }
            } else if(*source == '[') {
                source++;
                const char* start = source;
                while (*source && *source != ']' && *source != '\n') source++;
                size_t len = source - start;
                if(*source == ']') source++;

                char* filename = (char*)arena_alloc(arena, len + 1);
                memcpy(filename, start, len);
                filename[len] = '\0';

                resolved_path = join_path(base_dir, filename, arena);
                file_src = read_file(resolved_path, arena);
                new_base_dir = get_dir(resolved_path, arena);

                if(!file_src) {
                    fprintf(stderr, "Preprocessador: arquivo nao encontrado: '%s'\n", resolved_path);
                    exit(1);
                }
            } else {
                fprintf(stderr, "Preprocessador: sintaxe invalida em 'extract'. "
                                "Use '\"arquivo.lh\"' ou '[stdlib]'\n");
                exit(1);
            }

            if(file_policy == INCLUDE_SINGLE || policy == INCLUDE_SINGLE) {
                if(included_check(resolved_path)) {
                    while(*source && *source != '\n') source++;
                    if(*source == '\n') source++;
                    *w++ = '\n';
                    continue;
                }
                included_add(resolved_path);
            }

            char* extracted = preprocess_internal(file_src, new_base_dir, lib_dir, arena, INCLUDE_MULTIPLE);
            size_t extract_len = strlen(extracted);
            WRITE(extracted, extract_len);
            *w++ = '\n';

            while(*source && *source != '\n') source++;
            if(*source == '\n') source++;
        } else {
            source = line_start;
            while(*source && *source != '\n') *w++ = *source++;
            if(*source && *source == '\n') *w++ = *source++;
        }
    }

    *w = '\0';
    #undef WRITE
    return out;
}

static Macro* collect_macros(const char* src, char* out, Arena *arena) {
    Macro* head = NULL;
    const char* source = src;
    char* w = out;

    while(*source) {
        const char* line_start = source;
        while(*source == ' ' || *source == '\t') source++;

        const char* keyword = "preprocess";
        size_t keyword_len = 10;
        bool is_preprocess = (strncmp(source, keyword, keyword_len) == 0 && !is_identifier_char(source[keyword_len]));

        if(is_preprocess) {
            source += keyword_len;
            while (*source == ' ' || *source == '\t') source++;

            const char* start = source;
            while(is_identifier_char(*source)) source++;
            size_t len = source - start;

            char* name_copy = (char*)arena_alloc(arena, len + 1);
            memcpy(name_copy, start, len);
            name_copy[len] = '\0';

            while (*source == ' ' || *source == '\t') source++;
            if (*source == '-' && *(source + 1) == '>') source += 2;
            while (*source == ' ' || *source == '\t') source++;

            const char* value_start = source;
            while(*source && *source != '\n') source++;
            size_t value_len = source - value_start;

            while(value_len > 0 && value_start[value_len - 1] == ' ') value_len--;
            while(value_len > 0 && value_start[value_len - 1] == '\r') value_len--;

            char* value_copy = (char*)arena_alloc(arena, value_len + 1);
            memcpy(value_copy, value_start, value_len);
            value_copy[value_len] = '\0';

            Macro* macro = (Macro*)arena_alloc(arena, sizeof(Macro));
            macro->name = name_copy;
            macro->name_len = len;
            macro->value = value_copy;
            macro->value_len = value_len;
            macro->next = head;
            head = macro;

            *w++ = '\n';
            if(*source == '\n') source++;
        } else {
            source = line_start;
            while (*source && *source != '\n') *w++ = *source++;
            if (*source == '\n') *w++ = *source++;
        }
    }

    *w = '\0';
    return head;
}

static char* expand_macros(const char* src, Macro *macros, Arena *arena) {
    fprintf(stderr, "=== EXPAND_MACROS INPUT ===\n%s\n=== END ===\n", src);
    size_t needed = 0;
    const char* s = src;
    while(*s) {
        if(*s == '"') {
            needed++;
            s++;
            while(*s && *s != '"') {
                if(*s == '\\' && *(s+1)) { needed++; s++; }
                needed++; s++;
            }
            if(*s) { needed++; s++; }
            continue;
        }
        if(*s == '\'' ) {
            needed++;
            s++;
            while(*s && *s != '\'') {
                if(*s == '\\' && *(s+1)) { needed++; s++; }
                needed++; s++;
            }
            if(*s) { needed++; s++; }
            continue;
        }
        if(is_identifier_char(*s) && !(*s >= '0' && *s <= '9')) {
            const char* start = s;
            while(is_identifier_char(*s)) s++;
            size_t len = s - start;
            bool found = false;
            for(Macro* m = macros; m; m = m->next) {
                if(m->name_len == len && strncmp(m->name, start, len) == 0) {
                    needed += m->value_len;
                    found = true;
                    break;
                }
            }
            if(!found) needed += len;
            continue;
        }
        needed++;
        s++;
    }

    char* out = (char*)arena_alloc(arena, needed + 1);
    char* w = out;
    const char* source = src;

    while(*source) {
        if(*source == '"') {
            *w++ = *source++;
            while(*source && *source != '"') {
                if(*source == '\\' && *(source+1)) {
                    *w++ = *source++;
                }
                *w++ = *source++;
            }
            if(*source) *w++ = *source++;
            continue;
        }

        if(*source == '\'') {
            *w++ = *source++;
            while(*source && *source != '\'') {
                if(*source == '\\' && *(source+1)) *w++ = *source++;
                *w++ = *source++;
            }
            if(*source) *w++ = *source++;
            continue;
        }

        if(*source == '/' && *(source+1) == '/') {
            while(*source && *source != '\n') *w++ = *source++;
            continue;
        }

        if(strncmp(source, "/comment", 8) == 0 && !is_identifier_char(source[8])) {
            source += 8;
            const char* end_marker = "endcomment/";
            size_t end_marker_len = 11;
            while(*source && strncmp(source, end_marker, end_marker_len) != 0) source++;
            if(*source) {
                source += end_marker_len;
            } else {
                fprintf(stderr, "Preprocessador: comentario de bloco nao terminado "
                                "(esperado 'endcomment/')\n");
                exit(1);
            }
            continue;
        }
        
        if(is_identifier_char(*source) && !(*source >= '0' && *source <= '9')) {
            const char* start = source;
            while(is_identifier_char(*source)) source++;
            size_t len = source - start;

            Macro* found = NULL;
            for(Macro* macro = macros; macro; macro = macro->next) {
                if(macro->name_len == len && strncmp(macro->name, start, len) == 0) {
                    found = macro;
                    break;
                }
            }

            if(found) {
                memcpy(w, found->value, found->value_len);
                w += found->value_len;
            } else {
                memcpy(w, start, len);
                w += len;
            }
        } else {
            *w++ = *source++;
        }
    }

    *w = '\0';
    fprintf(stderr, "=== EXPAND_MACROS OUTPUT ===\n%s\n=== END ===\n", src);
    return out;
}

static char* expand_macros_recursive(const char* src, Macro *macros, Arena *arena) {
    char* result = (char*)src;
    for(int pass = 0; pass < 10; ++pass) {
        char* next = expand_macros(result, macros, arena);
        if(strcmp(next, result) == 0) break;
        result = next;
    }

    return result;
}

static char* preprocess_internal(const char* source, const char* base_dir, const char* lib_dir, Arena *arena, IncludePolicy policy) {
    return expand_extracts(source, base_dir, lib_dir, arena, policy);
}

char* preprocess_source(const char* source, const char* base_dir, const char* lib_dir, Arena *arena) {
    included_init(arena);
    char* fully_extracted = preprocess_internal(source, base_dir, lib_dir, arena, INCLUDE_MULTIPLE);

    size_t len = strlen(fully_extracted);
    char* stripped = (char*)arena_alloc(arena, len + 1);
    Macro* macros = collect_macros(fully_extracted, stripped, arena);

    if(!macros) return stripped;
    return expand_macros_recursive(stripped, macros, arena);
}