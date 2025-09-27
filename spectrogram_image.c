#include "spectrogram_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <math.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" 

int SpectrogramToImage(double complex* spectrogram, size_t numWindows, size_t numFreqBins, const char* outputPath) {
    if (!spectrogram || numWindows == 0 || numFreqBins == 0) return -1;

    
    double maxMag = 0.0;
    for (size_t i = 0; i < numWindows; i++) {
        for (size_t j = 0; j < numFreqBins; j++) {
            double mag = cabs(spectrogram[i*numFreqBins + j]);
            if (mag > maxMag) maxMag = mag;
        }
    }
    if (maxMag == 0.0) maxMag = 1.0;

    
    unsigned char* img = (unsigned char*)malloc(numWindows * numFreqBins);
    if (!img) return -2;

    for (size_t i = 0; i < numWindows; i++) {
        for (size_t j = 0; j < numFreqBins; j++) {
            double mag = cabs(spectrogram[i*numFreqBins + j]);
            unsigned char intensity = (unsigned char)floor(255.0 * (mag / maxMag));
            img[i*numFreqBins + j] = intensity; 
        }
    }

    
    int res = stbi_write_png(outputPath, (int)numFreqBins, (int)numWindows, 1, img, (int)numFreqBins);
    free(img);

    return res ? 0 : -3;
}
