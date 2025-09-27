#ifndef SHAZAM_H
#define SHAZAM_H

#include <stdint.h>
#include <stddef.h>
#include "spectrogram.h" 

#define TARGET_ZONE_SIZE 5

typedef struct {
    uint32_t anchorTimeMs;
    uint32_t songID;
} Couple;

typedef struct {
    uint32_t address;
    Couple couple;
} Fingerprint;

size_t Fingerprint_f(const Peak* peaks, size_t numPeaks, uint32_t songID, Fingerprint* outFingerprints, size_t maxFingerprints);

uint32_t createAddress(Peak anchor, Peak target);

#endif
