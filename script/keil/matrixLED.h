#ifndef __MATRIXLED_H__
#define __MATRIXLED_H__

#include "stm32f3xx.h"

//Pin definitions for row select
#define ROWSELECTA_1 7
#define ROWSELECTA_2 6
#define ROWSELECTA_3 10
#define ROWSELECTB_4 3
#define ROWSELECTB_5 5
#define ROWSELECTB_6 4
#define ROWSELECTB_7 10
#define ROWSELECTA_8 8
//Pin definitions for column select
#define COLUMNSELECTB_A 8
#define COLUMNSELECTB_B 9
#define COLUMNSELECTC_C 7

void init_matrix_pins(void);
//Function that updates the an entire column of values from a bit pattern
void matrix_set_row_values(uint8_t val);
//Function that reads a unsigned int, which represents the MUX number,
// and converts it into the correct GPIO pin assignment
void matrix_column_select(uint8_t val);


#endif