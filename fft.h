#ifndef FFT_H
#define FFT_H

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif





void FFT(const double* input, complex double* output, size_t length);

#ifdef __cplusplus
}
#endif

#endif 
