#ifndef RL2_FONT_H__
#define RL2_FONT_H__

#include "rl2_pixelsrc.h"
#include "rl2_filesys.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct rl2_Font* rl2_Font;

typedef int (*rl2_GlyphFilter)(void* const userdata, int const encoding, int const non_standard);

rl2_Font rl2_readFontWithFilter(rl2_Filesys const filesys, char const* const path, rl2_GlyphFilter const filter);
rl2_Font rl2_readFont(rl2_Filesys const filesys, char const* const path);
void rl2_destroyFont(rl2_Font const font);

void rl2_textSize(rl2_Font const font, int* const x0, int* const y0, int* const width, int* const height, char const* const text);

rl2_PixelSource rl2_renderText(
    rl2_Font const font, int* const x0, int* const y0, char const* const text,
    rl2_ARGB8888 const bg_color, rl2_ARGB8888 const fg_color);

#endif // RL2_FONT_H__
