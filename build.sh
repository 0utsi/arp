#!/bin/bash

# Build script for MIDI Arpeggiator
# Usage: ./build.sh [clean]

set -e  # Exit on any error

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "🎵 Building MIDI Arpeggiator..."

# Check if clean build requested
if [ "$1" = "clean" ]; then
    echo "🧹 Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "⚙️  Configuring with CMake..."
cmake ..

# Build
echo "🔨 Building..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "✅ Build complete!"
echo "🚀 Run with: $BUILD_DIR/midi_arp"
