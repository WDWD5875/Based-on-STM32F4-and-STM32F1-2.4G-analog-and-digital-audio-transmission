#include "bsp_i2c.h"

#include "stm32f10x.h"

#define BSP_I2C1_SCL_PORT GPIOB
#define BSP_I2C1_SCL_PIN  GPIO_Pin_8
#define BSP_I2C1_SDA_PORT GPIOB
#define BSP_I2C1_SDA_PIN  GPIO_Pin_9

static void i2c_delay(void)
{
    volatile uint8_t i;

    for (i = 0U; i < 8U; i++) {
    }
}

static void i2c_scl(uint8_t level)
{
    GPIO_WriteBit(BSP_I2C1_SCL_PORT, BSP_I2C1_SCL_PIN, (BitAction)(level != 0U));
    i2c_delay();
}

static void i2c_sda(uint8_t level)
{
    GPIO_WriteBit(BSP_I2C1_SDA_PORT, BSP_I2C1_SDA_PIN, (BitAction)(level != 0U));
    i2c_delay();
}

static void i2c_start(void)
{
    i2c_sda(1U);
    i2c_scl(1U);
    i2c_sda(0U);
    i2c_scl(0U);
}

static void i2c_stop(void)
{
    i2c_sda(0U);
    i2c_scl(1U);
    i2c_sda(1U);
}

static void i2c_send_byte(uint8_t data)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        i2c_sda((uint8_t)((data & (0x80U >> i)) != 0U));
        i2c_scl(1U);
        i2c_scl(0U);
    }

    /* The simple OLED demo driver does not sample ACK. Keep the ninth clock. */
    i2c_sda(1U);
    i2c_scl(1U);
    i2c_scl(0U);
}

void bsp_i2c1_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = BSP_I2C1_SCL_PIN | BSP_I2C1_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    i2c_scl(1U);
    i2c_sda(1U);
}

uint8_t bsp_i2c1_write(uint8_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if ((data == 0) || (len == 0U)) {
        return 0U;
    }

    i2c_start();
    i2c_send_byte(addr);
    for (i = 0U; i < len; i++) {
        i2c_send_byte(data[i]);
    }
    i2c_stop();

    return 1U;
}
