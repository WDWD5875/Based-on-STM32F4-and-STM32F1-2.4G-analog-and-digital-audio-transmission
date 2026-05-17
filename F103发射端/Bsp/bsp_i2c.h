#ifndef BSP_I2C_H
#define BSP_I2C_H

#include <stdint.h>

void bsp_i2c1_init(void);
uint8_t bsp_i2c1_write(uint8_t addr, const uint8_t *data, uint16_t len);

#define BSP_I2C1_OLED_ADDR_8BIT 0x78U

#endif /* BSP_I2C_H */
