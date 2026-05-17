#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include <stdint.h>

void bsp_audio_timer_init(void);
void bsp_audio_timer_start(void);
uint8_t bsp_audio_timer_take_due(void);

#endif /* BSP_TIMER_H */
