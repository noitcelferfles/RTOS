/*
 * rtos_cpu_timer.cpp
 *
 *  Created on: Feb 7, 2024
 *      Author: tian_
 */


#include "rtos_cpu_timer.hpp"
#include "rtos_impl.hpp"
#include "./Source/Driver/cortexm3_core.hpp"

namespace RTOS
{


void CPUTimer::initialize(void)
{
	m_duration = 0;
	m_max_time = 0;
}

void CPUTimer::start(void)
{
	m_start_time = CoreClock::get_cycle_count();
}

size_t CPUTimer::record(void)
{
	return CoreClock::get_cycle_count() - m_start_time;
}

void CPUTimer::stop(void)
{
	size_t volatile time_elapsed = CoreClock::get_cycle_count() - m_start_time;
	TX_ASSERT(time_elapsed < 0x80000000);
	m_duration += time_elapsed;
	if (time_elapsed > 200)
	{
		__NOP();
	}
}



}
