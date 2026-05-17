#ifndef BSP_TIME_H
#define BSP_TIME_H

#include <stdint.h>

void bsp_time_init(void);
void bsp_delay_us(uint32_t us);
void bsp_delay_ms(uint32_t ms);
uint32_t bsp_time_cycle_count(void);
uint32_t bsp_time_cycles_to_us(uint32_t cycles);

#endif /* BSP_TIME_H */
