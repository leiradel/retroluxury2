#ifndef HH2_IMAGE_H__
#define HH2_IMAGE_H__

#include "pixelsrc.h"
#include "canvas.h"

typedef struct hh2_Image* hh2_Image;

hh2_Image hh2_createImage(hh2_PixelSource source);
void hh2_destroyImage(hh2_Image image);

unsigned hh2_imageWidth(hh2_Image image);
unsigned hh2_imageHeight(hh2_Image image);
size_t hh2_changedPixels(hh2_Image image);

hh2_RGB565* hh2_blit(hh2_Image image, hh2_Canvas canvas, int x0, int y0, hh2_RGB565* bg);
void hh2_unblit(hh2_Image image, hh2_Canvas canvas, int x0, int y0, hh2_RGB565 const* bg);

void hh2_stamp(hh2_Image image, hh2_Canvas canvas, int x0, int y0);

#ifdef HH2_DEBUG
char const* hh2_getImagePath(hh2_Image image);
#endif

#endif // HH2_IMAGE_H__
