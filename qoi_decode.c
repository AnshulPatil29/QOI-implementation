#include "qoi_utils.h"
#include <stdlib.h>
#include <string.h>

static uint8_t qoi_calculate_hash_static(QOIPixel px) {
    return (px.r * 3 + px.g * 5 + px.b * 7 + px.a * 11) % QOI_INDEX_SIZE;
}


QOIPixel* qoi_decode_from_file(FILE *infile_ptr,
                               uint32_t *out_width,
                               uint32_t *out_height,
                               uint8_t *out_channels,
                               uint8_t *out_colorspace) {
    if (!infile_ptr || !out_width || !out_height || !out_channels || !out_colorspace) {
        return NULL;
    }

    unsigned char header_buffer[14];
    if (fread(header_buffer, 1, 14, infile_ptr) != 14) {
        fprintf(stderr, "Error: Could not read QOI header.\n");
        return NULL;
    }

    if (header_buffer[0] != 'q' || header_buffer[1] != 'o' ||
        header_buffer[2] != 'i' || header_buffer[3] != 'f') {
        fprintf(stderr, "Error: Invalid QOI magic bytes.\n");
        return NULL;
    }

    *out_width = ((uint32_t)header_buffer[4] << 24) |
                 ((uint32_t)header_buffer[5] << 16) |
                 ((uint32_t)header_buffer[6] << 8) |
                 ((uint32_t)header_buffer[7]);
    *out_height = ((uint32_t)header_buffer[8] << 24) |
                  ((uint32_t)header_buffer[9] << 16) |
                  ((uint32_t)header_buffer[10] << 8) |
                  ((uint32_t)header_buffer[11]);
    *out_channels = header_buffer[12];
    *out_colorspace = header_buffer[13];

    if (*out_width == 0 || *out_height == 0 || *out_channels < 3 || *out_channels > 4) {
        fprintf(stderr, "Error: Invalid image dimensions or channels in header.\n");
        return NULL;
    }
    if (*out_height > 0 && *out_width > UINT32_MAX / *out_height) {
        fprintf(stderr, "Error: Image dimensions (width * height) too large, would overflow uint32_t.\n");
        return NULL;
    }
    uint32_t num_pixels_to_decode = (*out_width) * (*out_height);

    if (num_pixels_to_decode > SIZE_MAX / sizeof(QOIPixel)) {
         fprintf(stderr, "Error: Image dimensions too large for memory allocation.\n");
        return NULL;
    }

    QOIPixel *decoded_pixels_data = (QOIPixel *)malloc(num_pixels_to_decode * sizeof(QOIPixel));
    if (decoded_pixels_data == NULL) {
        fprintf(stderr, "Error: Could not allocate memory for decoded pixels.\n");
        return NULL;
    }

    QOIPixel previous_pixel = {0, 0, 0, 255};
    QOIPixel index_array[QOI_INDEX_SIZE];
    memset(index_array, 0, sizeof(index_array));

    uint32_t decoded_pixel_count = 0;
    int byte1;
    unsigned char stream_end_check_buffer[8];
    int stream_end_bytes_read_ahead = 0;

    while (decoded_pixel_count < num_pixels_to_decode) {
        if (stream_end_bytes_read_ahead > 0) {
            byte1 = stream_end_check_buffer[0];
            if (stream_end_bytes_read_ahead > 1) {
                memmove(stream_end_check_buffer, stream_end_check_buffer + 1, stream_end_bytes_read_ahead - 1);
            }
            stream_end_bytes_read_ahead--;
        } else {
            byte1 = fgetc(infile_ptr);
        }

        if (byte1 == EOF) {
            fprintf(stderr, "Error: Unexpected EOF during pixel decoding. Decoded %u of %u pixels.\n", decoded_pixel_count, num_pixels_to_decode);
            free(decoded_pixels_data);
            return NULL;
        }

        QOIPixel current_pixel_val = previous_pixel;
        int pixels_in_this_chunk = 1;

        if (byte1 == QOI_OP_RGB_BYTE) {
            unsigned char rgb_buffer[3];
            if (fread(rgb_buffer, 1, 3, infile_ptr) != 3) { fprintf(stderr, "Error: EOF reading QOI_OP_RGB payload.\n"); free(decoded_pixels_data); return NULL; }
            current_pixel_val.r = rgb_buffer[0];
            current_pixel_val.g = rgb_buffer[1];
            current_pixel_val.b = rgb_buffer[2];
            current_pixel_val.a = previous_pixel.a;
        } else if (byte1 == QOI_OP_RGBA_BYTE) {
            unsigned char rgba_buffer[4];
            if (fread(rgba_buffer, 1, 4, infile_ptr) != 4) { fprintf(stderr, "Error: EOF reading QOI_OP_RGBA payload.\n"); free(decoded_pixels_data); return NULL; }
            current_pixel_val.r = rgba_buffer[0];
            current_pixel_val.g = rgba_buffer[1];
            current_pixel_val.b = rgba_buffer[2];
            current_pixel_val.a = rgba_buffer[3];
        } else {
            uint8_t tag = (byte1 >> 6) & 0x03;

            if (tag == QOI_OP_INDEX_TAG) {
                uint8_t index_val = byte1 & 0x3F;
                current_pixel_val = index_array[index_val];
            } else if (tag == QOI_OP_DIFF_TAG) {
                uint8_t dr_bias = (byte1 >> 4) & 0x03;
                uint8_t dg_bias = (byte1 >> 2) & 0x03;
                uint8_t db_bias = (byte1 >> 0) & 0x03;

                current_pixel_val.r = previous_pixel.r + (dr_bias - 2);
                current_pixel_val.g = previous_pixel.g + (dg_bias - 2);
                current_pixel_val.b = previous_pixel.b + (db_bias - 2);
                current_pixel_val.a = previous_pixel.a;
            } else if (tag == QOI_OP_LUMA_TAG) {
                int byte2 = fgetc(infile_ptr);
                if (byte2 == EOF) { fprintf(stderr, "Error: EOF reading QOI_OP_LUMA byte2.\n"); free(decoded_pixels_data); return NULL; }

                uint8_t dg_bias = byte1 & 0x3F;
                uint8_t dr_dg_bias = (byte2 >> 4) & 0x0F;
                uint8_t db_dg_bias = (byte2 >> 0) & 0x0F;

                int dg_val = dg_bias - 32;
                int dr_val = (dr_dg_bias - 8) + dg_val;
                int db_val = (db_dg_bias - 8) + dg_val;

                current_pixel_val.r = previous_pixel.r + dr_val;
                current_pixel_val.g = previous_pixel.g + dg_val;
                current_pixel_val.b = previous_pixel.b + db_val;
                current_pixel_val.a = previous_pixel.a;
            } else if (tag == QOI_OP_RUN_TAG) {
                uint8_t run_length = (byte1 & 0x3F) + 1;
                pixels_in_this_chunk = run_length;
                current_pixel_val = previous_pixel;
            } else {
                fprintf(stderr, "Error: Unknown QOI tag encountered: %02x\n", tag);
                free(decoded_pixels_data);
                return NULL;
            }
        }

        for (int i = 0; i < pixels_in_this_chunk; ++i) {
            if (decoded_pixel_count < num_pixels_to_decode) {
                decoded_pixels_data[decoded_pixel_count] = current_pixel_val;
                decoded_pixel_count++;
            } else {
                fprintf(stderr, "Error: Decoded more pixels than specified in header. Stream may be corrupt.\n");
                free(decoded_pixels_data);
                return NULL;
            }
        }

        uint8_t hash_idx = qoi_calculate_hash_static(current_pixel_val);
        index_array[hash_idx] = current_pixel_val;
        previous_pixel = current_pixel_val;

        if (decoded_pixel_count == num_pixels_to_decode && stream_end_bytes_read_ahead == 0) {
            size_t bytes_actually_read = fread(stream_end_check_buffer, 1, 8, infile_ptr);
            if (ferror(infile_ptr)) {
                perror("Error reading potential end-of-stream marker");
                free(decoded_pixels_data);
                return NULL;
            }
            stream_end_bytes_read_ahead = bytes_actually_read;
        }
    }

    if (decoded_pixel_count != num_pixels_to_decode) {
        fprintf(stderr, "Error: Decoded pixel count (%u) does not match expected (%u).\n", decoded_pixel_count, num_pixels_to_decode);
        free(decoded_pixels_data);
        return NULL;
    }

    const unsigned char QOI_END_MARKER[8] = {0,0,0,0,0,0,0,1};
    if (stream_end_bytes_read_ahead == 8) {
        if (memcmp(stream_end_check_buffer, QOI_END_MARKER, 8) != 0) {
            fprintf(stderr, "Warning: End-of-stream marker mismatch. File might be corrupt or have extra data.\n");
        }
    } else {
        fprintf(stderr, "Warning: Could not fully read/verify end-of-stream marker (read %d bytes of 8). File might be truncated.\n", stream_end_bytes_read_ahead);
    }

    if (stream_end_bytes_read_ahead == 8) {
        if (fgetc(infile_ptr) != EOF) {
            fprintf(stderr, "Warning: Additional data found after QOI end-of-stream marker.\n");
        }
    }

    return decoded_pixels_data;
}