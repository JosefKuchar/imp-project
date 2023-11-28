#pragma once

#include <stdint.h>

typedef struct {
    uint8_t* pixels;
    unsigned int w;
    unsigned int h;
} image_t;
