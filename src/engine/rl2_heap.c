#include "rl2_heap.h"
#include "rl2_log.h"

#include <stdlib.h>

#define TAG "MEM "

static void* rl2_libcAlloc(void* userdata, void* pointer, size_t size) {
    (void)userdata;

    if (pointer == NULL) {
        pointer = malloc(size);
        RL2_DEBUG(TAG "allocated %zu bytes at %p", size, pointer);
        return pointer;
    }
    else if (size == 0) {
        free(pointer);
        RL2_DEBUG(TAG "freed %p", pointer);
        return NULL;
    }
    else {
        void* new_pointer = realloc(pointer, size);
        RL2_DEBUG(TAG "reallocated %p to %p with %zu", pointer, new_pointer, size);
        return new_pointer;
    }
}

static rl2_Allocf rl2_heapAlloc = rl2_libcAlloc;
static void* rl2_heapUserdata = NULL;

void rl2_setAlloc(rl2_Allocf alloc, void* userdata) {
    rl2_heapAlloc = alloc;
    rl2_heapUserdata = userdata;
}

void* rl2_alloc(size_t size) {
    return rl2_heapAlloc(rl2_heapUserdata, NULL, size);
}

void rl2_free(void* pointer) {
    rl2_heapAlloc(rl2_heapUserdata, pointer, 0);
}

void* rl2_realloc(void* pointer, size_t size) {
    return rl2_heapAlloc(rl2_heapUserdata, pointer, size);
}
