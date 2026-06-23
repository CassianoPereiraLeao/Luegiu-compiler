#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32

void* VirtualAlloc(void* lpAddress, size_t dwSize, unsigned long flAllocationType, unsigned long flProtect);
int VirtualFree(void* lpAddress, size_t dwSize, unsigned long dwFreeType);

#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 0x04

static inline void* plataform_alloc(size_t size) {
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

static inline void plataform_free(void *ptr, size_t size) {
    VirtualFree(ptr, 0, MEM_RELEASE);
}
#endif