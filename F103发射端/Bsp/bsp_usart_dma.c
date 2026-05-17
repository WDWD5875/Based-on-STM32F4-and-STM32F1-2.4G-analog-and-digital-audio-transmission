#include "bsp_usart_dma.h"

#include "../Debug/debug_probe.h"
#include "stm32f10x.h"

uint8_t bsp_usart1_rx_block[PCM_BLOCK_SIZE];

static volatile uint8_t rx_block_ready = 0U;
static volatile uint8_t rx_dma_error = 0U;

static void usart1_clear_rx_error(void)
{
    volatile uint16_t sr;
    volatile uint16_t dr;

    sr = USART1->SR;
    dr = USART1->DR;
    (void)sr;
    (void)dr;
}

static void usart1_send_byte(uint8_t data)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
    }
    USART_SendData(USART1, data);
}

void bsp_usart1_send_ack(uint16_t status)
{
    usart1_send_byte(0xACU);
    usart1_send_byte(0xCAU);
    usart1_send_byte((uint8_t)(status & 0xFFU));
    usart1_send_byte((uint8_t)(status >> 8));
}

void bsp_usart1_start_rx_dma(void)
{
    DMA_Cmd(DMA1_Channel5, DISABLE);
    usart1_clear_rx_error();
    DMA_ClearFlag(DMA1_FLAG_GL5);
    DMA_SetCurrDataCounter(DMA1_Channel5, PCM_BLOCK_SIZE);
    DMA_Cmd(DMA1_Channel5, ENABLE);
    DBG_DmaRestartCount++;
    DBG_UsartSr = USART1->SR;
    DBG_DmaCndtr = DMA_GetCurrDataCounter(DMA1_Channel5);
}

uint8_t bsp_usart1_take_rx_block_ready(void)
{
    if (rx_block_ready == 0U) {
        return 0U;
    }

    rx_block_ready = 0U;
    return 1U;
}

uint8_t bsp_usart1_take_rx_error(void)
{
    if (rx_dma_error == 0U) {
        return 0U;
    }

    rx_dma_error = 0U;
    return 1U;
}

void bsp_usart1_rx_recover_task(void)
{
    DBG_UsartSr = USART1->SR;
    DBG_DmaCndtr = DMA_GetCurrDataCounter(DMA1_Channel5);

    if ((USART1->SR & USART_FLAG_ORE) != 0U) {
        DBG_UsartOreCount++;
        bsp_usart1_start_rx_dma();
    }
}

void bsp_usart1_audio_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    DMA_InitTypeDef dma;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_DeInit(USART1);
    USART_StructInit(&usart);
    usart.USART_BaudRate = USART_AUDIO_BAUD;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &usart);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    DMA_DeInit(DMA1_Channel5);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)bsp_usart1_rx_block;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = PCM_BLOCK_SIZE;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &dma);
    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC | DMA_IT_TE, ENABLE);

    nvic.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    USART_Cmd(USART1, ENABLE);
    bsp_usart1_start_rx_dma();
}

void DMA1_Channel5_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC5);
        DMA_Cmd(DMA1_Channel5, DISABLE);
        rx_block_ready = 1U;
    }

    if (DMA_GetITStatus(DMA1_IT_TE5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TE5);
        DMA_Cmd(DMA1_Channel5, DISABLE);
        rx_dma_error = 1U;
        DBG_DmaTeCount++;
        bsp_usart1_start_rx_dma();
    }
}
