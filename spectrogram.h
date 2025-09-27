#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_RATIO    4
#define FREQ_BIN_SIZE 1024
#define MAX_FREQ     5000.0
#define HOP_SIZE     (FREQ_BIN_SIZE / 32)
#define MAX_BANDS    6
#define NEIGHBORHOOD 3 

typedef struct {
    double time;
    double complex freq;
} Peak;


size_t LowPassFilter(double cutoffFrequency, double sampleRate, const double* input, size_t len, double* output);
size_t Downsample(const double* input, size_t inputLen, int originalRate, int targetRate, double* output);
void Spectrogram(const double* sample, size_t sampleLen, int sampleRate, double complex** spectrogram, size_t* numWindows);
void ExtractPeaks(const double complex* spectrogram, size_t numWindows, double audioDuration, Peak* peaks, size_t* numPeaks, size_t maxPeaks);

#ifdef __cplusplus
}
#endif

#endif 
