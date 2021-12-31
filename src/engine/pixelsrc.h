#ifndef HH2_PIXELSRC_H__
#define HH2_PIXELSRC_H__

#include "filesys.h"

#include <stddef.h>
#include <stdint.h>

#define HH2_ARGB8888_A(p) (((p) >> 24) & 255)
#define HH2_ARGB8888_B(p) (((p) >> 16) & 255)
#define HH2_ARGB8888_G(p) (((p) >> 8) & 255)
#define HH2_ARGB8888_R(p) (((p) >> 0) & 255)

typedef uint32_t hh2_ARGB8888;
typedef struct hh2_PixelSource* hh2_PixelSource;

hh2_PixelSource hh2_initPixelSource(void const* data, size_t size);
hh2_PixelSource hh2_readPixelSource(hh2_Filesys filesys, char const* path);
hh2_PixelSource hh2_subPixelSource(hh2_PixelSource parent, unsigned x0, unsigned y0, unsigned width, unsigned height);
void hh2_destroyPixelSource(hh2_PixelSource source);

unsigned hh2_pixelSourceWidth(hh2_PixelSource source);
unsigned hh2_pixelSourceHeight(hh2_PixelSource source);
hh2_ARGB8888 hh2_getPixel(hh2_PixelSource, unsigned x, unsigned y);

#ifdef HH2_DEBUG
    char const* hh2_getPixelSourcePath(hh2_PixelSource source);
#endif

#endif // HH2_PIXELSRC_H__
