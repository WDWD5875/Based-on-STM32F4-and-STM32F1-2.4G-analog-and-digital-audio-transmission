#include "debug_probe.h"

volatile uint8_t DBG_LastTxPayload[NRF_PAYLOAD_SIZE];
volatile uint8_t DBG_NrfLastStatus = 0U;
volatile uint8_t DBG_NrfConfig = 0U;
volatile uint8_t DBG_NrfRfCh = 0U;
volatile uint8_t DBG_NrfRfSetup = 0U;
volatile uint8_t DBG_NrfEnAa = 0U;
volatile uint8_t DBG_NrfSetupRetr = 0U;
volatile uint8_t DBG_NrfFifoStatus = 0U;
volatile uint8_t DBG_NrfSpiSelfTestPass = 0U;
volatile uint8_t DBG_NrfSpiOldRfCh = 0U;
volatile uint8_t DBG_NrfSpiReadBack = 0U;
volatile uint32_t DBG_TxLastCycle = 0U;
volatile uint32_t DBG_TxIntervalCycles = 0U;
volatile uint32_t DBG_TxIntervalUs = 0U;
volatile uint32_t DBG_TxIntervalMaxUs = 0U;
volatile uint8_t DBG_TxSeqLast = 0U;
volatile uint32_t DBG_TxSendBusyUs = 0U;
volatile uint32_t DBG_TxSendBusyMaxUs = 0U;
volatile uint32_t DBG_AdcHalfIntervalUs = 0U;
volatile uint32_t DBG_AdcHalfLastCycle = 0U;
volatile uint32_t DBG_TxBacklog = 0U;
volatile uint16_t DBG_PcmRingAvailable = 0U;
volatile uint16_t DBG_PcmRingFree = 0U;
volatile uint16_t DBG_PcmRingAvailableMin = 0xFFFFU;
volatile uint16_t DBG_PcmRingAvailableMax = 0U;
volatile uint32_t DBG_PcmRxBlockCount = 0U;
volatile uint32_t DBG_PcmOverflowCount = 0U;
volatile uint32_t DBG_PcmEmptyCount = 0U;
volatile uint16_t DBG_AckConsumedBytes = 0U;
volatile uint8_t DBG_AckCreditReady = 0U;
volatile uint32_t DBG_AckSendCount = 0U;
volatile uint8_t DBG_AckWaitingBlock = 0U;
volatile uint32_t DBG_UsartOreCount = 0U;
volatile uint32_t DBG_DmaTeCount = 0U;
volatile uint32_t DBG_DmaRestartCount = 0U;
volatile uint16_t DBG_UsartSr = 0U;
volatile uint16_t DBG_DmaCndtr = 0U;

volatile uint32_t nrf_tx_packet_count = 0U;
volatile uint32_t nrf_tx_ok_count = 0U;
volatile uint32_t nrf_tx_fail_count = 0U;
volatile uint32_t nrf_tx_fifo_full_count = 0U;
volatile uint32_t adc_half_count = 0U;
volatile uint32_t adc_half_overrun_count = 0U;

void debug_probe_init(void)
{
    DBG_PcmRingAvailableMin = 0xFFFFU;
    DBG_PcmRingAvailableMax = 0U;
}
