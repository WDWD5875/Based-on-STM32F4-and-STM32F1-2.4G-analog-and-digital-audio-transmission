#include "bsp_gpio.h"

#include "stm32f10x.h"

void bsp_gpio_mode_select_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = BSP_MODE_SELECT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BSP_MODE_SELECT_PORT, &gpio);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = BSP_MODE_GND_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(BSP_MODE_GND_PORT, &gpio);
    GPIO_ResetBits(BSP_MODE_GND_PORT, BSP_MODE_GND_PIN);
}

uint8_t bsp_gpio_is_adc_mode_selected(void)
{
    return GPIO_ReadInputDataBit(BSP_MODE_SELECT_PORT, BSP_MODE_SELECT_PIN) == Bit_RESET ? 1U : 0U;
}
