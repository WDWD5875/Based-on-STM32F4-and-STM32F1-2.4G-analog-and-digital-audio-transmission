#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>

void bsp_gpio_mode_select_init(void);
uint8_t bsp_gpio_is_adc_mode_selected(void);

#define BSP_MODE_SELECT_PORT GPIOB
#define BSP_MODE_SELECT_PIN  GPIO_Pin_12
#define BSP_MODE_GND_PORT    GPIOB
#define BSP_MODE_GND_PIN     GPIO_Pin_13

#endif /* BSP_GPIO_H */
