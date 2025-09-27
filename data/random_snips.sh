#!/bin/bash
# Usage: ./random_snips.sh input.wav

INPUT="$1"
NUM_SNIPS=4
SNIP_DURATION=10

# Extract base name without extension
BASE_NAME=$(basename "$INPUT" .wav)

# Get total duration of the input file in seconds
DURATION=$(ffprobe -i "$INPUT" -show_entries format=duration -v quiet -of csv="p=0")

for i in $(seq 1 $NUM_SNIPS); do
    # Pick a random start time
    START=$(awk -v d="$DURATION" -v t="$SNIP_DURATION" 'BEGIN{srand(); printf "%.2f\n", rand()*(d-t)}')
    
    # Extract snippet
    ffmpeg -ss $START -t $SNIP_DURATION -i "$INPUT" -c copy "${BASE_NAME}_snippet_$i.wav"
done

echo "Generated $NUM_SNIPS random snippets from $INPUT"
