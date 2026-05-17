#include "stm32f4xx.h"
#include <stdint.h>
#include <string.h>

/* 音频输出：PA5 = DAC_OUT2，TIM7 触发，DMA1_Stream6 双缓冲。 */
#define AUDIO_FRAME_SIZE       128U
#define TIM7_CLOCK_HZ          84000000U
/*
 * 采样率公式：Fs = TIM7_CLOCK_HZ / (TIM7_NOMINAL_ARR + 1)
 * 44100Hz 目标值：84000000 / (1904 + 1) = 44094.49Hz
 * 后面要改采样率时，只改这个溢出值。
 */
#define TIM7_NOMINAL_ARR       1904U
/* 32000Hz: 84000000 / (2624 + 1). */

/* Ring buffer size must be power-of-two because rb_w/r wrap with RING_BUF_MASK. */
#define RING_BUF_SIZE          8192U
#define RING_BUF_MASK          (RING_BUF_SIZE - 1U)
#if ((RING_BUF_SIZE & (RING_BUF_SIZE - 1U)) != 0U)
#error "RING_BUF_SIZE must be power-of-two"
#endif
#define TARGET_LEVEL           2048U
#define START_WATER_LEVEL      TARGET_LEVEL
#define STOP_WATER_LEVEL       1024U
/* 无线接收超时保护：超过这个时间没有收到有效音频包，就清空缓冲并静音。 */
#define AUDIO_RX_TIMEOUT_MS    80U

/*
 * SPLL 水位控制参数。
 * nRF 每包只写入 30 个音频样本，而 DAC DMA 每次消耗 128 个样本，
 * 环形缓冲水位天然是阶梯变化的，所以必须先低通滤波再进入 PI。
 */
#define SPLL_LPF_ALPHA         0.01f
#define SPLL_KP                0.008f
#define SPLL_KI                0.00008f
#define SPLL_INTEGRAL_LEAK     0.995f
#define SPLL_ERR_DEADBAND      40.0f
#define SPLL_INTEGRATE_LIMIT   1024.0f
#define SPLL_INTEGRAL_MAX      50000.0f
#define SPLL_ADJ_MAX           20.0f

#define DBG_DAC_TRACE_SIZE     1024U
#define DBG_DAC_TRACE_MASK     (DBG_DAC_TRACE_SIZE - 1U)

/* nRF24L01 接线：CE=PB11, CSN=PB12, SCK=PB13, MISO=PB14, MOSI=PB15, IRQ=PB10. */
#define NRF_CE_PORT            GPIOB
#define NRF_CE_PIN             GPIO_Pin_11
#define NRF_CSN_PORT           GPIOB
#define NRF_CSN_PIN            GPIO_Pin_12
#define NRF_IRQ_PIN            GPIO_Pin_10

#define NRF_PAYLOAD_SIZE       32U
#define NRF_RX_QUEUE_SIZE      64U
#define NRF_RX_QUEUE_MASK      (NRF_RX_QUEUE_SIZE - 1U)
#define NRF_STATUS_DEBUG_SPI   0U
/* SPI2 clock = APB1 42MHz / prescaler. 4 gives 10.5MHz, close to nRF24L01 limit. */
#define NRF_SPI_BAUD_PRESCALER SPI_BaudRatePrescaler_4
#define AUDIO_PACKET_TYPE      0xA1U
#define AUDIO_PACKET_SAMPLES   30U

#define NRF_CMD_R_REGISTER     0x00U
#define NRF_CMD_W_REGISTER     0x20U
#define NRF_CMD_R_RX_PAYLOAD   0x61U
#define NRF_CMD_FLUSH_RX       0xE2U
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
#define NRF_FIFO_RX_EMPTY      0x01U

#define NRF_RF_CHANNEL         76U
#define NRF_CONFIG_RX          0x3FU
#define NRF_RF_SETUP_2M_0DBM   0x0FU
#define NRF_RF_SETUP_1M_0DBM   0x07U
#define NRF_RF_SETUP_ACTIVE    NRF_RF_SETUP_2M_0DBM
#define NRF_IRQ_CAPTURE_TIM_HZ 1000000U
#define TIM2_CLOCK_HZ          84000000U

#define DWT_CYCCNT             (*(volatile uint32_t *)0xE0001004)
#define DEM_CR                 (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CR                 (*(volatile uint32_t *)0xE0001000)

