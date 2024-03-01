/*
 * rtos_thread_mgr.hpp
 *
 *  Created on: Feb 4, 2024
 *      Author: tian_
 */

#pragma once

#include "rtos_thread_impl.hpp"
#include "./Source/PublicApi/rtos.hpp"
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
	ThreadImpl *		m_thread_running; // Thread whose state in the scheduler is RUNNING (its context may not yet be on the core)
	ThreadImpl *		m_thread_on_core; // Thread associated to the current psp (its state need not be RUNNING if it is about to relinquish)
	// The two threads above are guaranteed to coincide outside of scheduler execution

	ThreadImpl			m_idle_thread;	// The idle thread this core executes when there is no job remaining (the scheduler does not register this thread)

	size_t					m_last_context_switch_cycle;

public:
	CoreInfo(void) noexcept = default;
	CoreInfo(CoreInfo const &) noexcept = delete;
	CoreInfo(CoreInfo &&) noexcept = delete;
	~CoreInfo(void) noexcept = default;
	void operator=(CoreInfo const &) noexcept = delete;
	void operator=(CoreInfo &&) noexcept = delete;

	void initialize(void);
};



class ExpirationList
{
public:
	TXLib::LinkedCycle		m_sorted_link;
	TXLib::LinkedCycle		m_unsorted_link;
	TimeType							m_earliest_unsorted_expire_time;

public:
	ExpirationList(void) noexcept = default;
	~ExpirationList(void) noexcept = default;
	ExpirationList(ExpirationList const &) noexcept = delete;
	ExpirationList(ExpirationList &&) noexcept = delete;
	void operator=(ExpirationList const &) noexcept = delete;
	void operator=(ExpirationList &&) noexcept = delete;

	bool is_initialized(void) const {return true;}
	void initialize(TimeType current_time);

	TXLib::LinkedCycle & get_next_thread_link(void)
	{
		return m_sorted_link.next();
	}

	TXLib::LinkedCycle & get_null_link(void)
	{
		return m_sorted_link;
	}

	void insert_thread(TXLib::LinkedCycleUnsafe & link, TimeType expire_time);

	void remove(TXLib::LinkedCycleUnsafe & link)
	{
		link.remove_from_cycle();
	}

	TXLib::LinkedCycle & remove_threads_sharing_smallest_expire_time(void);

	void sort_one_unsorted(void);
	void sort_all_unsorted(TimeType current_time);

};

class SleepHeap
{
public:
	static bool is_earlier(ThreadImpl * const & a, ThreadImpl * const & b)
	{
		return a->m_expire_time <= b->m_expire_time;
	}

public:
	TXLib::DynamicHeap<ThreadImpl *, is_earlier> 					m_heap;

public:
	SleepHeap(void) {}

	inline bool is_initialized(void) const {return m_heap.is_initialized();}
	void initialize(void);

	TimeType get_next_expire_time(void) const {return m_heap.get_size() > 0 ? m_heap.get_top()->m_expire_time : ~0;}

	void insert(ThreadImpl & thread) {m_heap.insert(&thread);}

	ThreadImpl * get_top(void) const {return m_heap.get_top();}
	ThreadImpl * pop_top(void) {return m_heap.pop_top();}

	size_t get_size(void) const {return m_heap.get_size();}
};


class ExpireHeap
{
public:
	static bool is_earlier(ThreadImpl * const & a, ThreadImpl * const & b)
	{
		return a->m_expire_time <= b->m_expire_time;
	}

public:
	TXLib::DynamicHeap<ThreadImpl *, is_earlier> 					m_heap;

public:
	ExpireHeap(void) {}

	inline bool is_initialized(void) const {return m_heap.is_initialized();}
	void initialize(void);

	TimeType get_next_expire_time(void) const {return m_heap.get_size() > 0 ? m_heap.get_top()->m_expire_time : ~0;}

	void insert(ThreadImpl & thread) {m_heap.insert(&thread);}
	bool remove(ThreadImpl & thread) {return m_heap.remove(&thread);}

	ThreadImpl * get_top(void) const {return m_heap.get_top();}
	ThreadImpl * pop_top(void) {return m_heap.pop_top();}

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

	ThreadImpl					m_first_user_thread;

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

	void change_paused_thread_to_ready(ThreadImpl & thread);
	void change_ready_thread_to_paused(ThreadImpl & thread);
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
	void change_sleeping_thread_to_sleepingpaused(ThreadImpl & thread);
	void change_sleepingpaused_thread_to_sleeping(ThreadImpl & thread);
	void change_top_mutexblocked_thread_to_ready(Mutex & mutex);
	void change_mutexblocked_thread_to_paused(ThreadImpl & thread);
	void change_top_messageblocked_thread_to_ready(MessageQueue & queue);
	void change_messageblocked_thread_to_paused(ThreadImpl & thread);

// Thread state-change primitives (helper functions)

	void set_effective_priority(ThreadImpl & thread, size_t priority);
	void increase_priority_of_blocking_mutexes_and_owners(Mutex * blocking_mutex, size_t priority);

	void change_expired_sleeping_thread_to_ready_version_list(TimeType time);
	void change_expired_sleeping_thread_to_ready_version_heap(TimeType time);
	void change_expired_softblocked_thread_to_ready_version_heap(TimeType time);




// Helper functions

	void lock_acquire(void);
	void lock_release(void);

	TimeType get_latest_wakeup_time_in_tick(TimeType time_now);
	void maintenance_procedure(void);
	void sleep_procedure(void);
	void switch_context(void);
	void pause_thread_impl(ThreadImpl & thread);


// High-level API

	__attribute__((noreturn)) void initialize(FunctionPtr entry, size_t stack_size, TimeType current_time);
	void systick_update(TimeType time);
	inline void kill_thread(ThreadImpl & thread);
	inline void pause_thread(ThreadImpl & thread);
	inline void unpause_thread(ThreadImpl & thread);
	inline void relinquish(CoreInfo & core);
	inline void sleep_until(CoreInfo & core, TimeType expire_time);


public:

	static __attribute__((noreturn)) size_t idle_thread(size_t arg);

};


extern "C" void PendSV_Handler(void); // Interrupt handler

} // namespace RTOS
