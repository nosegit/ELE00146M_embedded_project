/*
 * Alphabet Processing Workflow for Zedboard
 * --------------------------------------------------------
 * 1. Extract 8x8 letter from 512x512 image
 * 2. Display Original on 8x8 LED Matrix via SPI
 * 3. Process via Custom IP Block (7x7 Cross-Shaped Dilation)
 * 4. Display Filtered Result on 8x8 LED Matrix
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xil_io.h"
#include "myfilter.h"       
#include "myfilter_utils.h" 
#include "spi_led.h"        
#include "image_data.h"     
#include "sleep.h"

// Hardware Base Address
#define BASE_ADDR XPAR_MYFILTER_0_BASEADDR
#define LETTER_SIZE 8

// 7x7 Kernel containing a centered 3x3 Cross for Dilation
// Dilation expands the bright parts of the alphabet.
s16 cross_kernel[7][7] = {
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 0, 0, 0}, // Top of 3x3 Cross
    {0, 0, 1, 1, 1, 0, 0}, // Middle of 3x3 Cross
    {0, 0, 0, 1, 0, 0, 0}, // Bottom of 3x3 Cross
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0}
};

// Local buffers for processing single 8x8 blocks
u8 current_letter[LETTER_SIZE * LETTER_SIZE];
u8 filtered_letter[LETTER_SIZE * LETTER_SIZE];
u8 led_pattern[8];

/**
 * Converts 8x8 grayscale pixel data to an 8-byte bitmask 
 * for the SPI LED Matrix (1 bit per pixel).
 */
void convert_to_led_pattern(u8 *pixels, u8 *pattern_out) {
    for (int r = 0; r < 8; r++) {
        u8 row_byte = 0;
        for (int c = 0; c < 8; c++) {
            // Thresholding: If pixel brightness > 127, turn LED ON
            if (pixels[r * 8 + c] > 127) {
                row_byte |= (1 << (7 - c)); // Map column to bit position
            }
        }
        pattern_out[r] = row_byte;
    }
}

/**
 * Extracts an 8x8 letter from the global 512x512 input_image array
 */
void get_letter_block(int start_row, int start_col, u8 *out_buf) {
    for (int r = 0; r < LETTER_SIZE; r++) {
        for (int c = 0; c < LETTER_SIZE; c++) {
            out_buf[r * LETTER_SIZE + c] = input_image[start_row + r][start_col + c];
        }
    }
}

int main() {
    init_platform();
    xil_printf("--- Alphabet IP Dilation Demo Started ---\n\r");

    // 1. Initialize SPI LED Hardware and Interrupts
    int spi_status = init_spi_led();
    if (spi_status != XST_SUCCESS) {
        xil_printf("ERROR: SPI LED Matrix Initialization Failed!\n\r");
        return -1;
    }

    // 2. Setup IP Block Kernel
    load_kernel_7x7(BASE_ADDR, cross_kernel);
    
    // Configure Control: Enable=1, ABS=0, Clamp=1
    // For Dilation, we want the raw sum to expand the shapes, so mult=1, shift=0
    set_filter_control(BASE_ADDR, 1, 0, 1, 0, 1);

    // 3. Process the Alphabet Grid
    // The image is 512x512. We move in steps of 8 pixels.
    for (int r_idx = 0; r_idx < IMG_HEIGHT; r_idx += LETTER_SIZE) {
        for (int c_idx = 0; c_idx < IMG_WIDTH; c_idx += LETTER_SIZE) {
            
            xil_printf("Processing Letter at Row:%d Col:%d\n\r", r_idx, c_idx);

            // STEP 1: Split/Extract the 8x8 letter
            get_letter_block(r_idx, c_idx, current_letter);

            // STEP 2: Display ORIGINAL on LED Matrix
            convert_to_led_pattern(current_letter, led_pattern);
            send_pattern_to_led(led_pattern);
            usleep(600000); // 0.6 second delay to view original

            // STEP 3: Feed to IP Block for Cross-Shaped Dilation
            // process_image handles the 7x7 sliding window over the 8x8 block
            apply_hardware_filter(BASE_ADDR, current_letter, filtered_letter, LETTER_SIZE, LETTER_SIZE);

            // STEP 4: Display FILTERED result on LED Matrix
            convert_to_led_pattern(filtered_letter, led_pattern);
            send_pattern_to_led(led_pattern);
            
            // Wait before moving to the next letter in the sequence
            usleep(1200000); // 1.2 second delay
        }
    }

    xil_printf("Workflow Complete.\n\r");
    cleanup_platform();
    return 0;
}