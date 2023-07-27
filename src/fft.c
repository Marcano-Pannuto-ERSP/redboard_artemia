// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Gabriel Marcano, 2023
// SPDX-FileCopyrightText: Kristin Ebuengan, 2023
// SPDX-FileCopyrightText: Melody Gill, 2023

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "kiss_fftr.h"
#include <stdint.h>

#include <fft.h>

// Initialize FFT structure
void fft_init(struct fft *fft)
{
    fft->N = 512;
    fft->S = 7813;
}

// Change N
void fft_N(struct fft *fft, uint32_t number)
{
    fft->N = number;
}

// Get N
uint32_t fft_get_N(struct fft *fft)
{
    return fft->N;
}

// Change S
void fft_S(struct fft *fft, uint32_t sampling)
{
    fft->S = sampling;
}

// Get S
uint32_t fft_get_S(struct fft *fft)
{
    return fft->S;
}

// Gets the frequency with the highest amplitude
uint32_t TestFftReal(struct fft *fft, const kiss_fft_scalar in[], kiss_fft_cpx out[])
{
  kiss_fftr_cfg cfg;

  if ((cfg = kiss_fftr_alloc(fft->N, 0/*is_inverse_fft*/, NULL, NULL)) != NULL)
  {
    size_t i;

    kiss_fftr(cfg, in, out);
    free(cfg);

    double data[(fft->N/2)+1];
    for (i = 0; i < fft->N/2 + 1; i++)
    {
        double num = sqrt(out[i].r * out[i].r + out[i].i * out[i].i);
        data[i] = num;
    }
    int max = 0;
    int bucket = 0;
    for (int j = 1; j < fft->N/2 + 1; j++){
      if(data[j] > max){
        max = data[j];
        bucket = j;
      }
    }
    int freq = (bucket * fft->S)/fft->N;
    printf("Frequency: %d\r\n", freq);
    return freq;
  }
  else
  {
    printf("not enough memory?\n");
    exit(-1);
  }
}

// read the audio file and get the frequency with the highest amplitude
uint32_t fft_read(struct fft *fft, FILE * fp, uint16_t buffer[])
{
  // save contents of out.raw to a buffer
  fread(buffer, 2, fft->N, fp);
  
  // FFT transform
  kiss_fft_scalar in[fft->N];
  kiss_fft_cpx out[fft->N / 2 + 1];
  size_t i;

  for (i = 0; i < fft->N; i++){
    in[i] = buffer[i];
  }
  uint32_t toReturn = TestFftReal(fft, in, out);
  return toReturn;
}

// struct fft fft;

// int main(void)
// {
//     fft_init(&fft);
     
//     // open audio file
//     FILE * fp;
//     fp = fopen("out.raw", "r");
//     uint16_t buffer[fft.N];

//     fft_read(&fft, fp, buffer);

//     return 0;
    
// }