/**
 * @file
 * timers.c
 *
 * This file contains the timer functions
 *
 */

/*
 * This file is part of the Zodiac FX firmware.
 * Copyright (c) 2016 Northbound Networks.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Paul Zanna <paul@northboundnetworks.com>
 *
 */

#include <asf.h>
#include "board.h"
#include "tc.h"
#include "timers.h"
#include "lwip/init.h"
#include "lwip/sys.h"
#include "profiling.h"

/* Clock tick count. */
static volatile uint32_t gs_ul_clk_tick;

#include "pmc.h"
#include "sysclk.h"

/**
 *	TC0 Interrupt handler.
 *
 */
void TC0_Handler(void)
{
	//printf("-");
	/* Remove warnings. */
	volatile uint32_t ul_dummy;

	/* Clear status bit to acknowledge interrupt. */
	ul_dummy = TC0->TC_CHANNEL[0].TC_SR;

	/* Increase tick. */
	gs_ul_clk_tick++;
}

/**
 *	TC1 Interrupt handler.
 *
 */
void TC1_Handler(void)
{
	//printf(".");
	/* Remove warnings. */
	volatile uint32_t ul_dummy;

	/* Clear status bit to acknowledge interrupt. */
	ul_dummy = tc_get_status(TC0, 1);
	UNUSED(ul_dummy);

	///* Retrieve return address */
	//volatile uint32_t sp = 0;
	//uint32_t *pc_ptr = (uint32_t*)((uint32_t)&sp + 8 + 40);
	//uint32_t return_address = (uint32_t)*pc_ptr;
	//
	///* Send data */
	//spi_write_address(return_address);
}

/**
 * Initialize the timer counter (TC0).
 *
 */
void sys_init_timing(void)
{
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();
	
	uint32_t ul_div2;
	uint32_t ul_tcclks2;

	/* Clear tick value. */
	gs_ul_clk_tick = 0;

	/* Configure PMC. */
	pmc_enable_periph_clk(ID_TC0);
	pmc_enable_periph_clk(ID_TC1);

	/* Configure TC for a 1kHz frequency and therefore a 1ms rate */
	tc_find_mck_divisor(1000, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC0, 0, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC0, 0, (ul_sysclk / ul_div) / 1000);
	/* Repeat for channel 1; 10kHz frequency, 100us rate */
	tc_find_mck_divisor(10000, ul_sysclk, &ul_div2, &ul_tcclks2, ul_sysclk);
	tc_init(TC0, 1, ul_tcclks2 | TC_CMR_CPCTRG);
	tc_write_rc(TC0, 1, (ul_sysclk / ul_div2) / 10000);

	/* Configure and enable interrupt on RC compare. */
	NVIC_EnableIRQ((IRQn_Type)ID_TC0);
	tc_enable_interrupt(TC0, 0, TC_IER_CPCS);
	/* Repeat for Channel 1. */
	NVIC_EnableIRQ((IRQn_Type)ID_TC1);
	tc_enable_interrupt(TC0, 1, TC_IER_CPCS);

	/* Start timers. */
	tc_start(TC0, 0);
	tc_start(TC0, 1);
}

/**
 * Return the number of timer ticks (ms).
 *
 */
uint32_t sys_get_ms(void)
{
	return gs_ul_clk_tick;
}

#if ((LWIP_VERSION) != ((1U << 24) | (3U << 16) | (2U << 8) | (LWIP_VERSION_RC)))
u32_t sys_now(void)
{
	return (sys_get_ms());
}
#endif
