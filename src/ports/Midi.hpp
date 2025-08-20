#pragma once
#include <cstdint>
#include <optional>

namespace ports {

// Minimalna reprezentacja komunikatu MIDI (3 bajty + timestamp w ms).
// status: 0x8x = Note Off, 0x9x = Note On (x = kanał-1), itd.
struct MidiMsg {
  uint8_t status{0};
  uint8_t data1{0};   // np. numer nuty
  uint8_t data2{0};   // np. velocity
  uint64_t t_ms{0};   // timestamp w ms (od IClock)
};

// Wejście MIDI: non-blocking poll() — zwraca wiadomość albo std::nullopt
struct IMidiIn {
  virtual ~IMidiIn() = default;
  virtual std::optional<MidiMsg> poll() = 0;
};

// Wyjście MIDI: send() — wysyła jeden komunikat
struct IMidiOut {
  virtual ~IMidiOut() = default;
  virtual void send(const MidiMsg& msg) = 0;
};

} // namespace ports
