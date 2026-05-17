#ifndef DEBUG_PROBE_H
#define DEBUG_PROBE_H

#include "../Protocol/audio_defs.h"

void debug_probe_init(void);

extern volatile uint8_t DBG_LastTxPayload[NRF_PAYLOAD_SIZE];
extern volatile uint8_t DBG_NrfLastStatus;
extern volatile uint8_t DBG_NrfConfig;
extern volatile uint8_t DBG_NrfRfCh;
extern volatile uint8_t DBG_NrfRfSetup;
extern volatile uint8_t DBG_NrfEnAa;
extern volatile uint8_t DBG_NrfSetupRetr;
extern volatile uint8_t DBG_NrfFifoStatus;
extern volatile uint8_t DBG_NrfSpiSelfTestPass;
extern volatile uint8_t DBG_NrfSpiOldRfCh;
extern volatile uint8_t DBG_NrfSpiReadBack;
extern volatile uint32_t DBG_TxLastCycle;
extern volatile uint32_t DBG_TxIntervalCycles;
extern volatile uint32_t DBG_TxIntervalUs;
extern volatile uint32_t DBG_TxIntervalMaxUs;
extern volatile uint8_t DBG_TxSeqLast;
extern volatile uint32_t DBG_TxSendBusyUs;
extern volatile uint32_t DBG_TxSendBusyMaxUs;
extern volatile uint32_t DBG_AdcHalfIntervalUs;
extern volatile uint32_t DBG_AdcHalfLastCycle;
extern volatile uint32_t DBG_TxBacklog;
extern volatile uint16_t DBG_PcmRingAvailable;
extern volatile uint16_t DBG_PcmRingFree;
extern volatile uint16_t DBG_PcmRingAvailableMin;
extern volatile uint16_t DBG_PcmRingAvailableMax;
extern volatile uint32_t DBG_PcmRxBlockCount;
extern volatile uint32_t DBG_PcmOverflowCount;
extern volatile uint32_t DBG_PcmEmptyCount;
extern volatile uint16_t DBG_AckConsumedBytes;
extern volatile uint8_t DBG_AckCreditReady;
extern volatile uint32_t DBG_AckSendCount;
extern volatile uint8_t DBG_AckWaitingBlock;
extern volatile uint32_t DBG_UsartOreCount;
extern volatile uint32_t DBG_DmaTeCount;
extern volatile uint32_t DBG_DmaRestartCount;
extern volatile uint16_t DBG_UsartSr;
extern volatile uint16_t DBG_DmaCndtr;

extern volatile uint32_t nrf_tx_packet_count;
extern volatile uint32_t nrf_tx_ok_count;
extern volatile uint32_t nrf_tx_fail_count;
extern volatile uint32_t nrf_tx_fifo_full_count;
extern volatile uint32_t adc_half_count;
extern volatile uint32_t adc_half_overrun_count;

#endif /* DEBUG_PROBE_H */
