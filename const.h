
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#pragma pack(push,1)
typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];

    char subchunk1ID[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} WAVHeader;
#pragma pack(pop)

typedef struct {
    size_t t;
    size_t f;
    double mag;
} PeakInternal;

typedef struct {
    uint64_t hash;
    size_t t;
} Fingerprint64;


typedef struct {
    char songName[128]; 
    size_t numFP;
    Fingerprint64 *fps;
} SongEntry;

typedef struct {
    char songName[128];
    uint64_t hash;
    size_t t;
} FingerprintEntry;