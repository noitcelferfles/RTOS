/*
 * rtos_thread_impl.hpp
 *
 *  Created on: Feb 28, 2024
 *      Author: tian_
 */

#pragma once

#include "./Source/PublicApi/rtos_thread.hpp"

namespace RTOS
{

extern "C" void PendSV_Handler(void);

class ThreadImpl : public Thread
{
friend class Mutex;
friend class MessageQueue;
friend class PriorityList;
friend class Scheduler;
friend class ThreadMgr;
friend class CoreInfo;
friend class ExpirationList;
friend class SleepHeap;
friend class ExpireHeap;
friend void PendSV_Handler(void);



private:
	static constexpr size_t const StackLimitIdentifier = 0xDEADBEEF;
	static constexpr bool const PrefillStack = false;

public:
	struct StackContext;


public:


	void populate_stack_context(void);

	static ThreadImpl & get_thread_from_m_priority_link(TXLib::LinkedCycleUnsafe & link)
	{
		return *reinterpret_cast<ThreadImpl *>(reinterpret_cast<size_t>(&link) - __builtin_offsetof(ThreadImpl, m_priority_link));
	}

	static ThreadImpl & get_thread_from_m_expire_link(TXLib::LinkedCycleUnsafe & link)
	{
		return *reinterpret_cast<ThreadImpl *>(reinterpret_cast<size_t>(&link) - __builtin_offsetof(ThreadImpl, m_expire_link));
	}

	void initialize_self(FunctionPtr entry, size_t entry_argument, size_t priority, size_t stack_size);


public:
	ThreadImpl(void) noexcept = default;
	ThreadImpl(ThreadImpl const &) = delete;
	ThreadImpl(ThreadImpl &&) = delete;
	~ThreadImpl(void) noexcept = default;
	void operator=(ThreadImpl const & b) = delete;
	void operator=(ThreadImpl && b) = delete;

};

} // namespace RTOS
