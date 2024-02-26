/*
 * rtos_thread_mgr.hpp
 *
 *  Created on: Feb 4, 2024
 *      Author: tian_
 */

#pragma once

#include "./Source/PublicApi/rtos.hpp"
#include "rtos_thread_mgr.hpp"
#include <atomic>
#include "./External/MyLib/tx_heap.hpp"
#include "./External/MyLib/tx_array.hpp"
#include "./External/MyLib/tx_spinlock.hpp"
#include "./External/MyLib/tx_linkedlist.hpp"


namespace RTOS
{


class CoreInfo
{

public:
	Thread *		m_thread_running; // Thread whose state in the scheduler is RUNNING (its context may not yet be on the core)
	Thread *		m_thread_on_core; // Thread associated to the current psp (its state need not be RUNNING if it is about to relinquish)
	// The two threads above are guaranteed to coincide outside of scheduler execution

	size_t					m_last_context_switch_cycle;

public:
	CoreInfo(void) noexcept = default;
	CoreInfo(CoreInfo const &) noexcept = delete;
	CoreInfo(CoreInfo &&) noexcept = delete;
	~CoreInfo(void) noexcept = default;
	void operator=(CoreInfo const &) noexcept = delete;
	void operator=(CoreInfo &&) noexcept = delete;
};



class ExpirationList
{
public:
	static constexpr size_t const BuckerCountLog2 = 5;
	static constexpr size_t const BucketCount = 1u << BuckerCountLog2;
	static constexpr size_t const BucketMask = BucketCount - 1;

public:
	TXLib::LinkedCycle		m_link;

public:
	ExpirationList(void) noexcept = default;
	~ExpirationList(void) noexcept = default;
	ExpirationList(ExpirationList const &) noexcept = delete;
	ExpirationList(ExpirationList &&) noexcept = delete;
	void operator=(ExpirationList const &) noexcept = delete;
	void operator=(ExpirationList &&) noexcept = delete;

	bool is_initialized(void) const {return true;}
	void initialize(void) {}

	TXLib::LinkedCycle & get_next_thread_link(void)
	{
		return m_link.next();
	}

	TXLib::LinkedCycle & get_null_link(void)
	{
		return m_link;
	}

	void insert_thread(TXLib::LinkedCycleUnsafe & link, TimeType expire_time)
	{
		TXLib::LinkedCycle * iter = &m_link.prev();
		while (iter != &m_link && Thread::get_thread_from_m_expire_link(*iter).m_expire_time > expire_time)
		{
			iter = &iter->prev();
		}
		link.insert_single_as_next_of(*iter);
	}

	void remove(TXLib::LinkedCycleUnsafe & link)
	{
		link.remove_from_cycle();
	}

};

class SleepHeap
{
public:
	static bool is_earlier(Thread * const & a, Thread * const & b)
	{
		return a->m_expire_time <= b->m_expire_time;
	}

public:
	TXLib::DynamicHeap<Thread *, is_earlier> 					m_heap;

public:
	SleepHeap(void) {}

	inline bool is_initialized(void) const {return m_heap.is_initialized();}
	void initialize(void);

	TimeType get_next_expire_time(void) const {return m_heap.get_size() > 0 ? m_heap.get_top()->m_expire_time : ~0;}

	void insert(Thread & thread) {m_heap.insert(&thread);}

	Thread * get_top(void) const {return m_heap.get_top();}
	Thread * pop_top(void) {return m_heap.pop_top();}

	size_t get_size(void) const {return m_heap.get_size();}
};


class ExpireHeap
{
public:
	static bool is_earlier(Thread * const & a, Thread * const & b)
	{
		return a->m_expire_time <= b->m_expire_time;
	}

public:
	TXLib::DynamicHeap<Thread *, is_earlier> 					m_heap;

public:
	ExpireHeap(void) {}

	inline bool is_initialized(void) const {return m_heap.is_initialized();}
	void initialize(void);

