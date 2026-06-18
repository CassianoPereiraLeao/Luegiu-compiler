#include "diag.h"

#define DIAG_MAX_CONTEXTS 8

typedef struct SourceFile {
    const char* filename;
    const char* full_source;
    struct SourceFile* next;
} SourceFile;

typedef struct {
    DiagContext* ctx;
    SourceFile*  sources;
} ContextSources;

static ContextSources g_context_sources[DIAG_MAX_CONTEXTS];
static size_t g_context_sources_count = 0;

static ContextSources* find_or_create_slot(DiagContext *ctx) {
    for(size_t i = 0; i < g_context_sources_count; ++i)
        if(g_context_sources[i].ctx == ctx) return &g_context_sources[i];

    if(g_context_sources_count < DIAG_MAX_CONTEXTS) {
        ContextSources* slot = &g_context_sources[g_context_sources_count++];
        slot->ctx = ctx;
        slot->sources = NULL;
        return slot;
    }

    return NULL;
}

static const char* find_source(DiagContext *ctx, const char* filename) {
    if(!filename) return NULL;
    for(size_t i = 0; i < g_context_sources_count; ++i) {
        if(g_context_sources[i].ctx != ctx) continue;
        for(SourceFile* s = g_context_sources[i].sources; s; s = s->next)
            if(strcmp(s->filename, filename) == 0) return s->full_source;
    }
    return NULL;
}

static bool stdout_supports_color(void) {
    const char* term = getenv("TERM");
    const char* no_color = getenv("NO_COLOR");
    if(no_color) return false;
    if(!term) return false;
    if(strcmp(term, "dumb") == 0) return false;
    return true;
}

void diag_init(DiagContext *ctx, Arena *arena) {
    ctx->arena = arena;
    ctx->first = NULL;
    ctx->last = NULL;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->note_count = 0;
    ctx->max_errors = 0;
    ctx->warnings_as_errors = false;
    ctx->use_color = stdout_supports_color();
}

void diag_set_max_errors(DiagContext *ctx, size_t max_errors) {
    ctx->max_errors = max_errors;
}

void diag_set_warning_as_error(DiagContext *ctx, bool value) {
    ctx->warnings_as_errors = value;
}

void diag_register_source(DiagContext *ctx, const char* filename, const char* full_source) {
    if(!filename) return;

    ContextSources* slot = find_or_create_slot(ctx);
    if(!slot) return;

    for(SourceFile* s = slot->sources; s; s = s->next)
        if(strcmp(s->filename, filename) == 0) return;

    SourceFile* sf = (SourceFile*)arena_alloc(ctx->arena, sizeof(SourceFile));
    size_t flen = strlen(filename);
    char* fname_copy = (char*)arena_alloc(ctx->arena, flen + 1);
    memcpy(fname_copy, filename, flen + 1);

    sf->filename = fname_copy;
    sf->full_source = full_source;
    sf->next = slot->sources;
    slot->sources = sf;
}

static char* vformat(Arena *arena, const char* fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);

    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if(needed < 0) needed = 0;

    char* buf = (char*)arena_alloc(arena, (size_t)needed + 1);
    vsnprintf(buf, (size_t)needed + 1, fmt, args);
    return buf;
}

static char* extract_line(Arena *arena, const char* full_source, size_t target_line) {
    if(!full_source) return NULL;

    const char* cursor = full_source;
    size_t current_line = 1;

    while(current_line < target_line && *cursor) {
        if(*cursor == '\n') current_line++;
        cursor++;
    }

    if(current_line != target_line) return NULL;

    const char* line_start = cursor;
    while(*cursor && *cursor != '\n') cursor++;

    size_t len = (size_t)(cursor - line_start);
    char* out = (char*)arena_alloc(arena, len + 1);
    memcpy(out, line_start, len);
    out[len] = '\0';
    return out;
}

void diag_emit(DiagContext *ctx, DiagSeverity severity, const char* file,
            size_t line, size_t col, const char* code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* message = vformat(ctx->arena, fmt, args);
    va_end(args);

    Diagnostic* d = (Diagnostic*)arena_alloc(ctx->arena, sizeof(Diagnostic));
    d->severity = severity;
    d->loc.file = file;
    d->loc.line = line;
    d->loc.col = col;
    d->message = message;
    d->code = code;
    d->next = NULL;

    const char* source = find_source(ctx, file);
    d->source_line = extract_line(ctx->arena, source, line);

    if(!ctx->first) { ctx->first = d; ctx->last = d; }
    else { ctx->last->next = d; ctx->last = d; }

    switch(severity) {
    case NOTE:    ctx->note_count++; break;
    case WARNING:
        ctx->warning_count++;
        if(ctx->warnings_as_errors) ctx->error_count++;
        break;
    case ERROR:
    case FATAL:
        ctx->error_count++;
        break;
    }

    if(severity == FATAL) {
        diag_report_all(ctx);
        exit(1);
    }

    if(ctx->max_errors > 0 && ctx->error_count >= ctx->max_errors) {
        diag_report_all(ctx);
        fprintf(stderr, "\n(parando: limite de %zu erro(s) atingido)\n", ctx->max_errors);
        exit(1);
    }
}

