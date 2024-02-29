/*
 * rtos_time_info.hpp
 *
 *  Created on: Feb 15, 2024
 *      Author: tian_
 */

#pragma once

#include "./Source/PublicApi/rtos_time.hpp"

namespace RTOS
{

class SystemTimer
{

public:
	TimeType				m_last_recorded_tick;
	TimeType				m_last_recorded_core_cycle;
	size_t					m_max_allowable_tick_until_next_update;


public:
	SystemTimer(void) noexcept = default;
	~SystemTimer(void) noexcept = default;
	SystemTimer(SystemTimer const &) = delete;
	SystemTimer(SystemTimer &&) = delete;
	void operator=(SystemTimer const &) = delete;
	void operator=(SystemTimer &&) = delete;


	void initialize(void);

	TimeType update_time(TimeType core_cycle); /* Return the expected core cycle count at the next tick */

	TimeType get_tick(void) const {return m_last_recorded_tick;}
	TimeType get_core_cycle(void) const {return m_last_recorded_core_cycle;}
	void set_max_allowable_tick(size_t tick) {m_max_allowable_tick_until_next_update = tick;}



};

extern "C" void SysTick_Handler(void); // Interrupt handler

} // namespace RTOS
