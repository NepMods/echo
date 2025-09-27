#include "spectrogram.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <string.h>
#include "fft.h" 

size_t LowPassFilter(double cutoff, double sampleRate, const double* input, size_t len, double* output) {
    if (cutoff <= 0 || sampleRate <= 0) return 0;

    double rc = 1.0 / (2.0 * M_PI * cutoff);
    double dt = 1.0 / sampleRate;
    double alpha = dt / (rc + dt);

    double prev = 0.0;
    for (size_t i = 0; i < len; i++) {
        if (i == 0) output[i] = input[i] * alpha;
        else output[i] = alpha * input[i] + (1.0 - alpha) * prev;
        prev = output[i];
    }
    return len;
}

size_t Downsample(const double* input, size_t inputLen, int originalRate, int targetRate, double* output) {
    if (originalRate <= 0 || targetRate <= 0 || targetRate > originalRate) return 0;

    int ratio = originalRate / targetRate;
    size_t outIdx = 0;
    for (size_t i = 0; i < inputLen; i += ratio) {
        double sum = 0.0;
        size_t count = (i + ratio < inputLen) ? ratio : inputLen - i;
        for (size_t j = 0; j < count; j++) sum += input[i + j];
        output[outIdx++] = sum / (double)count;
    }
    return outIdx;
}

