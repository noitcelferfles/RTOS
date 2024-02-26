/*
 * rtos_thread_mgr.hpp
 *
 *  Created on: Feb 11, 2024
 *      Author: tian_
 */

#pragma once

#include "./Source/PublicApi/rtos_thread.hpp"
#include "./External/MyLib/tx_assert.h"
#include "./External/MyLib/tx_array.hpp"
#include "./External/MyLib/tx_linkedlist.hpp"
#include "./External/MyLib/tx_array.hpp"
#include "./External/MyLib/tx_spinlock.hpp"

namespace RTOS
{



class Mutex;
class MessageQueue;
class PriorityList;


class Thread
{
public:
	static constexpr size_t const StackLimitIdentifier = 0xDEADBEEF;
	static constexpr bool const PrefillStack = false;

public:
	struct StackContext;


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


public:
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


	void populate_stack_context(void);

	static Thread & get_thread_from_m_priority_link(TXLib::LinkedCycleUnsafe & link)
	{
		return *reinterpret_cast<Thread *>(reinterpret_cast<size_t>(&link) - __builtin_offsetof(Thread, m_priority_link));
	}

	static Thread & get_thread_from_m_expire_link(TXLib::LinkedCycleUnsafe & link)
	{
		return *reinterpret_cast<Thread *>(reinterpret_cast<size_t>(&link) - __builtin_offsetof(Thread, m_expire_link));
	}


public:
	Thread(void) noexcept : m_state(State::Reset) {};
	Thread(Thread const &) = delete;
	Thread(Thread &&) = delete;
	~Thread(void) noexcept;
	void operator=(Thread const & b) = delete;
	void operator=(Thread && b) = delete;

	bool is_initialized(void) const {return m_state != State::Reset;}
	void initialize(FunctionPtr entry, size_t entry_argument, size_t priority, size_t stack_size);
	void unitialize(void);

};





class ThreadMgr // Own and manage lifetime of the threads (control block and stack)
{
public:
	TXLib::LightDynamicArray<Thread *>				m_thread_list;

	Spinlock																	m_spinlock;


public:
	static void thread_entry(void);

public:
	ThreadMgr(void) noexcept = default;
	ThreadMgr(ThreadMgr const &) noexcept = delete;
	ThreadMgr(ThreadMgr &&) noexcept = delete;
	~ThreadMgr(void) noexcept;
	void operator=(ThreadMgr const &) noexcept = delete;
	void operator=(ThreadMgr &&) noexcept = delete;

	bool is_initialized(void) const {return m_thread_list.is_initialized();}
	void initialize(void);

	Thread & create_empty_thread(void);
	void delete_empty_thread(Thread & thread);
};



}
