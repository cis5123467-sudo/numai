#ifndef NUMAI_MATHVIEW_H
#define NUMAI_MATHVIEW_H

#include <stdbool.h>
#include <stdint.h>

typedef struct { int width, above, below; } MathBox;
enum { MathMultiply = 29, MathPi = 30, MathApprox = 31 };
typedef struct {
    void (*text)(int, int, const char *, uint16_t, uint16_t, bool);
    void (*rect)(int, int, int, int, uint16_t);
    uint16_t ink, paper;
} MathCanvas;

MathBox measure_math(const char *text, int width);
void paint_math(const char *text, int width, int x, int mid, const MathCanvas *canvas);

#endif
