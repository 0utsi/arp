#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// Konstruktor: stereo IN/OUT (stabilniej dla hostów)
MidiArpAudioProcessor::MidiArpAudioProcessor()
: juce::AudioProcessor(
    BusesProperties()
      .withInput ("In",  juce::AudioChannelSet::stereo(), true)
      .withOutput("Out", juce::AudioChannelSet::stereo(), true)
  ),
  engine_(midiOut_, clock_)
{
  // Domyślna konfiguracja silnika
  engCfg_.bpm        = 120.0;
  engCfg_.overlap_ms = 12;
  engine_.set_engine_config(engCfg_);

  // Domyślny pattern 0: [1,3,2] na 1/8
  auto& p0 = engine_.pattern(0);
  p0.channel  = 1;
  p0.division = 2; // 1/8
  p0.length   = 3;
  p0.steps[0] = core::Step{ .note_index=1, .velocity=100, .gate_pct=70, .octave=0, .enabled=true, .probability=100 };
  p0.steps[1] = core::Step{ .note_index=3, .velocity=108, .gate_pct=70, .octave=0, .enabled=true, .probability=100 };
  p0.steps[2] = core::Step{ .note_index=2, .velocity=96,  .gate_pct=70, .octave=0, .enabled=true, .probability=100 };
}

void MidiArpAudioProcessor::prepareToPlay (double sr, int /*samplesPerBlock*/)
{
  sampleRate_ = sr;
  clock_.set_ms(0);
  msFracAcc_ = 0.0;
}

void MidiArpAudioProcessor::processBlock (juce::AudioBuffer<float>& audio,
                                          juce::MidiBuffer& midi)
{
  juce::ScopedNoDenormals noDenormals;
  audio.clear(); // MIDI effect – wyzeruj audio

  // 1) tempo z hosta (nowe PlayHead API)
  if (auto* ph = getPlayHead())
    if (auto pos = ph->getPosition())
      if (pos->getBpm())
      {
        const double hostBpm = *pos->getBpm();
        if (hostBpm > 0.0 && hostBpm != engCfg_.bpm)
        {
          engCfg_.bpm = hostBpm;
          engine_.set_engine_config(engCfg_);
        }
      }

  // 2) przygotuj wyjście na ten blok
  const int      numSamples   = audio.getNumSamples();
  const double   blockMs      = 1000.0 * (double)numSamples / sampleRate_;
  const uint64_t blockStartMs = clock_.now_ms();

  // użyj 'midi' jako bufora wyjściowego – najpierw przerzuć wejście
  juce::MidiBuffer input;
  input.swapWith(midi); // 'midi' teraz puste (będzie OUT)
  midi.clear();

  midiOut_.beginBlock(midi, sampleRate_, blockStartMs);

  // 3) wejście MIDI -> engine.on_midi_in z timestampem w ms
  for (const auto meta : input)
  {
    const auto& msg       = meta.getMessage();
    const int   samplePos = meta.samplePosition;

    const uint64_t t_ms = blockStartMs
                        + (uint64_t) std::llround(1000.0 * (double)samplePos / sampleRate_);

    if (msg.isNoteOn() || msg.isNoteOff())
    {
      ports::MidiMsg mm{};
      const auto* raw = msg.getRawData();
      mm.status = raw[0];
      mm.data1  = raw[1];
      mm.data2  = msg.isNoteOn() ? raw[2] : 0;
      mm.t_ms   = t_ms;
      engine_.on_midi_in(mm);
    }
  }

  // 4) „pseudotik” – 1 ms tick w ramach bloku
  double toDo = msFracAcc_ + blockMs;
  int    steps = (int) std::floor(toDo);
  msFracAcc_   = toDo - (double)steps;

  for (int i = 0; i < steps; ++i)
  {
    engine_.tick();
    clock_.advance_ms(1.0);
  }

  midiOut_.endBlock();
}

// GUI
juce::AudioProcessorEditor* MidiArpAudioProcessor::createEditor()
{
  return new MidiArpAudioProcessorEditor(*this);
}

// Public setter
void MidiArpAudioProcessor::setBpm(double bpm)
{
  if (bpm <= 0) return;
  engCfg_.bpm = bpm;
  engine_.set_engine_config(engCfg_);
}

// --- FABRYKA WTYCZKI ---
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new MidiArpAudioProcessor();
}
