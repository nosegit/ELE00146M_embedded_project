/******************************************************************************
* Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 * uartns550   9600
 * uartlite    Configurable only in HW design
 * ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xparameters.h"
#include "xil_printf.h"
#include <stdint.h>
#include "xil_io.h" 
#include "myfilter.h"
#include "myfilter_utils.h" // <<< USING UTILITY FUNCTIONS

// --- Test Constants ---
// Base Address for the IP
#define BASE_ADDRESS XPAR_MYFILTER_0_BASEADDR 

// --- Test Image Setup ---
#define TEST_IMG_SIZE 7 // Smallest size to hold 7x7 window

// Dummy image (7x7 pixels) filled with the value 10
u8 dummy_image[TEST_IMG_SIZE][TEST_IMG_SIZE] = {
    {10, 10, 10, 10, 10, 10, 10},
    {10, 10, 10, 10, 10, 10, 10},
    {10, 10, 10, 10, 10, 10, 10},
    {10, 10, 10, 10, 10, 10, 10}, // Center Row (y=3)
    {10, 10, 10, 10, 10, 10, 10},
    {10, 10, 10, 10, 10, 10, 10},
    {10, 10, 10, 10, 10, 10, 10}
};

// 3x3 Box Blur Kernel centered in the 7x7 grid
// Sum of coefficients = 9.
s16 box_blur_kernel[7][7] = {
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 1, 0, 0}, // Row 2
    {0, 0, 1, 1, 1, 0, 0}, // Row 3 (Center)
    {0, 0, 1, 1, 1, 0, 0}, // Row 4
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0}
};

// --- Main Test Function ---
int main() {
    uint32_t result;
    u32 mult, shift;
    int i;
    
    init_platform();
    xil_printf("--- 1. STARTING FILTER CORE TEST (Using myfilter_utils.h) ---\n");
    
    // --- Step 1: Configure Coeffs and Math Parameters ---
    printf("\n--- 2. CONFIGURING COEFFICIENTS AND MATH ---\n");
    
    // Calculate multiplier and shift for Divisor = 9 (Box Blur)
    calculate_approx_params(9, &mult, &shift);
    printf("Calculated Math Params: Mult=%lu, Shift=%lu\n", mult, shift);
    
    // Load the 7x7 kernel to the hardware registers
    load_kernel_7x7(BASE_ADDRESS, box_blur_kernel);
    printf("Coefficient setup complete.\n");

    // Set control register: Enable=1, Abs=0, Clamp=1, using calculated mult/shift
    set_filter_control(BASE_ADDRESS, 1, 0, 1, shift, mult);
    printf("Control Register set.\n");
    
    // --- Step 2: Fill the Window (Simulate Streaming 7 Columns) ---
    // The center pixel value in the image is 10, and the sum of coefficients is 9.
    // Expected result: (9 * 10) / 9 = 10.
    printf("\n--- 3. FILLING WINDOW (Flat Field Value 10) ---\n");
    printf("Expected Result (Center Pixel): 10 (0x0A)\n");
    
    // Cast the 2D array to a flat pointer for the utility function
    u8 *img_ptr = (u8*)dummy_image;

    // Stream 7 columns (x=0 to x=6) to fill the 7x7 window
    // We send data for the center row (y_row=3)
    for (i = 0; i < TEST_IMG_SIZE; i++) {
        // x_col is the column index being loaded. y_row is the center row index (3)
        send_padded_column(BASE_ADDRESS, img_ptr, TEST_IMG_SIZE, TEST_IMG_SIZE, i, 3);
        
        // Short delay to allow hardware shift and calculation
        usleep(1000);
    }
    
    // Read the result after the window is full and calculation is stable.
    // This is the result for the center pixel at (x=3, y=3)
    result = get_filter_output(BASE_ADDRESS);
    
    printf("Result after flat field stream: 0x%X\n", result);
    if ((result & 0xFF) == 0x0A) { // Check the bottom 8 bits for pixel value 10
        printf("SUCCESS: Result matches expected blur value of 10.\n");
    } else {
        printf("FAILURE: Result 0x%X does not match expected value 0x0A.\n", result);
    }
    
    // --- Step 3: Shift in a column of 0s (Edge Test) ---
    // The test image is 7x7. The window is full. 
    // Now we shift in 3 columns of zero padding from the right (x=7, x=8, x=9) to move the edge.
    printf("\n--- 4. SHIFTING IN ZEROS (Edge Test) ---\n");
    // After 3 shifts, the 3x3 kernel covers 6 pixels of 10 and 3 pixels of 0 (padding).
    // Expected calculation: (6*10 + 3*0) / 9 = 60 / 9 = 6.66 -> clamped to 6 or 7.
    
    // We need to shift in columns x=7, x=8, and x=9 (zero padding)
    for (i = 7; i < 7 + 3; i++) {
        // The utility function `send_padded_column` handles the zero padding automatically 
        // when x_col is out of bounds (i.e., when i >= TEST_IMG_SIZE)
        send_padded_column(BASE_ADDRESS, img_ptr, TEST_IMG_SIZE, TEST_IMG_SIZE, i, 3);

        usleep(1000);
    }
    
    // Read the result after the edge has settled
    result = get_filter_output(BASE_ADDRESS);
    
    printf("Result after zero stream edge test: 0x%X\n", result);
    
    if ((result & 0xFF) == 0x06 || (result & 0xFF) == 0x07) { 
        printf("SUCCESS: Result 0x%X is within the expected range for the edge test (6 or 7).\n", result);
    } else {
        printf("FAILURE: Result 0x%X is unexpected for the edge test.\n", result);
    }

    printf("\n--- TEST COMPLETE ---\n");

    cleanup_platform();
    
    return 0;
}

