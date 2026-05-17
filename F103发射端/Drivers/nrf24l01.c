#include "nrf24l01.h"

#include "../Bsp/bsp_spi.h"
#include "../Bsp/bsp_time.h"
#include "../Debug/debug_probe.h"
#include "../Protocol/audio_defs.h"
#include "stm32f10x.h"

#define NRF_CE_PORT            GPIOB
#define NRF_CE_PIN             GPIO_Pin_0
#define NRF_CSN_PORT           GPIOA
#define NRF_CSN_PIN            GPIO_Pin_4

#define NRF_CMD_R_REGISTER     0x00U
#define NRF_CMD_W_REGISTER     0x20U
#define NRF_CMD_W_TX_PAYLOAD   0xA0U
#define NRF_CMD_FLUSH_TX       0xE1U
#define NRF_CMD_ACTIVATE       0x50U
#define NRF_CMD_NOP            0xFFU

#define NRF_REG_CONFIG         0x00U
#define NRF_REG_EN_AA          0x01U
#define NRF_REG_EN_RXADDR      0x02U
#define NRF_REG_SETUP_AW       0x03U
#define NRF_REG_SETUP_RETR     0x04U
#define NRF_REG_RF_CH          0x05U
#define NRF_REG_RF_SETUP       0x06U
#define NRF_REG_STATUS         0x07U
#define NRF_REG_RX_ADDR_P0     0x0AU
#define NRF_REG_TX_ADDR        0x10U
#define NRF_REG_RX_PW_P0       0x11U
#define NRF_REG_FIFO_STATUS    0x17U
#define NRF_REG_DYNPD          0x1CU
#define NRF_REG_FEATURE        0x1DU

#define NRF_STATUS_RX_DR       0x40U
#define NRF_STATUS_TX_DS       0x20U
#define NRF_STATUS_MAX_RT      0x10U
#define NRF_FIFO_TX_FULL       0x20U

#define NRF_RF_CHANNEL         76U
#define NRF_RF_SETUP_2M_0DBM   0x0FU
#define NRF_RF_SETUP_ACTIVE    NRF_RF_SETUP_2M_0DBM
#define NRF_CONFIG_TX          0x3EU

static const uint8_t NRF_ADDR[5] = {0xE7U, 0xE7U, 0xE7U, 0xE7U, 0xE7U};

static inline void nrf_ce_low(void) { GPIO_ResetBits(NRF_CE_PORT, NRF_CE_PIN); }
static inline void nrf_ce_high(void) { GPIO_SetBits(NRF_CE_PORT, NRF_CE_PIN); }
static inline void nrf_csn_low(void) { GPIO_ResetBits(NRF_CSN_PORT, NRF_CSN_PIN); }
static inline void nrf_csn_high(void) { GPIO_SetBits(NRF_CSN_PORT, NRF_CSN_PIN); }

static uint8_t nrf_read_reg(uint8_t reg)
{
    uint8_t value;

    nrf_csn_low();
    bsp_spi1_transfer(NRF_CMD_R_REGISTER | reg);
    value = bsp_spi1_transfer(NRF_CMD_NOP);
    nrf_csn_high();

    return value;
}

static void nrf_write_reg(uint8_t reg, uint8_t value)
{
    nrf_csn_low();
    bsp_spi1_transfer(NRF_CMD_W_REGISTER | reg);
    bsp_spi1_transfer(value);
    nrf_csn_high();
}

static void nrf_write_buf(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    nrf_csn_low();
    bsp_spi1_transfer(NRF_CMD_W_REGISTER | reg);
    for (uint8_t i = 0U; i < len; i++) {
        bsp_spi1_transfer(buf[i]);
    }
    nrf_csn_high();
}

static void nrf_cmd(uint8_t cmd)
{
    nrf_csn_low();
    bsp_spi1_transfer(cmd);
    nrf_csn_high();
}

static void nrf_activate_features(void)
{
    nrf_csn_low();
    bsp_spi1_transfer(NRF_CMD_ACTIVATE);
    bsp_spi1_transfer(0x73U);
    nrf_csn_high();
}

void nrf24_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = NRF_CSN_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(NRF_CSN_PORT, &gpio);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = NRF_CE_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(NRF_CE_PORT, &gpio);

    nrf_ce_low();
    nrf_csn_high();
}

