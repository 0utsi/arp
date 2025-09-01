#pragma once
#include <juce_audio_processors/juce_audio_processors.h>


// nasze core
#include "core/PatternEngine.hpp"
// adaptery JUCE <-> nasze porty
#include "plugin/JuceMidiPorts.hpp"
#include "plugin/HostClock.hpp"

//==============================================================================
class MidiArpAudioProcessor : public juce::AudioProcessor
{
public:
    MidiArpAudioProcessor();
    ~MidiArpAudioProcessor() override = default;

    // --- Życie audio ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        juce::ignoreUnused (layouts);
        return true;
    }

    // Główna pętla przetwarzania (MIDI)
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    

    // --- GUI ---
    // Używamy klasycznej sygnatury (pasuje do Twojej wersji JUCE):
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // --- Info o pluginie ---
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    // --- Programy (nieużywane) ---
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    // --- State (presety) ---
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // Public: prosty setter BPM z GUI
    void setBpm(double bpm);

    // Dostęp do silnika
    core::PatternEngine& engine() { return engine_; }

private:
    // Zegar hosta (ms)
    HostClock clock_;

    // Wyjście MIDI
    JuceMidiOut midiOut_;

    // Silnik patternów
    core::PatternEngine engine_;
    core::EngineConfig  engCfg_{};

    // Stan audio
    double   sampleRate_ { 44100.0 };
    double   msFracAcc_  { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiArpAudioProcessor)
};
