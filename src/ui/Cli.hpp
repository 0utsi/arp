#pragma once
#include <atomic>
#include <deque>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include "core/PatternEngine.hpp"  // do print_pattern & stałych

namespace ui {

// Jednolity typ komendy dla CLI → głównego wątku
struct Command {
  enum class Type {
    Help, Show, SetBpm,
    SetPatDiv, SetPatLen,
    SetStepIdx, SetStepVel, SetStepGate, SetStepOct, SetStepProb,
    ToggleStep,
    Quit
  } type{Type::Help};

  // Proste pola parametryczne – używamy w switchu
  int a{0}, b{0}, c{0};
};

// Minimalna, bezpieczna kolejka (CLI → main). Nie jest w hot-path.
class CommandQueue {
public:
  void push(const Command& cmd) {
    std::lock_guard<std::mutex> lg(mu_);
    q_.push_back(cmd);
  }
  // Zbierz wszystko, co jest, bez blokowania
  std::deque<Command> drain() {
    std::lock_guard<std::mutex> lg(mu_);
    std::deque<Command> out;
    out.swap(q_);
    return out;
  }
private:
  std::mutex mu_;
  std::deque<Command> q_;
};

// Pomoc: wypisz help
inline void print_help() {
  std::cout <<
    "Commands:\n"
    "  help                        - show this help\n"
    "  show [pat]                  - show pattern (0..3), or all if omitted\n"
    "  bpm <value>                 - set global BPM\n"
    "  div <pat> <division>        - set pattern division (1=1/4,2=1/8,4=1/16,...)\n"
    "  len <pat> <length>          - set pattern length (0.." << core::MAX_STEPS << ")\n"
    "  idx <pat> <step> <0..8>     - set step's note index (0=REST)\n"
    "  vel <pat> <step> <1..127>   - set velocity\n"
    "  gate <pat> <step> <1..200>  - set gate percent\n"
    "  oct <pat> <step> <-8..+8>   - set octave transpose\n"
    "  prob <pat> <step> <0..100>  - set probability\n"
    "  on <pat> <step>             - enable step\n"
    "  off <pat> <step>            - disable step\n"
    "  quit                        - exit\n";
}

// Pomoc: wypisz pattern w czytelnej formie
inline void print_pattern(const core::PatternConfig& p, int idx) {
  std::cout << "Pattern " << idx
            << " | ch=" << (int)p.channel
            << " div=" << p.division
            << " len=" << p.length << "\n";
  for (std::size_t i = 0; i < p.length; ++i) {
    const auto& s = p.steps[i];
    std::cout << "  [" << i << "] "
              << (s.enabled ? "on " : "off")
              << " idx=" << (int)s.note_index
              << " vel=" << (int)s.velocity
              << " gate=" << (int)s.gate_pct
              << " oct=" << (int)s.octave
              << " prob="<< (int)s.probability
              << "\n";
  }
}

// Wątek CLI – czyta stdin, zamienia na Command i wkłada do kolejki
inline std::thread start_cli(std::atomic<bool>& running, CommandQueue& cq) {
  return std::thread([&running, &cq](){
    print_help();
    std::string line;
    while (running.load() && std::getline(std::cin, line)) {
      std::istringstream iss(line);
      std::string cmd; iss >> cmd;
      if (cmd.empty()) continue;

      Command c;
      if      (cmd == "help") { c.type = Command::Type::Help; }
      else if (cmd == "show") { c.type = Command::Type::Show; if (!(iss >> c.a)) c.a = -1; }
      else if (cmd == "bpm")  { c.type = Command::Type::SetBpm; iss >> c.a; }
      else if (cmd == "div")  { c.type = Command::Type::SetPatDiv; iss >> c.a >> c.b; }
      else if (cmd == "len")  { c.type = Command::Type::SetPatLen; iss >> c.a >> c.b; }
      else if (cmd == "idx")  { c.type = Command::Type::SetStepIdx; iss >> c.a >> c.b >> c.c; }
      else if (cmd == "vel")  { c.type = Command::Type::SetStepVel; iss >> c.a >> c.b >> c.c; }
      else if (cmd == "gate") { c.type = Command::Type::SetStepGate; iss >> c.a >> c.b >> c.c; }
      else if (cmd == "oct")  { c.type = Command::Type::SetStepOct; iss >> c.a >> c.b >> c.c; }
      else if (cmd == "prob") { c.type = Command::Type::SetStepProb; iss >> c.a >> c.b >> c.c; }
      else if (cmd == "on")   { c.type = Command::Type::ToggleStep; iss >> c.a >> c.b; c.c = 1; }
      else if (cmd == "off")  { c.type = Command::Type::ToggleStep; iss >> c.a >> c.b; c.c = 0; }
      else if (cmd == "quit" || cmd == "exit") {
        c.type = Command::Type::Quit; cq.push(c); break;
      } else {
        std::cout << "Unknown. Type 'help'.\n";
        continue;
      }
      cq.push(c);
    }
    running.store(false);
  });
}

} // namespace ui
