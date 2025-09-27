#ifndef SPECTROGRAM_IMAGE_H
#define SPECTROGRAM_IMAGE_H

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif






int SpectrogramToImage(double complex* spectrogram, size_t numWindows, size_t numFreqBins, const char* outputPath);

#ifdef __cplusplus
}
#endif

#endif 
