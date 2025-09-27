
# Echo: A Shazam Algorithm Recreation in Pure C

Echo is an open-source project that attempts to recreate the core functionality of Shazam's music recognition algorithm using pure C. The goal is to provide a lightweight, efficient, and educational implementation of audio fingerprinting techniques.

## Features

- **Audio Fingerprinting**: Implements techniques to generate unique fingerprints for audio signals.
- **Spectrogram Analysis**: Analyzes audio signals to create spectrograms for comparison.
- **Matching Algorithm**: Compares generated fingerprints to identify audio matches.
- **Cross-Platform**: Designed to be portable and efficient across different platforms.

## Project Structure

The repository includes the following key components:

- `main.c`: The entry point of the application.
- `fft.c` / `fft.h`: Functions for Fast Fourier Transform (FFT) operations.
- `fingerprint.c` / `fingerprint.h`: Functions for generating and handling audio fingerprints.
- `match.c`: Contains the matching algorithm for comparing fingerprints.
- `matcher.c` / `matcher.h`: Utilities for managing and processing matches.
- `music.c` / `music.h`: Functions related to audio file handling and processing.
- `spectrogram.c` / `spectrogram.h`: Functions for generating and analyzing spectrograms.
- `spectrogram_image.c` / `spectrogram_image.h`: Utilities for visualizing spectrograms.

## Getting Started

### Prerequisites

- A C compiler (e.g., GCC)
- Make build system

### Building the Project

1. Clone the repository:

   ```bash
   git clone https://github.com/NepMods/echo.git
   cd echo
   ```

2. Build the project using Make:

   ```bash
   make
   ```

3. Run the application:
   # all song passed must be un .wav file.

   ```bash
   ./add_to_db
    Usage: ./add_to_db <db_file> <song_file1>

   ./match
   Usage: ./match <db_file> <snippet_audio_file>
   ```

### Usage

To use Echo, provide an audio file as input. The application will process the audio, generate fingerprints, and attempt to match them against a database of known fingerprints.

```bash
./echo input_audio.wav
```

## Contributing

Contributions are welcome! If you have suggestions, improvements, or bug fixes, please fork the repository and submit a pull request. Ensure that your code adheres to the existing coding style and includes appropriate tests.

## License

Echo is licensed under the [MIT License](LICENSE).

##

Exaple Output:


```
./add_to_db test.db data/noWay.wav noWAy
ℂ -gcc  21:10 add_song_to_db: frames=4605216 sampleRate=44100 duration=104.427 channels=2 S
pectrogram result: windows=1160 spec_ptr=0x7f408d998010
 ExtractPeaks result: numPeaks=2553
Saved 12750 fingerprints for 'noWAy' (songID=2029970567)
Added 'data/noWay.wav' to database as 'noWAy'

```

```
./match test.db data/song2_snippet_3.wav
ℂ -gcc  21:11 Identifying 'data/song2_snippet_3.wav' using database 'test.db'... Found 3 matches:
 1) songID=2357663286 title='song2' timestamp=40058 score=289
2) songID=2029970567 title='noWAy' timestamp=9723 score=67
3) songID=2046404001 title='song_1' timestamp=142360 score=1
```
