#include "arena.h"

void arena_init(Arena *arena) {
    arena->first = NULL;
    arena->last = NULL;
}

static Arena_Piece* create_piece(size_t size) {
    size_t capacity = size > ARENA_PIECE_SIZE ? size : ARENA_PIECE_SIZE;
    Arena_Piece* piece = (Arena_Piece*)malloc(sizeof(Arena_Piece) + capacity);
    if(!piece) return NULL;

    piece->next = NULL;
    piece->size = 0;
    piece->capacity = capacity;
    return piece;
}

void* arena_alloc(Arena *arena, size_t size) {
    if (arena == NULL || size == 0) return NULL;
    size = (size + 7)& ~7;

    if(!arena->last || arena->last->size + size > arena->last->capacity) {
        Arena_Piece* piece = create_piece(size);
        if(!piece) return NULL;

        if(!arena->first) {
            arena->first = piece;
            arena->last = piece;
        } else {
            arena->last->next = piece;
            arena->last = piece;
        }
    }

    void* ptr = &arena->last->data[arena->last->size];
    arena->last->size += size;
    return ptr;
}

void arena_free(Arena *arena) {
    Arena_Piece* current = arena->first;
    while(current) {
        Arena_Piece* next = current->next;
        free(current);
        current = next;
    }
    arena->first = NULL;
    arena->last = NULL;
}
