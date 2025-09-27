#define _GNU_SOURCE
#include "matcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "const.h"
/* Project headers (must exist in your project) */
#include "fingerprint.h"   
#include "spectrogram.h"   
/* load_db returns SongEntry_n array with songName, songID, numFingerprints, fingerprints */



typedef struct {
    uint32_t songID;
    char songName[128];
} SongMeta;
typedef struct {
    char songName[128];
    uint32_t songID;
    size_t numFingerprints;
    Fingerprint *fingerprints;
} SongEntry_n;


static SongMeta* load_metadata(const char *dbFile, size_t *numSongs) {
    char metaFile[512];
    snprintf(metaFile, sizeof(metaFile), "%s.meta", dbFile);

    FILE *f = fopen(metaFile, "rb");
    if (!f) { perror("fopen meta"); *numSongs = 0; return NULL; }

    SongMeta *songs = NULL;
    size_t count = 0;

    SongMeta temp;
    while (fread(&temp, sizeof(SongMeta), 1, f) == 1) {
        songs = realloc(songs, (count + 1) * sizeof(SongMeta));
        songs[count++] = temp;
    }

    fclose(f);
    *numSongs = count;
    return songs;
}


SongEntry_n* load_db(const char *dbFile, size_t *numSongs) {
    size_t numMeta;
    SongMeta *meta = load_metadata(dbFile, &numMeta);
    if (!meta || numMeta == 0) { *numSongs = 0; return NULL; }

    FILE *f = fopen(dbFile, "rb");
    if (!f) { perror("fopen fingerprints"); free(meta); *numSongs = 0; return NULL; }

    SongEntry_n *songs = malloc(numMeta * sizeof(SongEntry_n));

    for (size_t i = 0; i < numMeta; ++i) {
        songs[i].songID = meta[i].songID;
        strncpy(songs[i].songName, meta[i].songName, 128);

        size_t numFP;
        if (fread(&numFP, sizeof(size_t), 1, f) != 1) { songs[i].numFingerprints = 0; songs[i].fingerprints = NULL; continue; }

        Fingerprint *fps = malloc(numFP * sizeof(Fingerprint));
        if (fread(fps, sizeof(Fingerprint), numFP, f) != numFP) {
            free(fps);
            songs[i].numFingerprints = 0;
            songs[i].fingerprints = NULL;
            continue;
        }

        songs[i].numFingerprints = numFP;
        songs[i].fingerprints = fps;
    }

    fclose(f);
    free(meta);
    *numSongs = numMeta;
    return songs;
}
extern float *load_wav_floats(const char *filename, WAVHeader *header, size_t *numSamplesOut, double *durationOut, uint32_t *sampleRateOut);
extern void Spectrogram(const double* sample, size_t sampleLen, int sampleRate, double complex** spectrogram, size_t* numWindows);
extern void ExtractPeaks(const double complex* spectrogram, size_t numWindows, double audioDuration, Peak* peaks, size_t* numPeaks, size_t maxPeaks);
extern size_t Fingerprint_f(const Peak* peaks, size_t numPeaks, uint32_t songID, Fingerprint* outFingerprints, size_t maxFingerprints);

/* Internal small data types */

typedef struct {
    uint32_t sampleTime;
    uint32_t dbTime;
} TimePair;

/* matches map for one song: dynamic array of pairs, earliest timestamp, and target zone counts */
typedef struct {
    TimePair *pairs;
    size_t count;
    size_t cap;
    uint32_t earliest_timestamp;
} SongPairs;

/* We will build a map address -> vector<Couple> for addresses present in sample */
typedef struct AddrNode {
    uint32_t address;
    Couple *list;
    size_t count;
    size_t cap;
    struct AddrNode *next;
} AddrNode;

/* Simple hash table sizes (power-of-two) */
#define ADDR_BUCKETS (1 << 16) /* 65536 */
#define SONG_BUCKETS 4096

/* small song id -> index map node */
typedef struct SongIdxNode {
    uint32_t songID;
    size_t index;
    struct SongIdxNode *next;
} SongIdxNode;

/* hash multipliers */
static inline uint32_t hash32(uint32_t x) { return x * 2654435761u; }

