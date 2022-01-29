#ifndef RL2_RAND_H__
#define RL2_RAND_H__

#include <inttypes.h>

typedef struct rl2_Rand* rl2_Rand;

rl2_Rand rl2_randCreate(uint64_t seed);
uint32_t rl2_rnd(rl2_Rand rand);
int rl2_rndInterval(rl2_Rand rand, int min, int max);

#endif // RL2_RAND_H__
