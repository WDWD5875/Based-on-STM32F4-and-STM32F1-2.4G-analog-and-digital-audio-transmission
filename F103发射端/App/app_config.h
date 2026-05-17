#ifndef APP_CONFIG_H
#define APP_CONFIG_H

typedef enum {
    AUDIO_INPUT_UART_PCM = 0,
    AUDIO_INPUT_ADC = 1
} AudioInputMode;

void app_config_init(void);
AudioInputMode app_config_audio_input_mode(void);

#endif /* APP_CONFIG_H */
