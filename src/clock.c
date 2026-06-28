#include "clock.h"
#include "stm32f4xx.h"

static volatile uint32_t s_ticks = 0;

void SysTick_Handler(void) { s_ticks++; }

void systick_init(void) {
    SystemCoreClockUpdate();            /* sync the CMSIS clock variable */
    SysTick_Config(SystemCoreClock / 1000u);   /* 1 ms tick */
}

uint32_t millis(void) { return s_ticks; }

void delay_ms(uint32_t ms) {
    uint32_t start = s_ticks;
    while ((s_ticks - start) < ms) { __NOP(); }
}

void clock_init(void) {
    /* TODO (learning task): bring SYSCLK to 100 MHz off the 25 MHz HSE.
     *   PLLM = 25  -> 1 MHz PLL input
     *   PLLN = 200 -> 200 MHz VCO
     *   PLLP = 2   -> 100 MHz SYSCLK
     *   APB1 prescaler = 2 (<=50 MHz), APB2 = 1
     * Order: VOS scale + FLASH 3 wait-states; enable HSE, wait HSERDY;
     * set RCC->PLLCFGR; enable PLL, wait PLLRDY; set bus prescalers;
     * switch RCC->CFGR SW to PLL, wait SWS; then SystemCoreClockUpdate().
     *
     * Until you implement this, the core runs at 16 MHz HSI. If so, set
     * APB1_TIMER_CLOCK_HZ in clock.h to 16000000 so the servo PSC math holds. */
}
