#ifndef BSP_ADC_DMA_H
#define BSP_ADC_DMA_H

#include "../Protocol/audio_defs.h"

extern uint16_t bsp_adc_raw_buffer[ADC_DMA_BUF_SIZE];

void bsp_adc_dma_tim3_init(void);
uint8_t bsp_adc_dma_take_ready_half(void);

#endif /* BSP_ADC_DMA_H */
