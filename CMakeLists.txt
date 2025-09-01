cmake_minimum_required(VERSION 3.22)

# UWAGA: JUCE potrzebuje C, C++, ObjC, ObjC++
project(midi-arp VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_DESKTOP_APP "Build RtMidi desktop app" ON)
option(BUILD_VST3_PLUGIN "Build JUCE VST3 plugin" ON)

# ---- core (header-only) ----
add_library(midi_arp_core INTERFACE)
target_include_directories(midi_arp_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---- JUCE ----
add_subdirectory(external/JUCE)

# ---- Desktop EXE (RtMidi) ----
if(BUILD_DESKTOP_APP)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(RTMIDI REQUIRED rtmidi)

  add_executable(midi_arp
    src/main.cpp
    src/desktop/DesktopMidi.cpp
  )
  target_include_directories(midi_arp PRIVATE
    ${RTMIDI_INCLUDE_DIRS}
    src
  )
  target_link_directories(midi_arp PRIVATE ${RTMIDI_LIBRARY_DIRS})
  target_link_libraries(midi_arp PRIVATE ${RTMIDI_LIBRARIES} midi_arp_core)
  target_compile_definitions(midi_arp PRIVATE __MACOSX_CORE__) # macOS
endif()

# ---- VST3 plugin (JUCE) ----
if(BUILD_VST3_PLUGIN)
juce_add_plugin(midi_arp_plugin
    COMPANY_NAME            "YourCompany"
    PLUGIN_NAME             "MidiArpigi"             # <- NAZWA PLUGINA
    PRODUCT_NAME            "MidiArpigi"             # <- nazwa bundla na dysku
    BUNDLE_ID               com.yourcompany.midiarpigi# <- unikalny identyfikator
    IS_SYNTH                FALSE
    IS_MIDI_EFFECT          TRUE
    NEEDS_MIDI_INPUT        TRUE
    PRODUCES_MIDI_OUTPUT    TRUE
    FORMATS                 VST3
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_AUTO_MANIFEST      FALSE
)

juce_generate_juce_header(midi_arp_plugin)
target_compile_definitions(midi_arp_plugin PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)


  juce_generate_juce_header(midi_arp_plugin)

  target_sources(midi_arp_plugin PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginProcessor.h
    src/plugin/PluginEditor.cpp
    src/plugin/PluginEditor.h
    src/plugin/HostClock.hpp
    src/plugin/JuceMidiPorts.hpp
  )

  target_include_directories(midi_arp_plugin PRIVATE src)

  target_link_libraries(midi_arp_plugin
    PRIVATE
      midi_arp_core
      juce::juce_audio_utils
      juce::juce_audio_processors
      juce::juce_gui_extra
      juce::juce_gui_basics
      juce::juce_graphics
      juce::juce_core
  )

  juce_generate_juce_header(midi_arp_plugin)

  # >>> KLUCZOWE: wyłączamy mechanizm „VST3 może zastąpić VST2”
  target_compile_definitions(midi_arp_plugin PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
endif()
