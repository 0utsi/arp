#!/bin/bash

# Run script for MIDI Arpeggiator
# Usage: ./run.sh

set -e  # Exit on any error

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
EXECUTABLE="$BUILD_DIR/midi_arp"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ Executable not found. Building first..."
    "$PROJECT_DIR/build.sh"
fi

echo "🎵 Starting MIDI Arpeggiator..."
echo "📁 Working directory: $PROJECT_DIR"
echo ""

# Run the application
"$EXECUTABLE"
