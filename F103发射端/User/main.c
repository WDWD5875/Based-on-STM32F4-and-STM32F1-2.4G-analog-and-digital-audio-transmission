#include "../App/app_adc_stream.h"
#include "../App/app_audio_stream.h"
#include "../App/app_config.h"
#include "../Bsp/bsp_spi.h"
#include "../Bsp/bsp_time.h"
#include "../Debug/debug_probe.h"
#include "../Drivers/nrf24l01.h"
#include "../Drivers/oled_i2c.h"

int main(void)
{
    debug_probe_init();
    app_config_init();
    bsp_time_init();
    bsp_spi1_init();
    nrf24_gpio_init();
    nrf24_spi_self_test();
    nrf24_init_tx();
    oled_i2c_init();

    /* AUDIO_INPUT_UART_PCM uses USART1 RX DMA. AUDIO_INPUT_ADC uses ADC1 DMA. */
    if (app_config_audio_input_mode() == AUDIO_INPUT_UART_PCM) {
        app_audio_stream_init();
        oled_i2c_task();
        while (1) {
            app_audio_stream_task();
        }
    } else {
        app_adc_stream_init();
        oled_i2c_task();
        while (1) {
            app_adc_stream_task();
        }
    }
}
