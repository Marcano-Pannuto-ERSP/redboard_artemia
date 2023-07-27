// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Gabriel Marcano, 2023
// SPDX-FileCopyrightText: Kristin Ebuengan, 2023
// SPDX-FileCopyrightText: Melody Gill, 2023

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
#include <syscalls.h>
#include <asimple_littlefs.h>
#include <fft.h>
#include <kiss_fftr.h>

# define PRINT_LENGTH 80

struct uart uart;
struct spi spi;
struct adc adc;
struct am1815 rtc;
struct bmp280 temp;
struct flash flash;
struct pdm pdm;
struct asimple_littlefs fs;
struct fft fft;

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
	fft_init(&fft);

	// erase data
	flash_sector_erase(&flash, 0);
	flash_wait_busy(&flash);

	// print the data before write
	flash_print_int(&flash, &spi, 0, PRINT_LENGTH);

	// Struct asimple_littlefs fs; defined earlier
    asimple_littlefs_init(&fs, &flash);
    int err = asimple_littlefs_mount(&fs);
    if (err < 0)
    {   
        asimple_littlefs_format(&fs);
        asimple_littlefs_mount(&fs);
    }
    syscalls_littlefs_init(&fs);

	// Open all the files
	spi_chip_select(&spi, SPI_CS_0);
	FILE * tfile = fopen("fs:/temperature_data.csv", "a+");
	FILE * pfile = fopen("fs:/pressure_data.csv", "a+");
	FILE * lfile = fopen("fs:/light_data.csv", "a+");
	FILE * mfile = fopen("fs:/microphone_data.csv", "a+");

	// Add CSV headers
	fprintf(tfile, "temperature data celsius,time\r\n");
	fprintf(pfile, "pressure data pascals,time\r\n");
	fprintf(lfile, "light data ohms,time\r\n");
	fprintf(mfile, "microphone data Hz,time\r\n");

	// Trigger the ADC to start collecting data
	adc_trigger(&adc);

	// print the flash ID to make sure the CS is connected correctly
	spi_chip_select(&spi, SPI_CS_0);
	am_util_stdio_printf("flash ID: %02X\r\n", flash_read_id(&flash));

	// print the RTC ID to make sure the CS is connected correctly
	spi_chip_select(&spi, SPI_CS_3);
	am_util_stdio_printf("RTC ID: %02X\r\n", am1815_read_register(&rtc, 0x28));

	// Read current temperature from BMP280 sensor and write to flash
	spi_chip_select(&spi, SPI_CS_1);
    am_util_stdio_printf("BMP280 ID: %02X\r\n", bmp280_read_id(&temp));
	uint32_t raw_temp = bmp280_get_adc_temp(&temp);
    am_util_stdio_printf("compensate_temp float version: %F\r\n", bmp280_compensate_T_double(&temp, raw_temp));
	uint32_t compensate_temp = (uint32_t) (bmp280_compensate_T_double(&temp, raw_temp) * 1000);
	spi_chip_select(&spi, SPI_CS_3);
	struct timeval time = am1815_read_time(&rtc);
	spi_chip_select(&spi, SPI_CS_0);

	// FIXME wrap this in a function???
	// This is one way to prepare an uint64_t as a string
	int max = ceil(log10(time.tv_sec));
	uint8_t buffer[21] = {0}; // log_10(2^64) is just under 20, and an extra character for null
	uint64_t tmp = time.tv_sec;
	for (uint8_t *c = buffer + max - 1; c >= buffer; --c)
	{
		*c = '0' + (tmp % 10);
		tmp /= 10;
	}

	printf("time: %s\r\n", (const char*)buffer);
	fprintf(tfile, "%u,%s\r\n", compensate_temp, buffer);

	// // Write the RTC time to the flash chip
	// flash_write_time(&flash, &rtc, &spi, size);
	// size += 8;

	// // Read current pressure from BMP280 sensor and write to flash
	// spi_chip_select(&spi, SPI_CS_1);
	// uint32_t raw_press = bmp280_get_adc_pressure(&temp);
    // am_util_stdio_printf("compensate_press float version: %F\r\n", bmp280_compensate_P_double(&temp, raw_press, raw_temp));
	// uint32_t compensate_press = (uint32_t) (bmp280_compensate_P_double(&temp, raw_press, raw_temp));
	// spi_chip_select(&spi, SPI_CS_0);
	// flash_page_program(&flash, size, (uint8_t*)&compensate_press, sizeof(compensate_press));
	// flash_wait_busy(&flash);
	// size += 4;

	// // Write the RTC time to the flash chip
	// flash_write_time(&flash, &rtc, &spi, size);
	// size += 8;

	// // Read current resistance of the Photo Resistor and write to flash
	// uint32_t data = 0;
	// if (adc_get_sample(&adc, &data))
	// {
	// 	const double reference = 1.5;
	// 	double voltage = data * reference / ((1 << 14) - 1);
	// 	am_util_stdio_printf("voltage = <%.3f> (0x%04X)\r\n", voltage, data);
	// 	uint32_t resistance = (uint32_t)((10000 * voltage)/(3.3 - voltage));
	// 	am_util_stdio_printf("resistance = <%d>\r\n", resistance);
	// 	flash_page_program(&flash, size, (uint8_t*)&resistance, sizeof(resistance));
	// 	flash_wait_busy(&flash);
	// 	size += 4;
	// }

	// // Write the RTC time to the flash chip
	// flash_write_time(&flash, &rtc, &spi, size);
	// size += 8;

	// // MICROHPONE STUFF -------------------------------------------------------------------------------------------------------------------------
    // // Turn on the PDM, set it up for our chosen recording settings, and start
    // // the first DMA transaction.
    // am_hal_pdm_fifo_flush(pdm.PDMHandle);
    // pdm_data_get(&pdm, pdm.g_ui32PDMDataBuffer1);
    // bool toggle = true;
	// uint32_t max = 0;
	// uint32_t N = fft_get_N(&fft);
    // while(toggle)
    // {
    //     am_hal_uart_tx_flush(uart.handle);
    //     am_hal_interrupt_master_disable();
    //     bool ready = isPDMDataReady();
    //     am_hal_interrupt_master_enable();
    //     if (ready)
    //     {
    //         ready = false;
	// 		int16_t *pi16PDMData = (int16_t *)pdm.g_ui32PDMDataBuffer1;
	// 		// FFT transform
	// 		kiss_fft_scalar in[N];
	// 		kiss_fft_cpx out[N / 2 + 1];
	// 		for (int j = 0; j < N; j++){
	// 			in[j] = pi16PDMData[j];
	// 		}
	// 		max = TestFftReal(&fft, in, out);
	// 		toggle = false;
    //     }
    //     // Go to Deep Sleep.
    //     am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
    // }

	// // Save the frequency with highest amplitude to flash
	// flash_page_program(&flash, size, (uint8_t*)&max, sizeof(max));
	// flash_wait_busy(&flash);
	// size += 4;

	// // Write the RTC time to the flash chip
	// flash_write_time(&flash, &rtc, &spi, size);
	// size += 8;

	// // Read the banner from flash
	// flash_print_string(&flash, &spi, 0, sizeof(toWrite));
	// flash_print_int(&flash, &spi, sizeof(toWrite), PRINT_LENGTH);

	// // erase data
	// flash_sector_erase(&flash, 0);
	// flash_wait_busy(&flash);

	

	// Close files
	spi_chip_select(&spi, SPI_CS_0);
	fclose(tfile);
	fclose(pfile);
	fclose(lfile);
	fclose(mfile);

	// print flash data after close
	flash_print_int(&flash, &spi, 0, PRINT_LENGTH);
	am_util_stdio_printf("done\r\n");

	return 0;
}
