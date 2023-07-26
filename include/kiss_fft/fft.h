// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Gabriel Marcano, 2023
// SPDX-FileCopyrightText: Kristin Ebuengan, 2023
// SPDX-FileCopyrightText: Melody Gill, 2023

#ifndef FFT_H_
#define FFT_H_

#include <math.h>
#include "kiss_fftr.h"
#include <stdint.h>

/** Structure representing the information of the samples */
struct fft
{
	uint32_t N; // total number of samples (size of file in bytes / 2)
    uint32_t S; // sampling frequency
};

/**
 * FFT initialization.
 * 
 * @param[in, out] fft FFT structure to initialize.
*/
void fft_init(struct fft *fft);

/**
 * Changes the total number of samples.
 * 
 * @param[in, out] fft FFT structure to change.
 * @param[in] number new total number of samples to take.
*/
void fft_N(struct fft *fft, uint32_t number);

/**
 * Gets the total number of samples.
 * 
 * @param[in, out] fft FFT structure to get from.
*/
uint32_t fft_get_N(struct fft *fft);

/**
 * Changes the sampling rate.
 * 
 * @param[in, out] fft FFT structure to change.
 * @param[in] sampling new sampling rate.
*/
void fft_S(struct fft *fft, uint32_t sampling);

/**
 * Gets the sampling rate.
 * 
 * @param[in, out] fft FFT structure to get from.
*/
uint32_t fft_get_S(struct fft *fft);

/**
 * Gets the frequency with the highest amplitude
 * 
 * @param[in] fft FFT structure to get information from.
 * @param[in] in audio data points
 * @param[in] out magnitude data
 * 
 * @returns the frequency with the highest amplitude
*/
uint32_t TestFftReal(struct fft *fft, const kiss_fft_scalar in[], kiss_fft_cpx out[]);

/**
 * Reads the audio file and returns the frequency with the highest amplitude
 * 
 * @param[in] fft FFT struture to use.
 * @param[in] fp audio file to read from.
 * @param[in] buffer buffer to save the audio file to.
 * 
 * @returns the frequency with the highest amplitude
*/
uint32_t fft_read(struct fft *fft, FILE * fp, uint16_t buffer[]);

#endif//FFT_H_