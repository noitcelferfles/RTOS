/*
 * thread.cpp
 *
 *  Created on: Feb 29, 2024
 *      Author: tian_
 */

#include <stddef.h>
#include "./Source/PublicApi/rtos.hpp"
#include "./External/CMSIS/Device/ST/STM32F2xx/Include/stm32f207xx.h"






// Hardware abstraction

void enable_gpio_clock(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN_Msk;
}

void enable_green_led(void)
{
	GPIOB->MODER &= ~GPIO_MODER_MODER0_Msk;
	GPIOB->MODER |= 0x01u << GPIO_MODER_MODER0_Pos;
}

void toggle_green_led(void)
{
	GPIOB->ODR ^= GPIO_ODR_OD0;
}




// Threads

size_t toggle_green_procedure(size_t arg)
{
	enable_green_led();
	while(1)
	{
		toggle_green_led();
		RTOS::sleep(1000);
	}
	return 0;
}

constexpr size_t const SHORT_SLEEP_TIME = 1;
constexpr size_t const MED_SLEEP_TIME = 100;
constexpr size_t const LONG_SLEEP_TIME = 1000;

size_t g_time_stamp;
size_t g_last_sleep_time;
RTOS::TimeType g_last_systick;

/* The variables below record the longest latency of the sleep syscall.
 * The sleep syscall has constant time-complexity, with a small constant variation based on the wakeup time.
 */
size_t volatile g_short_sleep_syscall_latency = 0;
size_t volatile g_med_sleep_syscall_latency = 0;
size_t volatile g_long_sleep_syscall_latency = 0;

size_t do_useless_work(size_t arg)
{
	while(1)
	{
		g_last_systick = RTOS::system_time();
		g_last_sleep_time = arg;
		g_time_stamp = DWT->CYCCNT;
		RTOS::sleep(arg); // sleep for the number of ticks specified by the argument
		size_t delay = DWT->CYCCNT - g_time_stamp;

		if (g_last_systick == RTOS::system_time()) // Ensure that no Systick update procedure has been executed since last sleep
		{
			switch (g_last_sleep_time)
			{
			case SHORT_SLEEP_TIME:
				if (delay > g_short_sleep_syscall_latency)
				{
					g_short_sleep_syscall_latency = delay;
				}
				break;
			case MED_SLEEP_TIME:
				if (delay > g_med_sleep_syscall_latency)
				{
					g_med_sleep_syscall_latency = delay;
				}
				break;
			case LONG_SLEEP_TIME:
				if (delay > g_long_sleep_syscall_latency)
				{
					g_long_sleep_syscall_latency = delay;
				}
			default:
				break;
			}
			__NOP();
		}
	}
}

RTOS::Thread g_toggle_led_thread;
RTOS::Thread g_high_freq_useless_thread[30];
RTOS::Thread g_med_freq_useless_thread[30];
RTOS::Thread g_low_freq_useless_thread[30];


size_t os_main(size_t arg)
{
	enable_gpio_clock();

	/* Create a toggle-LED thread with lower priority and many useless threads sharing a higher priority.
	 * All useless threads must complete periodic execution before toggle-LED thread gets the chance to run.
	 * If the useless threads cannot be executed timely, the LED will stop toggling.
	 */

	g_toggle_led_thread.initialize(&toggle_green_procedure, 2, 0x100);

	for (size_t i = 0; i < sizeof(g_high_freq_useless_thread) / sizeof(RTOS::Thread); i++)
	{
		g_high_freq_useless_thread[i].initialize(&do_useless_work, SHORT_SLEEP_TIME, 1, 0x100);
	}
	for (size_t i = 0; i < sizeof(g_med_freq_useless_thread) / sizeof(RTOS::Thread); i++)
	{
		g_med_freq_useless_thread[i].initialize(&do_useless_work, MED_SLEEP_TIME, 1, 0x100);
	}
	for (size_t i = 0; i < sizeof(g_low_freq_useless_thread) / sizeof(RTOS::Thread); i++)
	{
		g_low_freq_useless_thread[i].initialize(&do_useless_work, LONG_SLEEP_TIME, 1, 0x100);
	}

	return 0;
}


__attribute__((section("os_heap"), aligned(8))) char g_os_heap[0x10000];

int main(void)
{
  RTOS::initialize(&os_main, 0x200, g_os_heap, sizeof(g_os_heap));
}
