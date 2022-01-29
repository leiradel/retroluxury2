#ifndef RL2_SOUND_H__
#define RL2_SOUND_H__

#include "rl2_filesys.h"

#include <stdint.h>

typedef struct rl2_Sound* rl2_Sound;
typedef struct rl2_Voice* rl2_Voice;

typedef void (*rl2_Finished)(rl2_Voice const);

rl2_Sound rl2_readSound(rl2_Filesys const filesys, char const* const path);
void rl2_destroySound(rl2_Sound const sound);

rl2_Voice rl2_play(rl2_Sound const sound, uint8_t const volume, bool const repeat, rl2_Finished finished_cb);
void rl2_setVolume(rl2_Voice const voice, uint8_t const volume);
void rl2_stop(rl2_Voice const voice);

void rl2_stopAll(void);

int16_t const* rl2_soundMix(size_t* const num_frames);

#endif // RL2_SOUND_H__
