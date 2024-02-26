/*
 * rtos.hpp
 *
 *  Created on: Jan 29, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "rtos_thread.hpp"
#include "rtos_mutex.hpp"
#include "rtos_message_queue.hpp"


namespace RTOS
{

typedef size_t (*FunctionPtr)(size_t arg);

// OS operations

void initialize(FunctionPtr entry, size_t stack_size, void * mem_ptr, size_t mem_size); /* Start the RTOS; this function does not return
The memory interval [mem_ptr, mem_ptr + mem_size) will be used for RTOS heap allocation.
After RTOS is initialized, it will execute the procedure @entry with highest priority on a stack of size @stack_size.
Hardware and thread initialization can be placed in @entry. */


// Memory operations

void * alloc(size_t size); // Allocate a memory block
void free(void * mem_ptr); // Free the memory block
size_t used_memory(void); // Get total memory used by OS memory allocator
size_t unused_memory(void); // Get total memory remaining





}
