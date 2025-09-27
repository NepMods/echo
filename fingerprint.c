#include "fingerprint.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define MAX_BANDS 6
#define TOP_PEAKS 3        
#define TARGET_ZONE_SIZE 5 

typedef struct {
    double time;        
    uint16_t freqBin;   
} PeakSmall;
static uint32_t createAddressReduced(const Peak anchor, const Peak target) {
    uint32_t anchorFreq = (uint32_t)creal(anchor.freq) & 0x1FF; 
    uint32_t targetFreq = (uint32_t)creal(target.freq) & 0x1FF; 
    uint32_t deltaMs = (uint32_t)((target.time - anchor.time) * 100.0); 
    if (deltaMs > 0x3FFF) deltaMs = 0x3FFF; 

    return (anchorFreq << 23) | (targetFreq << 14) | deltaMs;
}



size_t Fingerprint_f(
    const Peak* peaks,
    size_t numPeaks,
    uint32_t songID,
    Fingerprint* outFingerprints,
    size_t maxFingerprints
) {
    size_t count = 0;

    for (size_t i = 0; i < numPeaks; ++i) {
        const Peak anchor = peaks[i];

        for (size_t j = i + 1; j < numPeaks && j <= i + TARGET_ZONE_SIZE; ++j) {
            const Peak target = peaks[j];

            if (count >= maxFingerprints) return count;

            uint32_t address = createAddressReduced(anchor, target);
            outFingerprints[count].address = address;
            outFingerprints[count].couple.anchorTimeMs = (uint32_t)(anchor.time * 1000.0);
            outFingerprints[count].couple.songID = songID;

            count++;
        }
    }

    return count;
}