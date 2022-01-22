#ifndef RL2_IMAGE_H__
#define RL2_IMAGE_H__

#include "rl2_pixelsrc.h"
#include "rl2_canvas.h"

typedef struct rl2_Image* rl2_Image;

rl2_Image rl2_createImage(rl2_PixelSource const source);
void rl2_destroyImage(rl2_Image const image);

unsigned rl2_imageWidth(rl2_Image const image);
unsigned rl2_imageHeight(rl2_Image const image);
size_t rl2_changedPixels(rl2_Image const image);

rl2_RGB565* rl2_blit(rl2_Image const image, rl2_Canvas const canvas, int const x0, int const y0, rl2_RGB565* bg);
void rl2_unblit(rl2_Image const image, rl2_Canvas const canvas, int const x0, int const y0, rl2_RGB565 const* const bg);

void rl2_stamp(rl2_Image const image, rl2_Canvas const canvas, int const x0, int const y0);

#ifdef RL2_BUILD_DEBUG
char const* rl2_getImagePath(rl2_Image const image);
#endif

#endif // RL2_IMAGE_H__
