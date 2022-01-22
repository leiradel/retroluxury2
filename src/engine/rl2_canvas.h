#ifndef RL2_CANVAS_H__
#define RL2_CANVAS_H__

#include <stdint.h>
#include <stddef.h>

#define RL2_COLOR_RGB565(r, g, b) ((((rl2_RGB565)(r) << 8 | (rl2_RGB565)(b) >> 3) & 0xf81fU) | (((rl2_RGB565)(g) << 3) & 0x07e0U))

typedef uint16_t rl2_RGB565;
typedef struct rl2_Canvas* rl2_Canvas;

rl2_Canvas rl2_createCanvas(unsigned const width, unsigned const height);
void rl2_destroyCanvas(rl2_Canvas const canvas);

unsigned rl2_canvasWidth(rl2_Canvas const canvas);
unsigned rl2_canvasHeight(rl2_Canvas const canvas);
size_t rl2_canvasPitch(rl2_Canvas const canvas);

void rl2_clearCanvas(rl2_Canvas const canvas, rl2_RGB565 const color);

rl2_RGB565* rl2_canvasPixel(rl2_Canvas const canvas, unsigned const x, unsigned const y);

#endif // RL2_CANVAS_H__
