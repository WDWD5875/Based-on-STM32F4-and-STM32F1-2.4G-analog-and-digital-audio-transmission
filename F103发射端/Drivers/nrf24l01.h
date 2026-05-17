#ifndef NRF24L01_H
#define NRF24L01_H

#include <stdint.h>

void nrf24_gpio_init(void);
void nrf24_init_tx(void);
void nrf24_spi_self_test(void);
uint8_t nrf24_send_payload(const uint8_t *payload);

#endif /* NRF24L01_H */
