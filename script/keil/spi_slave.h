#ifndef __SPI_SLAVE_H__
#define __SPI_SLAVE_H__

#include "stm32f3xx.h"
#include <string.h>

// External reference to the ACTIVE pattern array used by main.c/timer
extern char pattern8x8Col[8];

// SHADOW buffer to receive new data
static char spi_shadow_buffer[8];

// Internal buffer index
static volatile uint8_t spi_rx_index = 0;

void spi_slave_init(void) {
    // 1. Enable Clocks for SPI2 and GPIOB
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    RCC->AHBENR  |= RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; // For EXTI

    // 2. Configure GPIOs: PB12(NSS), PB13(SCK), PB14(MISO), PB15(MOSI)
    // Mode: Alternate Function (10)
    GPIOB->MODER &= ~((3U << (12 * 2)) | (3U << (13 * 2)) | (3U << (14 * 2)) | (3U << (15 * 2)));
    GPIOB->MODER |=  ((2U << (12 * 2)) | (2U << (13 * 2)) | (2U << (14 * 2)) | (2U << (15 * 2)));

    // Pull-Up on NSS (PB12) - Critical for stability
    GPIOB->PUPDR &= ~(3U << (12 * 2)); 
    GPIOB->PUPDR |=  (1U << (12 * 2)); 

    // Alternate Function: AF5 (SPI2)
    GPIOB->AFR[1] &= ~((0xF << (4 * (12 - 8))) | (0xF << (4 * (13 - 8))) | (0xF << (4 * (14 - 8))) | (0xF << (4 * (15 - 8))));
    GPIOB->AFR[1] |=  ((0x5 << (4 * (12 - 8))) | (0x5 << (4 * (13 - 8))) | (0x5 << (4 * (14 - 8))) | (0x5 << (4 * (15 - 8))));

    // 3. Configure SPI2 as Slave
    SPI2->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI);
    SPI2->CR2 &= ~SPI_CR2_DS;
    SPI2->CR2 |= (0x7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH | SPI_CR2_RXNEIE;
    SPI2->CR1 |= SPI_CR1_SPE;

    // 4. Configure EXTI for PB12 (NSS) - Rising Edge (End of Frame)
    SYSCFG->EXTICR[3] &= ~(0xF << 0);
    SYSCFG->EXTICR[3] |=  (0x1 << 0); // Port B

    EXTI->IMR  |= (1 << 12);
    EXTI->RTSR |= (1 << 12); // Rising Edge Trigger
    EXTI->FTSR &= ~(1 << 12);

    // 5. Enable NVIC Interrupts
    NVIC_SetPriority(SPI2_IRQn, 1);
    NVIC_EnableIRQ(SPI2_IRQn);
    
    // EXTI Priority is slightly lower or equal. 
    // Actually, setting EXTI higher (0) is good to catch the end-of-frame immediately.
    NVIC_SetPriority(EXTI15_10_IRQn, 0); 
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

// SPI2 Interrupt Handler - Fills the SHADOW buffer
void SPI2_IRQHandler(void) {
    if (SPI2->SR & SPI_SR_RXNE) {
        uint8_t received_byte = (uint8_t)SPI2->DR;
        
        // Write to Shadow Buffer, NOT the active display buffer
        if (spi_rx_index < 8) {
            spi_shadow_buffer[spi_rx_index] = received_byte;
            spi_rx_index++;
        }
    }
}

// EXTI Handler - Sync & Swap
// Fires when CS goes High (Transaction Complete)
void EXTI15_10_IRQHandler(void) {
    if (EXTI->PR & (1 << 12)) {
        EXTI->PR |= (1 << 12); // Clear flag
        
        // ATOMIC SWAP:
        // Only update the display if we received exactly 8 bytes.
        // This prevents displaying partial/corrupted frames.
        if (spi_rx_index == 8) {
            // Copy shadow buffer to active buffer safely
            // (The Timer Interrupt reads pattern8x8Col, so this update is quick)
            memcpy(pattern8x8Col, spi_shadow_buffer, 8);
        }

        // Reset index for the next frame
        spi_rx_index = 0;
    }
}

#endif