/*
 * rtos_thread.hpp
 *
 *  Created on: Jan 31, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include "rtos_time.hpp"


namespace RTOS
{

typedef size_t (*FunctionPtr)(size_t arg);

class Thread;



Thread & create_thread(FunctionPtr entry, size_t entry_argument, size_t priority, size_t stack_size); // Does not guarantee switching to a higher-priority created thread until next systick
Thread & create_thread(FunctionPtr entry, size_t priority, size_t stack_size);
void relinquish(void); // Only relinquish to higher or equal priority ready threads
void sleep(size_t sleep_duration);

void pause_thread(Thread & thread);
void unpause_thread(Thread & thread);
void kill_thread(Thread & thread);


size_t get_total_thread_cycle(void);


}
