/*
 * cortexm3_core.hpp
 *
 *  Created on: Feb 15, 2024
 *      Author: tian_
 */

#pragma once

#include "./External/CMSIS/Device/ST/STM32F2xx/Include/stm32f207xx.h"
#include "./External/MyLib/tx_assert.h"
#include "stddef.h"


class CoreClock
{
public:
	static void initialize(void)
	{
		CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
		DWT->CYCCNT = 0;
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
		TX_ASSERT(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk); // Some cores require additional step to enable cycle counter
	}

	static size_t get_cycle_count(void)
	{
		return DWT->CYCCNT;
	}

	static size_t get_cycle_counter_address(void)
	{
		return (size_t) &DWT->CYCCNT;
	}

	static bool clock_is_enabled(void)
	{
		return DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk;
	}

};





class CoreInterrupt
{
public:
	static constexpr size_t const CoreCyclePerSystickCycle = 8;
	static constexpr size_t const CoreCyclePerSystickCycleLog2 = 3; static_assert(1u << CoreCyclePerSystickCycleLog2 == CoreCyclePerSystickCycle);

public:

	static void configure_priorities(void)
	{
//		SCB->SHP[7] = 0xFE; // SVC; This interrupt is unused
		SCB->SHP[10] = 0xFF; // PendSV; Lowest priority ensure that context switch does not happen during interrupts
		SCB->SHP[11] = 0xFE; // SysTick
	}

	static void configure_systick(void)
	{
		SysTick->CTRL = SysTick_CTRL_TICKINT_Msk;
	}

	static void initialize(void)
	{
		configure_priorities();
		configure_systick();
	}

	static void enable_systick(void)
	{
		SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	}

	static void disable_systick(void)
	{
		SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	}

	static void trigger_pendsv_interrupt(void)
	{
		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	}

	static void trigger_systick_interrupt(void)
	{
		SCB->ICSR |= SCB_ICSR_PENDSTSET_Msk;
	}

	static void clear_systick_interrupt(void)
	{
		SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
	}

	static void set_systick_countdown(size_t core_cycle)
	{
		SysTick->LOAD = core_cycle >> CoreCyclePerSystickCycleLog2;
	}

	static void reset_counter(size_t core_cycle)
	{
		size_t systick_cycle = core_cycle >> CoreCyclePerSystickCycleLog2;
		TX_ASSERT(systick_cycle > 1u && systick_cycle <= SysTick_LOAD_RELOAD_Msk);
		SysTick->LOAD = systick_cycle - 1u;
		SysTick->VAL = 0;
	}

	static constexpr size_t get_systick_max_countdown_in_core_cycle(void)
	{
		return SysTick_LOAD_RELOAD_Msk << CoreCyclePerSystickCycleLog2;
	}

};


class LowPowerState
{
public:

	static void initialize(void)
	{
		SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
	}

	static void enter_sleep_mode(void)
	{
		__WFI();
	}

};