/* add couple to address hash table */
static void addrnode_add(AddrNode **buckets, uint32_t address, Couple c) {
    uint32_t h = hash32(address) & (ADDR_BUCKETS - 1);
    AddrNode *n = buckets[h];
    while (n) {
        if (n->address == address) break;
        n = n->next;
    }
    if (!n) {
        n = (AddrNode*)calloc(1, sizeof(AddrNode));
        n->address = address;
        n->cap = 4;
        n->list = (Couple*)malloc(n->cap * sizeof(Couple));
        n->count = 0;
        n->next = buckets[h];
        buckets[h] = n;
    }
    if (n->count >= n->cap) {
        n->cap *= 2;
        n->list = (Couple*)realloc(n->list, n->cap * sizeof(Couple));
    }
    n->list[n->count++] = c;
}

/* find addr node */
static AddrNode *addrnode_find(AddrNode **buckets, uint32_t address) {
    uint32_t h = hash32(address) & (ADDR_BUCKETS - 1);
    AddrNode *n = buckets[h];
    while (n) {
        if (n->address == address) return n;
        n = n->next;
    }
    return NULL;
}

/* free addr buckets */
static void free_addr_buckets(AddrNode **buckets) {
    for (size_t i = 0; i < ADDR_BUCKETS; ++i) {
        AddrNode *n = buckets[i];
        while (n) {
            AddrNode *nx = n->next;
            free(n->list);
            free(n);
            n = nx;
        }
    }
}

/* build song index map for quick songID -> index lookup */
static void build_song_index_map(SongEntry_n *songs, size_t numSongs, SongIdxNode **buckets) {
    for (size_t i = 0; i < numSongs; ++i) {
        uint32_t sid = songs[i].songID;
        uint32_t h = hash32(sid) & (SONG_BUCKETS - 1);
        SongIdxNode *n = (SongIdxNode*)malloc(sizeof(SongIdxNode));
        n->songID = sid;
        n->index = i;
        n->next = buckets[h];
        buckets[h] = n;
    }
}
static int songid_to_index(SongIdxNode **buckets, uint32_t songID, size_t *outIndex) {
    uint32_t h = hash32(songID) & (SONG_BUCKETS - 1);
    SongIdxNode *n = buckets[h];
    while (n) {
        if (n->songID == songID) { *outIndex = n->index; return 1; }
        n = n->next;
    }
    return 0;
}
static void free_songidx_buckets(SongIdxNode **buckets) {
    for (size_t i = 0; i < SONG_BUCKETS; ++i) {
        SongIdxNode *n = buckets[i];
        while (n) {
            SongIdxNode *nx = n->next;
            free(n);
            n = nx;
        }
    }
}

/* analyzeRelativeTiming: same as Go version. 100ms tolerance. */
static double analyzeRelativeTiming(const TimePair *pairs, size_t n) {
    if (n < 2) return 0.0;
    int count = 0;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            double sampleDiff = fabs((double)((int32_t)pairs[i].sampleTime - (int32_t)pairs[j].sampleTime));
            double dbDiff = fabs((double)((int32_t)pairs[i].dbTime - (int32_t)pairs[j].dbTime));
            if (fabs(sampleDiff - dbDiff) < ((float)HOP_SIZE/1.0)) count++;
        }
    }
    return (double)count;
}

/* qsort comparator for Match: descending score */
static int match_cmp_desc(const void *a, const void *b) {
    const Match *ma = (const Match*)a;
    const Match *mb = (const Match*)b;
    if (ma->Score < mb->Score) return 1;
    if (ma->Score > mb->Score) return -1;
    return 0;
}

