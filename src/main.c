// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Gabriel Marcano, 2023
// SPDX-FileCopyrightText: Kristin Ebuengan, 2023
// SPDX-FileCopyrightText: Melody Gill, 2023

/*
 * Collects data from the sensors (temperature, pressure, photo resistor, microhpone)
 * and saves them in files that are written to the flash chip
*/

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include <uart.h>
#include <spi.h>
#include <adc.h>
#include <am1815.h>
#include <bmp280.h>
#include <flash.h>
#include <pdm.h>
#include <syscalls.h>
#include <asimple_littlefs.h>
#include <power_control.h>

#include <fft.h>
#include <kiss_fftr.h>

struct uart uart;
struct spi_bus spi_bus;
struct spi_device flash_spi;
struct spi_device bmp280_spi;
struct spi_device rtc_spi;
struct adc adc;
struct am1815 rtc;
struct bmp280 temp;
struct flash flash;
struct pdm pdm;
struct asimple_littlefs fs;
struct fft fft;
struct power_control power_control;

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

	power_control_init(&power_control, 42, 43);

	// After init is done, enable interrupts
	am_hal_interrupt_master_enable();
}

__attribute__((destructor))
static void redboard_shutdown(void)
{
	// Any destructors/code that should run when main returns should go here
	power_control_shutdown(&power_control);
}

// Add headers to the file if there isn't one already
void add_headers(char headers[][40], FILE * files[], size_t size)
{
    for(size_t i = 0; i < size; i++){
        fseek(files[i], 0, SEEK_SET);
        int len = strlen(headers[i]);
        char buffer[len];
        fread(buffer, len, 1, files[i]);
        if(strncmp(headers[i], buffer, len) != 0){
            fprintf(files[i], "%s", headers[i]);
        }
        fseek(files[i], 0, SEEK_END);
    }
}

// Convert tv_sec, which is a long representing seconds, to a string in buffer
// Make sure to initialize buffer before calling
// uint8_t buffer[21] = {0};
void time_to_string(uint8_t buffer[21], uint64_t tv_sec) {
	// This is one way to prepare an uint64_t as a string
	int max = ceil(log10(tv_sec));
	uint64_t tmp = tv_sec;
	for (uint8_t *c = buffer + max - 1; c >= buffer; --c)
	{
		*c = '0' + (tmp % 10);
		tmp /= 10;
	}
}

// Write a line to fp in the format "data,time\n"
// Gets time from the RTC
void write_csv_line(FILE * fp, uint32_t data) {
	struct timeval time = am1815_read_time(&rtc);
	uint8_t buffer[21] = {0};
	time_to_string(buffer, (uint64_t) time.tv_sec);
	fprintf(fp, "%s,%lu\r\n", buffer, data);
}

