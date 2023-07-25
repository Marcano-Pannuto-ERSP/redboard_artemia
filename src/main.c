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

# define PRINT_LENGTH 80

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
	syscalls_uart_init(&uart);

	// After init is done, enable interrupts
	am_hal_interrupt_master_enable();
}

__attribute__((destructor))
static void redboard_shutdown(void)
{
	// Any destructors/code that should run when main returns should go here
}

// Write the RTC time to the flash chip
void flash_write_time(struct flash *flash, struct am1815 *am1815, struct spi *spi, uint32_t addr){
	spi_chip_select(spi, SPI_CS_3);
	struct timeval time = am1815_read_time(am1815);
	uint64_t sec = (uint64_t)time.tv_sec;
	am_util_stdio_printf("Time: %lld\r\n", sec);
	uint8_t* tmp = (uint8_t*)&sec;
	spi_chip_select(spi, SPI_CS_0);
	flash_page_program(flash, addr, tmp, 8);
	flash_wait_busy(flash);
}

// Print the flash data as strings
void flash_print_string(struct flash *flash, struct spi *spi, uint32_t addr, size_t size){
	spi_chip_select(spi, SPI_CS_0);
	uint8_t buffer[size];
	flash_read_data(flash, addr, buffer, size);
	char* buf = buffer;
	for (int i = 0; i < size-addr; i++) {
		am_util_stdio_printf("%c", buf[i]);
	}
	return;
}

// Print the flash data as hex
void flash_print_int(struct flash *flash, struct spi *spi, uint32_t addr, size_t size){
	spi_chip_select(spi, SPI_CS_0);
	uint8_t buffer[size];
	flash_read_data(flash, addr, buffer, size);
	char* buf = buffer;
	for (int i = 0; i < size-addr; i++) {
		am_util_stdio_printf("%02X ", (int)buf[i]);
	}
	am_util_stdio_printf("\r\n");
	return;
}

int main(void)
{
	// Initialize all the necessary structs
	adc_init(&adc);
	spi_init(&spi, 0, 2000000u);
	spi_enable(&spi);
	am1815_init(&rtc, &spi);
	bmp280_init(&temp, &spi);
	flash_init(&flash, &spi);
	pdm_init(&pdm);

	// Initialize text to be written
	const char toWrite[] = "We're beginning\r\n";

	// print the data before write
	int size = sizeof(toWrite);
	flash_print_int(&flash, &spi, 0, PRINT_LENGTH);

	// print the flash ID to make sure the CS is connected correctly
	spi_chip_select(&spi, SPI_CS_0);
	am_util_stdio_printf("flash ID: %02X\r\n", flash_read_id(&flash));

	// print the RTC ID to make sure the CS is connected correctly
	spi_chip_select(&spi, SPI_CS_3);
	am_util_stdio_printf("RTC ID: %02X\r\n", am1815_read_register(&rtc, 0x28));

	// Write banner to Flash
	spi_chip_select(&spi, SPI_CS_0);
	flash_page_program(&flash, 0, (uint8_t*)&toWrite, sizeof(toWrite));
	flash_wait_busy(&flash);
	
	// Write the RTC time to the flash chip
	flash_write_time(&flash, &rtc, &spi, sizeof(toWrite));
	size += 8;

	// Read current temperature from BMP280 sensor and write to flash
	spi_chip_select(&spi, SPI_CS_1);
    am_util_stdio_printf("BMP280 ID: %02X\r\n", bmp280_read_id(&temp));
	uint32_t raw_temp = bmp280_get_adc_temp(&temp);
    am_util_stdio_printf("compensate_temp float version: %F\r\n", bmp280_compensate_T_double(&temp, raw_temp));
	uint32_t compensate_temp = (uint32_t) (bmp280_compensate_T_double(&temp, raw_temp) * 1000);
    
	spi_chip_select(&spi, SPI_CS_0);
	flash_page_program(&flash, size, (uint8_t*)&compensate_temp, sizeof(compensate_temp));
	flash_wait_busy(&flash);
	size += 4;

	// Write the RTC time to the flash chip
	flash_write_time(&flash, &rtc, &spi, size);
	size += 8;

	// uint32_t raw_press = bmp280_get_adc_pressure(&temp);
	// am_util_stdio_printf("pressure: %F\r\n", bmp280_compensate_P_double(&temp, raw_press, raw_temp));

	// Read current pressure from BMP280 sensor and write to flash
	spi_chip_select(&spi, SPI_CS_1);
	uint32_t raw_press = bmp280_get_adc_pressure(&temp);
    am_util_stdio_printf("compensate_press float version: %F\r\n", bmp280_compensate_P_double(&temp, raw_press, raw_temp));
	uint32_t compensate_press = (uint32_t) (bmp280_compensate_P_double(&temp, raw_press, raw_temp));
    
	spi_chip_select(&spi, SPI_CS_0);
	flash_page_program(&flash, size, (uint8_t*)&compensate_press, sizeof(compensate_press));
	flash_wait_busy(&flash);
	size += 4;

	// Write the RTC time to the flash chip
	flash_write_time(&flash, &rtc, &spi, size);
	size += 8;

	// Read the banner from flash
	flash_print_string(&flash, &spi, 0, sizeof(toWrite));
	flash_print_int(&flash, &spi, sizeof(toWrite), PRINT_LENGTH);

	// erase data
	flash_sector_erase(&flash, 0);
	flash_wait_busy(&flash);

	// print flash data after erase
	flash_print_int(&flash, &spi, 0, PRINT_LENGTH);
	am_util_stdio_printf("done\r\n");
	return 0;
}