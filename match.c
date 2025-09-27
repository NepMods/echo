#include "matcher.h"
#include "music.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <db_file> <snippet_file>\n", argv[0]);
        return 1;
    }

    const char *dbFile = argv[1];
    const char *snippetFile = argv[2];

    printf("Identifying '%s' using database '%s'...\n", snippetFile, dbFile);

    Match *matches = NULL;
    size_t numMatches = 0;

    int rc = FindMatchesFromWav(snippetFile, dbFile, &matches, &numMatches);
    if (rc != 0) {
        fprintf(stderr, "FindMatchesFromWav failed (code %d)\n", rc);
        return 3;
    }

    if (numMatches == 0) {
        printf("No matches found.\n");
    } else {
        printf("Found %zu matches:\n", numMatches);
        for (size_t i = 0; i < numMatches; ++i) {
            printf("%2zu) songID=%u title='%s' timestamp=%u score=%.0f\n",
                   i + 1,
                   matches[i].SongID,
                   matches[i].SongTitle,
                   matches[i].Timestamp,
                   matches[i].Score);
        }
    }

    free(matches); 
    return 0;
}
