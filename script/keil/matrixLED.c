#include "matrixLED.h"

void init_matrix_pins(void) {

	// Row and Column select pins are spread across
	// GPIO banks A, B and C, so enable them
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
	RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

	// Rows are directly selected using 8 GPIO pins:
	// GPIOA: PA6 PA10 PA7 PA8
	// GPIOB: PB3 PB4 PB5 PB10
	// Rows are active low, so enable pull up
	// to ensure default off
	// Configure pins as outputs
	// Careful with reset values!
	// Not clearing both bits may cause wrong pin  configurations.

	GPIOA->MODER &= ~((3UL << 2*ROWSELECTA_1) | (3UL <<2*ROWSELECTA_2) | (3UL << 2*ROWSELECTA_3) | (3UL <<2*ROWSELECTA_8));
	GPIOB->MODER &= ~((3UL << 2*ROWSELECTB_4) | (3UL <<2*ROWSELECTB_5) | (3UL << 2*ROWSELECTB_6) | (3UL <<2*ROWSELECTB_7));

	GPIOA->MODER |= ((1UL << 2*ROWSELECTA_1) | (1UL <<2*ROWSELECTA_2) | (1UL << 2*ROWSELECTA_3) | (1UL <<2*ROWSELECTA_8));
	GPIOB->MODER |= ((1UL << 2*ROWSELECTB_4) | (1UL <<2*ROWSELECTB_5) | (1UL << 2*ROWSELECTB_6) | (1UL <<2*ROWSELECTB_7));
	
	// Configure pull up resistor
	GPIOA->PUPDR &= ~((3UL << 2*ROWSELECTA_1) | (3UL <<2*ROWSELECTA_2) | (3UL << 2*ROWSELECTA_3) | (3UL <<2*ROWSELECTA_8));	
	GPIOB->PUPDR &= ~((3UL << 2*ROWSELECTB_4) | (3UL <<2*ROWSELECTB_5) | (3UL << 2*ROWSELECTB_6) | (3UL <<2*ROWSELECTB_7));
	
	GPIOA->PUPDR |= ((1UL << 2*ROWSELECTA_1) | (1UL <<2*ROWSELECTA_2) | (1UL << 2*ROWSELECTA_3) | (1UL <<2*ROWSELECTA_8));	
	GPIOB->PUPDR |= ((1UL << 2*ROWSELECTB_4) | (1UL <<2*ROWSELECTB_5) | (1UL << 2*ROWSELECTB_6) | (1UL <<2*ROWSELECTB_7));
	
	

	// Columns are multiplexed using 3 GPIO pins
	// and a 3:8 MUX:
	// GPIOB: PB8 PB9
	// GPIOC: PC7
	// Columns are active high
	// MUX output signals are already equipped with pull-ups, so none needed on the select signals
	// Configure pins as outputs
	GPIOB->MODER &= ~((3UL << 2*COLUMNSELECTB_A) | (3UL <<2*COLUMNSELECTB_B));
	GPIOC->MODER &= ~(3UL << 2*COLUMNSELECTC_C);
	
	GPIOB->MODER |= ((1UL << 2*COLUMNSELECTB_A) | (1UL <<2*COLUMNSELECTB_B));
	GPIOC->MODER |= (1UL << 2*COLUMNSELECTC_C);
	
	// Disable any pull up or down resistor
	GPIOB->PUPDR &= ~((3UL << 2*COLUMNSELECTB_A) | (3UL <<2*COLUMNSELECTB_B));
	GPIOC->PUPDR &= ~(3UL << 2*COLUMNSELECTC_C);

}



#define GET_BIT(val, bit) ( ((val) >> (bit)) & 0b1 )
// Function that updates the entire column of values from a bit pattern
void matrix_set_row_values(uint8_t val) {

	// YOUR CODE GOES HERE
//	ROWSELECTA_1 = val[0]
//	ROWSELECTA_2 = val[1] 
//	ROWSELECTA_3 = val[2] 
//	ROWSELECTB_4 = val[3]
//	ROWSELECTB_5 = val[4]
//	ROWSELECTB_6 = val[5]
//	ROWSELECTB_7 = val[6]
//	ROWSELECTA_8 = val[7]
	
	val = ~val;
	
	uint16_t GPIOA_MASK = (GET_BIT(val,0) << ROWSELECTA_1)	| (GET_BIT(val,1) << ROWSELECTA_2) 
											| (GET_BIT(val,2) << ROWSELECTA_3)	| (GET_BIT(val,7) << ROWSELECTA_8);
	GPIOA->ODR &= ~((1U<<ROWSELECTA_1) | (1U<<ROWSELECTA_2) | (1U<<ROWSELECTA_3) | (1U<<ROWSELECTA_8));
	GPIOA->ODR |= GPIOA_MASK;
	
	uint16_t GPIOB_MASK = (GET_BIT(val,3) << ROWSELECTB_4)	| (GET_BIT(val,4) << ROWSELECTB_5)
											| (GET_BIT(val,5) << ROWSELECTB_6)	| (GET_BIT(val,6) << ROWSELECTB_7);
	GPIOB->ODR &= ~((1U<<ROWSELECTB_4) | (1U<<ROWSELECTB_5) | (1U<<ROWSELECTB_6) | (1U<<ROWSELECTB_7));
	GPIOB->ODR |= GPIOB_MASK;

}
// Function that reads a unsigned int, which represents the MUX number,
// and converts it into the correct GPIO pin assignment
void matrix_column_select(uint8_t val) {
	
	// YOUR CODE GOES HERE
//	COLUMNSELECTB_A = val[0]
//	COLUMNSELECTB_B = val[1]
//	COLUMNSELECTC_C = val[2]
	
	uint16_t GPIOB_MASK = ((val&0b1) << COLUMNSELECTB_A) | ( ((val>>1)&0b1) << COLUMNSELECTB_B);
	GPIOB->ODR &= ~((1U << COLUMNSELECTB_A) | (1U << COLUMNSELECTB_B));
	GPIOB->ODR |= GPIOB_MASK;
	
	uint16_t GPIOC_MASK = ((val>>2)&0b1) << COLUMNSELECTC_C;
	GPIOC->ODR &= ~(1U << COLUMNSELECTC_C);
	GPIOC->ODR |= GPIOC_MASK;
	
}