static const uint8_t NRF_RX_ADDR[5] = {0xE7U, 0xE7U, 0xE7U, 0xE7U, 0xE7U};

static volatile uint8_t nrf_rx_queue[NRF_RX_QUEUE_SIZE][NRF_PAYLOAD_SIZE];
static volatile uint8_t nrf_q_w = 0U;
static volatile uint8_t nrf_q_r = 0U;
static volatile uint8_t nrf_q_n = 0U;

static uint8_t audio_ring[RING_BUF_SIZE];
static volatile uint32_t rb_w = 0U;
static volatile uint32_t rb_r = 0U;

static uint16_t play_buf0[AUDIO_FRAME_SIZE];
static uint16_t play_buf1[AUDIO_FRAME_SIZE];

static volatile uint8_t is_playing = 0U;
static float spll_integral = 0.0f;
static float spll_level_lpf = (float)TARGET_LEVEL;

static volatile uint32_t nrf_packet_count = 0U;
static volatile uint32_t nrf_irq_count = 0U;
static volatile uint32_t nrf_irq_rx_count = 0U;
static volatile uint32_t nrf_queue_overflow_count = 0U;
static volatile uint32_t audio_bytes_in = 0U;
static volatile uint32_t audio_bytes_out = 0U;
static volatile uint32_t audio_bytes_drop = 0U;
static volatile uint32_t audio_underflow_count = 0U;
static volatile uint32_t dma_irq_count = 0U;
static volatile uint32_t audio_silence_protect_count = 0U;
static volatile uint32_t audio_low_water_mute_count = 0U;
static volatile uint32_t audio_timeout_reset_count = 0U;
static volatile uint32_t LastAudioRxTick = 0U;
volatile uint32_t g_msTick = 0U;
static volatile uint32_t audio_packet_good_count = 0U;
static volatile uint32_t audio_packet_bad_type_count = 0U;
static volatile uint32_t audio_packet_seq_drop_count = 0U;
static volatile uint8_t audio_expected_seq = 0U;
static volatile uint8_t audio_last_seq = 0U;
volatile uint8_t DBG_SeqDropLastDelta = 0U;
volatile uint8_t DBG_SeqDropLastExpected = 0U;
volatile uint8_t DBG_SeqDropLastSeq = 0U;
volatile uint8_t DBG_SeqDropLastPrevSeq = 0U;
volatile uint32_t DBG_SeqDropLastIntervalUs = 0U;
volatile uint32_t DBG_SeqDropEvents = 0U;
volatile uint32_t DBG_SeqDropLastGoodCount = 0U;
volatile uint32_t DBG_SeqDropLastRbLevel = 0U;
volatile uint32_t DBG_SeqDropLastNrfCount = 0U;

volatile uint32_t DBG_NrfLastPacketCycle = 0U;
volatile uint32_t DBG_NrfPacketIntervalCycles = 0U;
volatile uint32_t DBG_NrfPacketIntervalMaxCycles = 0U;
volatile uint32_t DBG_NrfPacketIntervalMinCycles = 0xFFFFFFFFU;
volatile uint32_t DBG_NrfPacketGapOver3ms = 0U;
volatile uint32_t DBG_NrfPacketGapOver10ms = 0U;
volatile uint32_t DBG_NrfPacketIntervalUs = 0U;
volatile uint32_t DBG_NrfPacketIntervalMaxUs = 0U;
volatile uint8_t DBG_NrfLastStatus = 0U;
volatile uint8_t DBG_NrfLastFifoStatus = 0U;
volatile uint8_t DBG_NrfLastDrainPerIrq = 0U;
volatile uint8_t DBG_NrfLastIrqLoops = 0U;
volatile uint32_t DBG_NrfDrainMaxPerIrq = 0U;
volatile uint32_t DBG_NrfIrqLoopMax = 0U;
volatile uint32_t DBG_NrfFifoRescueCount = 0U;
volatile uint8_t DBG_NrfQueueLevel = 0U;
volatile uint32_t DBG_IrqCaptureCount = 0U;
volatile uint32_t DBG_IrqCaptureLastUs = 0U;
volatile uint32_t DBG_IrqCaptureIntervalUs = 0U;
volatile uint32_t DBG_IrqCaptureIntervalMaxUs = 0U;
volatile uint32_t DBG_IrqCaptureGapOver3ms = 0U;
volatile uint32_t DBG_IrqCaptureGapOver10ms = 0U;
volatile uint8_t DBG_LastRxPayload[NRF_PAYLOAD_SIZE];
volatile uint8_t DBG_LastGoodPayload[NRF_PAYLOAD_SIZE];
volatile uint8_t DBG_LastRxPayload0 = 0U;
volatile uint8_t DBG_LastRxPayload1 = 0U;
volatile uint8_t DBG_LastBadPayload0 = 0U;
volatile uint32_t DBG_RbLevelLive = 0U;
volatile uint32_t DBG_RbW = 0U;
volatile uint32_t DBG_RbR = 0U;
volatile uint32_t DBG_DmaLevelAtIrq = 0U;
volatile uint8_t DBG_DmaPlayingAtIrq = 0U;
volatile uint32_t DBG_DmaAudioFrameCount = 0U;
volatile uint32_t DBG_DmaSilentFrameCount = 0U;

