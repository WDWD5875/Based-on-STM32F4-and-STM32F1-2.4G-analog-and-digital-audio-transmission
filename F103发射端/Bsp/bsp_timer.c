#include "bsp_timer.h"

#include "../Protocol/audio_defs.h"
#include "stm32f10x.h"

static volatile uint16_t tx_due_count = 0U;

void bsp_audio_timer_init(void)
{
    TIM_TimeBaseInitTypeDef tim;
    NVIC_InitTypeDef nvic;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = 0U;
    tim.TIM_Period = TIM2_AUDIO_PKT_ARR;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &tim);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void bsp_audio_timer_start(void)
{
    TIM_SetCounter(TIM2, 0U);
    TIM_Cmd(TIM2, ENABLE);
}

uint8_t bsp_audio_timer_take_due(void)
{
    if (tx_due_count == 0U) {
        return 0U;
    }

    __disable_irq();
    tx_due_count = 0U;
    __enable_irq();
    return 1U;
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        if (tx_due_count < 1000U) {
            tx_due_count++;
        }
    }
}
