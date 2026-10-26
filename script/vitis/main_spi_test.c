/*
 * Xilinx Quad SPI Master for STM32 8x8 LED Matrix Slave
 * Connected via PMOD JB1 (Standard SPI Mode 0)
 *
 * --------------------------------------------------------
 * WIRING CONNECTION (Zedboard <-> STM32F303)
 * --------------------------------------------------------
 * Signal       | Zedboard Pin | Port | STM32 Pin
 * -------------|--------------|------|----------
 * CS (SS)      | W12          | JB1  | PB12 (NSS)
 * MOSI (IO0)   | W11          | JB2  | PB15 (MOSI)
 * MISO (IO1)   | V10          | JB3  | PB14 (MISO)
 * SCLK         | W8           | JB4  | PB13 (SCK)
 * GND          | GND          | GND  | GND
 * --------------------------------------------------------
 */
#include "platform.h"
#include "xil_printf.h"
#include "spi_led.h"
#include "sleep.h"

// Define some 8x8 Patterns
u8 pattern_smiley[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};
u8 pattern_x[8]      = {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};
u8 pattern_box[8]    = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF};

int main() {
    init_platform();

    xil_printf("--- Zedboard SPI LED Matrix Wrapper Test ---\n\r");

    // 1. Initialize the SPI and Interrupts using the wrapper
    int Status = init_spi_led();
    if (Status != XST_SUCCESS) {
        xil_printf("SPI LED Initialization Failed!\n\r");
        return XST_FAILURE;
    }
    xil_printf("Initialization Successful.\n\r");

    // 2. Clear the matrix initially
    clear_led_matrix();
    usleep(500000);

    // 3. Main Loop: Cycle through patterns
    while(1) {
        xil_printf("Displaying: Smiley\n\r");
        send_pattern_to_led(pattern_smiley);
        usleep(1500000); // 1.5 second delay

        xil_printf("Displaying: X\n\r");
        send_pattern_to_led(pattern_x);
        usleep(1500000);

        xil_printf("Displaying: Box\n\r");
        send_pattern_to_led(pattern_box);
        usleep(1500000);

        xil_printf("Clearing Display...\n\r");
        clear_led_matrix();
        usleep(1000000);
    }

    cleanup_platform();
    return 0;
}