static int float_cmp(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

void Spectrogram(const double* sample, size_t sampleLen, int sampleRate, double complex** spectrogram, size_t* numWindows) {
    if (!sample || sampleLen == 0 || !spectrogram || !numWindows) return;

    
    size_t downLen = (sampleLen + DSP_RATIO - 1) / DSP_RATIO; 
    double* downsampled = (double*)malloc(downLen * sizeof(double));
    if (!downsampled) { *numWindows = 0; return; }

    
    double cutoff = MAX_FREQ;
    for (size_t i = 0; i < downLen; i++) {
        size_t idx = i * DSP_RATIO;
        if (idx >= sampleLen) idx = sampleLen - 1;
        
        downsampled[i] = sample[idx]; 
    }
    size_t actualLen = downLen;

    
    double* hamming = (double*)malloc(FREQ_BIN_SIZE * sizeof(double));
    if (!hamming) { free(downsampled); *numWindows = 0; return; }
    for (int i = 0; i < FREQ_BIN_SIZE; i++)
        hamming[i] = 0.54 - 0.46 * cos(2*M_PI*i/(FREQ_BIN_SIZE-1));

    
    size_t windows = (actualLen + HOP_SIZE - 1) / HOP_SIZE;
    *numWindows = windows;

    
    *spectrogram = (double complex*)malloc(windows * FREQ_BIN_SIZE * sizeof(double complex));
    if (!(*spectrogram)) { free(downsampled); free(hamming); *numWindows = 0; return; }

    
    double binBuffer[FREQ_BIN_SIZE];        
    double complex fftOut[FREQ_BIN_SIZE];   

    
    for (size_t w = 0; w < windows; w++) {
        size_t start = w * HOP_SIZE;

        
        for (size_t i = 0; i < FREQ_BIN_SIZE; i++) {
            if (start + i < actualLen) binBuffer[i] = downsampled[start + i] * hamming[i];
            else binBuffer[i] = 0.0;
        }

        
        FFT(binBuffer, fftOut, FREQ_BIN_SIZE);

        
        for (size_t i = 0; i < FREQ_BIN_SIZE; i++)
            (*spectrogram)[w * FREQ_BIN_SIZE + i] = fftOut[i];
    }

    free(downsampled);
    free(hamming);
}

void ExtractPeaks(const double complex* spectrogram, size_t numWindows, double audioDuration, Peak* peaks, size_t* numPeaks, size_t maxPeaks) {
    if (!spectrogram || !peaks || !numPeaks || numWindows == 0 || maxPeaks == 0) {
        if (numPeaks) *numPeaks = 0;
        return;
    }
    *numPeaks = 0;

    const int N = FREQ_BIN_SIZE;
    const int halfN = N / 2;
    const double hopDuration = (numWindows > 0) ? (audioDuration / (double)numWindows) : 0.0;

    
    int bands[MAX_BANDS + 1];
    bands[0] = 0;
    for (int b = 1; b <= MAX_BANDS; ++b) {
        int prev = bands[b-1];
        int remain = (halfN) - prev;
        if (remain <= 0) bands[b] = prev;
        else bands[b] = prev + (int)(remain / pow(2.0, (double)(MAX_BANDS - b + 1)));
        if (bands[b] <= bands[b-1]) bands[b] = bands[b-1] + 1;
        if (bands[b] > halfN) bands[b] = halfN;
    }
    const size_t SAMPLE_EVERY = (numWindows > 2000) ? (numWindows / 500 + 1) : 1;
    double band_mean[MAX_BANDS];
    double band_std[MAX_BANDS];

    for (int b = 0; b < MAX_BANDS; ++b) {
        int start = bands[b];
        int end = bands[b+1];
        int width = end - start;
        if (width <= 0) { band_mean[b] = 0.0; band_std[b] = 1.0; continue; }

        double sum = 0.0;
        double sumsq = 0.0;
        size_t count = 0;
        for (size_t w = 0; w < numWindows; w += SAMPLE_EVERY) {
            const double complex* row = spectrogram + w * N;
            for (int f = start; f < end; ++f) {
                double m = cabs(row[f]);
                sum += m;
                sumsq += m * m;
                ++count;
            }
        }
        if (count == 0) { band_mean[b] = 0.0; band_std[b] = 1.0; }
        else {
            double mean = sum / (double)count;
            double var = sumsq / (double)count - mean * mean;
            if (var < 1e-12) var = 0.0;
            band_mean[b] = mean;
            band_std[b] = sqrt(var);
            if (band_std[b] < 1e-6) band_std[b] = 1e-6; 
        }
    }

    
    float *mag_prev = (float*)calloc(halfN, sizeof(float));
    float *mag_curr = (float*)calloc(halfN, sizeof(float));
    float *mag_next = (float*)calloc(halfN, sizeof(float));
    if (!mag_prev || !mag_curr || !mag_next) {
        if (mag_prev) free(mag_prev);
        if (mag_curr) free(mag_curr);
        if (mag_next) free(mag_next);
        *numPeaks = 0;
        return;
    }

    
    int maxBandWidth = halfN;
    typedef struct { int f; float mag; } Cand;
    Cand *cands = (Cand*)malloc(sizeof(Cand) * (size_t)maxBandWidth);
    if (!cands) {
        free(mag_prev); free(mag_curr); free(mag_next);
        *numPeaks = 0;
        return;
    }

    
    const float THRESH_FACTOR = 1.6f;   
    const int TOP_K_PER_BAND = 2;       
    const int LOCAL_NEIGH = NEIGHBORHOOD;
    const double time_tol_multiplier = 2.0; 

    
    
    for (int i = 0; i < halfN; ++i) mag_prev[i] = 0.0f;

    if (numWindows >= 1) {
        const double complex* row0 = spectrogram + 0 * N;
        for (int i = 0; i < halfN; ++i) mag_curr[i] = (float)cabs(row0[i]);
    } else {
        for (int i = 0; i < halfN; ++i) mag_curr[i] = 0.0f;
    }
    if (numWindows >= 2) {
        const double complex* row1 = spectrogram + 1 * N;
        for (int i = 0; i < halfN; ++i) mag_next[i] = (float)cabs(row1[i]);
    } else {
        for (int i = 0; i < halfN; ++i) mag_next[i] = 0.0f;
    }

    
    for (size_t w = 0; w < numWindows; ++w) {
        
        if (w > 0) {
            
            float *tmp = mag_prev; mag_prev = mag_curr; mag_curr = mag_next; mag_next = tmp;
            size_t nextIdx = w + 1;
            if (nextIdx < numWindows) {
                const double complex* rown = spectrogram + nextIdx * N;
                for (int i = 0; i < halfN; ++i) mag_next[i] = (float)cabs(rown[i]);
            } else {
                
                for (int i = 0; i < halfN; ++i) mag_next[i] = 0.0f;
            }
        }

        
        for (int b = 0; b < MAX_BANDS; ++b) {
            int start = bands[b];
            int end = bands[b+1];
            int width = end - start;
            if (width <= 0) continue;

            
            float threshold = (float)(band_mean[b] + THRESH_FACTOR * band_std[b]);

            int candCount = 0;
            for (int f = start; f < end; ++f) {
                float m = mag_curr[f];
                if (m <= threshold) continue;

                
                int isPeak = 1;
                
                for (int df = -LOCAL_NEIGH; df <= LOCAL_NEIGH && isPeak; ++df) {
                    int fi = f + df;
                    if (fi < start || fi >= end) continue;
                    if (mag_prev[fi] > m) isPeak = 0;
                }
                
                for (int df = -LOCAL_NEIGH; df <= LOCAL_NEIGH && isPeak; ++df) {
                    int fi = f + df;
                    if (fi < start || fi >= end) continue;
                    if (mag_curr[fi] > m) isPeak = 0;
                }
                
                for (int df = -LOCAL_NEIGH; df <= LOCAL_NEIGH && isPeak; ++df) {
                    int fi = f + df;
                    if (fi < start || fi >= end) continue;
                    if (mag_next[fi] > m) isPeak = 0;
                }
                if (!isPeak) continue;

                
                if (candCount < maxBandWidth) {
                    cands[candCount].f = f;
                    cands[candCount].mag = m;
                    candCount++;
                } else {
                    
                    int minIdx = 0;
                    float minV = cands[0].mag;
                    for (int cc = 1; cc < candCount; ++cc) {
                        if (cands[cc].mag < minV) { minV = cands[cc].mag; minIdx = cc; }
                    }
                    if (m > minV) {
                        cands[minIdx].f = f;
                        cands[minIdx].mag = m;
                    }
                }
            } 

            if (candCount == 0) continue;

            
            for (int k = 0; k < TOP_K_PER_BAND && k < candCount; ++k) {
                int best = k;
                for (int j = k+1; j < candCount; ++j)
                    if (cands[j].mag > cands[best].mag) best = j;
                if (best != k) { Cand tmp = cands[k]; cands[k] = cands[best]; cands[best] = tmp; }

                if (*numPeaks < maxPeaks) {
                    double time = w * hopDuration;
                    
                    peaks[*numPeaks].time = time;
                    peaks[*numPeaks].freq = cands[k].mag;
                    (*numPeaks)++;
                } else break;
            }
        } 
    } 

    
    if (*numPeaks > 1) {
        size_t pcount = *numPeaks;
        char *processed = (char*)calloc(pcount, 1);
        char *keep = (char*)calloc(pcount, 1);
        if (processed && keep) {
            double time_tol = hopDuration * time_tol_multiplier;
            while (1) {
                int best_idx = -1;
                float best_mag = -1.0f;
                for (size_t i = 0; i < pcount; ++i) {
                    if (processed[i]) continue;
                    float m = (float)peaks[i].freq;
                    if (m > best_mag) { best_mag = m; best_idx = (int)i; }
                }
                if (best_idx < 0) break;
                keep[best_idx] = 1;
                processed[best_idx] = 1;
                for (size_t j = 0; j < pcount; ++j) {
                    if (processed[j]) continue;
                    double dt = fabs(peaks[j].time - peaks[best_idx].time);
                    if (dt <= time_tol) processed[j] = 1;
                }
                
                int anyLeft = 0;
                for (size_t i = 0; i < pcount; ++i) if (!processed[i]) { anyLeft = 1; break; }
                if (!anyLeft) break;
            }
            
            size_t write = 0;
            for (size_t i = 0; i < pcount && write < maxPeaks; ++i) {
                if (keep[i]) {
                    if (write != i) peaks[write] = peaks[i];
                    write++;
                }
            }
            *numPeaks = write;
        }
        if (processed) free(processed);
        if (keep) free(keep);
    }

    free(mag_prev); free(mag_curr); free(mag_next); free(cands);
    return;
}
