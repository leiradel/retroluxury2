#ifndef RL2_HEAP_H__
#define RL2_HEAP_H__

typedef void* (*rl2_Allocf)(void* userdata, void* pointer, size_t size);

void rl2_setAlloc(rl2_Allocf alloc, void* userdata);

void* rl2_Alloc(size_t size);
void rl2_Free(void* pointer);
void* rl2_Realloc(void* pointer, size_t size);

#endif // RL2_HEAP_H__
