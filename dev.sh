#!/bin/bash

# Development script for MIDI Arpeggiator
# Usage: ./dev.sh [clean|run|build]

set -e  # Exit on any error

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
EXECUTABLE="$BUILD_DIR/midi_arp"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found. Please install CMake."
        exit 1
    fi
    
    # Check for pkg-config
    if ! command -v pkg-config &> /dev/null; then
        print_error "pkg-config not found. Please install pkg-config."
        exit 1
    fi
    
    # Check for RtMidi
    if ! pkg-config --exists rtmidi; then
        print_error "RtMidi not found. Please install RtMidi (brew install rtmidi)."
        exit 1
    fi
    
    print_success "All dependencies found!"
}

# Function to clean build
clean_build() {
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    print_success "Build directory cleaned!"
}

# Function to build
build() {
    print_status "Building MIDI Arpeggiator..."
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake
    print_status "Configuring with CMake..."
    cmake ..
    
    # Build
    print_status "Building..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    print_success "Build complete!"
}

# Function to run
run() {
    if [ ! -f "$EXECUTABLE" ]; then
        print_warning "Executable not found. Building first..."
        build
    fi
    
    print_status "Starting MIDI Arpeggiator..."
    echo ""
    "$EXECUTABLE"
}

# Function to show help
show_help() {
    echo "🎵 MIDI Arpeggiator Development Script"
    echo ""
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  clean    - Clean build directory"
    echo "  build    - Build the project"
    echo "  run      - Run the application"
    echo "  help     - Show this help message"
    echo ""
    echo "If no command is provided, the script will:"
    echo "  1. Check dependencies"
    echo "  2. Build the project"
    echo "  3. Run the application"
}

# Main script logic
case "${1:-}" in
    "clean")
        clean_build
        ;;
    "build")
        check_dependencies
        build
        ;;
    "run")
        run
        ;;
    "help"|"-h"|"--help")
        show_help
        ;;
    "")
        check_dependencies
        build
        run
        ;;
    *)
        print_error "Unknown command: $1"
        show_help
        exit 1
        ;;
esac
