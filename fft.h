/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Prototypes for the routines in fft.c
 *
 */
 
#include <fftw3.h>

#if 0
#define TIME_FFT
#endif

extern int fftLenIn;   
extern int fftLenOut;
 
void InitializeFFTW(int fftlen);
void fftW(short *in, short *out, int inLen);
void EndFFTW(void);
int  floor2(int num);
int  FFTactive(Signal *source, Signal *dest, int rateChange);
void displayFFT(fftw_complex *cp, short *out);
void initGraphX(void);

