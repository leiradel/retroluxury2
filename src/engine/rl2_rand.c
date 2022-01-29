#include "rl2_rand.h"
#include "rl2_heap.h"
#include "rl2_log.h"

#define TAG "RND "

struct rl2_Rand {
  uint64_t seed;
};

rl2_Rand rl2_randCreate(uint64_t seed) {
    rl2_Rand rand = (rl2_Rand)rl2_alloc(sizeof(*rand));

    if (rand == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    rand->seed = seed;
    return rand;
}

uint32_t rl2_rnd(rl2_Rand rand) {
      /*
      http://en.wikipedia.org/wiki/Linear_congruential_generator
      Newlib parameters
      */
  
    rand->seed = UINT64_C(6364136223846793005) * rand->seed + 1;
    return rand->seed >> 32;
}

int rl2_rndInterval(rl2_Rand rand, int min, int max) {
    int64_t const dividend = max - min + 1;
    return (int)(((dividend * rl2_rnd(rand)) >> 32) + min);
}
