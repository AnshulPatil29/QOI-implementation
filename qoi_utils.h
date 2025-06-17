#ifndef QOI_UTILS_H
#define QOI_UTILS_H

#include <stdint.h>
#include <stdio.h>

typedef struct QOIPixel {
    uint8_t r, g, b, a;
} QOIPixel;

#define QOI_INDEX_SIZE 64
#define QOI_MAX_RUN_LENGTH 62

#define QOI_OP_RGB_BYTE   0xFE
#define QOI_OP_RGBA_BYTE  0xFF

#define QOI_OP_INDEX_TAG 0  // Was 0b00
#define QOI_OP_DIFF_TAG  1  // Was 0b01
#define QOI_OP_LUMA_TAG  2  // Was 0b10
#define QOI_OP_RUN_TAG   3  // Was 0b11

int qoi_encode_to_file(const QOIPixel *pixel_data,
                       uint32_t width,
                       uint32_t height,
                       uint8_t channels,
                       uint8_t colorspace,
                       FILE *outfile_ptr);

QOIPixel* qoi_decode_from_file(FILE *infile_ptr,
                               uint32_t *width,
                               uint32_t *height,
                               uint8_t *channels,
                               uint8_t *colorspace);

#endif