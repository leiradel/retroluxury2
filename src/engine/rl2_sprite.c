#include "rl2_sprite.h"
#include "rl2_log.h"
#include "rl2_heap.h"

#include <stdlib.h>
#include <string.h>

#define TAG "SPT "

#define RL2_MIN_SPRITES 64

typedef enum {
    RL2_SPRITE_INVISIBLE = 0x4000U,
    RL2_SPRITE_DESTROY = 0x8000U,
    RL2_SPRITE_FLAGS = RL2_SPRITE_INVISIBLE | RL2_SPRITE_DESTROY,
    RL2_SPRITE_LAYER = ~RL2_SPRITE_FLAGS
}
rl2_SpriteFlags;

struct rl2_Sprite {
    rl2_Image image;
    rl2_RGB565* bg;

    int x;
    int y;

    uint16_t flags; // Sprite layer [0..16383] | visibility << 14 | << unused << 15
};

static rl2_Sprite* rl2_sprites = NULL;
static size_t rl2_spriteCount = 0;
static size_t rl2_reservedSprites = 0;
static size_t rl2_visibleSpriteCount = 0;

rl2_Sprite rl2_createSprite(void) {
    rl2_Sprite sprite = (rl2_Sprite)rl2_alloc(sizeof(*sprite));

    if (sprite == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    if (rl2_spriteCount == rl2_reservedSprites) {
        size_t const new_reserved = rl2_reservedSprites == 0 ? RL2_MIN_SPRITES : rl2_reservedSprites * 2;
        rl2_Sprite* const new_entries = rl2_realloc(rl2_sprites, sizeof(rl2_Sprite) * new_reserved);

        if (new_entries == NULL) {
            RL2_ERROR(TAG "out of memory");
            rl2_free(sprite);
            return NULL;
        }

        rl2_reservedSprites = new_reserved;
        rl2_sprites = new_entries;
    }

    sprite->image = NULL;
    sprite->bg = NULL;
    sprite->x = sprite->y = 0;
    sprite->flags = RL2_SPRITE_INVISIBLE;

    rl2_sprites[rl2_spriteCount++] = sprite;
    return sprite;
}

void rl2_destroySprite(rl2_Sprite const sprite) {
    sprite->flags = RL2_SPRITE_DESTROY;
}

void rl2_setPosition(rl2_Sprite const sprite, int x, int y) {
    sprite->x = x;
    sprite->y = y;
}

void rl2_setLayer(rl2_Sprite const sprite, unsigned const layer) {
    sprite->flags = (sprite->flags & RL2_SPRITE_FLAGS) | (layer & RL2_SPRITE_LAYER);
}

bool rl2_setImage(rl2_Sprite const sprite, rl2_Image const image) {
    if (image == sprite->image) {
        return true;
    }

    rl2_RGB565* bg = NULL;

    if (image != NULL) {
        size_t const count = rl2_changedPixels(image);
        bg = (rl2_RGB565*)rl2_alloc(count * sizeof(*sprite->bg));

        if (bg == NULL) {
            RL2_ERROR(TAG "out of memory");
            return false;
        }
    }

    rl2_free(sprite->bg);

    sprite->image = image;
    sprite->bg = bg;
    return true;
}

void rl2_setVisibility(rl2_Sprite const sprite, bool const visible) {
    if (visible) {
        sprite->flags &= ~RL2_SPRITE_INVISIBLE;
    }
    else {
        sprite->flags |= RL2_SPRITE_INVISIBLE;
    }
}

static int rl2_compareSprites(void const* e1, void const* e2) {
    rl2_Sprite const s1 = *(rl2_Sprite*)e1;
    rl2_Sprite const s2 = *(rl2_Sprite*)e2;

    uint16_t const f1 = s1->flags | (RL2_SPRITE_INVISIBLE * (s1->image == NULL));
    uint16_t const f2 = s2->flags | (RL2_SPRITE_INVISIBLE * (s2->image == NULL));

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

void rl2_blitSprites(rl2_Canvas const canvas) {
    if (rl2_spriteCount == 0) {
        return;
    }

    qsort(rl2_sprites, rl2_spriteCount, sizeof(*rl2_sprites), rl2_compareSprites);

    size_t i = 0;
    rl2_Sprite sprite = rl2_sprites[0];

    // Blit all sprites not invisible and not marked for destruction
    if (i < rl2_spriteCount && (sprite->flags & RL2_SPRITE_FLAGS) == 0 && sprite->image != NULL) {
        do {
            rl2_blit(sprite->image, canvas, sprite->x, sprite->y, sprite->bg);
            sprite = rl2_sprites[++i];
        }
        while (i < rl2_spriteCount && (sprite->flags & RL2_SPRITE_FLAGS) == 0 && sprite->image != NULL);
    }

    rl2_visibleSpriteCount = i;

    // Skip invisible sprites
    if (i < rl2_spriteCount && ((sprite->flags & RL2_SPRITE_FLAGS) == RL2_SPRITE_INVISIBLE || sprite->image == NULL)) {
        do {
            sprite = rl2_sprites[++i];
        }
        while (i < rl2_spriteCount && ((sprite->flags & RL2_SPRITE_FLAGS) == RL2_SPRITE_INVISIBLE || sprite->image == NULL));
    }

    // Destroy all remaining sprites
    size_t const new_count = i;

    if (i < rl2_spriteCount) {
        do {
            rl2_free(sprite->bg);
            rl2_free(sprite);
            sprite = rl2_sprites[++i];
        }
        while (i < rl2_spriteCount);
    }

    rl2_spriteCount = new_count;
}

void rl2_unblitSprites(rl2_Canvas const canvas) {
    if (rl2_visibleSpriteCount == 0) {
        return;
    }

    size_t i = rl2_visibleSpriteCount;
    rl2_Sprite sprite = rl2_sprites[i - 1];

    do {
        rl2_unblit(sprite->image, canvas, sprite->x, sprite->y, sprite->bg);
        sprite = rl2_sprites[--i - 1];
    }
    while (i > 0);
}
