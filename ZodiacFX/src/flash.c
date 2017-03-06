/**
 * @file
 * flash.c
 *
 * This file contains the function the Flashing functions
 *
 */

/*
 * This file is part of the Zodiac FX firmware.
 * Copyright (c) 2016 Google Inc.
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
 * Authors: Paul Zanna <paul@northboundnetworks.com>
 *		  & Kristopher Chen <Kristopher@northboundnetworks.com>
 *
 */

#include <asf.h>
#include <inttypes.h>
#include <string.h>
#include "flash.h"
#include "config_zodiac.h"
#include "openflow/openflow.h"
#include "trace.h"
#include "command.h"

// Global variables
extern uint8_t shared_buffer[SHARED_BUFFER_LEN];
struct verification_data verify;

// Static variables
static uint32_t page_addr;
//static uint32_t ul_rc;

static	uint32_t flash_page_addr;
static	uint32_t ul_rc;
static	uint32_t ul_idx;
static	uint32_t ul_page_buffer[IFLASH_PAGE_SIZE / sizeof(uint32_t)];


/*
*	Get the unique serial number from the CPU
*
*/
void get_serial(uint32_t *uid_buf)
{
	uint32_t uid_ok = flash_read_unique_id(uid_buf, 4);
	return;
}

/*
*	Firmware update function
*
*/
int firmware_update_init(void)
{	
	flash_page_addr = NEW_FW_BASE;
	
	/* Initialize flash: 6 wait states for flash writing. */
	ul_rc = flash_init(FLASH_ACCESS_MODE_128, 6);
	if (ul_rc != FLASH_RC_OK) {
		//printf("-F- Initialization error %lu\n\r", (unsigned long)ul_rc);
		return 0;
	}
	
	// Unlock 8k lock regions (these should be unlocked by default)
	uint32_t unlock_address = flash_page_addr;
	while(unlock_address < IFLASH_ADDR + IFLASH_SIZE - (IFLASH_LOCK_REGION_SIZE - 1))
	{
		//printf("-I- Unlocking region start at: 0x%08x\r\n", unlock_address);
		ul_rc = flash_unlock(unlock_address,
		unlock_address + (4*IFLASH_PAGE_SIZE) - 1, 0, 0);
		if (ul_rc != FLASH_RC_OK)
		{
			//printf("-F- Unlock error %lu\n\r", (unsigned long)ul_rc);
			return 0;
		}
		
		unlock_address += IFLASH_LOCK_REGION_SIZE;
	}

	// Erase 192k
	uint32_t erase_address = flash_page_addr;
	while(erase_address < IFLASH_ADDR + IFLASH_SIZE - (ERASE_SECTOR_SIZE - 1))
	{
		//printf("-I- Erasing sector with address: 0x%08x\r\n", erase_address);
		ul_rc = flash_erase_sector(erase_address);
		if (ul_rc != FLASH_RC_OK)
		{
			//printf("-F- Flash programming error %lu\n\r", (unsigned long)ul_rc);
			return 0;
		}
		
		erase_address += ERASE_SECTOR_SIZE;
	}
	
	return 1;
}

/*
*	Write a page to flash memory
*
*/
int flash_write_page(uint8_t *flash_page)
{
	TRACE("flash.c: writing to 0x%08x", flash_page_addr);
	if(flash_page_addr <= IFLASH_ADDR + IFLASH_SIZE - IFLASH_PAGE_SIZE)
	{
		ul_rc = flash_write(flash_page_addr, flash_page,
		IFLASH_PAGE_SIZE, 0);
	}
	else
	{
		// Out of flash range
		return 0;
	}

	if (ul_rc != FLASH_RC_OK)
	{
		return 0;
	}	
	
	flash_page_addr += IFLASH_PAGE_SIZE;
	
	return 1;
}

/*
*	Handle firmware update through CLI
*
*/
void cli_update(void)
{
	firmware_update_init();
	if(!xmodem_xfer())	// Receive new firmware image via XModem
	{
		printf("Error: failed to write firmware to memory\r\n");
	}
	if(verification_check() == SUCCESS)
	{
		printf("Firmware upload complete - Restarting the Zodiac FX.\r\n");
		for(int x = 0;x<100000;x++);	// Let the above message get send to the terminal before detaching
		udc_detach();	// Detach the USB device before restart
		rstc_start_software_reset(RSTC);	// Software reset
	}
	else
	{
		printf("\r\n");
		printf("Firmware verification check failed\r\n");
		printf("\r\n");
	}

	return;
}

