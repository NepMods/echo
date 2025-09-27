#include "fft.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>


static void recursiveFFT(complex double* data, size_t N) {
    if (N <= 1) return;

    size_t half = N / 2;
    complex double* even = (complex double*)malloc(half * sizeof(complex double));
    complex double* odd  = (complex double*)malloc(half * sizeof(complex double));

    for (size_t i = 0; i < half; i++) {
        even[i] = data[2*i];
        odd[i]  = data[2*i+1];
    }

    recursiveFFT(even, half);
    recursiveFFT(odd, half);

    for (size_t k = 0; k < half; k++) {
        complex double t = cos(-2*M_PI*k/N) + sin(-2*M_PI*k/N)*I;
        data[k]       = even[k] + t*odd[k];
        data[k+half]  = even[k] - t*odd[k];
    }

    free(even);
    free(odd);
}

void FFT(const double* input, complex double* output, size_t length) {
    for (size_t i = 0; i < length; i++) {
        output[i] = input[i] + 0.0*I;  
    }
    recursiveFFT(output, length);
}
