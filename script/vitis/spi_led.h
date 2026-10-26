#ifndef SPI_LED_H
#define SPI_LED_H

#include "xparameters.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xspi.h"
#include "sleep.h"
#include <stdio.h>

/* Constants */
#define LED_BUFFER_SIZE 8
#define SPI_DEVICE_ID    XPAR_AXI_QUAD_SPI_0_BASEADDR
#define INTC_DEVICE_ID   XPAR_XSCUGIC_0_BASEADDR
#define SPI_INTR_ID      61 // Standard for Zedboard SPI

/* Global Instances (Static to prevent naming conflicts) */
static XSpi SpiInstance;
static XScuGic IntcInstance;
static volatile int TransferInProgress;
static u8 SpiWriteBuffer[LED_BUFFER_SIZE];
static u8 SpiReadBuffer[LED_BUFFER_SIZE];

/* Internal Status Handler */
static void SpiStatusHandler(void *CallBackRef, u32 StatusEvent, u32 ByteCount) {
    if (StatusEvent == XST_SPI_TRANSFER_DONE) {
        TransferInProgress = FALSE;
    }
}

/**
 * @brief Initializes the SPI peripheral and GIC interrupts for the LED Matrix.
 * @return XST_SUCCESS if successful, XST_FAILURE otherwise.
 */
int init_spi_led() {
    XSpi_Config *ConfigPtr;
    XScuGic_Config *IntcConfig;
    int Status;

    // 1. Initialize SPI
    ConfigPtr = XSpi_LookupConfig(SPI_DEVICE_ID);
    if (ConfigPtr == NULL) return XST_DEVICE_NOT_FOUND;

    Status = XSpi_CfgInitialize(&SpiInstance, ConfigPtr, ConfigPtr->BaseAddress);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    XSpi_Reset(&SpiInstance);

    // 2. Initialize Interrupt Controller
    IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
    if (NULL == IntcConfig) return XST_FAILURE;

    Status = XScuGic_CfgInitialize(&IntcInstance, IntcConfig, IntcConfig->CpuBaseAddress);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    // 3. Register Exceptions
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT, 
                                 (Xil_ExceptionHandler)XScuGic_InterruptHandler, 
                                 &IntcInstance);

    // 4. Connect SPI to Interrupt Controller
    Status = XScuGic_Connect(&IntcInstance, SPI_INTR_ID, 
                             (Xil_ExceptionHandler)XSpi_InterruptHandler, 
                             (void *)&SpiInstance);
    if (Status != XST_SUCCESS) return Status;

    XScuGic_Enable(&IntcInstance, SPI_INTR_ID);
    Xil_ExceptionEnable();

    // 5. Configure SPI Options
    XSpi_SetStatusHandler(&SpiInstance, &SpiInstance, (XSpi_StatusHandler)SpiStatusHandler);
    
    u32 Options = XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION;
    Status = XSpi_SetOptions(&SpiInstance, Options);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    XSpi_Start(&SpiInstance);
    XSpi_SetSlaveSelect(&SpiInstance, 0x00); // Start with CS High

    return XST_SUCCESS;
}

/**
 * @brief Sends an 8-byte pattern to the STM32 LED Matrix Slave.
 * @param pattern Array of 8 bytes (one per column/row)
 */
void send_pattern_to_led(u8 *pattern) {
    for (int i = 0; i < LED_BUFFER_SIZE; i++) {
        SpiWriteBuffer[i] = pattern[i];
    }

    // Select Slave (CS Low)
    XSpi_SetSlaveSelect(&SpiInstance, 0x01);

    TransferInProgress = TRUE;
    XSpi_Transfer(&SpiInstance, SpiWriteBuffer, SpiReadBuffer, LED_BUFFER_SIZE);

    // Wait for interrupt to signal completion
    while (TransferInProgress);

    // Deselect Slave (CS High) - This triggers the STM32 update
    XSpi_SetSlaveSelect(&SpiInstance, 0x00);
}

/**
 * @brief Clears the LED Matrix (sends all 0s)
 */
void clear_led_matrix() {
    u8 empty[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    send_pattern_to_led(empty);
}

#endif // SPI_LED_H