/* Main function implementing the Go algorithm faithfully */
int FindMatchesFromWav(const char *wavFile, const char *dbFile, Match **outMatches, size_t *outNumMatches) {
    if (!wavFile || !dbFile || !outMatches || !outNumMatches) return -1;
    *outMatches = NULL;
    *outNumMatches = 0;

    /* 1) Load DB (metadata + fingerprints) */
    size_t numSongs = 0;
    SongEntry_n *songs = load_db(dbFile, &numSongs);
    if (!songs || numSongs == 0) {
        return -2;
    }

    /* 2) We'll later query couples for addresses present in sample.
       But to mimic db.GetCouples(addresses) we'll build an index
       of all fingerprints keyed by address so lookups are O(1) per address. */
    AddrNode *addrBuckets[ADDR_BUCKETS];
    memset(addrBuckets, 0, sizeof(addrBuckets));

    for (size_t s = 0; s < numSongs; ++s) {
        for (size_t k = 0; k < songs[s].numFingerprints; ++k) {
            Fingerprint *fp = &songs[s].fingerprints[k];
            /* fp->couple contains anchorTimeMs and songID */
            addrnode_add(addrBuckets, fp->address, fp->couple);
        }
    }

    /* 3) Load query WAV -> spectrogram -> peaks -> sample fingerprints map */
    WAVHeader header;
    size_t numFrames = 0;
    double duration = 0.0;
    uint32_t sampleRate = 0;
    float *audio_f = load_wav_floats(wavFile, &header, &numFrames, &duration, &sampleRate);
    if (!audio_f) {
        free_addr_buckets(addrBuckets);
        for (size_t i = 0; i < numSongs; ++i) free(songs[i].fingerprints);
        free(songs);
        return -3;
    }

    double *audio_d = (double*)malloc(numFrames * sizeof(double));
    if (!audio_d) { free(audio_f); free_addr_buckets(addrBuckets); for (size_t i=0;i<numSongs;i++) free(songs[i].fingerprints); free(songs); return -4; }
    for (size_t i = 0; i < numFrames; ++i) audio_d[i] = (double)audio_f[i];
    free(audio_f);

    double complex *spec = NULL;
    size_t numWindows = 0;
    Spectrogram(audio_d, numFrames, (int)sampleRate, &spec, &numWindows);
    if (!spec || numWindows == 0) {
        free(audio_d);
        free_addr_buckets(addrBuckets);
        for (size_t i = 0; i < numSongs; ++i) free(songs[i].fingerprints);
        free(songs);
        return -5;
    }

    size_t maxPeaks = numWindows * MAX_BANDS;
    Peak *peaks = (Peak*)malloc(maxPeaks * sizeof(Peak));
    size_t numPeaks = 0;
    ExtractPeaks(spec, numWindows, duration, peaks, &numPeaks, maxPeaks);
    if (numPeaks == 0) {
        free(peaks); free(spec); free(audio_d);
        free_addr_buckets(addrBuckets);
        for (size_t i = 0; i < numSongs; ++i) free(songs[i].fingerprints);
        free(songs);
        return -6;
    }

    size_t maxSampleFP = numPeaks * TARGET_ZONE_SIZE;
    Fingerprint *sampleFPs = (Fingerprint*)malloc(maxSampleFP * sizeof(Fingerprint));
    size_t numSampleFP = Fingerprint_f(peaks, numPeaks, 0u, sampleFPs, maxSampleFP);
    if (numSampleFP == 0) {
        free(sampleFPs); free(peaks); free(spec); free(audio_d);
        free_addr_buckets(addrBuckets);
        for (size_t i = 0; i < numSongs; ++i) free(songs[i].fingerprints);
        free(songs);
        return -7;
    }

    /* Build sample map: address -> earliest sample anchor time.
       We'll use a small hash table similar to addrBuckets but storing sample time. */
    typedef struct SampleNode {
        uint32_t address;
        uint32_t sampleTime;
        struct SampleNode *next;
    } SampleNode;
    SampleNode *sampleBuckets[ADDR_BUCKETS];
    memset(sampleBuckets, 0, sizeof(sampleBuckets));

    for (size_t i = 0; i < numSampleFP; ++i) {
        uint32_t addr = sampleFPs[i].address;
        uint32_t anchorMs = sampleFPs[i].couple.anchorTimeMs;
        uint32_t h = hash32(addr) & (ADDR_BUCKETS - 1);
        SampleNode *n = sampleBuckets[h];
        int found = 0;
        while (n) {
            if (n->address == addr) {
                if (anchorMs < n->sampleTime) n->sampleTime = anchorMs;
                found = 1;
                break;
            }
            n = n->next;
        }
        if (!found) {
            SampleNode *nn = (SampleNode*)malloc(sizeof(SampleNode));
            nn->address = addr;
            nn->sampleTime = anchorMs;
            nn->next = sampleBuckets[h];
            sampleBuckets[h] = nn;
        }
    }

    /* 4) Create song index map for songID -> index in songs[] */
    SongIdxNode *songIdxBuckets[SONG_BUCKETS];
    memset(songIdxBuckets, 0, sizeof(songIdxBuckets));
    build_song_index_map(songs, numSongs, songIdxBuckets);

    /* 5) Build matches: for each sample address, get couples from DB and append (sampleTime, dbTime) to song's list */
    SongPairs *songPairs = (SongPairs*)calloc(numSongs, sizeof(SongPairs));
    if (!songPairs) {
        
        for (size_t i = 0; i < ADDR_BUCKETS; ++i) {
            SampleNode *n = sampleBuckets[i];
            while (n) { SampleNode *nx = n->next; free(n); n = nx; }
        }
        free(sampleFPs); free(peaks); free(spec); free(audio_d);
        free_addr_buckets(addrBuckets);
        for (size_t i = 0; i < numSongs; ++i) free(songs[i].fingerprints);
        free(songs);
        free_songidx_buckets(songIdxBuckets);
        return -8;
    }

    /* iterate sample buckets */
    for (size_t b = 0; b < ADDR_BUCKETS; ++b) {
        SampleNode *sn = sampleBuckets[b];
        while (sn) {
            AddrNode *a = addrnode_find(addrBuckets, sn->address);
            if (a) {
                for (size_t k = 0; k < a->count; ++k) {
                    Couple c = a->list[k];
                    uint32_t dbSongID = c.songID;
                    uint32_t dbTime = c.anchorTimeMs;
                    size_t idx;
                    if (!songid_to_index(songIdxBuckets, dbSongID, &idx)) continue; 
                    SongPairs *sp = &songPairs[idx];
                    if (sp->count >= sp->cap) {
                        size_t newcap = sp->cap == 0 ? 16 : sp->cap * 2;
                        sp->pairs = (TimePair*)realloc(sp->pairs, newcap * sizeof(TimePair));
                        sp->cap = newcap;
                    }
                    sp->pairs[sp->count].sampleTime = sn->sampleTime;
                    sp->pairs[sp->count].dbTime = dbTime;
                    if (sp->count == 0 || dbTime < sp->earliest_timestamp) sp->earliest_timestamp = dbTime;
                    sp->count++;
                }
            }
            sn = sn->next;
        }
    }

    /* 6) Score songs using analyzeRelativeTiming and collect matches */
    Match *matches = (Match*)malloc(numSongs * sizeof(Match));
    size_t matchCount = 0;
    for (size_t s = 0; s < numSongs; ++s) {
        SongPairs *sp = &songPairs[s];
        if (sp->count < 2) continue;
        double score = analyzeRelativeTiming(sp->pairs, sp->count);
        if (score <= 0.0) continue;
        Match m;
        m.SongID = songs[s].songID;
        strncpy(m.SongTitle, songs[s].songName, sizeof(m.SongTitle));
        m.SongTitle[sizeof(m.SongTitle)-1] = '\0';
        /* we don't have Artist/YouTubeID in meta; set empty */
        m.SongArtist[0] = '\0';
        m.YouTubeID[0] = '\0';
        m.Timestamp = sp->earliest_timestamp;
        m.Score = score;
        matches[matchCount++] = m;
    }

    /* sort by score desc */
    if (matchCount > 1) qsort(matches, matchCount, sizeof(Match), match_cmp_desc);

    /* 7) Cleanup temporary structures but keep matches to return */
    *outMatches = matches;
    *outNumMatches = matchCount;

    /* free sample bucket nodes */
    for (size_t i = 0; i < ADDR_BUCKETS; ++i) {
        SampleNode *n = sampleBuckets[i];
        while (n) { SampleNode *nx = n->next; free(n); n = nx; }
    }

    free(sampleFPs);
    free(peaks);
    free(spec);
    free(audio_d);

    free_addr_buckets(addrBuckets);
    for (size_t i = 0; i < numSongs; ++i) free(songs[i].fingerprints);
    free(songs);

    for (size_t i = 0; i < numSongs; ++i) {
        free(songPairs[i].pairs);
    }
    free(songPairs);

    free_songidx_buckets(songIdxBuckets);

    return 0;
}
