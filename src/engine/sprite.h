#ifndef HH2_SPRITE_H__
#define HH2_SPRITE_H__

#include "image.h"

typedef struct hh2_Sprite* hh2_Sprite;

hh2_Sprite hh2_createSprite(void);
void hh2_destroySprite(hh2_Sprite sprite);

void hh2_setPosition(hh2_Sprite sprite, int x, int y);
void hh2_setLayer(hh2_Sprite sprite, unsigned layer);
bool hh2_setImage(hh2_Sprite sprite, hh2_Image image);
void hh2_setVisibility(hh2_Sprite sprite, bool visible);

void hh2_blitSprites(hh2_Canvas const canvas);
void hh2_unblitSprites(hh2_Canvas const canvas);

#endif // HH2_SPRITE_H__
