#ifndef RL2_HEAP_H__
#define RL2_HEAP_H__

#include <stddef.h>

typedef void* (*rl2_Allocf)(void* userdata, void* pointer, size_t size);

void rl2_setAlloc(rl2_Allocf alloc, void* userdata);

void* rl2_alloc(size_t size);
void rl2_free(void* pointer);
void* rl2_realloc(void* pointer, size_t size);

#endif // RL2_HEAP_H__
