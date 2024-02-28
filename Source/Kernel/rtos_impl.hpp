/*
 * rtos_impl.hpp
 *
 *  Created on: Feb 7, 2024
 *      Author: tian_
 */

#pragma once

#include "./Source/PublicApi/rtos.hpp"
#include "rtos_scheduler.hpp"
#include "rtos_cpu_timer.hpp"
#include "rtos_system_timer.hpp"
#include "./External/MyLib/tx_memory_halffit.hpp"
#include <utility>


namespace RTOS
{


class RTOSImpl
{
public:

	static constexpr size_t const CoreFrequency = 16000000;
	static constexpr size_t const TickPerSecond = 1000;
	static constexpr size_t const CoreCyclePerTick = CoreFrequency / TickPerSecond;


public:

	SystemTimer					m_system_timer;

	AllocatorHalfFit 		m_mem_allocator;

	Scheduler						m_scheduler;


public:

	static void thread_entry(void);


public:
	RTOSImpl(void) noexcept = default;
	RTOSImpl(RTOSImpl const &) noexcept = delete;
	RTOSImpl(RTOSImpl &&) noexcept = delete;
	~RTOSImpl(void) noexcept = default;
	void operator=(RTOSImpl const &) noexcept = delete;
	void operator=(RTOSImpl &&) noexcept = delete;


	__attribute__((noreturn)) void initialize(FunctionPtr entry, size_t stack_size, void * mem_ptr, size_t mem_size);

};




}
