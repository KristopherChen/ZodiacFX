/**
 * @file
 * main.c
 *
 * This file contains profiling functions
 *
 */

#ifndef STACKING_H_
#define STACKING_H_

#define SPI_Handler     SPI_Handler
#define SPI_IRQn        SPI_IRQn

#define SPI_SEND_CLEAR	0
#define SPI_SEND_STATS	1
#define SPI_SEND_PKT	2
#define SPI_RECEIVE		3
#define SPI_RCV_PREAMBLE	4
#define SPI_STATS_PREAMBLE		0xABAB
#define SPI_PACKET_PREAMBLE		0xBCBC
#define SPI_SEND_WAIT		0
#define SPI_HEADER_SIZE	13

#define ADDRESS_BUFFER_SIZE	72+16	// size + boundary padding (NOTE: needs to be a multiple of 4 to fit addresses)

void spi_profiling_init(void);
void spi_write_test(void);
void task_offload(void);

#endif /* STACKING_H_ */