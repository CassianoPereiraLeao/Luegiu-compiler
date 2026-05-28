#pragma once

#define ARENA_PIECE_SIZE (64 * 1024)

#include "common.h"

typedef struct Arena_Piece {
    size_t size;
    size_t capacity;
    struct Arena_Piece* next;
    uint8_t data[];
} Arena_Piece;

typedef struct {
    Arena_Piece* first;
    Arena_Piece* last;
} Arena;

void arena_init(Arena *arena);
void* arena_alloc(Arena *arena, size_t size);
void arena_free(Arena *arena);
