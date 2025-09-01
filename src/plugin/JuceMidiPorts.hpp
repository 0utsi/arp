#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ports/Midi.hpp"

// Adapter wyjścia MIDI: zamienia ports::MidiMsg (status,d1,d2,t_ms)
// na eventy w juce::MidiBuffer z poprawnym sample offsetem.
class JuceMidiOut final : public ports::IMidiOut
{
public:
  JuceMidiOut() = default;

  // Wywołuj na początku processBlock:
  //  - przekazujemy referencję do bufora wyjściowego JUCE,
  //  - bieżący sampleRate i start czasu bloku w ms.
  void beginBlock(juce::MidiBuffer& outBuf, double sampleRate, uint64_t blockStartMs)
  {
    out_          = &outBuf;
    sampleRate_   = sampleRate;
    blockStartMs_ = blockStartMs;
  }

  // Wywołuj na końcu processBlock (tu nic nie robimy, ale zostawiamy hook)
  void endBlock() { out_ = nullptr; }

  // Implementacja IMidiOut: zagraj pojedynczą wiadomość
  void send(const ports::MidiMsg& m) override
  {
    if (!out_) return;

    // przelicz ms → próbki (pozycja eventu w ramach bieżącego bloku)
    const double dtMs = (m.t_ms > blockStartMs_) ? double(m.t_ms - blockStartMs_) : 0.0;
    int samplePos = (int) std::llround( dtMs * sampleRate_ / 1000.0 );
    if (samplePos < 0) samplePos = 0;

    // zbuduj juce::MidiMessage z trzech bajtów
    const uint8_t raw[3] { m.status, m.data1, m.data2 };
    out_->addEvent( raw, 3, samplePos );
  }

private:
  juce::MidiBuffer* out_ { nullptr };
  double   sampleRate_   { 44100.0 };
  uint64_t blockStartMs_ { 0 };
};
