#ifndef SHAZAM_MATCH_H
#define SHAZAM_MATCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t SongID;
    char SongTitle[128];
    char SongArtist[128];  
    char YouTubeID[128];   
    uint32_t Timestamp;    
    double Score;
} Match;

/*
 * FindMatchesFromWav
 * - wavFile: path to query wav file
 * - dbFile: path to fingerprint DB (metadata in dbFile+".meta")
 * - outMatches: pointer set to malloc'd Match array (caller must free)
 * - outNumMatches: number of returned matches
 * Returns 0 on success, non-zero on error.
 */
int FindMatchesFromWav(const char *wavFile, const char *dbFile, Match **outMatches, size_t *outNumMatches);

#ifdef __cplusplus
}
#endif

#endif 
