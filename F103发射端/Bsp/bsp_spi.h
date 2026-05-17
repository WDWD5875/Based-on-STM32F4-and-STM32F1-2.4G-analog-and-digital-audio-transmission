#ifndef BSP_SPI_H
#define BSP_SPI_H

#include <stdint.h>

void bsp_spi1_init(void);
uint8_t bsp_spi1_transfer(uint8_t data);

#endif /* BSP_SPI_H */