int main(void)
{
	// Initialize all the necessary structs
	uint8_t pins[] = {16};
	size_t size = 1;
	adc_init(&adc, pins, size);
	spi_bus_init(&spi_bus, 0);
	spi_bus_enable(&spi_bus);
	spi_bus_init_device(&spi_bus, &flash_spi, SPI_CS_2, 4000000u);
	spi_bus_init_device(&spi_bus, &bmp280_spi, SPI_CS_1, 4000000u);
	spi_bus_init_device(&spi_bus, &rtc_spi, SPI_CS_3, 2000000u);
	am1815_init(&rtc, &rtc_spi);
	bmp280_init(&temp, &bmp280_spi);
	flash_init(&flash, &flash_spi);
	pdm_init(&pdm);
	fft_init(&fft);

	// Mount littlefs
    asimple_littlefs_init(&fs, &flash);
    int err = asimple_littlefs_mount(&fs);
    if (err < 0)
    {
        asimple_littlefs_format(&fs);
        asimple_littlefs_mount(&fs);
    }
    syscalls_littlefs_init(&fs);

	// Open all the files
	FILE * tfile = fopen("fs:/temperature_data.csv", "a+");
	FILE * pfile = fopen("fs:/pressure_data.csv", "a+");
	FILE * lfile = fopen("fs:/light_data.csv", "a+");
	FILE * mfile = fopen("fs:/microphone_data.csv", "a+");
	FILE * files[4] = {tfile, pfile, lfile, mfile};

	// Add CSV headers
	char headers[4][40] = {"time,temperature data celsius\r\n", "time,pressure data pascals\r\n", "time,light data ohms\r\n", "time,microphone data Hz\r\n"};
	add_headers(headers, files, 4);

	// print the flash ID to make sure the CS is connected correctly (should be 1520C2)
	am_util_stdio_printf("flash ID: %02X\r\n", flash_read_id(&flash));

	// print the RTC ID to make sure the CS is connected correctly (should be 18)
	am_util_stdio_printf("RTC ID: %02X\r\n", am1815_read_register(&rtc, 0x28));

	// Print BMP280 ID (should be 58)
    am_util_stdio_printf("BMP280 ID: %02X\r\n", bmp280_read_id(&temp));

	// Read current temperature from BMP280 sensor and write to flash
	uint32_t raw_temp = bmp280_get_adc_temp(&temp);
    am_util_stdio_printf("compensate_temp float version: %F\r\n", bmp280_compensate_T_double(&temp, raw_temp));
	uint32_t compensate_temp = (uint32_t) (bmp280_compensate_T_double(&temp, raw_temp) * 1000);
	write_csv_line(tfile, compensate_temp);

	// Read current pressure from BMP280 sensor and write to flash
	uint32_t raw_press = bmp280_get_adc_pressure(&temp);
    am_util_stdio_printf("compensate_press float version: %F\r\n", bmp280_compensate_P_double(&temp, raw_press, raw_temp));
	uint32_t compensate_press = (uint32_t) (bmp280_compensate_P_double(&temp, raw_press, raw_temp));
	write_csv_line(pfile, compensate_press);

	// Read current resistance of the Photo Resistor and write to flash
	adc_trigger(&adc);
	uint32_t data[1] = {0};
	uint32_t resistance;
	while(!(adc_get_sample(&adc, data, pins, size)));
	const double reference = 1.5;
	double voltage = data[0] * reference / ((1 << 14) - 1);
	am_util_stdio_printf("voltage = <%.3f> (0x%04X)\r\n", voltage, data[0]);
	resistance = (uint32_t)((10000 * voltage)/(3.3 - voltage));
	am_util_stdio_printf("resistance = <%d>\r\n", resistance);
	write_csv_line(lfile, resistance);

    // Turn on the PDM and start the first DMA transaction.
    pdm_flush(&pdm);
    pdm_data_get(&pdm, pdm.g_ui32PDMDataBuffer1);
    bool toggle = true;
	uint32_t max = 0;
	uint32_t N = fft_get_N(&fft);
    while(toggle)
    {
        am_hal_uart_tx_flush(uart.handle);
        am_hal_interrupt_master_disable();
        bool ready = isPDMDataReady();
        am_hal_interrupt_master_enable();
        if (ready)
        {
            ready = false;
			int16_t *pi16PDMData = (int16_t *)pdm.g_ui32PDMDataBuffer1;
			// FFT transform
			kiss_fft_scalar in[N];
			kiss_fft_cpx out[N / 2 + 1];
			for (uint32_t j = 0; j < N; j++){
				in[j] = pi16PDMData[j];
			}
			max = TestFftReal(&fft, in, out);
			toggle = false;
        }
        am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
    }
	// Save frequency with highest amplitude to flash
	write_csv_line(mfile, max);

	write_csv_line(stdout, 0);

	// Close files
	fclose(tfile);
	fclose(pfile);
	fclose(lfile);
	fclose(mfile);

	am_util_stdio_printf("done\r\n");

	return 0;
}
