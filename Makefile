
CC = gcc

CFLAGS = -Wall -O2

LIBS = -lm
COMMON_SRCS = music.c fft.c spectogram.c spectrogram_image.c fingerprint.c

all: match add_to_db

match: $(COMMON_SRCS) matcher.c match.c
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@
add_to_db: $(COMMON_SRCS) main.c
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f match add_to_db *.o
