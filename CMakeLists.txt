cmake_minimum_required(VERSION 3.25)
project(midi-arp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(midi_arp
  src/main.cpp
  src/desktop/DesktopMidi.cpp
)

target_include_directories(midi_arp PUBLIC src)

# Jeśli używasz pkg-config (Homebrew):
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTMIDI REQUIRED rtmidi)
target_include_directories(midi_arp PRIVATE ${RTMIDI_INCLUDE_DIRS})
target_link_directories(midi_arp PRIVATE ${RTMIDI_LIBRARY_DIRS})
target_link_libraries(midi_arp PRIVATE ${RTMIDI_LIBRARIES})
target_compile_options(midi_arp PRIVATE -Wall -Wextra -Wpedantic ${RTMIDI_CFLAGS_OTHER})
