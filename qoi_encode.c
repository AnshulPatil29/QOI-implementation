#include "qoi_utils.h"
#include <string.h>

static uint8_t qoi_hash_pixel(QOIPixel px) {
    return (px.r * 3 + px.g * 5 + px.b * 7 + px.a * 11) % QOI_INDEX_SIZE;
}

static int qoi_pixels_are_equal(QOIPixel p1, QOIPixel p2) {
    return (p1.r == p2.r && p1.g == p2.g && p1.b == p2.b && p1.a == p2.a);
}

static uint8_t qoi_make_chunk(uint8_t two_bit_tag, uint8_t payload) {
    return (two_bit_tag << 6) | payload;
}

static void qoi_write_header(
    uint32_t width,
    uint32_t height,
    uint8_t channels,
    uint8_t colorspace,
    FILE *outfile_ptr) {

    char magic[4] = {'q', 'o', 'i', 'f'};
    fwrite(magic, sizeof(char), 4, outfile_ptr);

    fputc((width >> 24) & 0xFF, outfile_ptr);
    fputc((width >> 16) & 0xFF, outfile_ptr);
    fputc((width >> 8) & 0xFF, outfile_ptr);
    fputc(width & 0xFF, outfile_ptr);

    fputc((height >> 24) & 0xFF, outfile_ptr);
    fputc((height >> 16) & 0xFF, outfile_ptr);
    fputc((height >> 8) & 0xFF, outfile_ptr);
    fputc(height & 0xFF, outfile_ptr);

    fputc(channels, outfile_ptr);
    fputc(colorspace, outfile_ptr);
}

int qoi_encode_to_file(const QOIPixel *image_data,
                       uint32_t width,
                       uint32_t height,
                       uint8_t channels,
                       uint8_t colorspace,
                       FILE *outfile_ptr) {

    if (!image_data || !outfile_ptr || width == 0 || height == 0) {
        return 1;
    }

    uint32_t num_pixels = width * height;

    qoi_write_header(width, height, channels, colorspace, outfile_ptr);

    QOIPixel previous_pixel = {0, 0, 0, 255};
    QOIPixel index_array[QOI_INDEX_SIZE];
    memset(index_array, 0, sizeof(index_array));

    uint8_t run_count = 0;
    for (uint32_t px_index = 0; px_index < num_pixels; px_index++) {
        QOIPixel current_pixel = image_data[px_index];

        if (qoi_pixels_are_equal(current_pixel, previous_pixel)) {
            run_count++;
            if (run_count == QOI_MAX_RUN_LENGTH) {
                uint8_t chunk_byte = qoi_make_chunk(QOI_OP_RUN_TAG, run_count - 1);
                fputc(chunk_byte, outfile_ptr);
                run_count = 0;
            }
        } else {
            if (run_count > 0) {
                uint8_t chunk_byte = qoi_make_chunk(QOI_OP_RUN_TAG, run_count - 1);
                fputc(chunk_byte, outfile_ptr);
                run_count = 0;
            }

            uint8_t hash_idx = qoi_hash_pixel(current_pixel);
            if (qoi_pixels_are_equal(index_array[hash_idx], current_pixel)) {
                uint8_t chunk_byte = qoi_make_chunk(QOI_OP_INDEX_TAG, hash_idx);
                fputc(chunk_byte, outfile_ptr);
            } else {
                if (previous_pixel.a == current_pixel.a) {
                    int dr = current_pixel.r - previous_pixel.r;
                    int dg = current_pixel.g - previous_pixel.g;
                    int db = current_pixel.b - previous_pixel.b;

                    if (dr >= -2 && dr <= 1 &&
                        dg >= -2 && dg <= 1 &&
                        db >= -2 && db <= 1) {
                        uint8_t payload = ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2);
                        uint8_t chunk_byte = qoi_make_chunk(QOI_OP_DIFF_TAG, payload);
                        fputc(chunk_byte, outfile_ptr);
                    } else {
                        int dr_dg = dr - dg;
                        int db_dg = db - dg;
                        if (dg >= -32 && dg <= 31 &&
                            dr_dg >= -8 && dr_dg <= 7 &&
                            db_dg >= -8 && db_dg <= 7) {

                            uint8_t chunk_byte1 = qoi_make_chunk(QOI_OP_LUMA_TAG, (uint8_t)(dg + 32));
                            fputc(chunk_byte1, outfile_ptr);
                            uint8_t chunk_byte2 = (uint8_t)((dr_dg + 8) << 4) | (uint8_t)(db_dg + 8);
                            fputc(chunk_byte2, outfile_ptr);
                        }
                        else {
                            fputc(QOI_OP_RGB_BYTE, outfile_ptr);
                            fputc(current_pixel.r, outfile_ptr);
                            fputc(current_pixel.g, outfile_ptr);
                            fputc(current_pixel.b, outfile_ptr);
                        }
                    }
                }
                else {
                    fputc(QOI_OP_RGBA_BYTE, outfile_ptr);
                    fputc(current_pixel.r, outfile_ptr);
                    fputc(current_pixel.g, outfile_ptr);
                    fputc(current_pixel.b, outfile_ptr);
                    fputc(current_pixel.a, outfile_ptr);
                }
            }
            index_array[hash_idx] = current_pixel;
        }
        previous_pixel = current_pixel;
    }

    if (run_count > 0) {
        uint8_t chunk_byte = qoi_make_chunk(QOI_OP_RUN_TAG, run_count - 1);
        fputc(chunk_byte, outfile_ptr);
    }

    const uint8_t END_OF_STREAM[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    fwrite(END_OF_STREAM, sizeof(uint8_t), 8, outfile_ptr);

    if (ferror(outfile_ptr)) {
        return 1;
    }

    return 0;
}