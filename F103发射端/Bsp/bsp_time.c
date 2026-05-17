#include "bsp_time.h"

#include "stm32f10x.h"

#define DWT_CR                 (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT             (*(volatile uint32_t *)0xE0001004)
#define DEM_CR                 (*(volatile uint32_t *)0xE000EDFC)
#define DEM_CR_TRCENA          (1U << 24)
#define DWT_CR_CYCCNTENA       (1U << 0)

void bsp_time_init(void)
{
    DEM_CR |= DEM_CR_TRCENA;
    DWT_CYCCNT = 0U;
    DWT_CR |= DWT_CR_CYCCNTENA;
}

void bsp_delay_us(uint32_t us)
{
    uint32_t start = DWT_CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);

    while ((DWT_CYCCNT - start) < cycles) {
    }
}

void bsp_delay_ms(uint32_t ms)
{
    while (ms-- != 0U) {
        bsp_delay_us(1000U);
    }
}

uint32_t bsp_time_cycle_count(void)
{
    return DWT_CYCCNT;
}

uint32_t bsp_time_cycles_to_us(uint32_t cycles)
{
    return cycles / (SystemCoreClock / 1000000U);
}