static volatile uint32_t dbg_rb_level = 0U;
static volatile uint32_t dbg_tim7_arr = TIM7_NOMINAL_ARR;
static volatile int32_t dbg_spll_adj = 0;
static volatile int32_t dbg_spll_error = 0;
static volatile uint32_t dbg_spll_level_lpf = TARGET_LEVEL;
static volatile int32_t dbg_spll_integral = 0;

volatile uint16_t DBG_DacTrace[DBG_DAC_TRACE_SIZE];
volatile uint32_t DBG_DacTraceW = 0U;
volatile uint32_t DBG_DacTraceWrap = 0U;
volatile uint16_t DBG_DacLastSample = 2048U;
volatile uint32_t DBG_DAC_DOR2 = 0U;
volatile uint32_t DBG_DAC_DHR12R2 = 0U;
volatile uint32_t DBG_DMA1S6_NDTR = 0U;
volatile uint32_t DBG_DMA1S6_CR = 0U;
volatile uint32_t DBG_TIM7_CNT = 0U;
volatile uint32_t DBG_TIM7_ARR_Live = TIM7_NOMINAL_ARR;

static void dwt_init(void)
{
    DEM_CR |= (1U << 24);
    DWT_CYCCNT = 0U;
    DWT_CR |= 1U;
}

/* 毫秒计时由 stm32f4xx_it.c 的 SysTick_Handler 递增，主循环只读取。 */
static uint32_t HAL_GetTick(void)
{
    return g_msTick;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = DWT_CYCCNT;
    uint32_t cycles = (SystemCoreClock / 1000U) * ms;

    while ((DWT_CYCCNT - start) < cycles) {
    }
}

static inline uint32_t rb_level(void)
{
    if (rb_w >= rb_r) {
        return rb_w - rb_r;
    }
    return RING_BUF_SIZE - rb_r + rb_w;
}

static inline uint32_t rb_free(void)
{
    return RING_BUF_SIZE - rb_level() - 1U;
}

static inline void rb_push(uint8_t v)
{
    audio_ring[rb_w] = v;
    rb_w = (rb_w + 1U) & RING_BUF_MASK;
}

static inline uint8_t rb_pop(void)
{
    uint8_t v = audio_ring[rb_r];
    rb_r = (rb_r + 1U) & RING_BUF_MASK;
    return v;
}

static void rb_write_audio_bytes(const uint8_t *data, uint8_t len)
{
    __disable_irq();
    for (uint8_t i = 0U; i < len; i++) {
        if (rb_free() == 0U) {
            (void)rb_pop();
            audio_bytes_drop++;
        }
        rb_push(data[i]);
        audio_bytes_in++;
    }
    dbg_rb_level = rb_level();
    DBG_RbLevelLive = dbg_rb_level;
    DBG_RbW = rb_w;
    DBG_RbR = rb_r;
    /* 只有收到协议正确的音频包才刷新接收时间，避免坏包误解除静音保护。 */
    LastAudioRxTick = HAL_GetTick();
    __enable_irq();
}

static void audio_silence_reset(void)
{
    __disable_irq();
    /* 断流保护：丢弃残留数据，DAC DMA 会继续输出 2048 中点电压。 */
    rb_w = 0U;
    rb_r = 0U;
    is_playing = 0U;
    spll_integral = 0.0f;
    spll_level_lpf = (float)TARGET_LEVEL;
    TIM7->ARR = TIM7_NOMINAL_ARR;
    dbg_rb_level = 0U;
    DBG_RbLevelLive = 0U;
    DBG_RbW = 0U;
    DBG_RbR = 0U;
    dbg_tim7_arr = TIM7_NOMINAL_ARR;
    dbg_spll_adj = 0;
    dbg_spll_error = 0;
    dbg_spll_level_lpf = TARGET_LEVEL;
    dbg_spll_integral = 0;
    audio_silence_protect_count++;
    audio_timeout_reset_count++;
    __enable_irq();
}

