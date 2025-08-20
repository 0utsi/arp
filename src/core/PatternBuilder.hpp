#pragma once
#include <array>
#include <cstdint>
#include <initializer_list>
#include <algorithm>                // std::clamp
#include "core/PatternEngine.hpp"   // Step, PatternConfig, MAX_STEPS

namespace core {

// Ułatwia składanie patternów czytelnie (bez ręcznego ustawiania pól).
class PatternBuilder {
public:
  explicit PatternBuilder(PatternConfig& cfg) : cfg_(cfg) {}

  // Wyczyść kroki (kanał/division zostają)
  PatternBuilder& clear() {
    cfg_.length = 0;
    for (auto& s : cfg_.steps) s = Step{};
    editing_ = 0;
    edit_all_ = false;
    return *this;
  }

  // Dodaj kroki według indeksów nut (1..8, 0 = REST)
  PatternBuilder& indices(std::initializer_list<int> idxs) {
    for (int idx : idxs) {
      if (cfg_.length >= cfg_.steps.size()) break;
      Step s{};
      s.note_index = static_cast<uint8_t>(std::clamp(idx, 0, 8));
      cfg_.steps[cfg_.length++] = s;
    }
    editing_ = (cfg_.length ? cfg_.length - 1 : 0);
    return *this;
  }

  // Rozpocznij edycję bieżącego kroku (gdy brak – tworzy pierwszy).
  PatternBuilder& step() {
    ensure_slot_();
    editing_ = cfg_.length - 1;
    return *this;
  }

  // Przejdź do następnego kroku (tworzy pusty jeśli jest miejsce).
  PatternBuilder& next() {
    ensure_slot_();
    return *this;
  }

  // Masowa edycja wszystkich istniejących kroków
  PatternBuilder& each() { edit_all_ = true; return *this; }
  PatternBuilder& done() { edit_all_ = false; return *this; }

  // Settery pojedynczego/powielonego zakresu
  PatternBuilder& idx (int v){return set_([&](Step& s){s.note_index=static_cast<uint8_t>(std::clamp(v,0,8));});}
  PatternBuilder& vel (int v){return set_([&](Step& s){s.velocity  =static_cast<uint8_t>(std::clamp(v,1,127));});}
  PatternBuilder& gate(int v){return set_([&](Step& s){s.gate_pct  =static_cast<uint8_t>(std::clamp(v,1,200));});}
  PatternBuilder& oct (int v){return set_([&](Step& s){s.octave    =static_cast<int8_t>(std::clamp(v,-8, 8));});}
  PatternBuilder& prob(int v){return set_([&](Step& s){s.probability=static_cast<uint8_t>(std::clamp(v,0,100));});}
  PatternBuilder& on()       {return set_([&](Step& s){s.enabled=true;});}
  PatternBuilder& off()      {return set_([&](Step& s){s.enabled=false;});}

  // Szybkie powtórzenie ostatniego kroku n razy
  PatternBuilder& repeat(std::size_t n) {
    if (cfg_.length == 0) return *this;
    const Step last = cfg_.steps[cfg_.length - 1];
    while (n-- && cfg_.length < cfg_.steps.size()) cfg_.steps[cfg_.length++] = last;
    editing_ = cfg_.length ? cfg_.length - 1 : 0;
    return *this;
  }

private:
  PatternConfig& cfg_;
  std::size_t editing_ = 0;
  bool edit_all_ = false;

  void ensure_slot_() {
    if (cfg_.length == 0) {
      cfg_.steps[0] = Step{}; cfg_.length = 1;
    } else if (editing_ == cfg_.length - 1 && cfg_.length < cfg_.steps.size()) {
      cfg_.steps[cfg_.length] = Step{}; cfg_.length++;
    }
  }

  template<class F>
  PatternBuilder& set_(F&& fn) {
    if (cfg_.length == 0) ensure_slot_();
    if (edit_all_) {
      for (std::size_t i = 0; i < cfg_.length; ++i) fn(cfg_.steps[i]);
    } else {
      fn(cfg_.steps[editing_]);
    }
    return *this;
  }
};

} // namespace core
