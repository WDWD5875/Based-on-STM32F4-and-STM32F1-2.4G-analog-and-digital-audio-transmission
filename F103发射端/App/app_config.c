#include "app_config.h"

#include "../Bsp/bsp_gpio.h"

static AudioInputMode s_audio_input_mode = AUDIO_INPUT_UART_PCM;

void app_config_init(void)
{
    bsp_gpio_mode_select_init();
    s_audio_input_mode = bsp_gpio_is_adc_mode_selected() != 0U ? AUDIO_INPUT_ADC : AUDIO_INPUT_UART_PCM;
}

AudioInputMode app_config_audio_input_mode(void)
{
    return s_audio_input_mode;
}
