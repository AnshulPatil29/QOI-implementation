#include <stdio.h>
#include <stdint.h>

typedef struct QOIPixel
{
    uint8_t r,g,b,a;
} QOIPixel;




#define QOI_INDEX_SIZE 64
#define QOI_MAX_RUN_LENGTH 62
#define QOI_OP_RUN_TAG 0b11 
#define QOI_OP_RGBA_TAG  0b11111111 
#define QOI_OP_RGB_TAG  0b11111110 
#define QOI_OP_INDEX_TAG 0b00 
#define QOI_OP_DIFF_TAG  0b01 
#define QOI_OP_LUMA_TAG  0b10 

// helper functions
uint8_t hash(QOIPixel px){
    return (px.r*3+px.g*5+px.b*7+px.a*11)%QOI_INDEX_SIZE;
}
int pixels_are_equal(QOIPixel p1, QOIPixel p2) {
    return (p1.r == p2.r && p1.g == p2.g && p1.b == p2.b && p1.a == p2.a);
}
uint8_t make_chunk(uint8_t two_bit_tag,uint8_t payload){
    return (two_bit_tag<<6)|payload;
}
void write_header(
                uint32_t width,
                uint32_t height,
                uint8_t channels,
                uint8_t colorspace,
                FILE *outfile_ptr){
                
    char magic[4]={'q','o','i','f'};
    fwrite(magic,sizeof(char),4,outfile_ptr);
    fputc(width>>24 & 0xFF,outfile_ptr);
    fputc(width>>16 & 0xFF,outfile_ptr);
    fputc(width>>8 & 0xFF,outfile_ptr);
    fputc(width & 0xFF,outfile_ptr);

    fputc(height>>24 & 0xFF,outfile_ptr);
    fputc(height>>16 & 0xFF,outfile_ptr);
    fputc(height>>8 & 0xFF,outfile_ptr);
    fputc(height & 0xFF,outfile_ptr);

    fputc(channels,outfile_ptr);
    fputc(colorspace,outfile_ptr);            
    printf("File header written");
    return; 
    }

#define IMG_WIDTH 3
#define IMG_HEIGHT 2
#define NUM_PIXELS (IMG_WIDTH * IMG_HEIGHT)


int main() {
    // sample data
    QOIPixel image_data[NUM_PIXELS] = {

        {255, 0, 0, 255}, {255, 0, 0, 255}, {255, 0, 0, 255}, 
        {0, 255, 0, 255}, {0, 0, 255, 255}, {0, 255, 0, 255}  
    };

    QOIPixel previous_pixel = {0, 0, 0, 255};
    QOIPixel index_array[QOI_INDEX_SIZE];
    for (int i = 0; i < QOI_INDEX_SIZE; i++) {
    index_array[i] = (QOIPixel){0, 0, 0, 0};
    }


    // HEADER
    uint32_t width=IMG_WIDTH;
    uint32_t height=IMG_HEIGHT; 
    uint8_t channels=4; //3 for RGB and 4 for RGBA
    uint8_t colorspace=0; // 0 for sRGB with linear alpha, 1 for all channels linear
    FILE *outfile=NULL;
    outfile=fopen("output.qoi","wb");
    if(outfile==NULL){
        perror("Error: Could not open file for writing");
        return 1;
    }
    printf("File opened successfully.\n");
    write_header(width,height,channels,colorspace,outfile);

    
    // MAIL LOOP
    uint8_t run_count = 0;
    for(int px_index=0;px_index<NUM_PIXELS;px_index++){
        QOIPixel current_pixel=image_data[px_index];
        // RLE
        if(pixels_are_equal(current_pixel,previous_pixel))
        {
            run_count++;
            if(run_count==QOI_MAX_RUN_LENGTH){
                uint8_t chunk_byte=make_chunk(QOI_OP_RUN_TAG,run_count-1);
                fputc(chunk_byte, outfile);
                run_count = 0;
            }
        }
        else
        {
            // IF RLE existed, setting it and writing it
            if(run_count>0){
                uint8_t chunk_byte=make_chunk(QOI_OP_RUN_TAG,run_count-1);
                fputc(chunk_byte,outfile);
                run_count=0;
            }
            // hash index match check
            uint8_t hash_idx = hash(current_pixel);
            if(pixels_are_equal(index_array[hash_idx],current_pixel)){
                uint8_t chunk_byte=make_chunk(QOI_OP_INDEX_TAG,hash_idx);
                fputc(chunk_byte,outfile);
            }
            // if HASH index match fails, try OP_DIFF check
            /*
            Assuming that the alpha value is the same for all
            What this does is checks if all the differences in the 
            diff of prev and current pixel is in inclusive range (-2 to +1) 
            since these are 2 bit for each diff
            3 channels hence 6 bits, along with 2 tag bytes allows us to store it in a single byte
            The differences are stored with a bias of 2
            */
            else{
                // DIFF CHECK
                if(previous_pixel.a==current_pixel.a){
                    int dr=current_pixel.r-previous_pixel.r;
                    int dg=current_pixel.g-previous_pixel.g;
                    int db=current_pixel.b-previous_pixel.b;
                    if (dr >= -2 && dr <= 1 &&
                        dg >= -2 && dg <= 1 &&
                        db >= -2 && db <= 1){
                        uint8_t chunk_byte=make_chunk(QOI_OP_DIFF_TAG,(dr+2)<<4 | (dg+2)<<2 | db+2);
                        fputc(chunk_byte,outfile);
                    }
                    else{
                        int dr_dg=dr-dg;
                        int db_dg=db-dg;
                        if (dg>=-32 && dg<=31 &&
                            dr_dg>=-8 && dr_dg<=7 &&
                            db_dg>=-8 && db_dg<=7){
                            // adding bias
                            dg+=32;
                            dr_dg+=8;
                            db_dg+=8;
                            uint8_t chunk_byte=make_chunk(QOI_OP_LUMA_TAG,(uint8_t)dg);
                            fputc(chunk_byte,outfile);
                            chunk_byte=(uint8_t)dr_dg<<4 | (uint8_t)db_dg;
                            fputc(chunk_byte,outfile);
                            }
                        // RGB fallback
                        else{
                            fputc(QOI_OP_RGB_TAG, outfile);
                            fputc(current_pixel.r, outfile);
                            fputc(current_pixel.g, outfile);
                            fputc(current_pixel.b, outfile);
                        }
                    }
                }
                else{
                        fputc(QOI_OP_RGBA_TAG, outfile);
                        fputc(current_pixel.r, outfile);
                        fputc(current_pixel.g, outfile);
                        fputc(current_pixel.b, outfile);
                        fputc(current_pixel.a, outfile);
                        
                }   
                
            }
            index_array[hash_idx] = current_pixel;
        }
        previous_pixel = current_pixel;

    }
    // if the image ends on a run
    if (run_count > 0) {
    uint8_t chunk_byte = make_chunk(QOI_OP_RUN_TAG,run_count-1);
    fputc(chunk_byte, outfile);
    }


    // the spec mentions the end of stream marker is 7 zero bytes followed by 1 one byte
    const uint8_t END_OF_STREAM[8]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01};
    fwrite(END_OF_STREAM,sizeof(uint8_t),8,outfile);
    fclose(outfile);
    return 0;
}