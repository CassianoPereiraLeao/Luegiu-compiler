#pragma once

#define WIN64_SHADOW_SPACE 32
#define WIN64_INT_PARAMS 4
#define CODEGEN_MAX_LOOP_DEPTH 64
#define CODEGEN_MAX_BREAK_DEPTH 64

#include "common.h"
#include "parser.h"
#include "arena.h"

static const char* const WIN64_INT_REGISTERS[WIN64_INT_PARAMS] = {
    "rcx", "rdx", "r8", "r9"
};

typedef enum {
    BREAK_CTX_LOOP,
    BREAK_CTX_SWITCH
} BreakCtxType;

typedef struct {
    BreakCtxType type;
    int label_end;
} BreakCtx;

typedef struct {
    char* data;
    size_t len;
    size_t capacity;
    Arena* arena;
} CodeGenBuffer;

typedef struct CodeGenLocal {
    View name;
    View typename;
    int offset;
    TokenType type;
    size_t pointer_lvl;
    bool is_implicit_ptr;
    struct CodeGenLocal* next;
} CodeGenLocal;

typedef struct CodeGenString {
    View text;
    size_t index;
    struct CodeGenString* next;
} CodeGenString;

typedef struct {
    int label_cond;
    int label_end;
} LoopFrame;

typedef struct {
    CodeGenBuffer sec_data;
    CodeGenBuffer sec_bss;
    CodeGenBuffer sec_text;
    CodeGenBuffer sec_init;

    Arena* arena;
    SymbolTable* table;
    const char* entry_name;

    CodeGenString* strings;
    size_t string_count;

    BreakCtx break_stack[CODEGEN_MAX_BREAK_DEPTH];
    int break_depth;

    CodeGenLocal* locals;
    int stack_top;

    int label_counter;

    LoopFrame loop_stack[CODEGEN_MAX_LOOP_DEPTH];
    int loop_depth;

    bool in_func;
    bool entry_emmited;

    bool has_va_reg_save;
    int  va_reg_save_offset;
} CodeGen;

void codegen_buffer_init(CodeGenBuffer *buffer, Arena *arena, size_t initial_capacity);
void codegen_buffer_write(CodeGenBuffer *buffer, const char* source, size_t len);
void codegen_buffer_writez(CodeGenBuffer *buffer, const char* source);
void codegen_buffer_writec(CodeGenBuffer *buffer, char c);
void codegen_buffer_writei(CodeGenBuffer *buffer, long long value);
void codegen_buffer_writeu(CodeGenBuffer *buffer, size_t value);
void codegen_init(CodeGen *codegen, Arena *arena, SymbolTable *table);
void codegen_program(CodeGen *codegen, Node *program);
CodeGenBuffer codegen_finalize(CodeGen *codegen);
