#ifndef RL2_BDFFONT_H
#define RL2_BDFFONT_H

#include "rl2_pixelsrc.h"
#include "rl2_filesys.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct rl2_Font* rl2_Font;

rl2_Font rl2_readFont(rl2_Filesys const filesys, char const* const path);
void rl2_destroyFont(rl2_Font const font);

void rl2_textSize(rl2_Font const font, int* const x0, int* const y0, int* const width, int* const height, char const* const text);

rl2_PixelSource rl2_renderText(
    rl2_Font const font, int* const x0, int* const y0, char const* const text,
    rl2_ARGB8888 const bg_color, rl2_ARGB8888 const fg_color);

#endif /* RL2_BDFFONT_H */