static void audio_timeout_check(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t level = rb_level();

    /*
     * 用 32 位无符号减法判断超时，SysTick 每毫秒递增。
     * 即使 g_msTick 回卷，now - LastAudioRxTick 仍然是正确的经过时间。
     */
    if (((level != 0U) || (is_playing != 0U)) &&
        ((now - LastAudioRxTick) > AUDIO_RX_TIMEOUT_MS)) {
        audio_silence_reset();
        LastAudioRxTick = now;
    }
}

static inline void dbg_dac_trace_push(uint16_t sample)
{
    DBG_DacTrace[DBG_DacTraceW] = sample;
    DBG_DacTraceW = (DBG_DacTraceW + 1U) & DBG_DAC_TRACE_MASK;
    if (DBG_DacTraceW == 0U) {
        DBG_DacTraceWrap++;
    }
    DBG_DacLastSample = sample;
}

static inline void dbg_capture_audio_regs(void)
{
    DBG_DAC_DOR2 = DAC->DOR2;
    DBG_DAC_DHR12R2 = DAC->DHR12R2;
    DBG_DMA1S6_NDTR = DMA1_Stream6->NDTR;
    DBG_DMA1S6_CR = DMA1_Stream6->CR;
    DBG_TIM7_CNT = TIM7->CNT;
    DBG_TIM7_ARR_Live = TIM7->ARR;
}

static inline void nrf_ce_low(void) { GPIO_ResetBits(NRF_CE_PORT, NRF_CE_PIN); }
static inline void nrf_ce_high(void) { GPIO_SetBits(NRF_CE_PORT, NRF_CE_PIN); }
static inline void nrf_csn_low(void) { GPIO_ResetBits(NRF_CSN_PORT, NRF_CSN_PIN); }
static inline void nrf_csn_high(void) { GPIO_SetBits(NRF_CSN_PORT, NRF_CSN_PIN); }

static uint8_t spi2_transfer(uint8_t data)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) {
    }
    SPI_I2S_SendData(SPI2, data);

    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) {
    }
    return (uint8_t)SPI_I2S_ReceiveData(SPI2);
}

static uint8_t nrf_read_reg(uint8_t reg)
{
    uint8_t value;

    nrf_csn_low();
    spi2_transfer(NRF_CMD_R_REGISTER | reg);
    value = spi2_transfer(NRF_CMD_NOP);
    nrf_csn_high();

    return value;
}

static void nrf_write_reg(uint8_t reg, uint8_t value)
{
    nrf_csn_low();
    spi2_transfer(NRF_CMD_W_REGISTER | reg);
    spi2_transfer(value);
    nrf_csn_high();
}

static void nrf_write_buf(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    nrf_csn_low();
    spi2_transfer(NRF_CMD_W_REGISTER | reg);
    for (uint8_t i = 0U; i < len; i++) {
        spi2_transfer(buf[i]);
    }
    nrf_csn_high();
}

static void nrf_cmd(uint8_t cmd)
{
    nrf_csn_low();
    spi2_transfer(cmd);
    nrf_csn_high();
}

static void nrf_activate_features(void)
{
    nrf_csn_low();
    spi2_transfer(NRF_CMD_ACTIVATE);
    spi2_transfer(0x73U);
    nrf_csn_high();
}

static void nrf_read_payload(uint8_t *buf)
{
    nrf_csn_low();
    spi2_transfer(NRF_CMD_R_RX_PAYLOAD);
    for (uint8_t i = 0U; i < NRF_PAYLOAD_SIZE; i++) {
        buf[i] = spi2_transfer(NRF_CMD_NOP);
    }
    nrf_csn_high();
}

