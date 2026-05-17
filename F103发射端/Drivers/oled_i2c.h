#ifndef OLED_I2C_H
#define OLED_I2C_H

#include <stdint.h>

void oled_i2c_init(void);
void oled_i2c_task(void);
void oled_i2c_clear(void);
void oled_i2c_show_string(uint8_t line, uint8_t column, const char *string);
void oled_i2c_show_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length);
void oled_i2c_show_hex_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length);

#endif /* OLED_I2C_H */