/*
*	Verify firmware data
*
*/
int verification_check(void)
{
	char* fw_end_pmem	= (char*)FLASH_BUFFER_END;	// Buffer pointer to store the last address
	char* fw_step_pmem  = (char*)FLASH_BUFFER;		// Buffer pointer to the starting address
	uint32_t crc_sum	= 0;						// Store CRC sum
	uint8_t	 pad_error	= 0;						// Set when padding is not found
	
	/* Add all bytes of the uploaded firmware */
	// Decrement the pointer until the previous address has data in it (not 0xFF)
	while(*(fw_end_pmem-1) == '\xFF' && fw_end_pmem > FLASH_BUFFER)
	{
		fw_end_pmem--;
	}

	for(int sig=1; sig<=4; sig++)
	{
		if(*(fw_end_pmem-sig) != NULL)
		{
			TRACE("signature padding %d not found - last address: %08x", sig, fw_end_pmem);
			pad_error = 1;
		}
		else
		{
			TRACE("signature padding %d found", sig);
		}
	}
	
	// Start summing all bytes
	if(pad_error)
	{
		// Calculate CRC for debug
		while(fw_step_pmem < fw_end_pmem)
		{
			crc_sum += *fw_step_pmem;
			fw_step_pmem++;
		}
	}
	else
	{
		// Exclude CRC & padding from calculation
		while(fw_step_pmem < (fw_end_pmem-8))
		{
			crc_sum += *fw_step_pmem;
			fw_step_pmem++;
		}
	}
	
	TRACE("fw_step_pmem %08x; fw_end_pmem %08x;", fw_step_pmem, fw_end_pmem);
	
	// Update structure entry
	TRACE("CRC sum:   %04x", crc_sum);
	verify.calculated = crc_sum;
	
	/* Compare with last 4 bytes of firmware */
	// Get last 4 bytes of firmware	(4-byte CRC, 4-byte padding)
	verify.found = *(uint32_t*)(fw_end_pmem - 8);
	
	TRACE("CRC found: %04x", verify.found);
	
	// Compare calculated and found CRC
	if(verify.found == verify.calculated)
	{
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}

/*
*	XModem transfer
*
*/
int xmodem_xfer(void)
{
	char ch;
	int timeout_clock = 0;
	int buff_ctr = 1;
	int byte_ctr = 1;
	int block_ctr = 0;
	uint8_t xmodem_crc = 0;
	
	// Prepare shared_buffer for storing each page of data
	memset(&shared_buffer, 0xFF, IFLASH_PAGE_SIZE);
	
	while(1)
	{
		while(udi_cdc_is_rx_ready())
		{
			ch = udi_cdc_getc();
			timeout_clock = 0;	// reset timeout clock
			
			// Check for <EOT>
			if (byte_ctr == 1 && ch == X_EOT)	// Note: byte_ctr is cleared to 0 and incremented in the previous loop
			{
				printf("%c", X_ACK);	// Send final <ACK>
				xmodem_clear_padding(&shared_buffer);	// strip the 0x1A fill bytes from the end of the last block
				
				// Send remaining data in buffer
				if(!flash_write_page(&shared_buffer))
				{
					return 0;
				}
				
				return 1;
			}
			else if(block_ctr == 4)
			{
				// Write the previous page of data
				if(!flash_write_page(&shared_buffer))
				{
					return 0;
				}
				
				// Reset block counter
				block_ctr = 0;
				
				// Reset buffer counter
				buff_ctr = 1;
				
				// Clear buffer to 0xFF for next page of data
				memset(&shared_buffer, 0xFF, IFLASH_PAGE_SIZE);
			}
			
			// Check for end of block
			if (byte_ctr == 132)
			{
				if (xmodem_crc == ch)		// Check CRC
				{
					printf("%c", X_ACK);	// If the CRC is OK then send a <ACK>
					block_ctr++;			// Increment block count
					byte_ctr = 0;			// Start a new 128-byte block
				}
				else
				{
					printf("%c", X_NAK);	// If the CRC is incorrect then send a <NAK>
					byte_ctr = 0;			// Start a new 128-byte block
					buff_ctr -= 128;		// Overwrite previous data
				}
				
				xmodem_crc = 0;				// Reset CRC
			}

			// Don't store the first 3 bytes <SOH>, <###>, <255-###>
			if (byte_ctr > 3)
			{
				shared_buffer[buff_ctr-1] = ch;	// Store received data
				buff_ctr++;
				xmodem_crc += ch;
			}
			
			byte_ctr++;
		}
		timeout_clock++;
		if (timeout_clock > 1000000)	// Timeout, send <NAK>
		{
			printf("%c", X_NAK);
			timeout_clock = 0;
		}
	}
}

/*
*	Remove XMODEM 0x1A padding at end of data
*
*/
xmodem_clear_padding(uint8_t *buff)
{
	int len = IFLASH_PAGE_SIZE;
	
	// Overwrite the padding element in the buffer (zero-indexed)
	while(len > 0)	// Move from end of buffer to beginning
	{
		if(buff[len-1] != 0xFF && buff[len-1] != 0x1A)
		{
			return;
		}
		else if(buff[len-1] == 0x1A)
		{
			// Latch onto 0x1A
			while(len > 0 && buff[len-1] == 0x1A)
			{
				// Write erase value
				buff[len-1] = 0xFF;
				len--;
			}
			
			return;
		}
		
		len--;
	}
	
	return;	// Padding characters removed
}