void diag_note(DiagContext *ctx, const char* file, size_t line, size_t col, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    char* msg = vformat(ctx->arena, fmt, args);
    va_end(args);
    diag_emit(ctx, NOTE, file, line, col, NULL, "%s", msg);
}

void diag_warning(DiagContext *ctx, const char* file, size_t line, size_t col, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    char* msg = vformat(ctx->arena, fmt, args);
    va_end(args);
    diag_emit(ctx, WARNING, file, line, col, NULL, "%s", msg);
}

void diag_error(DiagContext *ctx, const char* file, size_t line, size_t col, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    char* msg = vformat(ctx->arena, fmt, args);
    va_end(args);
    diag_emit(ctx, ERROR, file, line, col, NULL, "%s", msg);
}

void diag_fatal(DiagContext *ctx, const char* file, size_t line, size_t col, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    char* msg = vformat(ctx->arena, fmt, args);
    va_end(args);
    diag_emit(ctx, FATAL, file, line, col, NULL, "%s", msg);
}

void diag_error_code(DiagContext *ctx, const char* file, size_t line, size_t col,
                    const char* code, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    char* msg = vformat(ctx->arena, fmt, args);
    va_end(args);
    diag_emit(ctx, ERROR, file, line, col, code, "%s", msg);
}

bool has_error(DiagContext *ctx) {
    return ctx->error_count > 0;
}

size_t diag_error_count(DiagContext *ctx) {
    return ctx->error_count;
}

size_t diag_warning_count(DiagContext *ctx) {
    return ctx->warning_count;
}

static const char* severity_label(DiagSeverity s) {
    switch(s) {
    case NOTE:    return "nota";
    case WARNING:  return "aviso";
    case ERROR:    return "erro";
    case FATAL:    return "erro fatal";
    }
    return "?";
}

static const char* severity_color(DiagSeverity s) {
    switch(s) {
    case NOTE:    return "\x1b[36m";
    case WARNING:  return "\x1b[33m";
    case ERROR:
    case FATAL:    return "\x1b[31m";
    }
    return "";
}

static void print_diagnostic(DiagContext *ctx, Diagnostic *d) {
    const char* reset = ctx->use_color ? "\x1b[0m" : "";
    const char* bold  = ctx->use_color ? "\x1b[1m" : "";
    const char* color = ctx->use_color ? severity_color(d->severity) : "";
    const char* dim    = ctx->use_color ? "\x1b[2m" : "";

    const char* file = d->loc.file ? d->loc.file : "<desconhecido>";

    fprintf(stderr, "%s%s%s:%zu:%zu:%s %s%s%s",
            bold, file, reset,
            d->loc.line, d->loc.col, reset,
            color, severity_label(d->severity), reset);

    if(d->code)
        fprintf(stderr, " [%s]", d->code);

    fprintf(stderr, ": %s%s%s\n", bold, d->message, reset);

    if(d->source_line) {
        fprintf(stderr, "  %s%5zu |%s %s\n", dim, d->loc.line, reset, d->source_line);
        fprintf(stderr, "        %s|%s ", dim, reset);

        size_t col = d->loc.col;
        for(size_t i = 1; i < col; ++i) {
            char c = (i - 1 < strlen(d->source_line)) ? d->source_line[i - 1] : ' ';
            putc(c == '\t' ? '\t' : ' ', stderr);
        }
        fprintf(stderr, "%s^%s\n", color, reset);
    }
}

size_t diag_report_all(DiagContext *ctx) {
    for(Diagnostic* d = ctx->first; d; d = d->next)
        print_diagnostic(ctx, d);

    if(ctx->error_count > 0 || ctx->warning_count > 0) {
        fprintf(stderr, "\n");
        if(ctx->error_count > 0)
            fprintf(stderr, "%zu erro(s)", ctx->error_count);
        if(ctx->error_count > 0 && ctx->warning_count > 0)
            fprintf(stderr, ", ");
        if(ctx->warning_count > 0)
            fprintf(stderr, "%zu aviso(s)", ctx->warning_count);
        fprintf(stderr, " gerados.\n");
    }

    return ctx->error_count;
}

void diag_reset(DiagContext *ctx) {
    ctx->first = NULL;
    ctx->last = NULL;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->note_count = 0;
}