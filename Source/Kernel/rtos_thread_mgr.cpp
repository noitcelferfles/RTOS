/*
 * rtos_thread_mgr.cpp
 *
 *  Created on: Feb 11, 2024
 *      Author: tian_
 */

#include "rtos_thread_mgr.hpp"
#include "rtos_impl.hpp"


namespace RTOS
{

extern RTOSImpl g_rtos;

static ThreadMgr & g_thread_mgr = g_rtos.m_thread_mgr;
static Scheduler & g_scheduler = g_rtos.m_scheduler;



ThreadMgr::~ThreadMgr(void) noexcept
{
	if (m_thread_list.is_initialized())
	{
		for (size_t i = 0; i < m_thread_list.get_size(); i++)
		{
			TX_ASSERT(m_thread_list[i]->m_state == Thread::State::Reset);
			free(m_thread_list[i]);
		}
	}
}

void ThreadMgr::initialize(void)
{
	m_thread_list.initialize(alloc, free, 2);
}

Thread & ThreadMgr::create_empty_thread(void)
{
	TX_ASSERT(is_initialized());

	void * mem_ptr = alloc(sizeof(Thread));

	Thread * thread = ::new(mem_ptr) Thread();

	m_spinlock.acquire();
	m_thread_list.push_back(thread);
	m_spinlock.release();

	return *thread;
}

void ThreadMgr::delete_empty_thread(Thread & thread)
{
	TX_ASSERT(is_initialized());
	TX_ASSERT(thread.m_state == Thread::State::Reset);

	m_spinlock.acquire();
	for (size_t i = 0; i < m_thread_list.get_size(); i++)
	{
		if (m_thread_list[i] == &thread)
		{
			free(&thread);
			m_thread_list.pop_item_at(i);
			m_spinlock.release();
			return;
		}
	}
	m_spinlock.release();

	TX_ASSERT(0); // Reaching here means that the thread was not created by ThreadMgr
}


size_t get_total_thread_cycle(void)
{
	g_scheduler.lock_acquire();

	size_t tally = 0;
	for (size_t i = 0; i < g_thread_mgr.m_thread_list.get_size(); i++)
	{
		tally += g_thread_mgr.m_thread_list[i]->m_cpu_cycle_used;
	}

	g_scheduler.lock_release();

	return tally;
}



}
