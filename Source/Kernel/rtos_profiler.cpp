/*
 * rtos_profiler.cpp
 *
 *  Created on: Feb 17, 2024
 *      Author: tian_
 */


#include "rtos_profiler.hpp"




namespace RTOS
{


Profiler::GetTimeFunc Profiler::s_get_time_func = nullptr;
size_t Profiler::s_time_start;
size_t Profiler::s_last_delay;
size_t Profiler::s_max_delay;
size_t Profiler::s_last_profile_index;


}
