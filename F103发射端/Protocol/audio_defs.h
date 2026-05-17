#ifndef AUDIO_DEFS_H
#define AUDIO_DEFS_H

#include <stdint.h>

#define USART_AUDIO_BAUD       921600U
#define TIM2_AUDIO_PKT_ARR     48979U
#define TIM3_ADC_TRIG_ARR      1632U

#define AUDIO_PACKET_TYPE      0xA1U
#define AUDIO_SAMPLES_PER_PKT  30U
#define NRF_PAYLOAD_SIZE       32U
#define ADC_DMA_HALF_SIZE      AUDIO_SAMPLES_PER_PKT
#define ADC_DMA_BUF_SIZE       (ADC_DMA_HALF_SIZE * 2U)
#define PCM_BLOCK_SIZE         450U
#define PCM_RING_SIZE          4096U
#define PCM_RING_MASK          (PCM_RING_SIZE - 1U)
#define PCM_PREBUFFER_SIZE     2250U
#define PCM_LOW_WATERMARK      900U
#define PCM_HIGH_WATERMARK     3300U
#define PCM_SILENCE            0x80U

#define ACK_READY              0x0001U
#define ACK_BUF_LOW            0x0002U
#define ACK_BUF_HIGH           0x0004U
#define ACK_BUF_EMPTY          0x0008U
#define ACK_BUF_OVERFLOW       0x0010U
#define ACK_SPI_ERROR          0x0020U
#define ACK_RUNNING            0x0040U

#endif /* AUDIO_DEFS_H */
