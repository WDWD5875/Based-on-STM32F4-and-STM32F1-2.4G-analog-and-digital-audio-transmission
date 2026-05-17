#include "oled_i2c.h"

#include "oled_font.h"
#include "../App/app_config.h"
#include "../Bsp/bsp_i2c.h"
#include "../Bsp/bsp_time.h"
#include "../Debug/debug_probe.h"

static uint8_t oled_initialized = 0U;
static uint8_t oled_boot_check_rendered = 0U;

static void oled_write_command(uint8_t command)
{
    uint8_t buf[2];

    buf[0] = 0x00U;
    buf[1] = command;
    (void)bsp_i2c1_write(BSP_I2C1_OLED_ADDR_8BIT, buf, 2U);
}

static void oled_write_data(uint8_t data)
{
    uint8_t buf[2];

    buf[0] = 0x40U;
    buf[1] = data;
    (void)bsp_i2c1_write(BSP_I2C1_OLED_ADDR_8BIT, buf, 2U);
}

static void oled_write_data_block(const uint8_t *data, uint8_t len)
{
    uint8_t buf[17];
    uint8_t i;

    if ((data == 0) || (len == 0U) || (len > 16U)) {
        return;
    }

    buf[0] = 0x40U;
    for (i = 0U; i < len; i++) {
        buf[i + 1U] = data[i];
    }
    (void)bsp_i2c1_write(BSP_I2C1_OLED_ADDR_8BIT, buf, (uint16_t)(len + 1U));
}

static void oled_set_cursor(uint8_t page, uint8_t x)
{
    oled_write_command((uint8_t)(0xB0U | page));
    oled_write_command((uint8_t)(0x10U | ((x & 0xF0U) >> 4)));
    oled_write_command((uint8_t)(0x00U | (x & 0x0FU)));
}

static uint32_t oled_pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1U;

    while (y-- != 0U) {
        result *= x;
    }

    return result;
}

static void oled_show_char(uint8_t line, uint8_t column, char ch)
{
    uint8_t index;

    if ((line < 1U) || (line > 4U) || (column < 1U) || (column > 16U)) {
        return;
    }

    if ((ch < ' ') || (ch > '~')) {
        ch = ' ';
    }
    index = (uint8_t)(ch - ' ');

    oled_set_cursor((uint8_t)((line - 1U) * 2U), (uint8_t)((column - 1U) * 8U));
    oled_write_data_block(&OLED_F8x16[index][0], 8U);

    oled_set_cursor((uint8_t)((line - 1U) * 2U + 1U), (uint8_t)((column - 1U) * 8U));
    oled_write_data_block(&OLED_F8x16[index][8], 8U);
}

static void oled_render_boot_check(void)
{
    AudioInputMode mode = app_config_audio_input_mode();

    oled_i2c_clear();
    oled_i2c_show_string(1U, 1U, "BOOT SELF CHECK");
    oled_i2c_show_string(2U, 1U, (mode == AUDIO_INPUT_UART_PCM) ? "MODE:UART PCM" : "MODE:ADC");
    oled_i2c_show_string(3U, 1U, "SPI:");
    oled_i2c_show_string(3U, 5U, (DBG_NrfSpiSelfTestPass != 0U) ? "OK" : "ERR");
    oled_i2c_show_string(3U, 9U, "RF:");
    oled_i2c_show_num(3U, 12U, DBG_NrfRfCh, 3U);
    oled_i2c_show_string(4U, 1U, "CFG:");
    oled_i2c_show_hex_num(4U, 5U, DBG_NrfConfig, 2U);
    oled_i2c_show_string(4U, 9U, "SET:");
    oled_i2c_show_hex_num(4U, 13U, DBG_NrfRfSetup, 2U);
}

void oled_i2c_init(void)
{
    bsp_i2c1_init();
    bsp_delay_ms(100U);

    oled_write_command(0xAEU);
    oled_write_command(0xD5U);
    oled_write_command(0x80U);
    oled_write_command(0xA8U);
    oled_write_command(0x3FU);
    oled_write_command(0xD3U);
    oled_write_command(0x00U);
    oled_write_command(0x40U);
    oled_write_command(0xA1U);
    oled_write_command(0xC8U);
    oled_write_command(0xDAU);
    oled_write_command(0x12U);
    oled_write_command(0x81U);
    oled_write_command(0xCFU);
    oled_write_command(0xD9U);
    oled_write_command(0xF1U);
    oled_write_command(0xDBU);
    oled_write_command(0x30U);
    oled_write_command(0xA4U);
    oled_write_command(0xA6U);
    oled_write_command(0x8DU);
    oled_write_command(0x14U);
    oled_write_command(0xAFU);

    oled_i2c_clear();
    oled_initialized = 1U;
    oled_boot_check_rendered = 0U;
}

void oled_i2c_task(void)
{
    if ((oled_initialized == 0U) || (oled_boot_check_rendered != 0U)) {
        return;
    }

    oled_render_boot_check();
    oled_boot_check_rendered = 1U;
}

void oled_i2c_clear(void)
{
    uint8_t page;
    uint8_t x;

    for (page = 0U; page < 8U; page++) {
        oled_set_cursor(page, 0U);
        for (x = 0U; x < 128U; x++) {
            oled_write_data(0x00U);
        }
    }
}

void oled_i2c_show_string(uint8_t line, uint8_t column, const char *string)
{
    uint8_t i;

    if (string == 0) {
        return;
    }

    for (i = 0U; (string[i] != '\0') && ((column + i) <= 16U); i++) {
        oled_show_char(line, (uint8_t)(column + i), string[i]);
    }
}

void oled_i2c_show_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    uint8_t i;

    for (i = 0U; i < length; i++) {
        uint32_t divisor = oled_pow(10U, (uint32_t)(length - i - 1U));
        oled_show_char(line, (uint8_t)(column + i), (char)((number / divisor) % 10U + '0'));
    }
}

void oled_i2c_show_hex_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    uint8_t i;

    for (i = 0U; i < length; i++) {
        uint32_t divisor = oled_pow(16U, (uint32_t)(length - i - 1U));
        uint8_t digit = (uint8_t)((number / divisor) % 16U);
        oled_show_char(line, (uint8_t)(column + i), (char)(digit < 10U ? (digit + '0') : (digit - 10U + 'A')));
    }
}
