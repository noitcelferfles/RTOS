/*
 * rtos_profiler.hpp
 *
 *  Created on: Feb 17, 2024
 *      Author: tian_
 */

#pragma once

#include "./Source/Driver/cortexm3_core.hpp"
#include "./External/MyLib/tx_assert.h"
#include "./External/CMSIS/Include/cmsis_compiler.h"
#include <stddef.h>
#include <stdint.h>


#ifdef RTOS_PROFILER_ENABLE // Guard against duplicated directives
	#error
#endif

#define RTOS_PROFILER_ENABLE

#ifdef RTOS_PROFILER_ENABLE

	#define RTOS_PROFILER_INIT(input) 				Profiler::initialize(input)
	#define RTOS_PROFILER_START(name) 				Profiler::start<Profiler::get_index(name)>()
	#define RTOS_PROFILER_STOP(input)					Profiler::stop<Profiler::get_index(input)>()

#else

	#define RTOS_PROFILER_INIT(input)
	#define RTOS_PROFILER_START(name)
	#define RTOS_PROFILER_STOP(input)

#endif



namespace RTOS
{


class Profiler
{
private:
	static constexpr char const * ProfileList[] =
	{
//			"sleep",
			"systick_update",
//			"msgqueue_push",
	};

	static constexpr bool identical_string(char const * string1, char const * string2)
	{
		size_t offset = 0;
		while (string1[offset] == string2[offset])
		{
			if (string1[offset] == '\0') {return true;}
			offset++;
		}
		return false;
	}

	static constexpr size_t NullIndex = ~0u;



public:

	typedef size_t (*GetTimeFunc)(void);


public:
	static GetTimeFunc				s_get_time_func;
	static size_t							s_time_start;
	static size_t							s_last_delay;
	static size_t							s_max_delay;
	static size_t							s_last_profile_index;


public:

	static bool is_initialized(void) {return s_get_time_func != nullptr;}

	static void initialize(GetTimeFunc func)
	{
		s_get_time_func = func;
		s_max_delay = 0;
		s_last_profile_index = NullIndex;
	}

	static constexpr size_t get_index(char const * name)
	{
		for (size_t i = 0; i < sizeof(ProfileList) / sizeof(char const *); i++)
		{
			if (identical_string(name, ProfileList[i]))
			{
				return i;
			}
		}
		return NullIndex;
	}

	static void start(size_t profile_index)
	{
		TX_ASSERT(is_initialized());
		TX_ASSERT(s_last_profile_index == NullIndex);

		s_last_profile_index = profile_index;
		s_time_start = CoreClock::get_cycle_count();
	}

	static void stop(void)
	{
		TX_ASSERT(is_initialized());
		TX_ASSERT(s_last_profile_index != NullIndex);

		s_last_delay = CoreClock::get_cycle_count() - s_time_start;
		if (s_last_delay > s_max_delay)
		{
			s_max_delay = s_last_delay;
		}
		if (s_last_delay > 500)
		{
			__NOP();
		}
		__NOP();
		s_last_profile_index = NullIndex;
	}

	template <size_t ProfileIndex>
	static void start(void)
	{
		if (ProfileIndex != NullIndex)
		{
			start(ProfileIndex);
		}
	}

	template <size_t ProfileIndex>
	static void stop(void)
	{
		if (ProfileIndex != NullIndex)
		{
			stop();
		}
	}

};



} // namespace RTOS
