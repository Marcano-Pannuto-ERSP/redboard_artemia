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

	// Flash is CS pin 11 (SPI_CS_0)
	spi_chip_select(&spi, SPI_CS_0);

	// Write banner to Flash
	const char toWrite[] = "We're beginning\r\n";
	am_util_stdio_printf("Size: %u\r\n", sizeof(toWrite));
	flash_page_program(&flash, 0, (uint8_t*)&toWrite, sizeof(toWrite));
	
	// Write the RTC time to the flash chip
	spi_chip_select(&spi, SPI_CS_3);
	struct timeval time = am1815_read_time(&rtc);
	uint64_t sec = (uint64_t)time.tv_sec;
	uint8_t* tmp = (uint8_t*)&sec;
	spi_chip_select(&spi, SPI_CS_0);
	flash_page_program(&flash, sizeof(toWrite) + 0, tmp, 8);

	// Read the banner from flash
	int size = sizeof(toWrite);
	uint8_t buffer[size];
	flash_read_data(&flash, 0x11, buffer, size);
	const char* buf = buffer;
	for (int i = 0; i < size; i++) {
		am_util_stdio_printf("%02X", (int)buf[i]);
		am_util_delay_ms(10);
	}
	am_util_stdio_printf("\r\n");
	return 0;
}