static void nrf_queue_push(const uint8_t *payload)
{
    if (nrf_q_n < NRF_RX_QUEUE_SIZE) {
        uint32_t now = DWT_CYCCNT;

        if (DBG_NrfLastPacketCycle != 0U) {
            uint32_t dt = now - DBG_NrfLastPacketCycle;
            DBG_NrfPacketIntervalCycles = dt;
            DBG_NrfPacketIntervalUs = dt / (SystemCoreClock / 1000000U);

            if (dt > DBG_NrfPacketIntervalMaxCycles) {
                DBG_NrfPacketIntervalMaxCycles = dt;
                DBG_NrfPacketIntervalMaxUs = DBG_NrfPacketIntervalUs;
            }
            if (dt < DBG_NrfPacketIntervalMinCycles) {
                DBG_NrfPacketIntervalMinCycles = dt;
            }
            if (dt > ((SystemCoreClock / 1000U) * 3U)) {
                DBG_NrfPacketGapOver3ms++;
            }
            if (dt > ((SystemCoreClock / 1000U) * 10U)) {
                DBG_NrfPacketGapOver10ms++;
            }
        }
        DBG_NrfLastPacketCycle = now;

        memcpy((void *)nrf_rx_queue[nrf_q_w], payload, NRF_PAYLOAD_SIZE);
        nrf_q_w = (nrf_q_w + 1U) & NRF_RX_QUEUE_MASK;
        nrf_q_n++;
        DBG_NrfQueueLevel = nrf_q_n;
        nrf_packet_count++;
    } else {
        nrf_queue_overflow_count++;
    }
}

static uint8_t nrf_queue_pop(uint8_t *payload)
{
    uint8_t has_packet = 0U;

    __disable_irq();
    if (nrf_q_n > 0U) {
        memcpy(payload, (const void *)nrf_rx_queue[nrf_q_r], NRF_PAYLOAD_SIZE);
        nrf_q_r = (nrf_q_r + 1U) & NRF_RX_QUEUE_MASK;
        nrf_q_n--;
        DBG_NrfQueueLevel = nrf_q_n;
        has_packet = 1U;
    }
    __enable_irq();

    return has_packet;
}

static void spi2_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = NRF_CE_PIN | NRF_CSN_PIN;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = NRF_IRQ_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &gpio);

    nrf_ce_low();
    nrf_csn_high();
}

static void spi2_init(void)
{
    SPI_InitTypeDef spi;

    SPI_I2S_DeInit(SPI2);
    SPI_StructInit(&spi);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = NRF_SPI_BAUD_PRESCALER;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &spi);
    SPI_Cmd(SPI2, ENABLE);
}

static void nrf_irq_init(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource10);

    EXTI_StructInit(&exti);
    exti.EXTI_Line = EXTI_Line10;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

static void nrf_irq_capture_init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tim;
    TIM_ICInitTypeDef ic;
    NVIC_InitTypeDef nvic;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_TIM2);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = NRF_IRQ_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &gpio);

    TIM_DeInit(TIM2);
    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = (uint16_t)((TIM2_CLOCK_HZ / NRF_IRQ_CAPTURE_TIM_HZ) - 1U);
    tim.TIM_Period = 0xFFFFFFFFU;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &tim);

    TIM_ICStructInit(&ic);
    ic.TIM_Channel = TIM_Channel_3;
    ic.TIM_ICPolarity = TIM_ICPolarity_Falling;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = 0U;
    TIM_ICInit(TIM2, &ic);

    TIM_ClearITPendingBit(TIM2, TIM_IT_CC3);
    TIM_ITConfig(TIM2, TIM_IT_CC3, ENABLE);
    TIM_Cmd(TIM2, ENABLE);

    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void TIM2_IRQHandler(void)
{
    static uint8_t seen_first = 0U;
    static uint32_t last_capture = 0U;

    if (TIM_GetITStatus(TIM2, TIM_IT_CC3) != RESET) {
        uint32_t capture = TIM_GetCapture3(TIM2);

        TIM_ClearITPendingBit(TIM2, TIM_IT_CC3);
        DBG_IrqCaptureCount++;
        DBG_IrqCaptureLastUs = capture;

        if (seen_first != 0U) {
            uint32_t dt = capture - last_capture;

            DBG_IrqCaptureIntervalUs = dt;
            if (dt > DBG_IrqCaptureIntervalMaxUs) {
                DBG_IrqCaptureIntervalMaxUs = dt;
            }
            if (dt > 3000U) {
                DBG_IrqCaptureGapOver3ms++;
            }
            if (dt > 10000U) {
                DBG_IrqCaptureGapOver10ms++;
            }
        } else {
            seen_first = 1U;
        }

        last_capture = capture;
    }
}

