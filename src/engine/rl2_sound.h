#ifndef HH2_SOUND_H__
#define HH2_SOUND_H__

#include "filesys.h"

#include <stdint.h>

typedef struct hh2_Pcm* hh2_Pcm;

hh2_Pcm hh2_readPcm(hh2_Filesys filesys, char const* path);
void hh2_destroyPcm(hh2_Pcm pcm);

bool hh2_playPcm(hh2_Pcm pcm);
void hh2_stopPcms(void);

int16_t const* hh2_soundMix(size_t* const frames);

#endif // HH2_SOUND_H__
