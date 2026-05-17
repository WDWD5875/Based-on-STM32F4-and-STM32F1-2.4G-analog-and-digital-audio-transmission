#include "app_adc_stream.h"

#include "../Bsp/bsp_adc_dma.h"
#include "../Bsp/bsp_spi.h"
#include "../Bsp/bsp_time.h"
#include "../Debug/debug_probe.h"
#include "../Drivers/nrf24l01.h"
#include "../Protocol/audio_packet.h"
#include "stm32f10x.h"
#include <string.h>

static uint8_t s_adc_seq = 0U;

static void send_adc_half(uint8_t half_index)
{
    uint16_t *src = (half_index == 1U)
                        ? &bsp_adc_raw_buffer[0]
                        : &bsp_adc_raw_buffer[ADC_DMA_HALF_SIZE];
    uint8_t pcm[AUDIO_SAMPLES_PER_PKT];
    uint8_t payload[NRF_PAYLOAD_SIZE];
    uint8_t seq = s_adc_seq++;

    DBG_TxSeqLast = seq;

    for (uint8_t i = 0U; i < AUDIO_SAMPLES_PER_PKT; i++) {
        pcm[i] = (uint8_t)(src[i] >> 4);
    }

    audio_packet_make(payload, seq, pcm);
    memcpy((void *)DBG_LastTxPayload, payload, NRF_PAYLOAD_SIZE);

    {
        uint32_t now = bsp_time_cycle_count();
        if (DBG_TxLastCycle != 0U) {
            uint32_t dt = now - DBG_TxLastCycle;
            uint32_t us = bsp_time_cycles_to_us(dt);
            DBG_TxIntervalCycles = dt;
            DBG_TxIntervalUs = us;
            if (us > DBG_TxIntervalMaxUs) {
                DBG_TxIntervalMaxUs = us;
            }
        }
        DBG_TxLastCycle = now;
    }

    {
        uint32_t start = bsp_time_cycle_count();
        uint32_t us;

        (void)nrf24_send_payload(payload);
        us = bsp_time_cycles_to_us(bsp_time_cycle_count() - start);
        DBG_TxSendBusyUs = us;
        if (us > DBG_TxSendBusyMaxUs) {
            DBG_TxSendBusyMaxUs = us;
        }
    }

    nrf_tx_packet_count++;
    DBG_TxBacklog = adc_half_count - nrf_tx_packet_count;
}

void app_adc_stream_init(void)
{
    s_adc_seq = 0U;

    bsp_adc_dma_tim3_init();
}

void app_adc_stream_task(void)
{
    const uint8_t ready = bsp_adc_dma_take_ready_half();

    if (ready != 0U) {
        send_adc_half(ready);
    }
}
