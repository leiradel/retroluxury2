#ifndef RL2_SOUND_H__
#define RL2_SOUND_H__

#include "rl2_filesys.h"

#include <stdint.h>

bool rl2_playMusic(char const* const path, unsigned const max_height, uint8_t const volume);
bool rl2_pauseMusic(void);
bool rl2_resumeMusic(void);
bool rl2_rewindMusic(void);
bool rl2_stopMusic(void);

bool rl2_playSound(char const* const path, unsigned const max_height);

void rl2_stopAll(void);

#endif // RL2_SOUND_H__
