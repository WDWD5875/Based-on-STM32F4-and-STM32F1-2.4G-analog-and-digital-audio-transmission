#ifndef BSP_USART_DMA_H
#define BSP_USART_DMA_H

#include "../Protocol/audio_defs.h"

extern uint8_t bsp_usart1_rx_block[PCM_BLOCK_SIZE];

void bsp_usart1_audio_init(void);
void bsp_usart1_start_rx_dma(void);
uint8_t bsp_usart1_take_rx_block_ready(void);
uint8_t bsp_usart1_take_rx_error(void);
void bsp_usart1_rx_recover_task(void);
void bsp_usart1_send_ack(uint16_t status);

#endif /* BSP_USART_DMA_H */
