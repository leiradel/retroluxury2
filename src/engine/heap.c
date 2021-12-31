#include "heap.h"

#include <stdlib.h>

static void* rl2_libcAlloc(void* userdata, void* pointer, size_t size) {
    (void)userdata;

    if (pointer == NULL) {
        return malloc(size);
    }
    else if (size == 0) {
        free(pointer);
        return NULL;
    }
    else {
        return realloc(pointer, size);
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
