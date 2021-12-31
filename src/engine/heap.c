#include "heap.h"

static void* libc_Alloc(void* userdata, void* pointer, size_t size) {
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

static rl2_Allocf s_alloc = libcAlloc;
static void* s_userdata = NULL;

void rl2_setAlloc(rl2_Alloc alloc, void* userdata) {
    s_alloc = alloc;
    s_userdata = userdata;
}

void* rl2_Alloc(size_t size) {
    return s_alloc(NULL, size);
}

void rl2_Free(void* pointer) {
    s_alloc(pointer, 0);
}

void* rl2_Realloc(void* pointer, size_t size) {
    return s_alloc(pointer, size);
}
