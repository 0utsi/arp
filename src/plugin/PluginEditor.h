#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Minimalny edytor: slider BPM (host override), label, prosty layout.
class MidiArpAudioProcessorEditor : public juce::AudioProcessorEditor
                                  , private juce::Slider::Listener
{
public:
  explicit MidiArpAudioProcessorEditor (MidiArpAudioProcessor& p)
  : juce::AudioProcessorEditor (&p), proc_(p)
  {
    // BPM slider
    bpmSlider_.setRange(20.0, 300.0, 0.1);
    bpmSlider_.setTextValueSuffix(" BPM");
    bpmSlider_.setSkewFactorFromMidPoint(120.0);
    bpmSlider_.setValue(120.0);
    bpmSlider_.addListener(this);
    addAndMakeVisible(bpmSlider_);

    // etykieta
    title_.setText("midi-arp (VST3)", juce::dontSendNotification);
    title_.setJustificationType(juce::Justification::centredTop);
    addAndMakeVisible(title_);

    setSize(360, 140);
  }

  ~MidiArpAudioProcessorEditor() override = default;

  void paint (juce::Graphics& g) override
  {
    g.fillAll( juce::Colours::black );
    g.setColour( juce::Colours::white );
    g.drawFittedText ("midi-arp (pattern [1,3,2])", getLocalBounds().withTrimmedTop(8),
                      juce::Justification::centredTop, 1);
  }

  void resized() override
  {
    auto r = getLocalBounds().reduced(16);
    title_.setBounds(r.removeFromTop(24));
    r.removeFromTop(8);
    bpmSlider_.setBounds(r.removeFromTop(28));
  }

private:
  void sliderValueChanged(juce::Slider* s) override
  {
    if (s == &bpmSlider_) {
      proc_.setBpm( (double) bpmSlider_.getValue() );
    }
  }

  MidiArpAudioProcessor& proc_;
  juce::Label  title_;
  juce::Slider bpmSlider_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiArpAudioProcessorEditor)
};
