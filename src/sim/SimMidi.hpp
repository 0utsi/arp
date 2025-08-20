#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

#include <iostream>
#include "../ports/Midi.hpp"

// Prosty, bezpieczny wątkowo bufor FIFO. Krótki i wystarczający na start.
class TsQueue {
public:
  void push(const ports::MidiMsg& m) {
    std::lock_guard<std::mutex> lk(m_);
    q_.push(m);
    cv_.notify_one();
  }
  std::optional<ports::MidiMsg> try_pop() {
    std::lock_guard<std::mutex> lk(m_);
    if (q_.empty()) return std::nullopt;
    auto v = q_.front();
    q_.pop();
    return v;
  }
private:
  std::mutex m_;
  std::queue<ports::MidiMsg> q_;
  std::condition_variable cv_; // (na przyszłość: gdybyśmy chcieli wait)
};

// Symulowane wejście MIDI — czyta z kolejki (wypełnianej przez wątek-producenta).
class SimMidiIn final : public ports::IMidiIn {
public:
  explicit SimMidiIn(TsQueue& q): q_(q) {}
  std::optional<ports::MidiMsg> poll() override {
    return q_.try_pop(); // non-blocking
  }
private:
  TsQueue& q_;
};

// Symulowane wyjście MIDI — loguje do konsoli.
class SimMidiOut final : public ports::IMidiOut {
public:
  void send(const ports::MidiMsg& m) override {
    // Uwaga: to tylko debug. Docelowo tu będzie RtMidi/USB/UART.
    std::cout << "[MIDI OUT] status=0x" << std::hex << (int)m.status << std::dec
              << " d1=" << (int)m.data1 << " d2=" << (int)m.data2
              << " t_ms=" << m.t_ms << "\n";
  }
};
