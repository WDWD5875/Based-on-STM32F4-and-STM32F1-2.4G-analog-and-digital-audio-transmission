#include "bsp_adc_dma.h"

#include "bsp_time.h"
#include "../Debug/debug_probe.h"
#include "stm32f10x.h"

uint16_t bsp_adc_raw_buffer[ADC_DMA_BUF_SIZE];

static volatile uint8_t adc_half_ready = 0U;

void bsp_adc_dma_tim3_init(void)
{
    GPIO_InitTypeDef gpio;
    ADC_InitTypeDef adc;
    DMA_InitTypeDef dma;
    TIM_TimeBaseInitTypeDef tim;
    NVIC_InitTypeDef nvic;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_0;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio);

    DMA_DeInit(DMA1_Channel1);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)bsp_adc_raw_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = ADC_DMA_BUF_SIZE;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &dma);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_HT | DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    ADC_StructInit(&adc);
    adc.ADC_Mode = ADC_Mode_Independent;
    adc.ADC_ScanConvMode = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;
    adc.ADC_DataAlign = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &adc);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_13Cycles5);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) != RESET) {
    }
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) != RESET) {
    }

    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = 0U;
    tim.TIM_Period = TIM3_ADC_TRIG_ARR;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &tim);
    TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    nvic.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    TIM_Cmd(TIM3, ENABLE);
}

uint8_t bsp_adc_dma_take_ready_half(void)
{
    uint8_t ready = 0U;

    __disable_irq();
    if ((adc_half_ready & 0x01U) != 0U) {
        adc_half_ready &= (uint8_t)~0x01U;
        ready = 1U;
    } else if ((adc_half_ready & 0x02U) != 0U) {
        adc_half_ready &= (uint8_t)~0x02U;
        ready = 2U;
    }
    __enable_irq();

    return ready;
}

void DMA1_Channel1_IRQHandler(void)
{
    uint32_t now = bsp_time_cycle_count();

    if (DBG_AdcHalfLastCycle != 0U) {
        DBG_AdcHalfIntervalUs = bsp_time_cycles_to_us(now - DBG_AdcHalfLastCycle);
    }
    DBG_AdcHalfLastCycle = now;

    if (DMA_GetITStatus(DMA1_IT_HT1) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_HT1);
        if ((adc_half_ready & 0x01U) != 0U) {
            adc_half_overrun_count++;
        }
        adc_half_ready |= 0x01U;
        adc_half_count++;
    } else if (DMA_GetITStatus(DMA1_IT_TC1) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC1);
        if ((adc_half_ready & 0x02U) != 0U) {
            adc_half_overrun_count++;
        }
        adc_half_ready |= 0x02U;
        adc_half_count++;
    }
}
