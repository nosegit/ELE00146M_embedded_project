#ifndef FILTER_UTILS_H
#define FILTER_UTILS_H

#include "xil_io.h"
#include "xil_types.h"
#include "stdio.h"
#include "sleep.h"

// =============================================================
// REGISTER OFFSETS
// =============================================================
// Adjust these if your specific IP generation differs
#define REG_CONTROL_OFFSET  0x00
#define REG_DATA_1_OFFSET   0x08
#define REG_DATA_2_OFFSET   0x0C
#define REG_RESULT_OFFSET   0x10
#define REG_COEFF_BASE      0x14 // Register 5 starts here


/**
 * @brief Calculates the sum of all coefficients in a 7x7 kernel.
 * * Used to determine the divisor needed to normalize the filter
 * (e.g., to prevent the image from becoming too bright or dark).
 * * @param kernel The 7x7 signed integer kernel array.
 * @return The sum of all elements.
 */
static int calculate_divisor(s16 kernel[7][7]){

    int sum = 0;
    for (int i = 0; i < 7; i++){
        for(int j = 0; j < 7; j++){
            sum +=(int)kernel[i][j];
        }
    }

    return sum;

}

/**
 * @brief Calculates the multiplier and shift values for hardware approximation.
 * * Logic: Result = (Input * mult) >> shift.
 * We use a fixed shift of 16 for the best precision/safety balance.
 * * @param divisor The number to divide by (e.g., 9 for box blur).
 * @param mult Pointer to store the calculated multiplier.
 * @param shift Pointer to store the calculated shift amount.
 */
static void calculate_approx_params(int divisor, u32 *mult, u32 *shift) {
    *shift = 16;
    
    if (divisor <= 1) {
        *mult = 1;
        *shift = 0;
    } else {
        // Calculate (2^16) / divisor
        // Add half the divisor (divisor >> 1) to the numerator to rounding number
        *mult = ((1 << 16) + (divisor >> 1)) / divisor;
    }
}

/**
 * @brief Writes to the Filter Control Register.
 * * Register Layout:
 * - Bit 0: Enable
 * - Bit 1: Soft Reset (Optional)
 * - Bit 2: ABS (Absolute Value)
 * - Bit 3: Clamp (0-255)
 * - Bits 8-12: Shift Amount
 * - Bits 16-31: Multiplier
 * * @param BaseAddr Base address of the IP.
 * @param enable Set to 1 to enable the block.
 * @param abs Set to 1 to force absolute value on result.
 * @param clamp Set to 1 to clamp result between 0-255.
 * @param shift The shift amount (5 bits).
 * @param mult The multiplier value (16 bits).
 */
static void set_filter_control(UINTPTR BaseAddr, u8 enable, u8 abs, u8 clamp, u32 shift, u32 mult) {
    u32 val = 0;
    
    if (enable) val |= (1 << 0);
    if (abs)    val |= (1 << 2);
    if (clamp)  val |= (1 << 3);
    
    // Mask shift to 5 bits to be safe
    val |= ((shift & 0x1F) << 8);
    // Mask mult to 16 bits
    val |= ((mult & 0xFFFF) << 16);
    
    Xil_Out32(BaseAddr + REG_CONTROL_OFFSET, val);
}

/**
 * @brief Loads a 7x7 kernel into the hardware registers.
 * * Packs two 16-bit coefficients into one 32-bit register.
 * - Even Indices (0, 2...) -> Lower 16 bits [15:0]
 * - Odd Indices  (1, 3...) -> Upper 16 bits [31:16]
 * * @param BaseAddr Base address of the IP.
 * @param kernel The 7x7 signed integer kernel array.
 */
static void load_kernel_7x7(UINTPTR BaseAddr, s16 kernel[7][7]) {
    u32 reg_val = 0;
    int reg_offset = 0;
    int coeff_count = 0;
    
    // Loop through 7x7 grid
    for (int r = 0; r < 7; r++) {
        for (int c = 0; c < 7; c++) {
            
            s16 coeff = kernel[r][c];
            u32 coeff_masked = (u32)(coeff & 0xFFFF);
            
            if (coeff_count % 2 == 0) {
                // EVEN index (2, 4, ...): Lower 16 bits [15:0]
                reg_val = coeff_masked;
            } else {

                // ODD index (1, 3, ...): Upper 16 bits [31:16]
                reg_val |= (coeff_masked << 16);

                // Write complete register
                Xil_Out32(BaseAddr + REG_COEFF_BASE + reg_offset, reg_val);
                
                reg_offset += 4; // Next register
                reg_val = 0;     // Reset
                
            }
            coeff_count++;
        }
    }
    
    // Handle the last coefficient (Index 48) which is Even.
    // Based on logic above, it sits in the Upper 16 bits.
    // We must write this final partial register.
    if (coeff_count % 2 != 0) {
         Xil_Out32(BaseAddr + REG_COEFF_BASE + reg_offset, reg_val);
    }

}

/**
 * @brief Safely retrieves a pixel from the image buffer with padding.
 * * Returns 0 (Black) if coordinates are out of bounds (Zero Padding).
 * Uses flat array indexing: index = (y * width) + x.
 * * @param image Pointer to the image buffer.
 * @param width Width of the image.
 * @param height Height of the image.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return The pixel value or 0 if out of bounds.
 */
