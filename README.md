# Simple QOI Encoder/Decoder & Benchmark

A C implementation of the QOI (Quite OK Image) format encoder and decoder, along with a benchmarking tool.

## Files

*   `qoi_utils.h`: Header file defining the `QOIPixel` struct, QOI constants, and function prototypes for the encoder and decoder.
*   `qoi_encode.c`: Implementation of the QOI image encoder.
*   `qoi_decode.c`: Implementation of the QOI image decoder.
*   `qoi_benchmark.c`: Main program to load PNGs, encode to QOI, decode from QOI, save decoded images, and print benchmark statistics (speed, compression). Uses `stb_image.h` and `stb_image_write.h` (not included in this repo, must be downloaded separately).
*   `QOI.c`: The original code with comments before cleaning up the code

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
## Purpose of this branch
- I just wanted to also backup a copy of the original encoder code with comments which showed my learnings
- The version on main branch is the cleaned up version
> Note: This was my first attempt at C code so this is not in any way a well built code, I had to refine it multiple times just to get it working fine.
> The primary goal was to learn bit manipulation and a bit of memory management which this project achieved
