# Simple QOI Encoder/Decoder & Benchmark

A C implementation of the QOI (Quite OK Image) format encoder and decoder, along with a benchmarking tool.

## Files

*   `qoi_utils.h`: Header file defining the `QOIPixel` struct, QOI constants, and function prototypes for the encoder and decoder.
*   `qoi_encode.c`: Implementation of the QOI image encoder.
*   `qoi_decode.c`: Implementation of the QOI image decoder.
*   `qoi_benchmark.c`: Main program to load PNGs, encode to QOI, decode from QOI, save decoded images, and print benchmark statistics (speed, compression). Uses `stb_image.h` and `stb_image_write.h` (not included in this repo, must be downloaded separately).

## Compilation

Requires `stb_image.h` and `stb_image_write.h` to be in the same directory.

Using GCC:
```bash
gcc -mconsole qoi_benchmark.c qoi_encode.c qoi_decode.c -o qoi_benchmark -O2 -Wall -Wextra -pedantic -std=c99
```
Example usage:
```bash
./qoi_benchmark test.png encoded.qoi decoded_back.png
```
