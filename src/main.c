// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Gabriel Marcano, 2023

/** This is an example main executable program */

#include <example.h>

#include <uart.h>
#include <syscalls.h>

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

#include <sys/time.h>

#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include <spi.h>
#include <adc.h>
#include <am1815.h>
#include <bmp280.h>
#include <flash.h>
#include <pdm.h>

struct uart uart;
struct spi spi;
struct adc adc;
struct am1815 rtc;
struct bmp280 temp;
struct flash flash;
struct pdm pdm;

__attribute__((constructor))
static void redboard_init(void)
{
	// Prepare MCU by init-ing clock, cache, and power level operation
	am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);
	am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
	am_hal_cachectrl_enable();
	am_bsp_low_power_init();
	am_hal_sysctrl_fpu_enable();
	am_hal_sysctrl_fpu_stacking_enable(true);

	uart_init(&uart, UART_INST0);
	initialize_sys_uart(&uart);

	// After init is done, enable interrupts
	am_hal_interrupt_master_enable();
}

__attribute__((destructor))
static void redboard_shutdown(void)
{
	// Any destructors/code that should run when main returns should go here
}

int main(void)
{
	// Print the banner
	am_util_stdio_printf("Hello World!!!\r\n");
	printf("Hello World!!!\r\n");

	// Initialize all the necessary structs
	adc_init(&adc);
	spi_init(&spi, 0, 2000000u);
	spi_enable(&spi);
	am1815_init(&rtc, &spi);
	bmp280_init(&temp, &spi);
	flash_init(&flash, &spi);
	pdm_init(&pdm);

	// print the data before write
	int size1 = 18 + 8;
	uint8_t buffer1[size1];		// this is 4x bigger than necessary
	flash_read_data(&flash, 0x04, buffer1, size1);
	char* buf1 = buffer1;
	am_util_stdio_printf("before write:\r\n");
	for (int i = 0; i < size1; i++) {
		am_util_stdio_printf("%02X ", (int) buf1[i]);
		am_util_delay_ms(10);
	}
	am_util_stdio_printf("\r\n");

	// print the flash ID to make sure the CS is connected correctly
	spi_chip_select(&spi, SPI_CS_0);
	am_util_stdio_printf("flash ID: %02X\r\n", flash_read_id(&flash));

	// print the RTC ID to make sure the CS is connected correctly
	spi_chip_select(&spi, SPI_CS_3);
	am_util_stdio_printf("RTC ID: %02X\r\n", am1815_read_register(&rtc, 0x28));

	// Write banner to Flash
	spi_chip_select(&spi, SPI_CS_0);
	const char toWrite[] = "We're beginning\r\n";
	am_util_stdio_printf("Size: %u\r\n", sizeof(toWrite));
	flash_page_program(&flash, 0, (uint8_t*)&toWrite, sizeof(toWrite));
	flash_wait_busy(&flash);
	
	// Write the RTC time to the flash chip
	spi_chip_select(&spi, SPI_CS_3);
	struct timeval time = am1815_read_time(&rtc);
	uint64_t sec = (uint64_t)time.tv_sec;
	am_util_stdio_printf("Time: %lld\r\n", sec);
	initialize_time(&rtc);
	struct timeval currTime;
	gettimeofday(&currTime, NULL);
	am_util_stdio_printf("Time of Day (sec): %lld\r\n", currTime.tv_sec);
	am_util_stdio_printf("Time of Day (ms): %lld\r\n", currTime.tv_usec);
	uint8_t* tmp = (uint8_t*)&sec;
	spi_chip_select(&spi, SPI_CS_0);
	flash_page_program(&flash, sizeof(toWrite) + 0, tmp, 8);
	flash_wait_busy(&flash);

	// Read the banner from flash
	int size = sizeof(toWrite) + 8;
	uint8_t buffer[size];
	flash_read_data(&flash, 0, buffer, size);
	const char* buf = buffer;
	for (int i = 0; i < size - 8; i++) {
		am_util_stdio_printf("%c", buf[i]);
		am_util_delay_ms(10);
	}
	for (int i = size - 8; i < size; i++) {
		am_util_stdio_printf("%02X", (int)buf[i]);
		am_util_delay_ms(10);
	}
	am_util_stdio_printf("\r\n");

	// erase data
	flash_sector_erase(&flash, 0);
	flash_wait_busy(&flash);

	// print flash data after write
	flash_read_data(&flash, 0, buffer, size);
	buf = buffer;
	am_util_stdio_printf("after erase:\r\n");
	for (int i = 0; i < size; i++) {
		am_util_stdio_printf("%02X ", (int) buf[i]);
		am_util_delay_ms(10);
	}
	am_util_stdio_printf("\r\n");
	am_util_stdio_printf("done\r\n");
	return 0;
}
