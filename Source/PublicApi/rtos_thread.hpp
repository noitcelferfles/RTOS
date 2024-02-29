/*
 * rtos_thread.hpp
 *
 *  Created on: Jan 31, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include "rtos_time.hpp"
#include "./External/MyLib/tx_linkedlist.hpp"
#include "./External/MyLib/tx_assert.h"


namespace RTOS
{

typedef size_t (*FunctionPtr)(size_t arg);

class Mutex;
class PriorityList;

class Thread
// Implemented in Source/Kernel/rtos_scheduler.cpp
{


protected:
	static constexpr size_t const StackLimitIdentifier = 0xDEADBEEF;
	static constexpr bool const PrefillStack = false;

protected:

	enum class State
	{
		Reset, // This is the only state in which the stack is not allocated
		Paused,
		Ready,
		Running,
		Sleeping,
		SleepingAndPaused, // This state is reached if the thread is paused which sleeping (enter PAUSED after sleep expires)
		BlockedByMutex,
		SoftBlockedByMutex,
		BlockedByMessage,
		SoftBlockedByMessage,
		Terminated,
	};


protected:
	void * 												m_sp;
	size_t												m_stack_begin;
	size_t												m_stack_end;
	FunctionPtr 									m_entry;
	size_t												m_entry_argument;
	size_t												m_base_priority;	// Actual priority could be higher due to inheritance from owning mutexes
	size_t												m_effective_priority; // Actual priority used for scheduling
	PriorityList *								m_priority_list;		// Priority list (ready list, mutex list ...) which contains this thread
	TXLib::LinkedCycleUnsafe			m_priority_link;		// Link to the priority list
	TXLib::LinkedCycleUnsafe			m_expire_link;
	TimeType											m_expire_time;
	size_t												m_cpu_cycle_used;
	State													m_state;
	Mutex *												m_blocking_mutex;
	TXLib::LinkedCycle						m_owned_mutex;


public:
	Thread(void) noexcept : m_state(State::Reset) {};
	Thread(Thread const &) = delete;
	Thread(Thread &&) = delete;
	~Thread(void) noexcept;
	void operator=(Thread const & b) = delete;
	void operator=(Thread && b) = delete;

	bool is_initialized(void) const {return m_state != State::Reset;}
	void initialize(FunctionPtr entry, size_t entry_argument, size_t priority, size_t stack_size);
	void initialize(FunctionPtr entry, size_t priority, size_t stack_size) {initialize(entry, 0, priority, stack_size);}
	void uninitialize(void);

	void pause(void);
	void unpause(void);
	void kill(void);

};


// Operations on the running thread

void relinquish(void); // Only relinquish to higher or equal priority ready threads
void sleep(size_t sleep_duration);


} // namespace RTOS
