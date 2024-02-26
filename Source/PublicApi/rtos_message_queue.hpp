/*
 * rtos_event_listener.hpp
 *
 *  Created on: Feb 6, 2024
 *      Author: tian_
 */

#pragma once

#include "rtos_thread.hpp"
#include "./Source/Kernel/rtos_priority_list.hpp"
#include "./External/MyLib/tx_queue.hpp"
#include "./External/MyLib/tx_assert.h"
#include <stddef.h>

namespace RTOS
{

class Scheduler;

class MessageQueue
// Implemented in Source/rtos_scheduler.cpp
{
	friend Scheduler;


private:
	TXLib::Queue<size_t>									m_queue;
	PriorityList													m_blocked_threads;



public:
	MessageQueue(void) noexcept = default;
	MessageQueue(MessageQueue const &) noexcept = delete;
	MessageQueue(MessageQueue &&) noexcept = delete;
	~MessageQueue(void) noexcept {TX_ASSERT(m_blocked_threads.get_highest_priority() == m_blocked_threads.INVALID_PRIORITY);};
	void operator=(MessageQueue const &) noexcept = delete;
	void operator=(MessageQueue &&) noexcept = delete;

	inline bool is_initialized(void) const {return m_queue.is_initialized();}
	inline size_t get_size(void) const {return m_queue.get_size();}
	inline bool is_empty(void) const {return m_queue.is_empty();}
	inline bool is_full(void) const {return m_queue.is_full();}

	void initialize(size_t capacity);
	std::pair<size_t, bool> try_pull(size_t max_wait_time); // Wait time in ticks
	size_t pull(void); // Pull next message (if none is available, wait until one is)
	bool push(size_t message); /* Post message on the queue and relinquish to a higher-priority ready thread if one is available
	Return false if the message is not posted (due to a full queue) */


};



}
