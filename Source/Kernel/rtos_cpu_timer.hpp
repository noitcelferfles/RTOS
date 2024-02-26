/*
 * rtos_cpu_timer.hpp
 *
 *  Created on: Feb 7, 2024
 *      Author: tian_
 */

#pragma once

#include "./Source/PublicApi/rtos_time.hpp"
#include <stddef.h>
#include "./External/MyLib/tx_assert.h"


namespace RTOS
{

class CPUTimer
{
public:
	size_t			m_start_time;
	size_t			m_max_time;
	size_t			m_duration;

private:
	void start_debug_timer(void);

public:
	CPUTimer(void) noexcept {initialize();}
	~CPUTimer(void) noexcept = default;
	CPUTimer(CPUTimer const &) = delete;
	CPUTimer(CPUTimer &&) = delete;
	void operator=(CPUTimer const &) = delete;
	void operator=(CPUTimer &&) = delete;

	void initialize(void);

	size_t get_duration(void) const {return m_duration;}

	void start(void);
	size_t record(void);
	void stop(void);

};



//class Profiler2
//{
//public:
//	typedef size_t (*GetTimeFunc)(void);
//
//private:
//	size_t						m_historical_max;
//	GetTimeFunc				m_get_time_func;
//
//
//public:
//
//
//	class Timer
//	{
//		friend Profiler2;
//
//	private:
//		Profiler2 *	m_profiler;
//		size_t			m_start;
//
//	private:
//		Timer(Profiler2 * profiler) noexcept : m_profiler(profiler)
//		{
//			m_start = m_profiler->m_get_time_func();
//		}
//
//	public:
//		~Timer(void) noexcept
//		{
//			size_t duration = m_profiler->m_get_time_func() - m_start;
//			if (duration > m_profiler->m_historical_max)
//			{
//				m_profiler->m_historical_max = duration;
//			}
//		}
//
//		Timer(void) = delete;
//		Timer(Timer const &) = delete;
//		Timer(Timer &&) = delete;
//		void operator=(Timer const &) = delete;
//		void operator=(Timer &&) = delete;
//	};
//
//
//
//
//public:
//	Profiler2(GetTimeFunc func) noexcept : m_historical_max(0), m_get_time_func(func) {}
//	~Profiler2(void) noexcept = default;
//	Profiler2(Profiler2 const &) = delete;
//	Profiler2(Profiler2 &&) = delete;
//	void operator=(Profiler2 const &) = delete;
//	void operator=(Profiler2 &&) = delete;
//
//	size_t get_max_delay(void) const {return m_historical_max;}
//};




}
