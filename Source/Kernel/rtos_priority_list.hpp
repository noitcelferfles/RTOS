/*
 * rtos_priority_list.hpp
 *
 *  Created on: Feb 9, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include "./External/MyLib/tx_linkedlist.hpp"

namespace RTOS
{


class PriorityList
{
public:
	static constexpr size_t const PRIORITY_BLOCK = 1;
	static constexpr size_t const INVALID_PRIORITY = PRIORITY_BLOCK * sizeof(size_t) * 8; //= 0x10;
	static constexpr size_t const MAX_PRIORITY = 0;
	static constexpr size_t const MIN_PRIORITY = INVALID_PRIORITY - 1;

	static constexpr size_t const HIGHEST_BIT_MASK = 0x80000000; static_assert(HIGHEST_BIT_MASK != 0); static_assert((HIGHEST_BIT_MASK << 1) == 0);

	static constexpr size_t const ADDRESS_BYTE_SIZE_LOG2 = 2;
	static constexpr size_t const LINKEDCYCLE_BYTE_SIZE_LOG2 = ADDRESS_BYTE_SIZE_LOG2 + 1;
	static constexpr size_t const ADDRESS_BYTE_SIZE = 1u << ADDRESS_BYTE_SIZE_LOG2; static_assert(ADDRESS_BYTE_SIZE == sizeof(size_t));
	static constexpr size_t const LINKEDCYCLE_BYTE_SIZE = 1u << LINKEDCYCLE_BYTE_SIZE_LOG2; static_assert(LINKEDCYCLE_BYTE_SIZE == sizeof(TXLib::LinkedCycle));

public:
	TXLib::LinkedCycle			m_links[INVALID_PRIORITY]; // Each element of the array is a link in a cyclically linked list consisting of threads sharing the same priority
	size_t									m_occupancy;	// Bit-field indicating whether the link of each priority is single (i.e. there is no thread of that priority)


private:

	void remove_from_occupancy(TXLib::LinkedCycle * link)
	{
		size_t index = link - m_links;
		TX_ASSERT(index < INVALID_PRIORITY);
		m_occupancy &= ~(HIGHEST_BIT_MASK >> index);
	}



public:
	PriorityList(void) noexcept : m_occupancy(0) {}
	~PriorityList(void) noexcept = default;
	PriorityList(PriorityList const &) noexcept = delete;
	PriorityList(PriorityList &&) noexcept = delete;
	void operator=(PriorityList const &) noexcept = delete;
	void operator=(PriorityList &&) noexcept = delete;


	size_t get_highest_priority(void) const
	{
		static_assert(PRIORITY_BLOCK == 1);
		return (m_occupancy == 0) ? INVALID_PRIORITY : __builtin_clz(m_occupancy);
		/* NOTE: The above return value should be equivalent to __builtin_clz(m_occupancy).
		 * However, g++ seems to expect __builtin_clz to take value between 0 and 31,
		 *  and optimizes the inequality (get_highest_priority() < 32) into (true)
		 *  This return value prevents such incorrect optimization */
	}

	void insert(TXLib::LinkedCycleUnsafe & link, size_t priority)
	{
		TX_ASSERT(priority < INVALID_PRIORITY);
		link.insert_single_as_prev_of(m_links[priority]);
		m_occupancy |= (HIGHEST_BIT_MASK >> priority);
	}

	TXLib::LinkedCycle * get_max_priority_link(size_t priority_bound)
	{
		size_t index = get_highest_priority();
		return (index < priority_bound) ? &m_links[index].next() : nullptr;
	}

	TXLib::LinkedCycle * pop_max_priority_link(size_t priority_bound)
	{
		size_t index = get_highest_priority();
		if (index >= priority_bound) {return nullptr;}
		TX_ASSERT(!m_links[index].is_single());
		if (m_links[index].is_single_or_double())
		{
			m_occupancy &= ~(HIGHEST_BIT_MASK >> index);
		}
		TXLib::LinkedCycle & link = m_links[index].next();
		link.remove_from_cycle();
		return &link;
	}

	TXLib::LinkedCycleUnsafe * pop_link(size_t priority)
	{
		TX_ASSERT(!m_links[priority].is_single());
		if (m_links[priority].is_single_or_double())
		{
			m_occupancy &= ~(HIGHEST_BIT_MASK >> priority);
		}
		TXLib::LinkedCycleUnsafe * link = &m_links[priority].next();
		link->remove_from_cycle();
		return link;
	}

	void remove_link(TXLib::LinkedCycleUnsafe & link)
	// The link must be linked to one of m_links[i]
	{
		TXLib::LinkedCycle * link_safe = reinterpret_cast<TXLib::LinkedCycle *>(&link);
		if (link_safe->is_single_or_double())
		{
			remove_from_occupancy(& link_safe->prev());
		}
		link.remove_from_cycle();
	}

};




} // namespace RTOS