void nrf24_spi_self_test(void)
{
    uint8_t old_rf_ch = nrf_read_reg(NRF_REG_RF_CH);
    uint8_t test_rf_ch = (old_rf_ch == 0x2AU) ? 0x15U : 0x2AU;
    uint8_t read_back;

    nrf_write_reg(NRF_REG_RF_CH, test_rf_ch);
    read_back = nrf_read_reg(NRF_REG_RF_CH);
    nrf_write_reg(NRF_REG_RF_CH, old_rf_ch);

    DBG_NrfSpiOldRfCh = old_rf_ch;
    DBG_NrfSpiReadBack = read_back;
    DBG_NrfSpiSelfTestPass = (read_back == test_rf_ch) ? 1U : 0U;
}

void nrf24_init_tx(void)
{
    nrf_ce_low();
    bsp_delay_ms(5U);

    nrf_write_reg(NRF_REG_CONFIG, 0x0CU);
    nrf_write_reg(NRF_REG_EN_AA, 0x00U);
    nrf_write_reg(NRF_REG_EN_RXADDR, 0x01U);
    nrf_write_reg(NRF_REG_SETUP_AW, 0x03U);
    nrf_write_reg(NRF_REG_SETUP_RETR, 0x00U);
    nrf_write_reg(NRF_REG_RF_CH, NRF_RF_CHANNEL);
    nrf_write_reg(NRF_REG_RF_SETUP, NRF_RF_SETUP_ACTIVE);
    nrf_write_buf(NRF_REG_RX_ADDR_P0, NRF_ADDR, 5U);
    nrf_write_buf(NRF_REG_TX_ADDR, NRF_ADDR, 5U);
    nrf_write_reg(NRF_REG_RX_PW_P0, NRF_PAYLOAD_SIZE);
    nrf_activate_features();
    nrf_write_reg(NRF_REG_DYNPD, 0x00U);
    nrf_write_reg(NRF_REG_FEATURE, 0x00U);
    nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);
    nrf_cmd(NRF_CMD_FLUSH_TX);
    nrf_write_reg(NRF_REG_CONFIG, NRF_CONFIG_TX);
    bsp_delay_ms(2U);
    nrf_ce_high();

    DBG_NrfConfig = nrf_read_reg(NRF_REG_CONFIG);
    DBG_NrfRfCh = nrf_read_reg(NRF_REG_RF_CH);
    DBG_NrfRfSetup = nrf_read_reg(NRF_REG_RF_SETUP);
    DBG_NrfEnAa = nrf_read_reg(NRF_REG_EN_AA);
    DBG_NrfSetupRetr = nrf_read_reg(NRF_REG_SETUP_RETR);
}

uint8_t nrf24_send_payload(const uint8_t *payload)
{
    uint8_t status;
    uint8_t fifo_status;
    uint8_t ok = 1U;

    if (DBG_NrfSpiSelfTestPass == 0U) {
        ok = 0U;
    }

    fifo_status = nrf_read_reg(NRF_REG_FIFO_STATUS);
    DBG_NrfFifoStatus = fifo_status;
    if (fifo_status == 0xFFU || fifo_status == 0x00U) {
        ok = 0U;
    }
    if ((fifo_status & NRF_FIFO_TX_FULL) != 0U) {
        nrf_tx_fifo_full_count++;
        nrf_cmd(NRF_CMD_FLUSH_TX);
        ok = 0U;
    }

    status = nrf_read_reg(NRF_REG_STATUS);
    DBG_NrfLastStatus = status;
    if (status == 0xFFU || status == 0x00U) {
        ok = 0U;
    }
    if ((status & NRF_STATUS_MAX_RT) != 0U) {
        nrf_cmd(NRF_CMD_FLUSH_TX);
        ok = 0U;
    }
    nrf_write_reg(NRF_REG_STATUS, status & (NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT));

    nrf_csn_low();
    bsp_spi1_transfer(NRF_CMD_W_TX_PAYLOAD);
    for (uint8_t i = 0U; i < NRF_PAYLOAD_SIZE; i++) {
        bsp_spi1_transfer(payload[i]);
    }
    nrf_csn_high();

    DBG_NrfFifoStatus = nrf_read_reg(NRF_REG_FIFO_STATUS);
    if (DBG_NrfFifoStatus == 0xFFU) {
        ok = 0U;
    }
    if (ok != 0U) {
        nrf_tx_ok_count++;
    } else {
        nrf_tx_fail_count++;
    }
    return ok;
}
