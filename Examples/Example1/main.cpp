#include <stdint.h>
#include <stddef.h>
#include <atomic>

#include "stm32f207_peripheral.hpp"
#include "./Source/PublicApi/rtos.hpp"
#include "./External/MyLib/tx_assert.h"
#include "./External/CMSIS/Device/ST/STM32F2xx/Include/stm32f207xx.h"



size_t volatile g_time_stamp;

RTOS::Mutex g_mutex_rcc;
RTOS::Mutex g_mutex_gpiob;
RTOS::Mutex g_mutex;

RTOS::MessageQueue g_message_red;


void enable_rcc(void)
{
	g_mutex_rcc.lock();
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN_Msk;
	g_mutex_rcc.unlock();
}


void LED_Blue_Enable(void)
{
	enable_rcc();

	g_mutex_gpiob.lock();
	GPIOB->MODER &= ~GPIO_MODER_MODER7_Msk;
	GPIOB->MODER |= 0x01u << GPIO_MODER_MODER7_Pos;
	g_mutex_gpiob.unlock();
}

void LED_Red_Enable(void)
{
	enable_rcc();

	g_mutex_gpiob.lock();
	GPIOB->MODER &= ~GPIO_MODER_MODER14_Msk;
	GPIOB->MODER |= 0x01u << GPIO_MODER_MODER14_Pos;
	g_mutex_gpiob.unlock();
}

void LED_Green_Enable(void)
{
	enable_rcc();

	g_mutex_gpiob.lock();
	GPIOB->MODER &= ~GPIO_MODER_MODER0_Msk;
	GPIOB->MODER |= 0x01u << GPIO_MODER_MODER0_Pos;
	g_mutex_gpiob.unlock();
}

void LED_Blue_Toggle(void)
{
	g_mutex_gpiob.lock();
	GPIOB->ODR ^= GPIO_ODR_OD7;
	g_mutex_gpiob.unlock();
}

void LED_Red_Toggle(void)
{
	g_mutex_gpiob.lock();
	GPIOB->ODR ^= GPIO_ODR_OD14;
	g_mutex_gpiob.unlock();
}

void LED_Green_Toggle(void)
{
	g_mutex_gpiob.lock();
	GPIOB->ODR ^= GPIO_ODR_OD0;
	g_mutex_gpiob.unlock();
}

size_t toggle_red(size_t arg)
{
	LED_Red_Enable();
	size_t volatile message; message;
	while(1)
	{
		message = g_message_red.try_pull(1000).first;
		size_t volatile delay = DWT->CYCCNT - g_time_stamp; delay;
		g_time_stamp = DWT->CYCCNT;
		LED_Red_Toggle();
	}
	return 0;
}

size_t drive_red(size_t arg)
{
	while(1)
	{
		g_time_stamp = DWT->CYCCNT;
		g_message_red.push(0);
		size_t volatile delay = DWT->CYCCNT - g_time_stamp; delay;
		__NOP();
		g_time_stamp = DWT->CYCCNT;
		RTOS::sleep(1000);
	}
}

size_t toggle_green(size_t arg)
{
	LED_Green_Enable();
	while(1)
	{
		LED_Green_Toggle();
		RTOS::sleep(1000);
	}
	return 0;
}

size_t toggle_blue(size_t arg)
{
	LED_Blue_Enable();
	__NOP();
	int num = 0;
//	g_mutex.lock();
	while(num < 10)
	{
		bool locked = g_mutex.try_lock(1000);
		LED_Blue_Toggle();
		if (locked) g_mutex.unlock();
		RTOS::sleep(1000);

	}
//	g_mutex.unlock();

	return 0;
}

size_t acquire_lock(size_t arg)
{
	g_mutex.lock();
	RTOS::sleep(2000);
	g_mutex.unlock();
	return 0;
}

size_t useless_work(size_t arg)
{
	while(1)
	{
		g_mutex.lock();
		size_t volatile delay = DWT->CYCCNT - g_time_stamp; delay;
		g_time_stamp = DWT->CYCCNT;
		RTOS::sleep(1);
		delay = DWT->CYCCNT - g_time_stamp;
		g_time_stamp = DWT->CYCCNT;
		g_mutex.unlock();
	}
}



size_t useless_work_2(size_t arg)
{
	while(1)
	{
		size_t volatile delay = DWT->CYCCNT - g_time_stamp; delay;
		g_time_stamp = DWT->CYCCNT;
		RTOS::sleep(arg);
		__NOP();
	}
}

