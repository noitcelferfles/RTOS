/*
 * stm32f207_peripheral.hpp
 *
 *  Created on: Feb 17, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "./External/CMSIS/Device/ST/STM32F2xx/Include/stm32f207xx.h"
#include "./External/MyLib/tx_assert.h"



template <uint32_t LOW_BIT_POSITION, uint32_t NUMBER_OF_BITS>
inline void set_register_bits(uint32_t volatile & reg, uint32_t value)
{
	uint32_t temp = reg;
	constexpr uint32_t MASK = (1u << NUMBER_OF_BITS) - 1;
	temp &= ~(MASK << LOW_BIT_POSITION);
	temp |= (value << LOW_BIT_POSITION);
	reg = temp;
}

inline void set_register_bits(uint32_t volatile & reg, size_t low_bit_position, size_t number_of_bits, uint32_t value)
{
	uint32_t temp = reg;
	uint32_t mask = (1u << number_of_bits) - 1;
	temp &= ~(mask << low_bit_position);
	temp |= (value << low_bit_position);
	reg = temp;
}

inline void set_register_bit_on(uint32_t volatile & reg, size_t bit_position)
{
	reg |= 1u << bit_position;
}

inline void set_register_bit_off(uint32_t volatile & reg, size_t bit_position)
{
	reg &= ~(1u << bit_position);
}




class ClockControl
{
private:

	static size_t get_gpio_index(GPIO_TypeDef * gpio)
	{
		return ((size_t)gpio - (size_t)GPIOA_BASE) / ((size_t)GPIOB_BASE - (size_t)GPIOA_BASE);
	}

public:

	static void set_enabled(uint32_t volatile & clock_enable_reg, size_t clock_enable_bit_position)
	{
		set_register_bit_on(clock_enable_reg, clock_enable_bit_position);
		*(&clock_enable_reg); // Synchronization read
		TX_ASSERT(clock_enable_reg & (1u << clock_enable_bit_position));
	}

	static void set_disabled(uint32_t volatile & clock_enable_reg, size_t clock_enable_bit_position)
	{
		set_register_bit_off(clock_enable_reg, clock_enable_bit_position);
		*(&clock_enable_reg); // Synchronization read
		TX_ASSERT(!(clock_enable_reg & (1u << clock_enable_bit_position)));
	}

	static void assert_enabled(uint32_t volatile & clock_enable_reg, size_t clock_enable_bit_position)
	{
		TX_ASSERT(clock_enable_reg & (1u << clock_enable_bit_position));
	}

	static void assert_disabled(uint32_t volatile & clock_enable_reg, size_t clock_enable_bit_position)
	{
		TX_ASSERT(!(clock_enable_reg & (1u << clock_enable_bit_position)));
	}


	static void enable_gpio(GPIO_TypeDef * gpio)
	{
		set_enabled(RCC->AHB1ENR, get_gpio_index(gpio));
	}

	static void disable_gpio(GPIO_TypeDef * gpio)
	{
		set_disabled(RCC->AHB1ENR, get_gpio_index(gpio));
	}

	static void assert_gpio_enabled(GPIO_TypeDef * gpio)
	{
		assert_enabled(RCC->AHB1ENR, get_gpio_index(gpio));
	}

	static void assert_gpio_disabled(GPIO_TypeDef * gpio)
	{
		assert_disabled(RCC->AHB1ENR, get_gpio_index(gpio));
	}



};







class UART : USART_TypeDef
{
public:

	UART(void) noexcept = default;
	~UART(void) noexcept = default;
	UART(UART const &) = delete;
	UART(UART &&) = delete;
	void operator=(UART const &) = delete;
	void operator=(UART &&) = delete;

	template <uint32_t CoreFrequency, uint32_t BaudRate>
	void initialize(uint32_t volatile & clock_enable_reg, size_t clock_enable_bit_position, GPIO_TypeDef * transmit_gpio_port, size_t transmit_pin_pos, GPIO_TypeDef * receive_gpio_port, size_t receive_pin_pos)
	{
		// Enable clock

//		RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
		ClockControl::assert_gpio_enabled(transmit_gpio_port);
		ClockControl::assert_gpio_enabled(receive_gpio_port);
		ClockControl::assert_enabled(clock_enable_reg, clock_enable_bit_position);
//		ClockControl::enable_gpio(transmit_gpio_port);
//		ClockControl::enable_gpio(receive_gpio_port);
//		RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

		// Initialize GPIO

		configure_gpio_pin(transmit_gpio_port, transmit_pin_pos);
		configure_gpio_pin(receive_gpio_port, receive_pin_pos);

		// Initialize UART

		CR1 &= ~USART_CR1_UE_Msk;
		CR2 &= ~(USART_CR2_LINEN | USART_CR2_CLKEN);
		CR3 &= ~(USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN);

		constexpr uint32_t const DownSample = CoreFrequency / BaudRate;
		TX_ASSERT(DownSample < (1u << 16));
		BRR = DownSample;

		CR1 |= USART_CR1_UE_Msk;
	}


	/*
	 *	1. Enable the USART by writing the UE bit in USART_CR1 register to 1.
	 *	2. Program the M bit in USART_CR1 to define the word length.
	 *	3. Program the number of stop bits in USART_CR2.
	 *	4. Select DMA enable (DMAT) in USART_CR3 if Multi buffer Communication is to take
	 *	place. Configure the DMA register as explained in multibuffer communication.
	 *	5. Select the desired baud rate using the USART_BRR register.
	 *	6. Set the TE bit in USART_CR1 to send an idle frame as first transmission.
	 *	7. Write the data to send in the USART_DR register (this clears the TXE bit). Repeat this
	 *	for each data to be transmitted in case of single buffer.
	 *	8. After writing the last data into the USART_DR register, wait until TC=1. This indicates
	 *	that the transmission of the last frame is complete. This is required for instance when
	 *	the USART is disabled or enters the Halt mode to avoid corrupting the last
	 *	transmission.
	 */
	void transmit_char(char content)
	{
//		TX_ASSERT(content != 0);

		CR1 |= USART_CR1_TE;
		SR; // clear status register
		DR = (uint32_t)content;

		while (!(SR & USART_SR_TC)); // Wait until transmission
		CR1 &= ~USART_CR1_TE;
	}

	void transmit_string(char const * string)
	{
		CR1 |= USART_CR1_TE;
		SR;  // Read first so that the next write to the data register sets the ready bit

		while (string[0] != 0)
		{
			size_t time_start = DWT->CYCCNT;

			DR = (uint32_t) string[0];
			string ++;
			while (!(SR & USART_SR_TC));

			size_t volatile delay = DWT->CYCCNT - time_start; delay;
			__NOP();
		}

		CR1 &= ~USART_CR1_TE;
	}


private:

	void configure_gpio_pin(GPIO_TypeDef * port, size_t pin_pos)
	{
		set_register_bits(port->OSPEEDR, 2 * pin_pos, 2, 0b11u); // set very high IO speed
		set_register_bit_off(port->OTYPER, pin_pos); // set push-pull mode
		set_register_bits(port->PUPDR, 2 * pin_pos, 2, 0b00u); // no pull-up or pull-down
		{
			size_t index = (pin_pos >= 8) ? 1 : 0;
			size_t offset = (pin_pos >= 8) ? pin_pos - 8 : pin_pos;
			set_register_bits(port->AFR[index], 4 * offset, 4, 0b111u); // configure alternate function as UART
		}
		set_register_bits(port->MODER, 2 * pin_pos, 2, 0b10u); // set alternate function mode
	}

};


