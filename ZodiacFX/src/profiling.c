/**
 * @file
 * main.c
 *
 * This file contains profiling functions
 *
 */

#include <asf.h>
#include <string.h>
#include "trace.h"
#include "lwip/def.h"
#include "openflow/openflow.h"
#include "timers.h"
#include "profiling.h"

/* Chip select. */
#define SPI_CHIP_SEL 0
/* Clock polarity. */
#define SPI_CLK_POLARITY 0
/* Clock phase. */
#define SPI_CLK_PHASE 1

static uint8_t testbuffer[16] = {0xAA, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // REMOVE THIS
uint8_t addressbuffer[ADDRESS_BUFFER_SIZE];
uint16_t addressbuffer_count = 8;

/*
*	Initialize the SPI interface as a SLAVE
*
*/
void spi_profiling_init(void)
{
	//NVIC_DisableIRQ(SPI_IRQn);
	//NVIC_ClearPendingIRQ(SPI_IRQn);
	//NVIC_SetPriority(SPI_IRQn, 0);
	//NVIC_EnableIRQ(SPI_IRQn);

	/* Configure an SPI peripheral. */
	spi_enable_clock(SPI_SLAVE_BASE);
	spi_disable(SPI_SLAVE_BASE);
	spi_reset(SPI_SLAVE_BASE);
	spi_set_slave_mode(SPI_SLAVE_BASE);
	spi_disable_mode_fault_detect(SPI_SLAVE_BASE);
	spi_set_peripheral_chip_select_value(SPI_SLAVE_BASE, SPI_CHIP_SEL);
	spi_set_clock_polarity(SPI_SLAVE_BASE, SPI_CHIP_SEL, SPI_CLK_POLARITY);
	spi_set_clock_phase(SPI_SLAVE_BASE, SPI_CHIP_SEL, SPI_CLK_PHASE);
	spi_set_bits_per_transfer(SPI_SLAVE_BASE, SPI_CHIP_SEL, SPI_CSR_BITS_8_BIT);
	//spi_enable_interrupt(SPI_SLAVE_BASE, SPI_IER_RDRF);
	spi_enable(SPI_SLAVE_BASE);
	ioport_set_pin_level(SPI_IRQ1, false);
	
	return;
}

/*
*	Send dummy test data
*
*/
void spi_write_test(void)
{
	// Write walking pattern to buffer
	for (uint16_t i = 0; i < ADDRESS_BUFFER_SIZE-8; i++)
	{
		addressbuffer[i+8] = i;
	}
	
	// Write starting & closing boundaries
	for (uint8_t i = 0; i < 8; i++)
	{
		addressbuffer[i] = 0xDD;
		addressbuffer[ADDRESS_BUFFER_SIZE-1-i] = 0xEE;
	}
	
	for (uint16_t i = 0; i < ADDRESS_BUFFER_SIZE; i++)
	{
		//while ((spi_read_status(SPI_MASTER_BASE) & SPI_SR_RDRF) == 0);
		spi_write(SPI_MASTER_BASE, addressbuffer[i], 0, 0);
	}
	
	return;
}

/*
*	Send address data
*
*/
void task_offload(void)
{
	if(addressbuffer_count >= ADDRESS_BUFFER_SIZE-8)
	{	
		// Write starting & closing boundaries
		for (uint8_t i = 0; i < 8; i++)
		{
			addressbuffer[i] = 0xDD;
			addressbuffer[ADDRESS_BUFFER_SIZE-1-i] = 0xEE;
		}
		
		for (uint16_t i = 0; i < ADDRESS_BUFFER_SIZE; i++)
		{
			//while ((spi_read_status(SPI_MASTER_BASE) & SPI_SR_RDRF) == 0);
			spi_write(SPI_MASTER_BASE, addressbuffer[i], 0, 0);
		}
		
		// Reset local SPI buffer
		spi_write(SPI_MASTER_BASE, 0xFF, 0, 0);
		
		addressbuffer_count = 8;
	}
	
	return;
}

//void spi_write_address(uint32_t addr)
//{
	//uint8_t boundary[2] = {0xDE, 0xAD};
	//// Send start boundary
	//for (uint8_t i = 0; i < 2; i++)
	//{
		//spi_write(SPI_MASTER_BASE, boundary[i], 0, 0);
	//}
	//
	//// Send address
	//uint8_t * addr_byte = (&addr);
	//addr_byte+=3;
	//spi_write(SPI_MASTER_BASE, *addr_byte--, 0, 0);
	//spi_write(SPI_MASTER_BASE, *addr_byte--, 0, 0);
	//spi_write(SPI_MASTER_BASE, *addr_byte--, 0, 0);
	//spi_write(SPI_MASTER_BASE, *addr_byte, 0, 0);
	//
	//boundary[0] = 0xBE;
	//boundary[1] = 0xEF;
	//// Send start boundary
	//for (uint8_t i = 0; i < 2; i++)
	//{
		//spi_write(SPI_MASTER_BASE, boundary[i], 0, 0);
	//}
	//
	//return;
//}
