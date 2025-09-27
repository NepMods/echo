
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "const.h"
#include "complex.h"

void stereo_to_mono(int16_t *stereoBuffer, int16_t *monoBuffer, size_t numFrames);
float *load_wav_floats(
    const char *filename,
    WAVHeader *header,
    size_t *numSamplesOut,
    double *durationOut,
    uint32_t *sampleRateOut
);
void add_song_to_db(const char *wavFile,const char *songName,const char *dbFile);
SongEntry *load_db(const char *dbFile,size_t *numSongs);
SongEntry *load_db(const char *dbFile,size_t *numSongs);