	TimeType get_next_expire_time(void) const {return m_heap.get_size() > 0 ? m_heap.get_top()->m_expire_time : ~0;}

	void insert(Thread & thread) {m_heap.insert(&thread);}
	bool remove(Thread & thread) {return m_heap.remove(&thread);}

	Thread * get_top(void) const {return m_heap.get_top();}
	Thread * pop_top(void) {return m_heap.pop_top();}

	size_t get_size(void) const {return m_heap.get_size();}
};



class Scheduler // Determines which thread to run; does not own the threads
{
	static constexpr bool const UseListVersionForThreadSleep = true;
	static constexpr bool const UseListVersionForSoftBlockExpiration = true;

public:

	CoreInfo						m_core;		// Running thread
	PriorityList				m_ready_threads; // Contains all waiting threads
	ExpirationList			m_expiration_list; // Contains threads with timed events (wakeup, try_lock expiration, etc.)
	SleepHeap						m_sleep_heap;
	ExpireHeap					m_expire_heap;

	Thread							m_idle_thread;
	Thread							m_first_user_thread;

	Spinlock						m_spinlock;


public:
	static void thread_entry(void);


public:

	Scheduler(void) noexcept = default;
	~Scheduler(void) noexcept = default;
	Scheduler(Scheduler const &) noexcept = delete;
	Scheduler(Scheduler &&) noexcept = delete;
	void operator=(Scheduler const &) noexcept = delete;
	void operator=(Scheduler &&) noexcept = delete;






// Thread state-change primitives

	void change_paused_thread_to_ready(Thread & thread);
	void change_ready_thread_to_paused(Thread & thread);
	void change_top_ready_thread_to_running(CoreInfo & core);
	bool exchange_top_ready_thread_with_running_thread(CoreInfo & core, size_t skip_priority);
	void change_running_thread_to_ready(CoreInfo & core);
	void change_running_thread_to_sleeping(CoreInfo & core, TimeType expire_time);
	void change_running_thread_to_mutexblocked(CoreInfo & core, Mutex & blocking_mutex);
	void change_running_thread_to_softmutexblocked(CoreInfo & core, Mutex & blocking_mutex, TimeType expire_time);
	void change_running_thread_to_messageblocked(CoreInfo & core, MessageQueue & queue);
	void change_running_thread_to_softmessageblocked(CoreInfo & core, MessageQueue & queue, TimeType expire_time);
	void change_running_thread_to_paused(CoreInfo & core);
	void change_running_thread_to_terminated(CoreInfo & core);
	void change_expired_thread_to_ready(TimeType time);
	void change_sleeping_thread_to_sleepingpaused(Thread & thread);
	void change_sleepingpaused_thread_to_sleeping(Thread & thread);
	void change_top_mutexblocked_thread_to_ready(Mutex & mutex);
	void change_mutexblocked_thread_to_paused(Thread & thread);
	void change_top_messageblocked_thread_to_ready(MessageQueue & queue);
	void change_messageblocked_thread_to_paused(Thread & thread);

// Thread state-change primitives (helper functions)

	void set_effective_priority(Thread & thread, size_t priority);
	void increase_priority_of_blocking_mutexes_and_owners(Mutex * blocking_mutex, size_t priority);

	void change_expired_sleeping_thread_to_ready_version_list(TimeType time);
	void change_expired_sleeping_thread_to_ready_version_heap(TimeType time);
	void change_expired_softblocked_thread_to_ready_version_heap(TimeType time);




// Helper functions

	void lock_acquire(void);
	void lock_release(void);
	void enter_sleep_mode(void);

	TimeType get_latest_wakeup_time_in_tick(TimeType time_now);
	void sleep_procedure(void);
	void switch_context(void);
	void pause_thread_impl(Thread & thread);


// High-level API

	void initialize(FunctionPtr entry, size_t stack_size);
	void systick_update(TimeType time);
	void kill_thread(Thread & thread);


};


extern "C" void PendSV_Handler(void);

}
