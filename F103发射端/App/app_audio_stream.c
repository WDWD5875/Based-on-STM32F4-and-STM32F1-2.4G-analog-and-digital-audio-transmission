#include "app_audio_stream.h"

#include "../Bsp/bsp_spi.h"
#include "../Bsp/bsp_time.h"
#include "../Bsp/bsp_timer.h"
#include "../Bsp/bsp_usart_dma.h"
#include "../Debug/debug_probe.h"
#include "../Drivers/nrf24l01.h"
#include "../Protocol/audio_packet.h"
#include "stm32f10x.h"
#include <string.h>

typedef struct {
    uint8_t ring[PCM_RING_SIZE];
    uint16_t write_index;
    uint16_t read_index;
    uint16_t status_flags;
    uint16_t ack_consumed_bytes;
    uint8_t running;
    uint8_t ack_ready_sent;
    uint8_t ack_credit_ready;
    uint8_t ack_waiting_block;
    uint8_t seq;
} AudioStreamContext;

static AudioStreamContext s_audio;

static uint16_t ring_available_unsafe(void)
{
    return (uint16_t)((s_audio.write_index - s_audio.read_index) & PCM_RING_MASK);
}

static uint16_t ring_free_unsafe(void)
{
    return (uint16_t)(PCM_RING_SIZE - 1U - ring_available_unsafe());
}

static uint16_t ring_available(void)
{
    return ring_available_unsafe();
}

static uint16_t ring_free(void)
{
    return ring_free_unsafe();
}

static void update_ring_debug(void)
{
    DBG_PcmRingAvailable = ring_available_unsafe();
    DBG_PcmRingFree = ring_free_unsafe();
    if (DBG_PcmRingAvailable < DBG_PcmRingAvailableMin) {
        DBG_PcmRingAvailableMin = DBG_PcmRingAvailable;
    }
    if (DBG_PcmRingAvailable > DBG_PcmRingAvailableMax) {
        DBG_PcmRingAvailableMax = DBG_PcmRingAvailable;
    }
}

static void ring_write_block_unsafe(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) {
        s_audio.ring[s_audio.write_index] = data[i];
        s_audio.write_index = (uint16_t)((s_audio.write_index + 1U) & PCM_RING_MASK);
    }
}

static void ring_read_block(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) {
        data[i] = s_audio.ring[s_audio.read_index];
        s_audio.read_index = (uint16_t)((s_audio.read_index + 1U) & PCM_RING_MASK);
    }
    update_ring_debug();
}

static uint16_t build_ack_status(void)
{
    uint16_t status = ACK_READY | s_audio.status_flags;
    const uint16_t available = ring_available();

    if (s_audio.running != 0U) {
        status |= ACK_RUNNING;
    }
    if (available < PCM_LOW_WATERMARK) {
        status |= ACK_BUF_LOW;
    }
    if (available > PCM_HIGH_WATERMARK) {
        status |= ACK_BUF_HIGH;
    }

    DBG_PcmRingAvailable = available;
    DBG_PcmRingFree = ring_free();
    return status;
}

static void send_audio_packet(void)
{
    uint8_t payload[NRF_PAYLOAD_SIZE];
    uint8_t pcm[AUDIO_SAMPLES_PER_PKT];
    uint8_t seq = s_audio.seq++;

    DBG_TxSeqLast = seq;

    if (ring_available() >= AUDIO_SAMPLES_PER_PKT) {
        ring_read_block(pcm, AUDIO_SAMPLES_PER_PKT);
    } else {
        memset(pcm, PCM_SILENCE, AUDIO_SAMPLES_PER_PKT);
        s_audio.status_flags |= ACK_BUF_EMPTY;
        DBG_PcmEmptyCount++;
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

        if (nrf24_send_payload(payload) == 0U) {
            s_audio.status_flags |= ACK_SPI_ERROR;
        }
        us = bsp_time_cycles_to_us(bsp_time_cycle_count() - start);
        DBG_TxSendBusyUs = us;
        if (us > DBG_TxSendBusyMaxUs) {
            DBG_TxSendBusyMaxUs = us;
        }
    }

    nrf_tx_packet_count++;

    s_audio.ack_consumed_bytes = (uint16_t)(s_audio.ack_consumed_bytes + AUDIO_SAMPLES_PER_PKT);
    if (s_audio.ack_consumed_bytes >= PCM_BLOCK_SIZE) {
        s_audio.ack_consumed_bytes = (uint16_t)(s_audio.ack_consumed_bytes - PCM_BLOCK_SIZE);
        s_audio.ack_credit_ready = 1U;
        s_audio.ack_ready_sent = 0U;
    }
    DBG_AckConsumedBytes = s_audio.ack_consumed_bytes;
    DBG_AckCreditReady = s_audio.ack_credit_ready;
    DBG_AckWaitingBlock = s_audio.ack_waiting_block;
}

void app_audio_stream_init(void)
{
    memset(&s_audio, 0, sizeof(s_audio));

    bsp_usart1_audio_init();
    bsp_audio_timer_init();
}

void app_audio_stream_task(void)
{
    bsp_usart1_rx_recover_task();

    if (bsp_usart1_take_rx_error() != 0U) {
        s_audio.status_flags |= ACK_BUF_OVERFLOW;
    }

    if (bsp_usart1_take_rx_block_ready() != 0U) {
        if (ring_free() >= PCM_BLOCK_SIZE) {
            ring_write_block_unsafe(bsp_usart1_rx_block, PCM_BLOCK_SIZE);
            DBG_PcmRxBlockCount++;
        } else {
            s_audio.status_flags |= ACK_BUF_OVERFLOW;
            DBG_PcmOverflowCount++;
        }
        if (s_audio.running == 0U) {
            s_audio.ack_ready_sent = 0U;
        }
        s_audio.ack_waiting_block = 0U;
        update_ring_debug();
        bsp_usart1_start_rx_dma();
    }

    if (s_audio.running == 0U && ring_available() >= PCM_PREBUFFER_SIZE) {
        s_audio.running = 1U;
        bsp_audio_timer_start();
    }

    if (bsp_audio_timer_take_due() != 0U) {
        send_audio_packet();
    }

    {
        const uint16_t available = ring_available();
        const uint16_t free_bytes = ring_free();
        const uint8_t prebuffer_ack = (s_audio.running == 0U && available < PCM_PREBUFFER_SIZE) ? 1U : 0U;
        const uint8_t stream_ack = (s_audio.running != 0U
                                    && s_audio.ack_credit_ready != 0U
                                    && s_audio.ack_waiting_block == 0U) ? 1U : 0U;

        if (s_audio.ack_ready_sent == 0U && free_bytes >= PCM_BLOCK_SIZE
            && (prebuffer_ack != 0U || stream_ack != 0U)) {
            bsp_usart1_send_ack(build_ack_status());
            s_audio.status_flags &= (uint16_t)~(ACK_BUF_EMPTY | ACK_BUF_OVERFLOW | ACK_SPI_ERROR);
            s_audio.ack_ready_sent = 1U;
            if (stream_ack != 0U) {
                s_audio.ack_credit_ready = 0U;
                s_audio.ack_waiting_block = 1U;
            }
            DBG_AckCreditReady = s_audio.ack_credit_ready;
            DBG_AckWaitingBlock = s_audio.ack_waiting_block;
            DBG_AckSendCount++;
        }
    }
}
