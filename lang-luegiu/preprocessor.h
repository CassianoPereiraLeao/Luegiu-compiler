#pragma once

#include "common.h"
#include "arena.h"

typedef enum {
    INCLUDE_SINGLE,
    INCLUDE_MULTIPLE
} IncludePolicy;

typedef struct IncludedFile {
    const char* path;
    struct IncludedFile* next;
} IncludedFile;

typedef struct Macro {
    const char* name;
    size_t name_len;
    const char* value;
    size_t value_len;
    struct Macro* next;
} Macro;

char* preprocess_source(const char* source, const char* base_dir, const char* lib_dir, Arena *arena);
