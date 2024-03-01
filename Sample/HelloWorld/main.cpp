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

void enable_blue_led(void)
{
	GPIOB->MODER &= ~GPIO_MODER_MODER7_Msk;
	GPIOB->MODER |= 0x01u << GPIO_MODER_MODER7_Pos;
}

void enable_red_led(void)
{
	GPIOB->MODER &= ~GPIO_MODER_MODER14_Msk;
	GPIOB->MODER |= 0x01u << GPIO_MODER_MODER14_Pos;
}

void toggle_green_led(void)
{
	GPIOB->ODR ^= GPIO_ODR_OD0;
}

void toggle_blue_led(void)
{
	GPIOB->ODR ^= GPIO_ODR_OD7;

}

void toggle_red_led(void)
{
	GPIOB->ODR ^= GPIO_ODR_OD14;
}




// Threads

RTOS::Mutex g_mutex_gpiob; // Mutex lock is used to prevent race condition on GPIO write (all three LED pins share the same address)

size_t toggle_green_procedure(size_t arg)
{
	g_mutex_gpiob.lock();
	enable_green_led();
	g_mutex_gpiob.unlock();
	while(1)
	{
		g_mutex_gpiob.lock();
		toggle_green_led();
		g_mutex_gpiob.unlock();
		RTOS::sleep(1000);
	}
	return 0;
}

size_t toggle_blue_procedure(size_t arg)
{
	g_mutex_gpiob.lock();
	enable_blue_led();
	g_mutex_gpiob.unlock();
	while(1)
	{
		g_mutex_gpiob.lock();
		toggle_blue_led();
		g_mutex_gpiob.unlock();
		RTOS::sleep(1000);
	}
	return 0;
}

size_t toggle_red_procedure(size_t arg)
{
	g_mutex_gpiob.lock();
	enable_red_led();
	g_mutex_gpiob.unlock();
	while(1)
	{
		g_mutex_gpiob.lock();
		toggle_red_led();
		g_mutex_gpiob.unlock();
		RTOS::sleep(1000);
	}
	return 0;
}


RTOS::Thread g_toggle_green_thread;
RTOS::Thread g_toggle_blue_thread;
RTOS::Thread g_toggle_red_thread;



size_t os_main(size_t arg)
/* This procedure is run once RTOS initialization is complete.
 * This is the place to initialize all RTOS-related entities like threads, message queues.
 * The thread running is procedure is given the highest priority (numerical value of 0), hence
 *  is never preempted by threads created here unless this thread invokes system calls like
 *  sleep or relinquish.
 */
{
	enable_gpio_clock();

	g_toggle_green_thread.initialize(&toggle_green_procedure, 5, 0x200);
	g_toggle_red_thread.initialize(&toggle_red_procedure, 5, 0x200);
	g_toggle_blue_thread.initialize(&toggle_blue_procedure, 5, 0x200);

	return 0;
}


__attribute__((section("os_heap"), aligned(8))) char g_os_heap[0x10000];
// This array is reserved as the heap memory for the RTOS.
// It must be aligned to double-word because the stack pointer is aligned to double-word.
// The section attribute (optional) is used to place this array in a memory segment that does not
//  require value-initialization on startup (see the linker script).

int main(void)
{
  RTOS::initialize(&os_main, 0x200, g_os_heap, sizeof(g_os_heap));
  // Start the RTOS and create a thread to execute the function os_main on a thread with stack size 0x200.
  // The stack is allocated from a memory pool specified in the argument.
  // Memory allocation and deallocation runs in constant time.
}
