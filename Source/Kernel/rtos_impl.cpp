/*
 * thread.cpp
 *
 *  Created on: Jan 29, 2024
 *      Author: tian_
 */



#include "rtos_impl.hpp"
#include "rtos_profiler.hpp"
#include "./Source/Driver/cortexm3_core.hpp"
#include "./External/MyLib/tx_assert.h"
#include "./External/CMSIS/Include/cmsis_compiler.h"



namespace RTOS
{


RTOSImpl g_rtos;




void RTOSImpl::initialize(FunctionPtr entry, size_t stack_size, void * mem_ptr, size_t mem_size)
{
	CoreClock::initialize();
	RTOS_PROFILER_INIT(& CoreClock::get_cycle_count);

	m_system_timer.initialize();
	m_mem_allocator.initialize(mem_ptr, mem_size);
	m_scheduler.initialize(entry, stack_size);
}




// All interrupts should be disabled at this point
void initialize(FunctionPtr entry, size_t stack_size, void * mem_ptr, size_t mem_size)
{
	g_rtos.initialize(entry, stack_size, mem_ptr, mem_size);
}

void * alloc(size_t size)
{
	return g_rtos.m_mem_allocator.alloc(size);
}

void free(void * mem_ptr)
{
	g_rtos.m_mem_allocator.free(mem_ptr);
}

size_t used_memory(void)
{
	return g_rtos.m_mem_allocator.get_used_size();
}

size_t unused_memory(void)
{
	return g_rtos.m_mem_allocator.get_unused_size();
}




}


