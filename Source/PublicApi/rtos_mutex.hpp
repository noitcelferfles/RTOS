/*
 * rtos_mutex.hpp
 *
 *  Created on: Feb 3, 2024
 *      Author: tian_
 */

#pragma once

#include "rtos_thread.hpp"
#include "./Source/Kernel/rtos_priority_list.hpp"
#include "./External/MyLib/tx_linkedlist.hpp"
#include "./External/MyLib/tx_assert.h"
#include <stddef.h>

namespace RTOS
{

class ThreadImpl;
class Scheduler;

class Mutex
// Implemented in Source/rtos_scheduler.cpp
{
friend Scheduler;


private:
	ThreadImpl *									m_owner;
	PriorityList									m_blocked_threads;
	TXLib::LinkedCycleUnsafe			m_sibling; // Linked to mutexes owned by the same thread


public:
	Mutex(void) noexcept : m_owner(nullptr) {}
	Mutex(Mutex const &) = delete;
	Mutex(Mutex &&) = delete;
	~Mutex(void) noexcept {TX_ASSERT(m_owner == nullptr && m_blocked_threads.get_highest_priority() == PriorityList::INVALID_PRIORITY);};
	void operator=(Mutex const &) = delete;
	void operator=(Mutex &&) = delete;

	bool try_lock(size_t max_wait_time); // Wait time is measured in ticks
	void lock(void);
	void unlock(void); // Automatically relinquish to higher-priority thread after unlock
	bool is_locked(void) const {return m_owner != nullptr;}




private:
	size_t get_inherited_priority(void) const {return m_blocked_threads.get_highest_priority();}

	void set_owner(ThreadImpl & thread);
	void set_orphan(void);

	static void unlock_all_mutex(ThreadImpl & thread);
	static size_t get_max_inherited_priority_among_siblings(TXLib::LinkedCycle & sibling_link, size_t base_priority);
	static Mutex & get_mutex_from_m_sibling(TXLib::LinkedCycle & sibling)
	{
		return *reinterpret_cast<Mutex *>(reinterpret_cast<size_t>(&sibling) - __builtin_offsetof(Mutex, m_sibling));
	}

};





}
