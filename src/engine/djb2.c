#include "djb2.h"

hh2_Djb2Hash hh2_djb2(char const* str) {
    hh2_Djb2Hash hash = 5381;

    while (*str != 0) {
        hash = hash * 33 + (uint8_t)*str++;
    }

    return hash;
}