static uint8_t nrf_drain_rx_fifo(void)
{
    uint8_t payload[NRF_PAYLOAD_SIZE];
    uint8_t drained = 0U;

    while ((nrf_read_reg(NRF_REG_FIFO_STATUS) & NRF_FIFO_RX_EMPTY) == 0U) {
        nrf_read_payload(payload);
        nrf_queue_push(payload);
        drained++;
    }
    return drained;
}

static void nrf_init_rx(void)
{
    nrf_ce_low();
    delay_ms(5U);

    nrf_write_reg(NRF_REG_CONFIG, 0x0CU);
    /* 实时音频不等待 ACK，丢包直接跳过，避免重发造成播放延迟和水位抖动。 */
    nrf_write_reg(NRF_REG_EN_AA, 0x00U);
    nrf_write_reg(NRF_REG_EN_RXADDR, 0x01U);
    nrf_write_reg(NRF_REG_SETUP_AW, 0x03U);
    nrf_write_reg(NRF_REG_SETUP_RETR, 0x00U);
    nrf_write_reg(NRF_REG_RF_CH, NRF_RF_CHANNEL);
    nrf_write_reg(NRF_REG_RF_SETUP, NRF_RF_SETUP_ACTIVE);
    nrf_write_buf(NRF_REG_RX_ADDR_P0, NRF_RX_ADDR, 5U);
    nrf_write_buf(NRF_REG_TX_ADDR, NRF_RX_ADDR, 5U);
    nrf_write_reg(NRF_REG_RX_PW_P0, NRF_PAYLOAD_SIZE);
    nrf_activate_features();
    nrf_write_reg(NRF_REG_DYNPD, 0x00U);
    nrf_write_reg(NRF_REG_FEATURE, 0x00U);
    nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);
    nrf_cmd(NRF_CMD_FLUSH_RX);

    nrf_write_reg(NRF_REG_CONFIG, NRF_CONFIG_RX);
    delay_ms(2U);
    nrf_ce_high();
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line10) != RESET) {
        uint8_t status;
        uint8_t fifo_status;
        uint8_t drained;
        uint8_t total_drained = 0U;
        uint8_t loops = 0U;

        EXTI_ClearITPendingBit(EXTI_Line10);
        nrf_irq_count++;

        do {
            loops++;
            status = nrf_read_reg(NRF_REG_STATUS);
            fifo_status = nrf_read_reg(NRF_REG_FIFO_STATUS);
            DBG_NrfLastStatus = status;
            DBG_NrfLastFifoStatus = fifo_status;

            if (((status & NRF_STATUS_RX_DR) == 0U) &&
                ((fifo_status & NRF_FIFO_RX_EMPTY) != 0U)) {
                nrf_write_reg(NRF_REG_STATUS, status & (NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT));
                break;
            }

            if (((status & NRF_STATUS_RX_DR) == 0U) &&
                ((fifo_status & NRF_FIFO_RX_EMPTY) == 0U)) {
                DBG_NrfFifoRescueCount++;
            }

            drained = nrf_drain_rx_fifo();
            total_drained += drained;

            /* Clear RX_DR, then loop back to re-check FIFO_STATUS. */
            nrf_write_reg(NRF_REG_STATUS, status & (NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT));
        } while (loops < 8U);

        nrf_irq_rx_count += total_drained;
        DBG_NrfLastDrainPerIrq = total_drained;
        DBG_NrfLastIrqLoops = loops;
        if (total_drained > DBG_NrfDrainMaxPerIrq) {
            DBG_NrfDrainMaxPerIrq = total_drained;
        }
        if (loops > DBG_NrfIrqLoopMax) {
            DBG_NrfIrqLoopMax = loops;
        }
    }
}

