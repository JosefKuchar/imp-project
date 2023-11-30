#pragma once

#include <stdint.h>

typedef struct {
    uint8_t* pixels;
    unsigned int w;
    unsigned int h;
    unsigned int offsetX;
    unsigned int offsetY;
    unsigned int sectionWidth;
    unsigned int sectionHeight;
} in_image_t;

typedef struct {
    uint8_t* pixels;
    unsigned int w;
    unsigned int h;
} out_image_t;

void scale(in_image_t* in_image, out_image_t* out_image, unsigned int out_w, unsigned int out_h);
