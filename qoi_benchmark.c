#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "qoi_utils.h"

long get_file_size(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Error opening file for size check");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <input.png> <output.qoi> [decoded_output.png]\n", argv[0]);
        return 1;
    }

    const char *input_png_filename = argv[1];
    const char *output_qoi_filename = argv[2];
    const char *decoded_png_filename = (argc == 4) ? argv[3] : NULL;

    int width, height, channels_in_file;
    unsigned char *stb_pixel_data = stbi_load(input_png_filename, &width, &height, &channels_in_file, 4);

    if (!stb_pixel_data) {
        fprintf(stderr, "Error loading PNG '%s': %s\n", input_png_filename, stbi_failure_reason());
        return 1;
    }
    printf("Loaded PNG '%s': %d x %d, Original Channels: %d (loaded as RGBA)\n",
           input_png_filename, width, height, channels_in_file);

    QOIPixel *image_pixels = (QOIPixel *)stb_pixel_data;

    printf("\nEncoding to QOI '%s'...\n", output_qoi_filename);
    FILE *qoi_outfile = fopen(output_qoi_filename, "wb");
    if (!qoi_outfile) {
        perror("Error opening QOI output file");
        stbi_image_free(stb_pixel_data);
        return 1;
    }

    uint8_t qoi_header_channels_for_file = (channels_in_file == 3) ? 3 : 4;
    uint8_t qoi_colorspace = 0;

    clock_t start_time = clock();
    int encode_status = qoi_encode_to_file(image_pixels, width, height, qoi_header_channels_for_file, qoi_colorspace, qoi_outfile);
    clock_t end_time = clock();
    double encode_time_seconds = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    fclose(qoi_outfile);

    if (encode_status != 0) {
        fprintf(stderr, "QOI encoding failed.\n");
        stbi_image_free(stb_pixel_data);
        remove(output_qoi_filename);
        return 1;
    }
    printf("Encoding successful. Time: %.4f seconds\n", encode_time_seconds);

    printf("\nDecoding from QOI '%s'...\n", output_qoi_filename);
    FILE *qoi_infile = fopen(output_qoi_filename, "rb");
    if (!qoi_infile) {
        perror("Error opening QOI input file for decoding");
        stbi_image_free(stb_pixel_data);
        return 1;
    }

    uint32_t decoded_width, decoded_height;
    uint8_t decoded_channels_header, decoded_colorspace_header;

    start_time = clock();
    QOIPixel *decoded_image_pixels = qoi_decode_from_file(qoi_infile, &decoded_width, &decoded_height, &decoded_channels_header, &decoded_colorspace_header);
    end_time = clock();
    double decode_time_seconds = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    fclose(qoi_infile);

    if (!decoded_image_pixels) {
        fprintf(stderr, "QOI decoding failed.\n");
        stbi_image_free(stb_pixel_data);
        return 1;
    }
    printf("Decoding successful. Time: %.4f seconds\n", decode_time_seconds);
    printf("Decoded QOI: %u x %u, Channels in header: %u, Colorspace in header: %u\n",
           decoded_width, decoded_height, decoded_channels_header, decoded_colorspace_header);

    if (decoded_width != (uint32_t)width || decoded_height != (uint32_t)height) {
        fprintf(stderr, "Error: Decoded dimensions (%ux%u) do not match original (%dx%d)!\n",
                decoded_width, decoded_height, width, height);
    } else {
        printf("Dimensions match original.\n");
    }

    if (decoded_png_filename) {
        printf("Saving decoded image to '%s'...\n", decoded_png_filename);
        int write_status = stbi_write_png(decoded_png_filename, decoded_width, decoded_height, 4,
                                          (const void*)decoded_image_pixels, decoded_width * sizeof(QOIPixel));
        if (write_status == 0) {
            fprintf(stderr, "Error writing decoded PNG to '%s'.\n", decoded_png_filename);
        } else {
            printf("Decoded image saved successfully.\n");
        }
    }

    printf("\n--- Benchmark Statistics ---\n");
    long input_png_size = get_file_size(input_png_filename);
    long output_qoi_size = get_file_size(output_qoi_filename);

    long raw_rgba_size = (long)width * height * 4;

    printf("Raw RGBA (uncompressed) size: %ld bytes\n", raw_rgba_size);

    if (output_qoi_size > 0) {
        double compression_ratio_vs_raw = (double)raw_rgba_size / output_qoi_size;
        printf("Output QOI size: %ld bytes\n", output_qoi_size);
        printf("Compression ratio (Raw_RGBA_size / QOI_size): %.2f : 1\n", compression_ratio_vs_raw);

        if (input_png_size > 0) {
            double compression_ratio_vs_png = (double)input_png_size / output_qoi_size;
            printf("Input PNG size: %ld bytes\n", input_png_size);
            printf("Compression ratio (PNG_size / QOI_size): %.2f : 1\n", compression_ratio_vs_png);
        } else {
            printf("Could not get input PNG file size for PNG ratio calculation.\n");
        }

        double Bpp_qoi = (double)(output_qoi_size * 8) / (width * height);
        printf("QOI Bits per pixel (Bpp): %.2f\n", Bpp_qoi);
        printf("Raw RGBA Bits per pixel (Bpp): %.2f (32.00 for reference)\n", (double)(raw_rgba_size * 8) / (width*height) );
    } else {
        printf("Could not get QOI file size for ratio calculations.\n");
        if (input_png_size <=0) printf("Could not get input PNG file size either.\n");
    }

    uint64_t total_pixels = (uint64_t)width * height;
    if (encode_time_seconds > 0) {
        printf("Encoding speed: %.2f Megapixels/sec\n", (total_pixels / encode_time_seconds) / 1000000.0);
    }
    if (decode_time_seconds > 0) {
        printf("Decoding speed: %.2f Megapixels/sec\n", (total_pixels / decode_time_seconds) / 1000000.0);
    }

    stbi_image_free(stb_pixel_data);
    free(decoded_image_pixels);

    printf("\nBenchmark finished.\n");
    return 0;
}
