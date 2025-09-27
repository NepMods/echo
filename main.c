#include "music.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 4 || (argc - 2) % 2 != 0) {
        printf("Usage: %s <db_file> <song_file1> <song_name1> [<song_file2> <song_name2> ...]\n", argv[0]);
        return 1;
    }

    const char *dbFile = argv[1];

    for (int i = 2; i < argc; i += 2) {
        const char *songPath = argv[i];
        const char *songName = argv[i + 1];

        add_song_to_db(songPath, songName, dbFile);
        printf("Added '%s' to database as '%s'\n", songPath, songName);
    }

    return 0;
}
