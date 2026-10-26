#include "stm32f3xx.h"
#include "matrixLED.h"
#include "spi_slave.h" 
#include "string.h"

// Function Prototypes
void device_setup(void);
void TIM4_config(void);

// Global buffer for LED Matrix pattern
// Updated by SPI interrupt, read by TIM4 interrupt
char pattern8x8Col[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

int main(void) { 
	// 1. Setup System Clock (72MHz / 36MHz APB1)
	device_setup();
	
	// 2. Initialize LED Matrix Pins (Rows/Cols)
	init_matrix_pins();
	
	// 3. Initialize Timer for Matrix Multiplexing
	TIM4_config();
	
	// 4. Initialize SPI Slave to receive data from Zedboard
	spi_slave_init();
	
	// Main loop is empty; everything is handled by Interrupts (TIM4 + SPI2)
	for(;;){
		// Optional: Add heartbeat LED blink on PC13 or PA5 to show system is alive
		// but not strictly necessary for the matrix operation.
	}
}

// --- Matrix Multiplexing Timer ---
uint8_t cnt = 0;
void TIM4_IRQHandler(){
	
	/* Check finish count flag on timer 4*/
	if(TIM4->SR & TIM_SR_UIF){
		/* Reset flag */
		TIM4->SR &= ~TIM_SR_UIF;
		
		/* Update Matrix Row/Col */
		// We write the value from our global buffer to the current row
		matrix_set_row_values(pattern8x8Col[cnt]);
		matrix_column_select(cnt);
		
		cnt++;
		if(cnt >= 8) cnt = 0;
	}
}

void TIM4_config(){
	//Enable TIM4 clock (bit 2, 1 << 2)
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
	
	//Enable interrupt on reaching max value
	TIM4->DIER |= TIM_DIER_UIE;

	//Register with Nested Vector Interrupt Controller
	NVIC_EnableIRQ(TIM4_IRQn);
	
	/* config prescaler/automatic reload register */
	TIM4->PSC = 360-1;	// clock is 36,000,000 hz--prescaler--> 100,000 hz
	TIM4->ARR = 125-1;  // ~800 hz refresh rate
	
	//Set count to zero
	TIM4->CNT = 0UL;
	
	//Enable timer
	TIM4->CR1 |= (1UL << 0UL);
}

//Function to enable CPU clock running at 36MHz 
//APB1 Clock is 36MHz, HSE Clock Domain is 72MHz
void device_setup(void) {
		//Enable HSE (High Speed External Clock) - 8MHz ext. clock
		RCC->CR |= (1UL << 16UL);
		//Wait for HSE clock to be stable
		while(!(RCC->CR & (1UL << 17UL))) {}

		//Configure PLL to upscale HSE to 36MHz
		
		//Set PLL prescaller to devide by 2 - 4MHz 
		//Clear bits
		RCC->CFGR2 &= ~(3UL << 15UL);
		RCC->CFGR2 |= (1UL << 0UL);

		//Enable HSE as PLL source and multiply by 9 (9*4 = 36)
		RCC->CFGR |= (1UL << 16UL);
		RCC->CFGR |= (7UL << 18UL);

		//Enable PLL to active
		RCC->CR |= (1UL << 24UL);
		//Wait for the PLL clock to be stable 
		while(!(RCC->CR & (1U << 25U))) {}

		//Select PLL as main clock source
		RCC->CFGR &= ~(3UL << 0UL);
		RCC->CFGR |= (2UL << 0UL);

		//Wait until system clock is stable
		while ((RCC->CFGR & (3UL << 2UL)) != (1UL << 3UL)) {}
			
		//Update system clock in system config
		SystemCoreClockUpdate();
}