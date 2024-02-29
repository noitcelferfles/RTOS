/*
 * rtos_time.hpp
 *
 *  Created on: Feb 7, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>

namespace RTOS
{

class TimeType
{
private:
	static constexpr size_t const NegativeStart = 0x80000000;
	static_assert(NegativeStart << 1u == 0 && NegativeStart > 0);

public:
	size_t m_time;

public:
	static size_t get_max_positive(void)
	/* Return a number @return with the property that for every instance @time of TimeType
	 *   (@time + @return > @time) and (@time - @return < @time)
	 */
	{return NegativeStart - 1;}


public:
	TimeType(void) noexcept {};
	TimeType(TimeType const &) noexcept = default;
	TimeType(TimeType && b) noexcept = default;
	constexpr TimeType(size_t b) noexcept : m_time(b) {}
	~TimeType(void) noexcept = default;
	void operator=(TimeType const & b) {m_time = b.m_time;};
	void operator=(TimeType && b) {m_time = b.m_time;};

	TimeType operator+(size_t b) const {return TimeType(m_time + b);}
	TimeType operator-(size_t b) const {return TimeType(m_time - b);}
	void operator+=(size_t b) {m_time += b;}
	void operator-=(size_t b) {m_time -= b;}
	size_t operator-(TimeType const & b) const {return m_time - b.m_time;}
	bool operator>(TimeType const & b) const {return b.m_time - m_time >= NegativeStart;}
	bool operator>=(TimeType const & b) const {return m_time - b.m_time < NegativeStart;}
	bool operator<(TimeType const & b) const {return m_time - b.m_time >= NegativeStart;}
	bool operator<=(TimeType const & b) const {return b.m_time - m_time < NegativeStart;}
	bool operator==(TimeType const & b) const {return m_time == b.m_time;}
	bool operator!=(TimeType const & b) const {return m_time != b.m_time;}
};

TimeType system_time(void);




} // namespace RTOS
