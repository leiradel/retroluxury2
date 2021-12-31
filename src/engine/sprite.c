#include "sprite.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

#define TAG "SPT "

#define HH2_MIN_SPRITES 64

typedef enum {
    HH2_SPRITE_INVISIBLE = 0x4000U,
    HH2_SPRITE_DESTROY = 0x8000U,
    HH2_SPRITE_FLAGS = HH2_SPRITE_INVISIBLE | HH2_SPRITE_DESTROY,
    HH2_SPRITE_LAYER = ~HH2_SPRITE_FLAGS
}
hh2_SpriteFlags;

struct hh2_Sprite {
    hh2_Image image;
    hh2_RGB565* bg;

    int x;
    int y;

    uint16_t flags; // Sprite layer [0..16383] | visibility << 14 | << unused << 15
};

static hh2_Sprite* hh2_sprites = NULL;
static size_t hh2_spriteCount = 0;
static size_t hh2_reservedSprites = 0;
static size_t hh2_visibleSpriteCount = 0;

hh2_Sprite hh2_createSprite(void) {
    hh2_Sprite sprite = (hh2_Sprite)malloc(sizeof(*sprite));

    if (sprite == NULL) {
        HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
        return NULL;
    }

    if (hh2_spriteCount == hh2_reservedSprites) {
        size_t const new_reserved = hh2_reservedSprites == 0 ? HH2_MIN_SPRITES : hh2_reservedSprites * 2;
        hh2_Sprite* const new_entries = realloc(hh2_sprites, sizeof(hh2_Sprite) * new_reserved);

        if (new_entries == NULL) {
            HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
            free(sprite);
            return NULL;
        }

        hh2_reservedSprites = new_reserved;
        hh2_sprites = new_entries;
    }

    sprite->image = NULL;
    sprite->bg = NULL;
    sprite->x = sprite->y = 0;
    sprite->flags = HH2_SPRITE_INVISIBLE;

    hh2_sprites[hh2_spriteCount++] = sprite;
    return sprite;
}

void hh2_destroySprite(hh2_Sprite const sprite) {
    sprite->flags = HH2_SPRITE_DESTROY;
}

void hh2_setPosition(hh2_Sprite const sprite, int x, int y) {
    sprite->x = x;
    sprite->y = y;
}

void hh2_setLayer(hh2_Sprite const sprite, unsigned const layer) {
    sprite->flags = (sprite->flags & HH2_SPRITE_FLAGS) | (layer & HH2_SPRITE_LAYER);
}

bool hh2_setImage(hh2_Sprite const sprite, hh2_Image const image) {
    if (image == sprite->image) {
        return true;
    }

    hh2_RGB565* bg = NULL;

    if (image != NULL) {
        size_t const count = hh2_changedPixels(image);
        bg = (hh2_RGB565*)malloc(count * sizeof(*sprite->bg));

        if (bg == NULL) {
            HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
            return false;
        }
    }

    free(sprite->bg);

    sprite->image = image;
    sprite->bg = bg;
    return true;
}

void hh2_setVisibility(hh2_Sprite const sprite, bool const visible) {
    sprite->flags = (HH2_SPRITE_INVISIBLE * !visible) | (sprite->flags & HH2_SPRITE_LAYER);
}

static int hh2_compareSprites(void const* e1, void const* e2) {
    hh2_Sprite const s1 = *(hh2_Sprite*)e1;
    hh2_Sprite const s2 = *(hh2_Sprite*)e2;

    uint16_t const f1 = s1->flags | (HH2_SPRITE_INVISIBLE * (s1->image == NULL));
    uint16_t const f2 = s2->flags | (HH2_SPRITE_INVISIBLE * (s2->image == NULL));

    if (f1 == f2) {
        return 0;
    }
    else if (f1 < f2) {
        return -1;
    }
    else {
        return 1;
    }
}

void hh2_blitSprites(hh2_Canvas const canvas) {
    if (hh2_spriteCount == 0) {
        return;
    }

    qsort(hh2_sprites, hh2_spriteCount, sizeof(*hh2_sprites), hh2_compareSprites);

    size_t i = 0;
    hh2_Sprite sprite = hh2_sprites[0];

    // Blit all sprites not invisible and not marked for destruction
    if (i < hh2_spriteCount && (sprite->flags & HH2_SPRITE_FLAGS) == 0 && sprite->image != NULL) {
        do {
            hh2_blit(sprite->image, canvas, sprite->x, sprite->y, sprite->bg);
            sprite = hh2_sprites[++i];
        }
        while (i < hh2_spriteCount && (sprite->flags & HH2_SPRITE_FLAGS) == 0 && sprite->image != NULL);
    }

    hh2_visibleSpriteCount = i;

    // Skip invisible sprites
    if (i < hh2_spriteCount && ((sprite->flags & HH2_SPRITE_FLAGS) == HH2_SPRITE_INVISIBLE || sprite->image == NULL)) {
        do {
            sprite = hh2_sprites[++i];
        }
        while (i < hh2_spriteCount && ((sprite->flags & HH2_SPRITE_FLAGS) == HH2_SPRITE_INVISIBLE || sprite->image == NULL));
    }

    // Destroy all remaining sprites
    size_t const new_count = i;

    if (i < hh2_spriteCount) {
        do {
            free(sprite->bg);
            free(sprite);
            sprite = hh2_sprites[++i];
        }
        while (i < hh2_spriteCount);
    }

    hh2_spriteCount = new_count;
}

void hh2_unblitSprites(hh2_Canvas const canvas) {
    if (hh2_visibleSpriteCount == 0) {
        return;
    }

    size_t i = hh2_visibleSpriteCount;
    hh2_Sprite sprite = hh2_sprites[i - 1];

    do {
        hh2_unblit(sprite->image, canvas, sprite->x, sprite->y, sprite->bg);
        sprite = hh2_sprites[--i - 1];
    }
    while (i > 0);
}
