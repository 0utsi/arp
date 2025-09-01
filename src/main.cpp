#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>
#include <iostream>
#include "ports/Clock.hpp"
#include "ports/Midi.hpp"
#include "desktop/DesktopMidi.hpp"
#include "core/PatternEngine.hpp"
#include "core/PatternBuilder.hpp"
#include "ui/Cli.hpp"

class DesktopClock final : public ports::IClock {
public:
  uint64_t now_ms() const override {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return duration_cast<milliseconds>(steady_clock::now() - t0).count();
  }
};

static std::atomic<bool> g_running{true};
void handle_sigint(int){ g_running.store(false); }

int main() {
  std::signal(SIGINT, handle_sigint);
  DesktopClock clock;

  auto midiIn  = desktop_midi::makeIn(clock);
  auto midiOut = desktop_midi::makeOut();

  core::PatternEngine eng(*midiOut, clock);

  // Global config
  core::EngineConfig ec;
  ec.bpm = 122.0;
  ec.overlap_ms = 12;
  eng.set_engine_config(ec);

  // Pattern 0: [1,3,2], ósemki, delikatnie dłuższy gate
auto& p0 = eng.pattern(0);
p0.channel  = 1;
p0.division = 2;
core::PatternBuilder b0(p0);
b0.clear()
  .indices({1,3,2})
  .each().gate(70).vel(100).oct(0).prob(100).on().done();

// Pattern 1: trzy kroki na +1 oktawie, szesnastki
auto& p1 = eng.pattern(1);
p1.channel  = 2;
p1.division = 4;
core::PatternBuilder b1(p1);
b1.clear()
  .indices({1,2,3})
  .each().gate(50).vel(90).oct(+1).on().done();

// Pattern 2: REST w środku, akcent na koniec
auto& p2 = eng.pattern(2);
p2.channel = 1;
p2.division= 2;
core::PatternBuilder b2(p2);
b2.clear()
  .indices({1,0,2,3})                 // 0 = REST
  .step().idx(3).vel(120).gate(80);   // dopracuj ostatni krok

  // CLI
  ui::CommandQueue cq;
  auto cli_thread = ui::start_cli(g_running, cq);
  std::cout << "Ready. Type 'help'.\n";

  using clock_t = std::chrono::steady_clock;
  auto next = clock_t::now();

  while (g_running.load()) {
    // MIDI IN
    while (auto m = midiIn->poll()) eng.on_midi_in(*m);

    // Komendy z CLI (aplikuj TYLKO tutaj, w wątku głównym)
    for (auto& cmd : cq.drain()) {
      using T = ui::Command::Type;
      switch (cmd.type) {
        case T::Help: ui::print_help(); break;
        case T::Show: {
          if (cmd.a >= 0 && cmd.a < (int)core::PatternEngine::NUM_PATTERNS) {
            ui::print_pattern(eng.pattern((std::size_t)cmd.a), cmd.a);
          } else {
            for (int i = 0; i < (int)core::PatternEngine::NUM_PATTERNS; ++i)
              ui::print_pattern(eng.pattern((std::size_t)i), i);
          }
        } break;
        case T::SetBpm: {
          ec.bpm = (cmd.a > 0 ? cmd.a : (int)ec.bpm);
          eng.set_engine_config(ec);
          std::cout << "BPM = " << ec.bpm << "\n";
        } break;
        case T::SetPatDiv: {
          int pat = cmd.a, div = cmd.b;
          if (pat>=0 && pat<(int)core::PatternEngine::NUM_PATTERNS && div>0) {
            eng.pattern((std::size_t)pat).division = (uint16_t)div;
            std::cout << "pat " << pat << " division = " << div << "\n";
          }
        } break;
        case T::SetPatLen: {
          int pat = cmd.a, len = cmd.b;
          if (pat>=0 && pat<(int)core::PatternEngine::NUM_PATTERNS) {
            auto& p = eng.pattern((std::size_t)pat);
            p.length = (std::size_t)std::clamp(len, 0, (int)core::MAX_STEPS);
            std::cout << "pat " << pat << " length = " << p.length << "\n";
          }
        } break;
        case T::SetStepIdx: {
          int pat=cmd.a, st=cmd.b, v=cmd.c;
          if (pat>=0 && pat<(int)core::PatternEngine::NUM_PATTERNS) {
            auto& p = eng.pattern((std::size_t)pat);
            if (st>=0 && st<(int)p.length) { p.steps[(std::size_t)st].note_index = (uint8_t)std::clamp(v,0,8); }
          }
        } break;
        case T::SetStepVel: {
          int pat=cmd.a, st=cmd.b, v=cmd.c;
          if (pat>=0 && pat<(int)core::PatternEngine::NUM_PATTERNS) {
            auto& p = eng.pattern((std::size_t)pat);
            if (st>=0 && st<(int)p.length) { p.steps[(std::size_t)st].velocity = (uint8_t)std::clamp(v,1,127); }
          }
        } break;
        case T::SetStepGate: {
          int pat=cmd.a, st=cmd.b, v=cmd.c;
          if (pat>=0 && pat<(int)core::PatternEngine::NUM_PATTERNS) {
            auto& p = eng.pattern((std::size_t)pat);
            if (st>=0 && st<(int)p.length) { p.steps[(std::size_t)st].gate_pct = (uint8_t)std::clamp(v,1,200); }
          }
        } break;
        case T::SetStepOct: {
          int pat=cmd.a, st=cmd.b, v=cmd.c;
          if (pat>=0 && pat<(int)core::PatternEngine::NUM_PATTERNS) {
            auto& p = eng.pattern((std::size_t)pat);
            if (st>=0 && st<(int)p.length) { p.steps[(std::size_t)st].octave = (int8_t)std::clamp(v,-8,8); }
          }
        } break;
        case T::SetStepProb: {
          int pat=cmd.a, st=cmd.b, v=cmd.c;
          if (pat>=0 && pat<(int)core::PatternEngine::NUM_PATTERNS) {
            auto& p = eng.pattern((std::size_t)pat);
            if (st>=0 && st<(int)p.length) { p.steps[(std::size_t)st].probability = (uint8_t)std::clamp(v,0,100); }
          }
        } break;
        case T::ToggleStep: {
          int pat=cmd.a, st=cmd.b, on=cmd.c;
          if (pat>=0 && pat<(int)core::PatternEngine::NUM_PATTERNS) {
            auto& p = eng.pattern((std::size_t)pat);
            if (st>=0 && st<(int)p.length) { p.steps[(std::size_t)st].enabled = (on!=0); }
          }
        } break;
        case T::Quit:
          g_running.store(false);
          break;
      }
    }

    // Granie / czas
    eng.tick();

    // Równy tick na PC
    next += std::chrono::milliseconds(1);
    std::this_thread::sleep_until(next);
  }

  if (cli_thread.joinable()) cli_thread.join();
  std::cout << "Bye\n";
  return 0;
}