size_t useless_work_3(size_t arg)
{
	while(1)
	{
		size_t volatile delay = DWT->CYCCNT - g_time_stamp; delay;
		g_time_stamp = DWT->CYCCNT;
		RTOS::sleep(arg);
		__NOP();
	}
}

size_t count_free_memory(size_t arg)
{
	size_t volatile used;
	size_t volatile unused;
	while (1)
	{
		RTOS::sleep(100);
		used = RTOS::used_memory(); used;
		unused = RTOS::unused_memory(); unused;
	}
}




//size_t manage_useless_work(size_t arg)
//{
//	size_t count = 10;
//	RTOS::ThreadPtr ptr[count];
//	for (size_t i = 0; i < count; i++)
//	{
//		ptr[i] = RTOS::create_thread(&useless_work_2, 4, 0x200);
//	}
//
//	while (1)
//	{
//		for (size_t i = 0; i < count; i++)
//		{
//			RTOS::unpause_thread(ptr[i]);
//		}
//		RTOS::sleep(5000);
//		for (size_t i = 0; i < count; i++)
//		{
//			RTOS::pause_thread(ptr[i]);
//		}
//		RTOS::sleep(5000);
//	}
//
//}

size_t hold_lock(size_t arg)
{
	g_mutex_rcc.lock();
	while(1)
	{
	RTOS::sleep(5000);
	}
	return 0;
}


size_t send_uart(size_t arg)
{
	ClockControl::enable_gpio(GPIOD);
	ClockControl::set_enabled(RCC->APB1ENR, RCC_APB1ENR_USART3EN_Pos);

	UART * uart = reinterpret_cast<UART *>(USART3);
	uart->initialize<16000000, 115200>(RCC->APB1ENR, RCC_APB1ENR_USART3EN_Pos, GPIOD, 8, GPIOD, 9);
	while (1)
	{
		size_t time_start = DWT->CYCCNT;
		uart->transmit_string("12345678901234567890\r\n");
		size_t volatile delay = DWT->CYCCNT - time_start; delay;
		__NOP();
		RTOS::sleep(1000);
	}
	return 0;
}


RTOS::Thread g_toggle_green;
RTOS::Thread g_toggle_red;
RTOS::Thread g_toggle_blue;
RTOS::Thread g_drive_blue;
RTOS::Thread g_acquire_lock;
RTOS::Thread g_send_uart;
RTOS::Thread g_useless_work[5];
RTOS::Thread g_useless_work_3[5];
RTOS::Thread g_useless_work_2[30];
RTOS::Thread g_manage_useless_work;


size_t os_main(size_t arg)
{
	g_message_red.initialize(5);

	int volatile num = 0;

	g_time_stamp = DWT->CYCCNT;
	for (size_t i = 0; i < 10; i++)
	{
		num += i;
	}
	size_t volatile delay = DWT->CYCCNT - g_time_stamp; delay;

//	g_acquire_lock.initialize(&acquire_lock, 3, 0x200);

//	g_send_uart.initialize(&send_uart, 5, 0x200);

	g_toggle_green.initialize(&toggle_green, 5, 0x200);
	RTOS::sleep(333);
	g_toggle_red.initialize(&toggle_red, 5, 0x200);
	g_drive_blue.initialize(&drive_red, 1, 0x200);
	RTOS::sleep(333);
	g_toggle_blue.initialize(&toggle_blue, 5, 0x200);
	RTOS::sleep(333);

	for (size_t i = 0; i < sizeof(g_useless_work) / sizeof(RTOS::Thread); i++)
	{
		g_useless_work[i].initialize(&useless_work, 3, 0x200);
	}
	for (size_t i = 0; i < sizeof(g_useless_work_3) / sizeof(RTOS::Thread); i++)
	{
		g_useless_work_3[i].initialize(&useless_work_3, 1, 3, 0x200);
	}

	for (size_t i = 0; i < sizeof(g_useless_work_2) / sizeof(RTOS::Thread); i++)
	{
		g_useless_work_2[i].initialize(&useless_work_2, 1000, 4, 0x200);
	}


	//	g_manage_useless_work.initialize(&manage_useless_work, 2, 0x200);

	return 0;
}


__attribute__((section("os_heap"), aligned(8))) char g_os_heap[0x10000];

int main(void)
{
  RTOS::initialize(&os_main, 0x200, g_os_heap, sizeof(g_os_heap));
  TX_ASSERT(0);
}