static void audio_dma_system_init(void)
{
    GPIO_InitTypeDef gpio;
    DAC_InitTypeDef dac;
    DMA_InitTypeDef dma;
    TIM_TimeBaseInitTypeDef tim;
    NVIC_InitTypeDef nvic;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC | RCC_APB1Periph_TIM7, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_5;
    gpio.GPIO_Mode = GPIO_Mode_AN;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &gpio);

    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = 0U;
    tim.TIM_Period = TIM7_NOMINAL_ARR;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM7, &tim);
    TIM_SelectOutputTrigger(TIM7, TIM_TRGOSource_Update);

    for (uint32_t i = 0U; i < AUDIO_FRAME_SIZE; i++) {
        play_buf0[i] = 2048U;
        play_buf1[i] = 2048U;
    }

    DMA_DeInit(DMA1_Stream6);
    while (DMA_GetCmdStatus(DMA1_Stream6) != DISABLE) {
    }
    DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_FEIF6 | DMA_FLAG_DMEIF6 | DMA_FLAG_TEIF6 |
                               DMA_FLAG_HTIF6 | DMA_FLAG_TCIF6);

    DMA_StructInit(&dma);
    dma.DMA_Channel = DMA_Channel_7;
    dma.DMA_PeripheralBaseAddr = (uint32_t)&DAC->DHR12R2;
    dma.DMA_Memory0BaseAddr = (uint32_t)play_buf0;
    dma.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    dma.DMA_BufferSize = AUDIO_FRAME_SIZE;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_Init(DMA1_Stream6, &dma);

    DMA_DoubleBufferModeConfig(DMA1_Stream6, (uint32_t)play_buf1, DMA_Memory_0);
    DMA_DoubleBufferModeCmd(DMA1_Stream6, ENABLE);
    DMA_ITConfig(DMA1_Stream6, DMA_IT_TC, ENABLE);

    DAC_StructInit(&dac);
    dac.DAC_Trigger = DAC_Trigger_T7_TRGO;
    dac.DAC_WaveGeneration = DAC_WaveGeneration_None;
    dac.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
    DAC_Init(DAC_Channel_2, &dac);

    DMA_Cmd(DMA1_Stream6, ENABLE);
    DAC_DMACmd(DAC_Channel_2, ENABLE);
    DAC_Cmd(DAC_Channel_2, ENABLE);
    TIM_Cmd(TIM7, ENABLE);

    nvic.NVIC_IRQChannel = DMA1_Stream6_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void DMA1_Stream6_IRQHandler(void)
{
    uint16_t *out;
    uint32_t level;

    if (DMA_GetITStatus(DMA1_Stream6, DMA_IT_TCIF6) == RESET) {
        return;
    }
    DMA_ClearITPendingBit(DMA1_Stream6, DMA_IT_TCIF6);
    dma_irq_count++;
    dbg_capture_audio_regs();

    out = (DMA1_Stream6->CR & DMA_SxCR_CT) ? play_buf0 : play_buf1;
    level = rb_level();
    dbg_rb_level = level;
    DBG_RbLevelLive = level;
    DBG_RbW = rb_w;
    DBG_RbR = rb_r;
    DBG_DmaLevelAtIrq = level;
    DBG_DmaPlayingAtIrq = is_playing;

    if (is_playing == 0U) {
        /* 水位迟滞：先攒到目标水位再开播，避免刚启动时缓冲太浅导致抖动。 */
        if (level >= START_WATER_LEVEL) {
            is_playing = 1U;
            spll_level_lpf = (float)level;
            spll_integral = 0.0f;
        }
    } else if (level < STOP_WATER_LEVEL) {
        /* 低水位保护：缓冲低于保护线时先静音，等重新攒够水位再恢复播放。 */
        is_playing = 0U;
        audio_underflow_count++;
        audio_low_water_mute_count++;
        audio_silence_protect_count++;
    }

    if (is_playing != 0U) {
        float error;
        float p_term;
        float new_integral;
        float adj;
        int32_t adj_i;

        /* 对阶梯水位做一阶低通，避免 30 字节一包的写入步进直接冲击 PI。 */
        spll_level_lpf += SPLL_LPF_ALPHA * ((float)level - spll_level_lpf);
        error = spll_level_lpf - (float)TARGET_LEVEL;
        if ((error > -SPLL_ERR_DEADBAND) && (error < SPLL_ERR_DEADBAND)) {
            error = 0.0f;
        }
        dbg_spll_error = (int32_t)error;
        dbg_spll_level_lpf = (uint32_t)spll_level_lpf;

        p_term = SPLL_KP * error;
        /*
         * 积分器加泄漏，并且只在误差不太大时积分。
         * 大误差主要交给比例项和限幅处理，避免启动或断包后积分器严重超调。
         */
        new_integral = spll_integral * SPLL_INTEGRAL_LEAK;
        if ((error > -SPLL_INTEGRATE_LIMIT) && (error < SPLL_INTEGRATE_LIMIT)) {
            new_integral += error;
        }
        if (new_integral > SPLL_INTEGRAL_MAX) {
            new_integral = SPLL_INTEGRAL_MAX;
        } else if (new_integral < -SPLL_INTEGRAL_MAX) {
            new_integral = -SPLL_INTEGRAL_MAX;
        }
        spll_integral = new_integral;
        dbg_spll_integral = (int32_t)spll_integral;

        adj = p_term + (SPLL_KI * spll_integral);
        if (adj > SPLL_ADJ_MAX) {
            adj = SPLL_ADJ_MAX;
        } else if (adj < -SPLL_ADJ_MAX) {
            adj = -SPLL_ADJ_MAX;
        }

        adj_i = (adj >= 0.0f) ? (int32_t)(adj + 0.5f) : (int32_t)(adj - 0.5f);
        TIM7->ARR = (uint32_t)((int32_t)TIM7_NOMINAL_ARR - adj_i);
        dbg_tim7_arr = TIM7->ARR;
        dbg_spll_adj = adj_i;

        for (uint32_t i = 0U; i < AUDIO_FRAME_SIZE; i++) {
            uint16_t sample = ((uint16_t)rb_pop()) << 4;
            out[i] = sample;
            dbg_dac_trace_push(sample);
        }
        audio_bytes_out += AUDIO_FRAME_SIZE;
        DBG_DmaAudioFrameCount++;
    } else {
        TIM7->ARR = TIM7_NOMINAL_ARR;
        dbg_tim7_arr = TIM7_NOMINAL_ARR;
        dbg_spll_adj = 0;
        dbg_spll_error = 0;
        dbg_spll_level_lpf = TARGET_LEVEL;
        dbg_spll_integral = 0;
        spll_integral = 0.0f;
        spll_level_lpf = (float)TARGET_LEVEL;

        for (uint32_t i = 0U; i < AUDIO_FRAME_SIZE; i++) {
            out[i] = 2048U;
            dbg_dac_trace_push(2048U);
        }
        DBG_DmaSilentFrameCount++;
    }

    __DSB();
}

