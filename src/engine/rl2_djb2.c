#include "rl2_djb2.h"

rl2_Djb2Hash rl2_djb2(char const* str) {
    rl2_Djb2Hash hash = 5381;

    while (*str != 0) {
        hash = hash * 33 + (uint8_t)*str++;
    }

    return hash;
}
