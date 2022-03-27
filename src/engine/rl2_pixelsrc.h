#ifndef RL2_PIXELSRC_H__
#define RL2_PIXELSRC_H__

#include "rl2_filesys.h"

#include <stddef.h>
#include <stdint.h>

#define RL2_ARGB8888_A(p) (((p) >> 24) & 255)
#define RL2_ARGB8888_B(p) (((p) >> 16) & 255)
#define RL2_ARGB8888_G(p) (((p) >> 8) & 255)
#define RL2_ARGB8888_R(p) (((p) >> 0) & 255)

typedef enum {
    RL2_PIXELSOURCE_ROTATE_0,
    RL2_PIXELSOURCE_ROTATE_90,
    RL2_PIXELSOURCE_ROTATE_180,
    RL2_PIXELSOURCE_ROTATE_270,
    RL2_PIXELSOURCE_ROTATE_HORIZONTAL_FLIP,
    RL2_PIXELSOURCE_ROTATE_VERTICAL_FLIP
}
rl2_PixelSourceTransform;

typedef uint32_t rl2_ARGB8888;
typedef struct rl2_PixelSource* rl2_PixelSource;

rl2_PixelSource rl2_newPixelSource(unsigned const width, unsigned const height);
rl2_PixelSource rl2_initPixelSource(void const* const data, size_t const size);
rl2_PixelSource rl2_readPixelSource(char const* const path, unsigned const max_height);

rl2_PixelSource rl2_subPixelSource(
    rl2_PixelSource const parent, unsigned const x0, unsigned const y0, unsigned const width, unsigned const height);

rl2_PixelSource rl2_transformPixelSource(rl2_PixelSource const parent, rl2_PixelSourceTransform const transform);

void rl2_destroyPixelSource(rl2_PixelSource const source);

unsigned rl2_pixelSourceWidth(rl2_PixelSource const source);
unsigned rl2_pixelSourceHeight(rl2_PixelSource const source);

rl2_ARGB8888 rl2_getPixel(rl2_PixelSource const source, unsigned const x, unsigned const y);
void rl2_fillPixelSource(rl2_PixelSource const source, rl2_ARGB8888 const color);
void rl2_putPixel(rl2_PixelSource const source, unsigned const x, unsigned const y, rl2_ARGB8888 const color);

#ifdef RL2_BUILD_DEBUG
    char const* rl2_getPixelSourcePath(rl2_PixelSource const source);
#endif

#endif // RL2_PIXELSRC_H__
