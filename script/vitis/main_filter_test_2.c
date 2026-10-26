#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xil_io.h"
#include "myfilter.h"       // Standard Vivado header
#include "myfilter_utils.h"   // <<< USING UTILITY FUNCTIONS

// Define your IP Base Address
#define BASE_ADDR XPAR_MYFILTER_0_BASEADDR

// Define global image buffers (Statically allocated)
#include "image_data_lenna.h"
u8 output_image[IMG_HEIGHT][IMG_WIDTH];

int main() {
    init_platform();
    xil_printf("\n\r--- Filter Utils Usage Example ---\n\r");

    // ====================================================
    // 1. DEFINE THE KERNEL (Sharpen Filter)
    // ====================================================
    // A simple 3x3 Sharpen filter centered in the 7x7 grid.
    // Sum = 1. (5 - 4 neighbors)
    s16 kernel[7][7] = {
        { 1,   6,  15,  20,  15,   6,  1}, // Row 0
        { 6,  36,  90, 120,  90,  36,  6}, // Row 1
        {15,  90, 225, 300, 225,  90, 15}, // Row 2
        {20, 120, 300, 400, 300, 120, 20}, // Row 3 (Center)
        {15,  90, 225, 300, 225,  90, 15}, // Row 4
        { 6,  36,  90, 120,  90,  36,  6}, // Row 5
        { 1,   6,  15,  20,  15,   6,  1}  // Row 6
    };

    // ====================================================
    // 2. LOAD KERNEL TO HARDWARE
    // ====================================================
    load_kernel_7x7(BASE_ADDR, kernel);

    // ====================================================
    // 3. CALCULATE MATH PARAMS
    // ====================================================
    // int divisor = calculate_divisor(kernel);
    int divisor = 4096;
    u32 mult, shift;
    calculate_approx_params(divisor, &mult, &shift);
    printf("Math Params -> Divisor: %d, Mult: %lu, Shift: %lu\n\r", divisor, mult, shift);

    // // ====================================================
    // // 4. CONFIGURE CONTROL
    // // ====================================================
    // Enable=1, Abs=0, Clamp=1, shift=0, mult=1
    set_filter_control(BASE_ADDR, 1, 0, 1, shift, mult);
    // set_filter_control(BASE_ADDR, 1, 0, 1, 0, 1);



    // ====================================================
    // 5. PROCESS IMAGE (Using Utils)
    // ====================================================
    xil_printf("Processing Image...\n\r");

    apply_hardware_filter(BASE_ADDR, (u8*)input_image, (u8*)output_image, IMG_WIDTH, IMG_HEIGHT);

    
    export_image_raw_binary((u8*) output_image, IMG_WIDTH, IMG_HEIGHT);

    cleanup_platform();
    return 0;
}