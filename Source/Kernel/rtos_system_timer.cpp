/*
 * rtos_system_timer.cpp
 *
 *  Created on: Feb 15, 2024
 *      Author: tian_
 */

#include "rtos_system_timer.hpp"
#include "rtos_impl.hpp"
#include "rtos_scheduler.hpp"
#include "./Source/Driver/cortexm3_core.hpp"
#include "./External/MyLib/tx_arithmetic.hpp"
#include "./External/MyLib/tx_assert.h"

namespace RTOS
{


void SystemTimer::initialize(void)
{
	m_last_recorded_tick = 0;
	m_last_recorded_core_cycle = 0;
	m_max_allowable_tick_until_next_update = TimeType::get_max_positive();
}

TimeType SystemTimer::update_time(TimeType core_cycle)
/* Return the number of cycle count of the next tick */
{
	size_t time_since_record_in_cycle = core_cycle - m_last_recorded_core_cycle
			 + (RTOSImpl::CoreCyclePerTick / 2); // This term is added so that the recorded tick is updated to the closest tick rather than the last tick that passed
	auto result = TXLib::divide(time_since_record_in_cycle, RTOSImpl::CoreCyclePerTick);
	size_t time_since_record_in_tick = result.first;
	tx_api_assert(time_since_record_in_tick <= m_max_allowable_tick_until_next_update); // Failing means that a systick update could not execute in time
	m_last_recorded_tick += time_since_record_in_tick;
	m_last_recorded_core_cycle += time_since_record_in_cycle - result.second;
	return m_last_recorded_core_cycle + RTOSImpl::CoreCyclePerTick;
}






extern RTOSImpl g_rtos;
static SystemTimer & g_system_timer = g_rtos.m_system_timer;


extern "C" void SysTick_Handler(void)
{
	TimeType next_systick_time = g_system_timer.update_time(CoreClock::get_cycle_count());
	CoreInterrupt::reset_counter(next_systick_time - TimeType(CoreClock::get_cycle_count()));
	g_system_timer.set_max_allowable_tick(1);

	g_rtos.m_scheduler.systick_update(g_system_timer.get_tick());
}

TimeType system_time(void)
{
	return g_system_timer.get_tick();
}

} // namespace RTOS