static void process_nrf_audio_packet(const uint8_t *payload)
{
    uint8_t seq;

    memcpy((void *)DBG_LastRxPayload, payload, NRF_PAYLOAD_SIZE);
    DBG_LastRxPayload0 = payload[0];
    DBG_LastRxPayload1 = payload[1];

    if (payload[0] != AUDIO_PACKET_TYPE) {
        DBG_LastBadPayload0 = payload[0];
        audio_packet_bad_type_count++;
        return;
    }

    seq = payload[1];

    if (audio_packet_good_count != 0U) {
        uint8_t expected = audio_expected_seq;
        uint8_t delta = (uint8_t)(seq - expected);
        if (delta != 0U) {
            audio_packet_seq_drop_count += delta;
            DBG_SeqDropEvents++;
            DBG_SeqDropLastDelta = delta;
            DBG_SeqDropLastExpected = expected;
            DBG_SeqDropLastSeq = seq;
            DBG_SeqDropLastPrevSeq = audio_last_seq;
            DBG_SeqDropLastIntervalUs = DBG_NrfPacketIntervalUs;
            DBG_SeqDropLastGoodCount = audio_packet_good_count;
            DBG_SeqDropLastRbLevel = rb_level();
            DBG_SeqDropLastNrfCount = nrf_packet_count;
        }
    }
    audio_expected_seq = (uint8_t)(seq + 1U);
    audio_last_seq = seq;
    audio_packet_good_count++;
    memcpy((void *)DBG_LastGoodPayload, payload, NRF_PAYLOAD_SIZE);

    rb_write_audio_bytes(&payload[2], AUDIO_PACKET_SAMPLES);
}

int main(void)
{
    uint8_t payload[NRF_PAYLOAD_SIZE];

    SystemInit();
    SystemCoreClockUpdate();
    dwt_init();
    SysTick_Config(SystemCoreClock / 1000U);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    memset(audio_ring, 0, sizeof(audio_ring));
    spi2_gpio_init();
    spi2_init();
    nrf_init_rx();
    nrf_irq_capture_init();
    nrf_irq_init();
    audio_dma_system_init();

    while (1) {
        /*
         * 正常收包靠 PB10 的 nRF IRQ 中断完成。
         * 主循环只做低频备用轮询，避免频繁 SPI 读状态抢占音频处理时间。
         */
        while (nrf_queue_pop(payload) != 0U) {
            process_nrf_audio_packet(payload);
        }

        audio_timeout_check();
    }
}
