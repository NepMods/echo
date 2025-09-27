
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <complex.h>
#include "music.h"
#include "fingerprint.h"
#include "spectrogram.h"
#include "spectrogram_image.h"

/*
 * load_wav_floats:
 *  - returns a newly malloc'd float buffer containing ONE channel (left)
 *  - numSamplesOut = number of frames (samples per channel)
 *  - durationOut = frames / sampleRate
 *
 * NOTE: this implementation assumes 16-bit PCM WAV. If you need other formats,
 * extend parsing accordingly.
 */
float *load_wav_floats(
    const char *filename,
    WAVHeader *header,
    size_t *numSamplesOut,
    double *durationOut,
    uint32_t *sampleRateOut
) {
    if (!filename || !header) return NULL;
    FILE *f = fopen(filename, "rb");
    if (!f) { perror("fopen"); return NULL; }

    if (fread(header, sizeof(WAVHeader), 1, f) != 1) {
        fclose(f);
        fprintf(stderr, "Failed to read WAV header\n");
        return NULL;
    }

    
    char chunkID[4];
    uint32_t chunkSize = 0;
    int found = 0;
    while (fread(chunkID, 1, 4, f) == 4) {
        if (fread(&chunkSize, 4, 1, f) != 1) break;
        if (memcmp(chunkID, "data", 4) == 0) { found = 1; break; }
        
        if (fseek(f, (long)chunkSize, SEEK_CUR) != 0) break;
    }
    if (!found) {
        fclose(f);
        fprintf(stderr, "No data chunk found in WAV\n");
        return NULL;
    }

    if (header->bitsPerSample != 16) {
        fclose(f);
        fprintf(stderr, "Only 16-bit PCM supported (bitsPerSample=%u)\n", header->bitsPerSample);
        return NULL;
    }

    uint32_t bytesPerSample = header->bitsPerSample / 8; 
    size_t totalSamples = (size_t)chunkSize / bytesPerSample; 
    size_t frames = totalSamples / (header->numChannels > 0 ? header->numChannels : 1);

    int16_t *raw = (int16_t*)malloc(totalSamples * sizeof(int16_t));
    if (!raw) { fclose(f); return NULL; }

    size_t read = fread(raw, sizeof(int16_t), totalSamples, f);
    fclose(f);
    if (read != totalSamples) {
        free(raw);
        fprintf(stderr, "Short read from WAV data chunk\n");
        return NULL;
    }

    float *out = (float*)malloc(frames * sizeof(float));
    if (!out) { free(raw); return NULL; }

    
    int channels = (header->numChannels > 0) ? header->numChannels : 1;
    for (size_t i = 0; i < frames; ++i) {
        int16_t s = raw[i * (size_t)channels + 0];
        out[i] = (float)s / 32768.0f;
    }

    free(raw);

    if (numSamplesOut) *numSamplesOut = frames;
    if (sampleRateOut) *sampleRateOut = header->sampleRate;
    if (durationOut) *durationOut = (double)frames / (double)header->sampleRate;

    return out;
}

uint32_t GenerateUniqueID() {
    
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    
    uint32_t r = ((uint32_t)rand() << 16) | ((uint32_t)rand() & 0xFFFF);
    return r;
}
typedef struct {
    uint32_t songID;
    char songName[128];
} SongMeta;

/* Save song metadata (songID + songName) */
static void add_song_metadata(const char *metaDBFile, uint32_t songID, const char *songName) {
    FILE *f = fopen(metaDBFile, "ab");
    if (!f) { perror("fopen meta"); return; }

    SongMeta meta = {songID, ""};
    strncpy(meta.songName, songName, 128);
    fwrite(&meta, sizeof(SongMeta), 1, f);
    fclose(f);
}

/* Main function to add a song: fingerprints + metadata */
void add_song_to_db(const char *wavFile, const char *songName, const char *dbFile) {
    WAVHeader header;
    size_t numFrames;        
    double duration;
    uint32_t sampleRate;

    
    float *samples_f = load_wav_floats(wavFile, &header, &numFrames, &duration, &sampleRate);
    if (!samples_f) {
        fprintf(stderr, "Failed to load WAV: %s\n", wavFile);
        return;
    }

    if (numFrames == 0 || sampleRate == 0) {
        fprintf(stderr, "Invalid audio length or sampleRate\n");
        free(samples_f);
        return;
    }

    
    double *samples_d = malloc(numFrames * sizeof(double));
    if (!samples_d) { perror("malloc"); free(samples_f); return; }
    for (size_t i = 0; i < numFrames; ++i) samples_d[i] = (double)samples_f[i];
    free(samples_f);

    fprintf(stdout, "add_song_to_db: frames=%zu sampleRate=%u duration=%.3f channels=%u\n",
            numFrames, sampleRate, duration, header.numChannels);

    
    double complex *spec = NULL;
    size_t numWindows = 0;
    Spectrogram(samples_d, numFrames, sampleRate, &spec, &numWindows);
    fprintf(stdout, "Spectrogram result: windows=%zu spec_ptr=%p\n", numWindows, (void*)spec);
    if (!spec || numWindows == 0) {
        fprintf(stderr, "Spectrogram failed or produced zero windows\n");
        free(samples_d);
        return;
    }

    
    size_t maxPeaks = numWindows * MAX_BANDS;
    Peak *peaks = malloc(maxPeaks * sizeof(Peak));
    size_t numPeaks = 0;
    ExtractPeaks(spec, numWindows, duration, peaks, &numPeaks, maxPeaks);
    fprintf(stdout, "ExtractPeaks result: numPeaks=%zu\n", numPeaks);

    
    uint32_t songID = GenerateUniqueID();

    
    char metaDBFile[512];
    snprintf(metaDBFile, sizeof(metaDBFile), "%s.meta", dbFile);
    add_song_metadata(metaDBFile, songID, songName);

    
    size_t maxFingerprints = numPeaks * TARGET_ZONE_SIZE;
    Fingerprint *fingerprints = malloc(maxFingerprints * sizeof(Fingerprint));
    size_t numFingerprints = Fingerprint_f(peaks, numPeaks, songID, fingerprints, maxFingerprints);

    
    FILE *f = fopen(dbFile, "ab");
    if (!f) { perror("fopen fingerprints"); goto cleanup; }

    fwrite(&numFingerprints, sizeof(size_t), 1, f);
    fwrite(fingerprints, sizeof(Fingerprint), numFingerprints, f);
    fclose(f);

    printf("Saved %zu fingerprints for '%s' (songID=%u)\n", numFingerprints, songName, songID);

cleanup:
    free(fingerprints);
    free(peaks);
    free(spec);
    free(samples_d);
}