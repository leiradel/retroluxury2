#ifndef RL2_SPRITE_H__
#define RL2_SPRITE_H__

#include "rl2_image.h"

typedef struct rl2_Sprite* rl2_Sprite;

rl2_Sprite rl2_createSprite(void);
void rl2_destroySprite(rl2_Sprite const sprite);

void rl2_setPosition(rl2_Sprite const sprite, int const x, int const y);
void rl2_setLayer(rl2_Sprite const sprite, unsigned const layer);
bool rl2_setImage(rl2_Sprite const sprite, rl2_Image const image);
void rl2_setVisibility(rl2_Sprite const sprite, bool const visible);

void rl2_blitSprites(rl2_Canvas const canvas);
void rl2_unblitSprites(rl2_Canvas const canvas);

#endif // RL2_SPRITE_H__