static u8 get_pixel_safe(u8 *image, int width, int height, int x, int y) {
    // printf("x:%d y:%d value:", x, y);
    if (x < 0 || x >= width || y < 0 || y >= height) {
        // printf("0\n");
        return 0; // ZERO PADDING
    }
    // printf("%d\n",image[(y * width) + x]);
    return image[(y * width) + x];
}

/**
 * @brief Sends a column of 7 pixels to the hardware window registers.
 * * Uses 'get_pixel_safe' to automatically handle edge padding.
 * * @param BaseAddr Base address of the IP.
 * @param image Pointer to the source image.
 * @param width Image width.
 * @param height Image height.
 * @param x_col The X coordinate of the CENTER pixel.
 * @param y_row The Y coordinate of the CENTER pixel.
 */
static void send_padded_column(UINTPTR BaseAddr, u8 *image, int width, int height, int x_col, int y_row) {
    u32 pack1, pack2;

    // Pack Top Half (Rows -3, -2, -1, 0 relative to center)
    pack1 = (get_pixel_safe(image, width, height, x_col, y_row-3))       | 
            (get_pixel_safe(image, width, height, x_col, y_row-2) << 8)  | 
            (get_pixel_safe(image, width, height, x_col, y_row-1) << 16) | 
            (get_pixel_safe(image, width, height, x_col, y_row)   << 24);
            
    // Pack Bottom Half (Rows +1, +2, +3 relative to center)
    pack2 = (get_pixel_safe(image, width, height, x_col, y_row+1))       | 
            (get_pixel_safe(image, width, height, x_col, y_row+2) << 8)  | 
            (get_pixel_safe(image, width, height, x_col, y_row+3) << 16);

    // printf("x_col: %d\n", x_col);
    // printf("    pack1: %08x\n", pack1);
    // printf("    pack2: %08x\n", pack2);

    Xil_Out32(BaseAddr + REG_DATA_1_OFFSET, pack1);
    Xil_Out32(BaseAddr + REG_DATA_2_OFFSET, pack2); // Triggers Shift
}

/**
 * @brief Reads the result from the hardware filter.
 * * @param BaseAddr Base address of the IP.
 * @return The calculated pixel value.
 */
static u32 get_filter_output(UINTPTR BaseAddr) {
    return Xil_In32(BaseAddr + REG_RESULT_OFFSET);
}



/**
 * @brief Processes an entire image using the 7x7 Hardware Filter IP.
 * * @param BaseAddr      Base address of the Hardware IP (e.g., XPAR_MYFILTER_0_BASEADDR)
 * @param input_image   Pointer to the source 8-bit grayscale image buffer
 * @param output_image  Pointer to the destination 8-bit grayscale image buffer
 * @param width         Width of the image in pixels
 * @param height        Height of the image in pixels
 */
static void apply_hardware_filter(UINTPTR BaseAddr, u8 *input_image, u8 *output_image, int width, int height) {
    
    // Iterate through every row of the image
    for (int y = 0; y < height; y++) {
        
        // --- A. PRIME THE WINDOW (Cold Start) ---
        // To calculate the very first pixel (x=0), the 7x7 window needs 
        // to be filled with the center column and the 3 columns to its left.
        // We send indices -3, -2, -1, 0, 1, 2.
        // send_padded_column handles the negative indices by applying zero-padding.
        for (int c = -3; c <= 2; c++) {
            send_padded_column(BaseAddr, input_image, width, height, c, y);
        }
        
        // --- B. SLIDING WINDOW ---
        // For every pixel in the row, we slide the window one step to the right.
        for (int x = 0; x < width; x++) {
            
            // 1. Send the new incoming column at the right edge of the 7x7 window.
            // If the center is x, the right edge is x + 3.
            send_padded_column(BaseAddr, input_image, width, height, x + 3, y);
            
            // 2. Read the Result
            // The Hardware IP calculates the sum of products (Convolution)
            // and provides the result in the Result Register.
            u32 result = get_filter_output(BaseAddr);
            
            // 3. Store in the output buffer
            // Flattened indexing: (y * width) + x
            output_image[(y * width) + x] = (u8)result;
        }
    }
}

/**
 * @brief Sends the image as raw binary bytes with a metadata header.
 * * Flow: 
 * 1. Sync Header (4 bytes: 0xAA, 0xBB, 0xCC, 0xDD)
 * 2. Width (2 bytes, Big Endian)
 * 3. Height (2 bytes, Big Endian)
 * 4. Raw Pixels (Width * Height bytes)
 * * @param image Pointer to the image buffer.
 * @param width Width of the image.
 * @param height Height of the image.
 */
static void export_image_raw_binary(u8 *image, int width, int height) {
    // 1. Send Sync Header
    outbyte(0xAA); 
    outbyte(0xBB);
    outbyte(0xCC);
    outbyte(0xDD);

    // 2. Send Width (Break 16-bit into two 8-bit bytes)
    outbyte((u8)((width >> 8) & 0xFF));
    outbyte((u8)(width & 0xFF));

    // 3. Send Height
    outbyte((u8)((height >> 8) & 0xFF));
    outbyte((u8)(height & 0xFF));

    // 4. Send Raw Pixels
    int total_pixels = width * height;
    for (int i = 0; i < total_pixels; i++) {
        outbyte(image[i]);
    }
}

#endif