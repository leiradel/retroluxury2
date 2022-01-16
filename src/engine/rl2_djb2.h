#ifndef RL2_DJB2_H__
#define RL2_DJB2_H__

#include <inttypes.h>
#include <stddef.h>

#define RL2_PRI_DJB2HASH "0x%08" PRIx32

typedef uint32_t rl2_Djb2Hash;

rl2_Djb2Hash rl2_djb2(char const* str);

#endif // RL2_DJB2_H__
