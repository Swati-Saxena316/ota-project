#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>

void lcd_hw_init(void);
void lcd_hw_clear(void);
void lcd_hw_set_cursor(uint8_t row, uint8_t col);
void lcd_hw_print(const char *str);

#endif
