#include "image_manipulation.h"

/**
 * Original Bilinear interpolation code from https://rosettacode.org/wiki/Bilinear_interpolation
 */

uint8_t getpixel(in_image_t* image, unsigned int x, unsigned int y) {
    return image->pixels[((y + image->offsetY) * image->w) + (x + image->offsetX)];
}

float max(float a, float b) {
    return (a < b) ? a : b;
};

float lerp(float s, float e, float t) {
    return s + (e - s) * t;
}

float blerp(float c00, float c10, float c01, float c11, float tx, float ty) {
    return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

void putpixel(out_image_t* image, unsigned int x, unsigned int y, uint8_t color) {
    image->pixels[(y * image->w) + x] = color;
}

void scale(in_image_t* src, out_image_t* dst, unsigned int newWidth, unsigned int newHeight) {
    int x, y;
    for (x = 0, y = 0;; x++) {
        if (x >= newWidth) {
            x = 0;
            y++;
            if (y >= newHeight) {
                break;
            }
        }
        float gx = max(x / (float)(newWidth) * (src->sectionWidth) - 0.5f, src->sectionWidth - 1);
        float gy = max(y / (float)(newHeight) * (src->sectionHeight) - 0.5, src->sectionHeight - 1);
        int gxi = (int)gx;
        int gyi = (int)gy;
        uint8_t c00 = getpixel(src, gxi, gyi);
        uint8_t c10 = getpixel(src, gxi + 1, gyi);
        uint8_t c01 = getpixel(src, gxi, gyi + 1);
        uint8_t c11 = getpixel(src, gxi + 1, gyi + 1);
        uint8_t result = (uint8_t)blerp(c00, c10, c01, c11, gx - gxi, gy - gyi);
        putpixel(dst, x, y, result > 220 ? 255 : 0);
    }
}
