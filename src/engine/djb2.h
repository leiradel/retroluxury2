#ifndef HH2_DJB2_H__
#define HH2_DJB2_H__

#include <inttypes.h>

#define HH2_PRI_DJB2HASH "0x%08" PRIx32

typedef uint32_t hh2_Djb2Hash;

hh2_Djb2Hash hh2_djb2(char const* str);

#endif // HH2_DJB2_H__
