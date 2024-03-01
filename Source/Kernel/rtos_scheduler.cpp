/*
 * rtos_thread.cpp
 *
 *  Created on: Feb 3, 2024
 *      Author: tian_
 */

#include "rtos_scheduler.hpp"
#include "rtos_impl.hpp"
#include "rtos_profiler.hpp"
#include "./Source/Driver/cortexm3_core.hpp"
#include "./External/MyLib/tx_assert.h"
#include "./External/MyLib/tx_memory_halffit.hpp"
#include "./External/MyLib/tx_arithmetic.hpp"
#include <stddef.h>
#include <atomic>


namespace RTOS
{

extern RTOSImpl g_rtos;

static Scheduler & g_scheduler = g_rtos.m_scheduler;
static SystemTimer & g_system_timer = g_rtos.m_system_timer;






void ThreadImpl::initialize_self(FunctionPtr entry, size_t entry_argument, size_t priority, size_t stack_size)
{
	TX_ASSERT((stack_size & 0b111) == 0);
	TX_ASSERT(m_state == State::Reset);

	m_stack_begin = (size_t) alloc(stack_size);
	m_stack_end = m_stack_begin + stack_size;
	m_sp = (void *) m_stack_end;
	m_entry = entry;
	m_entry_argument = entry_argument;
	m_base_priority = priority;
	m_effective_priority = priority;
	m_priority_list = nullptr;
	m_cpu_cycle_used = 0;
	m_state = State::Paused;
	m_blocking_mutex = nullptr;

	populate_stack_context();
}

struct ThreadImpl::StackContext
{
	size_t	r4;
	size_t	r5;
	size_t	r6;
	size_t	r7;
	size_t	r8;
	size_t	r9;
	size_t	r10;
	size_t	r11;
	size_t	r0;
	size_t	r1;
	size_t	r2;
	size_t	r3;
	size_t	r12;
	size_t	lr;
	size_t	pc;
	size_t	psr;
};

void ThreadImpl::populate_stack_context(void)
{
	m_sp = (void *)((size_t)m_sp - sizeof(StackContext));
	StackContext * context = (StackContext *) m_sp;

	context->pc = (size_t) &Scheduler::thread_entry;
	context->psr = (1u << 24);

	*((size_t *) m_stack_begin) = StackLimitIdentifier;

	if (PrefillStack)
	{
		for (size_t i = 0; (size_t)((size_t*) m_stack_begin + i) < (size_t) m_sp; i++)
		{
			*((size_t *) m_stack_begin + i) = StackLimitIdentifier;
		}
	}
}






void CoreInfo::initialize(void)
{
	m_idle_thread.initialize_self(& Scheduler::idle_thread, 0, PriorityList::INVALID_PRIORITY, 0x100);
	m_thread_running = &m_idle_thread;
	m_thread_on_core = &m_idle_thread;
}






void ExpirationList::insert_thread(TXLib::LinkedCycleUnsafe & link, TimeType expire_time)
{
	TX_ASSERT(m_unsorted_link.is_single() || !m_sorted_link.is_single());
	TX_ASSERT(ThreadImpl::get_thread_from_m_expire_link(link).m_expire_time == expire_time);

	if (m_sorted_link.is_single())
	{
		link.insert_single_as_next_of(m_sorted_link);
	}
	else
	{
		if (expire_time <= ThreadImpl::get_thread_from_m_expire_link(m_sorted_link.next()).m_expire_time)
		{
			link.insert_single_as_next_of(m_sorted_link);
		}
		else if (expire_time >= ThreadImpl::get_thread_from_m_expire_link(m_sorted_link.prev()).m_expire_time)
		{
			link.insert_single_as_prev_of(m_sorted_link);
		}
		else
		{
			link.insert_single_as_next_of(m_unsorted_link);
		}
	}
}

void ExpirationList::sort_unsorted(void)
{
	TXLib::LinkedCycleUnsafe * link = &m_unsorted_link.next();
	while (link != &m_unsorted_link)
	{
		TXLib::LinkedCycleUnsafe * link_next = &link->next();

		TXLib::LinkedCycle * iter = &m_sorted_link.prev();
		while (iter != &m_sorted_link && ThreadImpl::get_thread_from_m_expire_link(*iter).m_expire_time > ThreadImpl::get_thread_from_m_expire_link(*link).m_expire_time)
		{
			iter = &iter->prev();
		}
		link->insert_single_as_next_of(*iter);

		link = link_next;
	}
	link->become_safe();
}

TXLib::LinkedCycle & ExpirationList::remove_threads_sharing_smallest_expire_time(void)
{
	TX_ASSERT(!m_sorted_link.is_single());

	TXLib::LinkedCycle * removed = &m_sorted_link.next();
	TXLib::LinkedCycle * link = removed;
	while (link != &m_sorted_link &&
			ThreadImpl::get_thread_from_m_expire_link(*link).m_expire_time == ThreadImpl::get_thread_from_m_expire_link(*removed).m_expire_time)
	{
		link = &link->next();
	}
	m_sorted_link.criss_cross_with(*link);

	return *removed;
}

void SleepHeap::initialize(void)
{
	m_heap.initialize(alloc, free, 2);
}

void ExpireHeap::initialize(void)
{
	m_heap.initialize(alloc, free, 2);
}











void Scheduler::thread_entry(void)
{
	ThreadImpl & thread = *g_scheduler.m_core.m_thread_on_core;

	(*thread.m_entry)(thread.m_entry_argument); // Execute user-space code

	TX_ASSERT(*((size_t *)thread.m_stack_begin) == ThreadImpl::StackLimitIdentifier); // Failing means potential stack overflow


	Mutex::unlock_all_mutex(thread);

	g_scheduler.lock_acquire();
	g_scheduler.change_running_thread_to_terminated(g_scheduler.m_core);
	g_scheduler.change_top_ready_thread_to_running(g_scheduler.m_core);
	g_scheduler.switch_context();
	g_scheduler.lock_release();

	TX_ASSERT(0); // Should not reach here
}


// Context switch
extern "C" __attribute__((naked, flatten)) void PendSV_Handler(void)
{
	__disable_irq();
	__DMB();

	// Load psp to register r1
	__asm volatile("mrs r1, psp");

	// Save registers
	__asm volatile("stmdb r1!, {r4-r11}");

	// Save psp to ThreadInfo
	__asm volatile("str r1, [%0]" : : "r"(&g_scheduler.m_core.m_thread_on_core->m_sp) : "r1");

	// Broadcast removal of context
	__asm volatile("mov %0, %1" : "=r"(g_scheduler.m_core.m_thread_on_core) : "r"(g_scheduler.m_core.m_thread_running));

	// Update cpu cycle
	__asm volatile(
			"mov r8, lr \n"
			"ldr r4, [%0] \n"
			"ldr r5, [%1] \n"
			"ldr r6, [%2] \n"
			"sub r7, r4, r5 \n"
			"add r7, r7, r6 \n"
			"str r4, [%1] \n"
			"str r7, [%2] \n"
			"mov lr, r8"
			:
			: "r"(&DWT->CYCCNT), // This can be replaced with "r"(CoreClock::get_cycle_counter_address()) if the compiler flattens the function call
				"r"(&g_scheduler.m_core.m_last_context_switch_cycle),
				"r"(&g_scheduler.m_core.m_thread_running->m_cpu_cycle_used)
			: "memory");

	// Load psp from ThreadInfo
	__asm volatile("ldr r1, [%0]" : : "r"(&g_scheduler.m_core.m_thread_running->m_sp));

	// Load registers
	__asm volatile("ldm r1!, {r4-r11}");

	// Set psp
	__asm volatile("msr psp, r1");

	__DMB();
	__enable_irq();

	// Return
	__asm volatile("bx  lr");
}











void Scheduler::set_effective_priority(ThreadImpl & thread, size_t priority)
{
	thread.m_effective_priority = priority;

	if (thread.m_priority_list != nullptr)
	{
		thread.m_priority_list->remove_link(thread.m_priority_link);
		thread.m_priority_list->insert(thread.m_priority_link, priority);
	}
}

void Scheduler::increase_priority_of_blocking_mutexes_and_owners(Mutex * blocking_mutex, size_t priority)
{
	// The loop recurses through all mutexes that (directly or indirectly) block @blocking_mutex
	while (blocking_mutex != nullptr && priority < blocking_mutex->get_inherited_priority())
	{
		set_effective_priority(*blocking_mutex->m_owner, priority);
		blocking_mutex = blocking_mutex->m_owner->m_blocking_mutex;
	}
}










void Scheduler::change_paused_thread_to_ready(ThreadImpl & thread)
{
	TX_ASSERT(thread.m_state == ThreadImpl::State::Paused);

	thread.m_state = ThreadImpl::State::Ready;
	thread.m_priority_list = &m_ready_threads;
	m_ready_threads.insert(thread.m_priority_link, thread.m_effective_priority);
}

void Scheduler::change_ready_thread_to_paused(ThreadImpl & thread)
{
	TX_ASSERT(thread.m_state == ThreadImpl::State::Ready);

	thread.m_priority_list->remove_link(thread.m_priority_link);
	thread.m_priority_list = nullptr;
	thread.m_state = ThreadImpl::State::Paused;
}

void Scheduler::change_top_ready_thread_to_running(CoreInfo & core)
{
	TX_ASSERT(core.m_thread_running == nullptr);

	size_t available_priority = m_ready_threads.get_highest_priority();
	if (available_priority < PriorityList::INVALID_PRIORITY)
	{
		TXLib::LinkedCycleUnsafe * link = m_ready_threads.pop_link(available_priority);

		core.m_thread_running = & ThreadImpl::get_thread_from_m_priority_link(*link);
		core.m_thread_running->m_state = ThreadImpl::State::Running;
		core.m_thread_running->m_priority_list = nullptr;
	}
	else
	{
		// Execute idle thread
		core.m_thread_running = &core.m_idle_thread;
	}
}

bool Scheduler::exchange_top_ready_thread_with_running_thread(CoreInfo & core, size_t skip_priority)
{
	size_t waiting_priority = m_ready_threads.get_highest_priority();
	if (waiting_priority < skip_priority)
	{
		TXLib::LinkedCycleUnsafe * link = m_ready_threads.pop_link(waiting_priority);

		ThreadImpl & thread_exit = *core.m_thread_running;
		thread_exit.m_state = ThreadImpl::State::Ready;
		thread_exit.m_priority_list = &m_ready_threads;
		m_ready_threads.insert(thread_exit.m_priority_link, thread_exit.m_effective_priority);

		core.m_thread_running = & ThreadImpl::get_thread_from_m_priority_link(*link);
		core.m_thread_running->m_state = ThreadImpl::State::Running;
		core.m_thread_running->m_priority_list = nullptr;

		return true;
	}
	else
	{
		return false;
	}
}

void Scheduler::change_running_thread_to_ready(CoreInfo & core)
{
	core.m_thread_running->m_state = ThreadImpl::State::Ready;
	core.m_thread_running->m_priority_list = &m_ready_threads;
	m_ready_threads.insert(core.m_thread_running->m_priority_link, core.m_thread_running->m_effective_priority);

	core.m_thread_running = nullptr;
}

void Scheduler::change_running_thread_to_terminated(CoreInfo & core)
{
	TX_ASSERT(core.m_thread_running->m_state == ThreadImpl::State::Running);

	core.m_thread_running->m_state = ThreadImpl::State::Terminated;
	core.m_thread_running = nullptr;
}

void Scheduler::change_running_thread_to_paused(CoreInfo & core)
{
	TX_ASSERT(core.m_thread_running->m_state == ThreadImpl::State::Running);

	core.m_thread_running->m_state = ThreadImpl::State::Paused;
	core.m_thread_running = nullptr;
}

void Scheduler::change_running_thread_to_sleeping(CoreInfo & core, TimeType expire_time)
{
	core.m_thread_running->m_state = ThreadImpl::State::Sleeping;
	core.m_thread_running->m_expire_time = expire_time;

	if (UseListVersionForThreadSleep)
	{
		TX_ASSERT(expire_time > g_system_timer.get_tick());
		m_expiration_list.insert_thread(core.m_thread_running->m_expire_link, expire_time);
	}
	else
	{
		m_sleep_heap.insert(*core.m_thread_running);
	}

	core.m_thread_running = nullptr;
}

void Scheduler::change_running_thread_to_mutexblocked(CoreInfo & core, Mutex & blocking_mutex)
{
	increase_priority_of_blocking_mutexes_and_owners(&blocking_mutex, core.m_thread_running->m_effective_priority);

	core.m_thread_running->m_state = ThreadImpl::State::BlockedByMutex;
	core.m_thread_running->m_blocking_mutex = &blocking_mutex;
	core.m_thread_running->m_priority_list = &blocking_mutex.m_blocked_threads;
	blocking_mutex.m_blocked_threads.insert(core.m_thread_running->m_priority_link, core.m_thread_running->m_effective_priority);

	core.m_thread_running = nullptr;
}

void Scheduler::change_running_thread_to_softmutexblocked(CoreInfo & core, Mutex & blocking_mutex, TimeType expire_time)
{
	increase_priority_of_blocking_mutexes_and_owners(&blocking_mutex, core.m_thread_running->m_effective_priority);

	core.m_thread_running->m_state = ThreadImpl::State::SoftBlockedByMutex;
	core.m_thread_running->m_blocking_mutex = &blocking_mutex;
	core.m_thread_running->m_priority_list = &blocking_mutex.m_blocked_threads;
	core.m_thread_running->m_expire_time = expire_time;
	blocking_mutex.m_blocked_threads.insert(core.m_thread_running->m_priority_link, core.m_thread_running->m_effective_priority);

	if (UseListVersionForSoftBlockExpiration)
	{
		TX_ASSERT(expire_time > g_system_timer.get_tick());
		m_expiration_list.insert_thread(core.m_thread_running->m_expire_link, expire_time);
	}
	else
	{
		m_expire_heap.insert(*core.m_thread_running);
	}

	core.m_thread_running = nullptr;
}

void Scheduler::change_running_thread_to_messageblocked(CoreInfo & core, MessageQueue & queue)
{
	core.m_thread_running->m_state = ThreadImpl::State::BlockedByMessage;
	core.m_thread_running->m_priority_list = &queue.m_blocked_threads;
	queue.m_blocked_threads.insert(core.m_thread_running->m_priority_link, core.m_thread_running->m_effective_priority);
	core.m_thread_running = nullptr;
}

void Scheduler::change_running_thread_to_softmessageblocked(CoreInfo & core, MessageQueue & queue, TimeType expire_time)
{
	core.m_thread_running->m_state = ThreadImpl::State::SoftBlockedByMessage;
	core.m_thread_running->m_priority_list = &queue.m_blocked_threads;
	core.m_thread_running->m_expire_time = expire_time;
	queue.m_blocked_threads.insert(core.m_thread_running->m_priority_link, core.m_thread_running->m_effective_priority);

	if (UseListVersionForSoftBlockExpiration)
	{
		TX_ASSERT(expire_time > g_system_timer.get_tick());
		m_expiration_list.insert_thread(core.m_thread_running->m_expire_link, expire_time);
	}
	else
	{
		m_expire_heap.insert(*core.m_thread_running);
	}

	core.m_thread_running = nullptr;
}

void Scheduler::change_expired_thread_to_ready(TimeType time)
{
	if (UseListVersionForThreadSleep || UseListVersionForSoftBlockExpiration)
	{
		change_expired_sleeping_thread_to_ready_version_list(time);
	}

	if (!UseListVersionForThreadSleep)
	{
		change_expired_sleeping_thread_to_ready_version_heap(time);
	}
	if (!UseListVersionForSoftBlockExpiration)
	{
		change_expired_softblocked_thread_to_ready_version_heap(time);
	}
}

void Scheduler::change_sleeping_thread_to_sleepingpaused(ThreadImpl & thread)
{
	TX_ASSERT(thread.m_state == ThreadImpl::State::Sleeping);

	thread.m_state = ThreadImpl::State::SleepingAndPaused;
}

void Scheduler::change_sleepingpaused_thread_to_sleeping(ThreadImpl & thread)
{
	TX_ASSERT(thread.m_state == ThreadImpl::State::SleepingAndPaused);

	thread.m_state = ThreadImpl::State::SleepingAndPaused;
}

void Scheduler::change_top_mutexblocked_thread_to_ready(Mutex & mutex)
{
	size_t blocked_priority = mutex.m_blocked_threads.get_highest_priority();
	if (blocked_priority < PriorityList::INVALID_PRIORITY)
	{
		TXLib::LinkedCycleUnsafe * link = mutex.m_blocked_threads.pop_link(blocked_priority);
		ThreadImpl & thread = ThreadImpl::get_thread_from_m_priority_link(*link);
		TX_ASSERT(thread.m_state == ThreadImpl::State::BlockedByMutex || thread.m_state == ThreadImpl::State::SoftBlockedByMutex);

		if (thread.m_state == ThreadImpl::State::SoftBlockedByMutex)
		{
			if (UseListVersionForSoftBlockExpiration)
			{
				m_expiration_list.remove(thread.m_expire_link);
			}
			else
			{
				bool success = m_expire_heap.remove(thread);
				tx_assert(success);
			}
		}

		thread.m_state = ThreadImpl::State::Ready;
		thread.m_blocking_mutex = nullptr;
		thread.m_priority_list = &m_ready_threads;
		m_ready_threads.insert(*link, thread.m_effective_priority);
	}
}

void Scheduler::change_mutexblocked_thread_to_paused(ThreadImpl & thread)
{
	TX_ASSERT(thread.m_state == ThreadImpl::State::BlockedByMutex);

	thread.m_priority_list->remove_link(thread.m_priority_link);
	thread.m_priority_list = nullptr;
	thread.m_state = ThreadImpl::State::Paused;
	thread.m_blocking_mutex = nullptr;
}

void Scheduler::change_top_messageblocked_thread_to_ready(MessageQueue & queue)
{
	size_t blocked_priority = queue.m_blocked_threads.get_highest_priority();
	if (blocked_priority < PriorityList::INVALID_PRIORITY)
	{
		TXLib::LinkedCycleUnsafe * link = queue.m_blocked_threads.pop_link(blocked_priority);
		ThreadImpl & thread = ThreadImpl::get_thread_from_m_priority_link(*link);
		TX_ASSERT(thread.m_state == ThreadImpl::State::BlockedByMessage || thread.m_state == ThreadImpl::State::SoftBlockedByMessage);

		if (thread.m_state == ThreadImpl::State::SoftBlockedByMessage)
		{
			if (UseListVersionForSoftBlockExpiration)
			{
				m_expiration_list.remove(thread.m_expire_link);
			}
			else
			{
				bool success = m_expire_heap.remove(thread);
				tx_assert(success);
			}
		}

		thread.m_state = ThreadImpl::State::Ready;
		thread.m_priority_list = &m_ready_threads;
		m_ready_threads.insert(thread.m_priority_link, thread.m_effective_priority);
	}
}

void Scheduler::change_messageblocked_thread_to_paused(ThreadImpl & thread)
{
	TX_ASSERT(thread.m_state == ThreadImpl::State::BlockedByMessage);

	thread.m_priority_list->remove_link(thread.m_priority_link);
	thread.m_priority_list = nullptr;
	thread.m_state = ThreadImpl::State::Paused;
}


// List version

void Scheduler::change_expired_sleeping_thread_to_ready_version_list(TimeType time)
{
	if (&m_expiration_list.get_next_thread_link() != &m_expiration_list.get_null_link())
	{
		ThreadImpl * thread = & ThreadImpl::get_thread_from_m_expire_link(m_expiration_list.get_next_thread_link());

		TX_ASSERT(thread->m_expire_time >= time);
		if (thread->m_expire_time <= time)
		{
			TXLib::LinkedCycle * head = & m_expiration_list.remove_threads_sharing_smallest_expire_time();
			TXLib::LinkedCycle * link = head;

			do
			{
				thread = & ThreadImpl::get_thread_from_m_expire_link(*link);

				switch (thread->m_state)
				{
				case ThreadImpl::State::Sleeping:
					m_ready_threads.insert(thread->m_priority_link, thread->m_effective_priority);
					thread->m_priority_list = &m_ready_threads;
					thread->m_state = ThreadImpl::State::Ready;
					break;
				case ThreadImpl::State::SleepingAndPaused:
					thread->m_state = ThreadImpl::State::Paused;
					break;
				case ThreadImpl::State::SoftBlockedByMessage:
				case ThreadImpl::State::SoftBlockedByMutex:
					thread->m_priority_list->remove_link(thread->m_priority_link);
					m_ready_threads.insert(thread->m_priority_link, thread->m_effective_priority);
					thread->m_priority_list = &m_ready_threads;
					thread->m_state = ThreadImpl::State::Ready;
					break;
				default:
					TX_ASSERT(0);
				}

				link = &link->next();
			}
			while (link != head);

			m_expiration_list.sort_unsorted();
		}
	}
}


// Heap version

void Scheduler::change_expired_sleeping_thread_to_ready_version_heap(TimeType time)
{
	while (m_sleep_heap.get_size() > 0)
	{
		if (m_sleep_heap.get_top()->m_expire_time > time) {break;}
		ThreadImpl & thread = *m_sleep_heap.pop_top();

		switch (thread.m_state)
		{
		case ThreadImpl::State::Sleeping:
			m_ready_threads.insert(thread.m_priority_link, thread.m_effective_priority);
			thread.m_priority_list = &m_ready_threads;
			thread.m_state = ThreadImpl::State::Ready;
			break;
		case ThreadImpl::State::SleepingAndPaused:
			thread.m_state = ThreadImpl::State::Paused;
			break;
		default:
			TX_ASSERT(0);
		}
	}
}

void Scheduler::change_expired_softblocked_thread_to_ready_version_heap(TimeType time)
{
	while (m_expire_heap.get_size() > 0)
	{
		if (m_expire_heap.get_top()->m_expire_time > time) {break;}
		ThreadImpl & thread = *m_expire_heap.pop_top();

		TX_ASSERT(thread.m_state == ThreadImpl::State::SoftBlockedByMessage
				|| thread.m_state == ThreadImpl::State::SoftBlockedByMutex);

		m_expire_heap.remove(thread);
		thread.m_priority_list->remove_link(thread.m_priority_link);
		m_ready_threads.insert(thread.m_priority_link, thread.m_effective_priority);
		thread.m_priority_list = &m_ready_threads;
		thread.m_state = ThreadImpl::State::Ready;
	}
}

























/* Helper functions */

void Scheduler::lock_acquire(void)
{
	m_spinlock.acquire();
}

void Scheduler::lock_release(void)
{

	m_spinlock.release();
}



extern "C" size_t Scheduler::idle_thread(size_t arg) // Extern "C" is needed for the linker to find @_estack
{
	extern uint32_t _estack;
	__asm volatile(
			"mov sp, %0 \n" // Reset msp
			"mov r1, #0b10 \n"
			"msr control, r1 \n" // Use psp
			"mov sp, %1 \n" // Set psp
			:
			: "r"(&_estack), "r"(g_scheduler.m_core.m_thread_running->m_stack_end)
			: "memory");

	CoreInterrupt::enable_systick();
	CoreInterrupt::trigger_systick_interrupt();

	while (1)
	{
		g_scheduler.maintenance_procedure();
		g_scheduler.sleep_procedure();
	}
}



TimeType Scheduler::get_latest_wakeup_time_in_tick(TimeType time_now)
{
	constexpr size_t const MAX_TICK_UNTIL_WAKEUP = CoreInterrupt::get_systick_max_countdown_in_core_cycle() / RTOSImpl::CoreCyclePerTick - 1u;
	TimeType expire_time = time_now + MAX_TICK_UNTIL_WAKEUP; // This number ensures that the SysTick countdown timer does not overflow

	if (&m_expiration_list.get_next_thread_link() != &m_expiration_list.get_null_link())
	{
		ThreadImpl & thread = ThreadImpl::get_thread_from_m_expire_link(m_expiration_list.get_next_thread_link());
		if (thread.m_expire_time - 1 < expire_time)
		{
			expire_time = thread.m_expire_time - 1;
		}
	}

	if (m_sleep_heap.get_size() > 0 && m_sleep_heap.get_top()->m_expire_time - 1 < expire_time)
	{
		expire_time = m_sleep_heap.get_top()->m_expire_time - 1;
	}

	if (m_expire_heap.get_size() > 0 && m_expire_heap.get_top()->m_expire_time - 1 < expire_time)
	{
		expire_time = m_expire_heap.get_top()->m_expire_time - 1;
	}

	return expire_time;
}

void Scheduler::maintenance_procedure(void)
{
	TX_ASSERT(__get_PRIMASK() == 0);
	__disable_irq();
	__DMB();

	m_expiration_list.sort_unsorted();

	__DMB();
	__enable_irq();
}

void Scheduler::sleep_procedure(void)
{
	TX_ASSERT(__get_PRIMASK() == 0);
	__disable_irq();
	__DMB();

	lock_acquire();
	TimeType system_time_in_tick = g_system_timer.get_tick();
	TimeType wakeup_time_in_tick = get_latest_wakeup_time_in_tick(system_time_in_tick);
	lock_release();

	if (wakeup_time_in_tick <= system_time_in_tick)
	{
		// Abort if there is no time to sleep
		__DMB();
		__enable_irq();
		return;
	}

	// Commit to sleep

	size_t tick_until_wakeup = wakeup_time_in_tick - system_time_in_tick;
	TimeType wakeup_time_in_cycle = g_system_timer.get_core_cycle() + tick_until_wakeup * RTOSImpl::CoreCyclePerTick;
	TX_ASSERT(TimeType(wakeup_time_in_cycle) > CoreClock::get_cycle_count() + (RTOSImpl::CoreCyclePerTick / 2));
	CoreInterrupt::reset_counter(wakeup_time_in_cycle - TimeType(CoreClock::get_cycle_count()));
	g_system_timer.set_max_allowable_tick(tick_until_wakeup);

	LowPowerState::enter_sleep_mode();

	TimeType next_systick_time = g_system_timer.update_time(CoreClock::get_cycle_count());
	CoreInterrupt::reset_counter(next_systick_time - TimeType(CoreClock::get_cycle_count()));
	g_system_timer.set_max_allowable_tick(1);
	CoreInterrupt::clear_systick_interrupt();

	__DMB();
	__enable_irq();
}

void Scheduler::switch_context(void)
{
	CoreInterrupt::trigger_pendsv_interrupt();

	TX_ASSERT(*((size_t *)m_core.m_thread_on_core->m_stack_begin) == ThreadImpl::StackLimitIdentifier); // Failing means potential stack overflow
}


void Scheduler::pause_thread_impl(ThreadImpl & thread)
{
	switch (thread.m_state)
	{
	case ThreadImpl::State::Ready:
		g_scheduler.change_ready_thread_to_paused(thread);
		break;
	case ThreadImpl::State::BlockedByMutex:
		g_scheduler.change_mutexblocked_thread_to_paused(thread);
		break;
	case ThreadImpl::State::BlockedByMessage:
		g_scheduler.change_messageblocked_thread_to_paused(thread);
		break;
	case ThreadImpl::State::Sleeping:
		g_scheduler.change_sleeping_thread_to_sleepingpaused(thread);
		break;
	case ThreadImpl::State::Running:  // TODO: This does not work for thread running on a different core
		TX_ASSERT(&thread == m_core.m_thread_running);
		change_running_thread_to_paused(m_core);
		change_top_ready_thread_to_running(m_core);
		switch_context();
		break;
	case ThreadImpl::State::Paused:
	case ThreadImpl::State::SleepingAndPaused:
		break;
	default:
		TX_ASSERT(0);
	}
}









void Scheduler::initialize(FunctionPtr entry, size_t stack_size)
{
	m_expiration_list.initialize();
	m_sleep_heap.initialize();
	m_expire_heap.initialize();
	m_core.initialize();

	m_first_user_thread.initialize_self(entry, 0, PriorityList::MAX_PRIORITY, stack_size);
	change_paused_thread_to_ready(m_first_user_thread);

	LowPowerState::initialize();
	CoreInterrupt::initialize();

	idle_thread(0); // The main thread becomes the idle thread here
	TX_ASSERT(0); // Should not reach here
}

void Scheduler::systick_update(TimeType time)
{
	lock_acquire();
	RTOS_PROFILER_START("systick_update");

	change_expired_thread_to_ready(time);

	if (m_core.m_thread_running != &m_core.m_idle_thread)
	{
		if (exchange_top_ready_thread_with_running_thread(m_core, m_core.m_thread_running->m_effective_priority))
		{
			switch_context();
		}
	}
	else
	{
		// Reaching here means that the core is running the idle thread
		m_core.m_thread_running = nullptr;
		change_top_ready_thread_to_running(m_core);
		switch_context();
	}

	RTOS_PROFILER_STOP("systick_update");
	lock_release();
}

void Scheduler::kill_thread(ThreadImpl & thread)
{
	lock_acquire();
	RTOS_PROFILER_START("kill_thread");

	pause_thread_impl(thread);
	thread.m_state = ThreadImpl::State::Terminated;

	RTOS_PROFILER_STOP("kill_thread");
	lock_release();

	Mutex::unlock_all_mutex(thread);
}

void Scheduler::pause_thread(ThreadImpl & thread)
{
	lock_acquire();
	RTOS_PROFILER_START("pause_thread");

	pause_thread_impl(thread);

	RTOS_PROFILER_STOP("pause_thread");
	lock_release();
}

void Scheduler::unpause_thread(ThreadImpl & thread)
{
	lock_acquire();
	RTOS_PROFILER_START("unpause_thread");

	switch (thread.m_state)
	{
	case ThreadImpl::State::Paused:
		change_paused_thread_to_ready(thread);
		break;
	case ThreadImpl::State::SleepingAndPaused:
		change_sleepingpaused_thread_to_sleeping(thread);
		break;
	case ThreadImpl::State::Reset:
	case ThreadImpl::State::Terminated:
		TX_ASSERT(0);
		break;
	default:
		break;
	}

	RTOS_PROFILER_STOP("unpause_thread");
	lock_release();
}

void Scheduler::relinquish(CoreInfo & core)
/* Replace the thread with state RUNNING on @core with a thread of equal or higher priority
 * Do nothing if no candidate exists
 */
{
	lock_acquire();
	RTOS_PROFILER_START("relinquish");

	if (exchange_top_ready_thread_with_running_thread(core, core.m_thread_running->m_effective_priority + 1))
	{
		switch_context();
	}

	RTOS_PROFILER_STOP("relinquish");
	lock_release();
}

void Scheduler::sleep_until(CoreInfo & core, TimeType expire_time)
// Put the thread with state RUNNING on @core to sleep
{
	lock_acquire();
	RTOS_PROFILER_START("sleep");

	if (expire_time > g_system_timer.get_tick())
	{
		change_running_thread_to_sleeping(core, expire_time);
		change_top_ready_thread_to_running(core);
		switch_context();
	}

	RTOS_PROFILER_STOP("sleep");
	lock_release();
}








inline void Mutex::set_owner(ThreadImpl & thread)
{
	m_owner = &thread;
	m_sibling.insert_single_as_prev_of(thread.m_owned_mutex);
}

inline void Mutex::set_orphan(void)
{
	m_owner = nullptr;
	m_sibling.remove_from_cycle();
}

size_t Mutex::get_max_inherited_priority_among_siblings(TXLib::LinkedCycle & sibling_link, size_t base_priority)
{
	TXLib::LinkedCycle * link = &sibling_link.next();
	while (link != &sibling_link)
	{
		Mutex & mutex = Mutex::get_mutex_from_m_sibling(*link);
		size_t mutex_priority = mutex.get_inherited_priority();
		if (mutex_priority < base_priority)
		{
			base_priority = mutex_priority;
		}
		link = &link->next();
	}
	return base_priority;
}

void Mutex::unlock_all_mutex(ThreadImpl & thread) // Acquire scheduler lock, does not change thread priority
{
	while (!thread.m_owned_mutex.is_single())
	{
		g_scheduler.lock_acquire();
		Mutex & mutex = Mutex::get_mutex_from_m_sibling(thread.m_owned_mutex.next());
		mutex.set_orphan();
		g_scheduler.change_top_mutexblocked_thread_to_ready(mutex);
		g_scheduler.lock_release();
	}
}

void Mutex::lock(void)
{
	TX_ASSERT(__get_CONTROL() & 0x10b); // Cannot be called in handler mode
	TX_ASSERT(g_scheduler.m_core.m_thread_running == g_scheduler.m_core.m_thread_on_core);

	bool success = false;
	while (!success)
	{
		g_scheduler.lock_acquire();
		RTOS_PROFILER_START("mutex_lock");

		if (m_owner == nullptr)
		{
			set_owner(*g_scheduler.m_core.m_thread_running);
			TX_ASSERT(m_owner->m_effective_priority <= get_inherited_priority()); // Should not happen during single-core execution; multi-core TODO
			success = true;
		}
		else
		{
			TX_Assert(m_owner != g_scheduler.m_core.m_thread_running); // Re-acquiring an acquired lock is forbidden

			g_scheduler.change_running_thread_to_mutexblocked(g_scheduler.m_core, *this);
			g_scheduler.change_top_ready_thread_to_running(g_scheduler.m_core);
			g_scheduler.switch_context();
		}

		RTOS_PROFILER_STOP("mutex_lock");
		g_scheduler.lock_release();
	}
}

bool Mutex::try_lock(size_t max_wait_time)
{
	TX_ASSERT(__get_CONTROL() & 0x10b); // Cannot be called in handler mode
	TX_ASSERT(g_scheduler.m_core.m_thread_running == g_scheduler.m_core.m_thread_on_core);

	TimeType skip_time = g_system_timer.get_tick() + max_wait_time;

	enum class State
	{
		Trying,
		Acquired,
		TimeOut,
	} state = State::Trying;

	do
	{
		g_scheduler.lock_acquire();
		RTOS_PROFILER_START("mutex_try_lock");

		if (m_owner == nullptr)
		{
			set_owner(*g_scheduler.m_core.m_thread_running);
			TX_ASSERT(m_owner->m_effective_priority <= get_inherited_priority()); // Should not happen during single-core execution; multi-core TODO
			state = State::Acquired;
		}
		else if (skip_time <= g_system_timer.get_tick())
		{
			state = State::TimeOut;
		}
		else
		{
			TX_Assert(m_owner != g_scheduler.m_core.m_thread_running); // Re-acquiring an acquired lock is forbidden

			g_scheduler.change_running_thread_to_softmutexblocked(g_scheduler.m_core, *this, skip_time);
			g_scheduler.change_top_ready_thread_to_running(g_scheduler.m_core);
			g_scheduler.switch_context();
		}

		RTOS_PROFILER_STOP("mutex_try_lock");
		g_scheduler.lock_release();
	}
	while (state == State::Trying);

	return state == State::Acquired;
}

void Mutex::unlock(void)
{
	TX_ASSERT(__get_CONTROL() & 0x10b); // Cannot be called in handler mode
	TX_ASSERT(g_scheduler.m_core.m_thread_running == g_scheduler.m_core.m_thread_on_core);
	TX_ASSERT(m_owner == g_scheduler.m_core.m_thread_on_core);

	g_scheduler.lock_acquire();
	RTOS_PROFILER_START("mutex_unlock");

	ThreadImpl & thread = *m_owner;

	set_orphan();
	g_scheduler.change_top_mutexblocked_thread_to_ready(*this);

	// Reset priority of owner
	size_t thread_priority = get_max_inherited_priority_among_siblings(thread.m_owned_mutex, thread.m_base_priority);
	g_scheduler.set_effective_priority(thread, thread_priority);

	// Relinquish
	if (g_scheduler.exchange_top_ready_thread_with_running_thread(g_scheduler.m_core, thread.m_effective_priority))
	{
		g_scheduler.switch_context();
	}

	RTOS_PROFILER_STOP("mutex_unlock");
	g_scheduler.lock_release();
}



void MessageQueue::initialize(size_t capacity)
{
	m_queue.initialize(alloc, free, capacity);
}

size_t MessageQueue::pull(void)
{
	TX_ASSERT(__get_CONTROL() & 0x10b); // Cannot be called in handler mode
	TX_ASSERT(is_initialized());

	size_t message;
	bool success = false;
	while (!success)
	{
		g_scheduler.lock_acquire();
		RTOS_PROFILER_START("msgqueue_pull");

		if (!m_queue.is_empty())
		{
			message = m_queue.pop_front();
			success = true;
		}
		else
		{
			g_scheduler.change_running_thread_to_messageblocked(g_scheduler.m_core, *this);
			g_scheduler.change_top_ready_thread_to_running(g_scheduler.m_core);
			g_scheduler.switch_context();
		}

		RTOS_PROFILER_STOP("msgqueue_pull");
		g_scheduler.lock_release();
	}
	return message;
}

std::pair<size_t, bool> MessageQueue::try_pull(size_t max_wait_time)
{
	TX_ASSERT(__get_CONTROL() & 0x10b); // Cannot be called in handler mode
	TX_ASSERT(is_initialized());

	TimeType skip_time = g_system_timer.get_tick() + max_wait_time;
	size_t message;

	enum class State
	{
		Trying,
		Acquired,
		TimeOut,
	} state = State::Trying;

	while (state == State::Trying)
	{
		g_scheduler.lock_acquire();
		RTOS_PROFILER_START("msgqueue_try_pull");

		if (!m_queue.is_empty())
		{
			message = m_queue.pop_front();
			state = State::Acquired;
		}
		else if (skip_time <= g_system_timer.get_tick())
		{
			state = State::TimeOut;
		}
		else
		{
			g_scheduler.change_running_thread_to_softmessageblocked(g_scheduler.m_core, *this, skip_time);
			g_scheduler.change_top_ready_thread_to_running(g_scheduler.m_core);
			g_scheduler.switch_context();
		}

		RTOS_PROFILER_STOP("msgqueue_try_pull");
		g_scheduler.lock_release();
	}

	return std::pair<size_t, bool>(message, state == State::Acquired);
}

bool MessageQueue::push(size_t message)
{
	TX_ASSERT(__get_CONTROL() & 0x10b); // Cannot be called in handler mode
	TX_ASSERT(g_scheduler.m_core.m_thread_running == g_scheduler.m_core.m_thread_on_core);
	TX_ASSERT(is_initialized());

	bool success;

	g_scheduler.lock_acquire();
	RTOS_PROFILER_START("msgqueue_push");

	if (m_queue.is_full())
	{
		success = false;
	}
	else
	{
		m_queue.push_back(message);

		g_scheduler.change_top_messageblocked_thread_to_ready(*this);

		if (g_scheduler.exchange_top_ready_thread_with_running_thread(g_scheduler.m_core, g_scheduler.m_core.m_thread_running->m_effective_priority))
		{
			g_scheduler.switch_context();
		}

		success = true;
	}

	RTOS_PROFILER_STOP("msgqueue_push");
	g_scheduler.lock_release();

	return success;
}








void Thread::initialize(FunctionPtr entry, size_t entry_argument, size_t priority, size_t stack_size)
{
	ThreadImpl & thread = *reinterpret_cast<ThreadImpl *>(this);
	thread.initialize_self(entry, entry_argument, priority, stack_size);

	g_scheduler.lock_acquire();
	g_scheduler.change_paused_thread_to_ready(thread);
	g_scheduler.lock_release();
}

void Thread::uninitialize(void)
{
	TX_ASSERT(m_state == State::Terminated);
	free((void*) m_stack_begin);
	m_state = State::Reset; // In this state, the thread does not have ownership of any stack memory
}

Thread::~Thread(void)
{
	TX_ASSERT(m_state == State::Reset || m_state == State::Terminated);
	if (m_state == State::Terminated)
	{
		free((void*) m_stack_begin);
	}
}

void relinquish(void)
{
	TX_ASSERT(__get_CONTROL() & 0x10b); // Cannot be called in handler mode
	TX_ASSERT(g_scheduler.m_core.m_thread_running == g_scheduler.m_core.m_thread_on_core);

	g_scheduler.relinquish(g_scheduler.m_core);
}

void sleep(size_t sleep_duration)
{
	TX_ASSERT(__get_CONTROL() & 0x10b); // Cannot be called in handler mode
	TX_ASSERT(g_scheduler.m_core.m_thread_running == g_scheduler.m_core.m_thread_on_core);

	g_scheduler.sleep_until(g_scheduler.m_core, g_system_timer.get_tick() + sleep_duration);
}

void Thread::pause(void)
{
	g_scheduler.pause_thread(*reinterpret_cast<ThreadImpl *>(this));
}

void Thread::kill(void)
{
	g_scheduler.kill_thread(*reinterpret_cast<ThreadImpl *>(this));
}

void Thread::unpause(void)
{
	g_scheduler.unpause_thread(*reinterpret_cast<ThreadImpl *>(this));
}








} // namespace RTOS
