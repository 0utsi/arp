#if __has_include(<rtmidi/RtMidi.h>)
  #include <rtmidi/RtMidi.h>   // Homebrew (macOS)
#elif __has_include(<RtMidi.h>)
  #include <RtMidi.h>
#else
  #error "RtMidi header not found. Install rtmidi."
#endif

#include <memory>
#include <vector>
#include <iostream>
#include <optional>
#include <stdexcept>

#include "ports/Midi.hpp"
#include "ports/Clock.hpp"

// Wypisz porty i wybierz pierwszy
static unsigned autoSelectPort(RtMidi* dev, const char* label, const std::string& preferName) {
  unsigned n = dev->getPortCount();
  if (n == 0) {
    std::cerr << "[MIDI] Brak portów " << label << "\n";
    throw std::runtime_error("No MIDI ports found");
  }
  std::cerr << "[MIDI] Dostępne porty " << label << ":\n";
  for (unsigned i = 0; i < n; ++i) {
    std::string name = dev->getPortName(i);
    std::cerr << "  [" << i << "] " << name << "\n";
    if (name.find(preferName) != std::string::npos) {
      std::cerr << "[MIDI] Wybieram port " << i << " (" << name << ")\n";
      return i;
    }
  }
  std::cerr << "[MIDI] Domyślnie otwieram port 0\n";
  return 0; // fallback
}

class DesktopMidiIn final : public ports::IMidiIn {
public:
  explicit DesktopMidiIn(const ports::IClock& clock)
    : clock_(clock), in_(std::make_unique<RtMidiIn>()) {
    in_->ignoreTypes(false, false, false);
    auto idx = autoSelectPort(in_.get(), "IN", "MPKmini2");
    in_->openPort(idx);
  }

  std::optional<ports::MidiMsg> poll() override {
    std::vector<unsigned char> msg;
    (void)in_->getMessage(&msg);       // non-blocking; msg=[] jeśli nic nie ma
    if (msg.empty()) return std::nullopt;

    ports::MidiMsg m{};
    if (msg.size() >= 1) m.status = msg[0];
    if (msg.size() >= 2) m.data1  = msg[1];
    if (msg.size() >= 3) m.data2  = msg[2];
    m.t_ms = clock_.now_ms();
    return m;
  }

private:
  const ports::IClock& clock_;
  std::unique_ptr<RtMidiIn> in_;
};

class DesktopMidiOut final : public ports::IMidiOut {
public:
  DesktopMidiOut() : out_(std::make_unique<RtMidiOut>()) {
    auto idx = autoSelectPort(out_.get(), "OUT", "IAC");
    out_->openPort(idx);
  }

  void send(const ports::MidiMsg& m) override {
    std::vector<unsigned char> v{ m.status, m.data1, m.data2 };
    out_->sendMessage(&v);             // lub: unsigned char b[3]{...}; out_->sendMessage(b,3);
    std::cout << ( (m.status & 0xF0) == 0x90 ? "[OUT ON ] " : "[OUT OFF] " )
          << "note=" << (int)m.data1 << " t=" << m.t_ms << "\n";

  }
private:
  std::unique_ptr<RtMidiOut> out_;
};

// Fabryki (jedyna definicja)
namespace desktop_midi {
  std::unique_ptr<ports::IMidiIn>  makeIn (const ports::IClock& clk) { return std::make_unique<DesktopMidiIn>(clk); }
  std::unique_ptr<ports::IMidiOut> makeOut()                         { return std::make_unique<DesktopMidiOut>(); }
}
