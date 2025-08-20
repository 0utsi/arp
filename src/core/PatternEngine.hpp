#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <random>        // na PC; na MCU można podmienić na prosty xorshift
#include "ports/Midi.hpp"
#include "ports/Clock.hpp"

namespace core {

/*
 * =========================
 * 1) MODELE / KONFIGURACJA
 * =========================
 */

// Maksymalna długość patternu (kroków) – stały bufor (bez alokacji)
constexpr std::size_t MAX_STEPS     = 64;
// Maks. liczba zaplanowanych NoteOff w kolejce (globalnie)
constexpr std::size_t MAX_PENDING_OFFS = 64;
// Maks. liczba trzymanych nut w akordzie
constexpr std::size_t MAX_HELD_NOTES   = 8;

// Jeden krok patternu – wszystko, czego potrzebujemy na wyjściu
struct Step {
  uint8_t note_index = 0;      // 1..8 => indeks w posortowanym akordzie; 0 => REST (cisza)
  uint8_t velocity   = 100;    // 1..127 (siła uderzenia)
  uint8_t gate_pct   = 50;     // 1..200 (% długości kroku; >100% = dłużej niż krok)
  int8_t  octave     = 0;      // transpozycja w oktawach (np. -1..+3)
  bool    enabled    = true;   // włącz/wyłącz krok
  uint8_t probability= 100;    // 0..100 (% szansy, że krok zagra)
};

// Konfiguracja pojedynczego patternu
struct PatternConfig {
  uint8_t channel  = 1;        // kanał MIDI 1..16
  uint16_t division= 2;        // ile kroków na ćwierćnutę (1=1/4, 2=1/8, 4=1/16)
  std::size_t length = 0;      // ile kroków jest aktywnych w "steps"
  std::array<Step, MAX_STEPS> steps{};  // stały bufor kroków
};

// Konfiguracja globalna silnika
struct EngineConfig {
  double  bpm = 120.0;         // globalne tempo
  uint8_t overlap_ms = 10;     // ile ms nakładki między nutami (tie/legato)
  bool    external_clock = false; // (na przyszłość) czy korzystamy z MIDI Clock
};

/*
 * ======================
 * 2) STAN AKORDU (HELD)
 * ======================
 *
 * Złota zasada: trzymamy posortowany rosnąco bufor 8 nut.
 * Indeksowanie 1..8 to po prostu "pozycja+1" w tej tablicy.
 */
class ChordState {
public:
  // NoteOn – dodaj nutę do posortowanego bufora (jeśli nie ma duplikatu)
  void note_on(uint8_t note) {
    // duplikat? -> nic nie rób
    for (std::size_t i = 0; i < size_; ++i)
      if (notes_[i] == note) return;

    if (size_ < notes_.size()) {
      // wstaw tak, aby była zachowana kolejność rosnąca
      std::size_t pos = 0;
      while (pos < size_ && notes_[pos] < note) ++pos;
      for (std::size_t j = size_; j > pos; --j) notes_[j] = notes_[j-1];
      notes_[pos] = note;
      ++size_;
    }
  }

  // NoteOff – usuń nutę
  void note_off(uint8_t note) {
    for (std::size_t i = 0; i < size_; ++i) {
      if (notes_[i] == note) {
        for (std::size_t j = i + 1; j < size_; ++j) notes_[j-1] = notes_[j];
        --size_;
        break;
      }
    }
  }

  // Zwróć MIDI note wg indeksu 1..8 (spec: "indeksowanie nut")
  std::optional<uint8_t> by_index(uint8_t idx_1based) const {
    if (idx_1based == 0) return std::nullopt;
    const std::size_t i = static_cast<std::size_t>(idx_1based - 1);
    if (i < size_) return notes_[i];
    return std::nullopt; // indeks wskazuje "pusty slot"
  }

  std::size_t size() const { return size_; }
  void clear() { size_ = 0; }

private:
  std::array<uint8_t, MAX_HELD_NOTES> notes_{};
  std::size_t size_{0};
};

/*
 * ==========================
 * 3) STAN / PLAYBACK PATTERNU
 * ==========================
 *
 * Każdy pattern ma własny kursor kroku i własny zegar (harmonogram).
 * Dodatkowo przechowujemy ostatnią zagraną nutę, by zrobić overlap (tie).
 */
struct PatternState {
  // runtime
  std::size_t step_pos = 0;           // który krok gramy
  uint64_t    next_step_ms = 0;       // kiedy zagrać następny krok
  bool        last_on_valid = false;  // czy jakaś nuta tego patternu aktualnie gra
  uint8_t     last_on_note = 0;       // ostatnio zagrana nuta
  uint8_t     last_on_ch   = 0;       // na jakim kanale ją graliśmy
};

/*
 * ==========================
 * 4) GŁÓWNY ENGINE PATTERNÓW
 * ==========================
 *
 * Zasada działania:
 *  - zbieramy NoteOn/Off z MIDI In i aktualizujemy ChordState,
 *  - w tick() sprawdzamy każdy z 4 patternów: czy pora na krok?
 *    - jeśli tak: bierzemy Step -> mapujemy index->nuta z ChordState,
 *      stosujemy octave/velocity/gate/probability,
 *      wysyłamy NoteOn i planujemy NoteOff (co najmniej gate, a gdy kolejny ON
 *      przyjdzie wcześniej, wydłużymy OFF o "overlap_ms" = brak dziur).
 *  - kolejka OFF-ów to stały bufor, wysyłamy wszystko co "dojrzało".
 */
class PatternEngine {
public:
  static constexpr std::size_t NUM_PATTERNS = 4;

  PatternEngine(ports::IMidiOut& out, const ports::IClock& clock)
    : out_(out), clock_(clock), rng_(0xC0FFEE) {}

  // Konfiguracje (globalna + dla każdego patternu)
  void set_engine_config(const EngineConfig& ec) { eng_ = ec; }
  PatternConfig& pattern(std::size_t i) { return patterns_[i]; }         // konfiguracja
  const PatternConfig& pattern(std::size_t i) const { return patterns_[i]; }
  PatternState& state(std::size_t i) { return states_[i]; }               // stan runtime

  // MIDI IN -> aktualizuj akord
  void on_midi_in(const ports::MidiMsg& m) {
    const uint8_t status = (m.status & 0xF0);
    const uint8_t note   = m.data1;
    const uint8_t vel    = m.data2;
    (void)vel; // w tej wersji velocity wejściowe nie jest używane (krok je nadpisuje)

    if (status == 0x90 && vel > 0) {
      chord_.note_on(note);
    } else if (status == 0x80 || (status == 0x90 && vel == 0)) {
      chord_.note_off(note);
    }
  }

  // Główna pętla czasu – wołaj często (np. co 1 ms)
  void tick() {
    const uint64_t now = clock_.now_ms();

    // 1) wyślij wszystkie NoteOff, które są już „po czasie”
    flush_due_offs_(now);

    // 2) dla każdego patternu, jeśli pora – zrób krok
    for (std::size_t i = 0; i < NUM_PATTERNS; ++i) {
      auto& cfg = patterns_[i];
      auto& st  = states_[i];

      if (cfg.length == 0) continue; // pattern pusty
      if (st.next_step_ms == 0) st.next_step_ms = now; // inicjalizacja

      while (now >= st.next_step_ms) {
        do_pattern_step_(cfg, st, now);
        // policz długość kroku z BPM i division patternu
        const uint64_t step_ms = step_ms_for_(cfg.division);
        st.next_step_ms += step_ms;
      }
    }
  }

private:
  // =============== Pamięć / stan ===============
  EngineConfig eng_{};
  std::array<PatternConfig, NUM_PATTERNS> patterns_{};
  std::array<PatternState,  NUM_PATTERNS> states_{};
  ChordState chord_{};

  struct PendingOff { uint64_t at_ms; uint8_t ch; uint8_t note; };
  std::array<PendingOff, MAX_PENDING_OFFS> off_q_{};
  std::size_t off_count_{0};

  ports::IMidiOut&     out_;
  const ports::IClock& clock_;
  std::mt19937         rng_;  // PC: OK. MCU: wymień na xorshift/LCG

  // =============== Narzędzia ===============

  uint64_t step_ms_for_(uint16_t division) const {
    // ćwierćnuta [ms] = 60000 / BPM
    const double q_ms = 60000.0 / (eng_.bpm > 0 ? eng_.bpm : 120.0);
    // długość kroku = ćwierćnuta / division
    uint64_t ms = static_cast<uint64_t>(q_ms / (division > 0 ? division : 2));
    return ms == 0 ? 1 : ms;
  }

  bool chance_(uint8_t probability_0_100) {
    if (probability_0_100 >= 100) return true;
    if (probability_0_100 == 0)   return false;
    std::uniform_int_distribution<int> d(1,100);
    return d(rng_) <= probability_0_100;
  }

  // Zaplanuj NoteOff w stałym buforze (bez alokacji)
  void schedule_off_(uint64_t at_ms, uint8_t ch, uint8_t note) {
    if (off_count_ < off_q_.size()) {
      off_q_[off_count_++] = PendingOff{at_ms, ch, note};
    } else {
      // awaryjnie – wyślij od razu (nie gub nut)
      send_off_(ch, note, at_ms);
    }
  }

  // Wydłuż NoteOff ostatniej nuty tego patternu, jeśli istnieje w kolejce
  void extend_last_off_(uint8_t ch, uint8_t note, uint64_t new_time) {
    for (std::size_t i = off_count_; i > 0; --i) {
      auto& p = off_q_[i-1];
      if (p.ch == ch && p.note == note) {
        if (p.at_ms < new_time) p.at_ms = new_time;
        return;
      }
    }
  }

  // Wyślij i przepchnij kolejkę OFF-ów
  void flush_due_offs_(uint64_t now) {
    std::size_t w = 0;
    for (std::size_t r = 0; r < off_count_; ++r) {
      const auto& p = off_q_[r];
      if (p.at_ms <= now) {
        send_off_(p.ch, p.note, now);
      } else {
        off_q_[w++] = p;
      }
    }
    off_count_ = w;
  }

  // Realny „krok” patternu
  void do_pattern_step_(const PatternConfig& cfg, PatternState& st, uint64_t now) {
    const Step& s = cfg.steps[st.step_pos % cfg.length];
    st.step_pos = (st.step_pos + 1) % cfg.length;

    if (!s.enabled) return;
    if (!chance_(s.probability)) return;

    // Mapowanie indeksu -> nuta MIDI (1..8)
    const auto base_opt = chord_.by_index(s.note_index);
    if (!base_opt.has_value()) {
      // indeks pusty (np. mniejszy akord) – nic nie gramy
      return;
    }

    // Transpozycja o oktawy
    int note_i = static_cast<int>(*base_opt) + 12 * static_cast<int>(s.octave);
    if (note_i < 0)   note_i = 0;
    if (note_i > 127) note_i = 127;
    const uint8_t note = static_cast<uint8_t>(note_i);

    // Kanał i czasy
    const uint8_t ch = static_cast<uint8_t>((cfg.channel - 1) & 0x0F);
    const uint64_t step_ms = step_ms_for_(cfg.division);
    const uint64_t gate_ms = std::max<uint64_t>(1, step_ms * (s.gate_pct < 1 ? 1 : s.gate_pct) / 100);

    // Legato/overlap – żeby nie było dziur:
    // - minimalnie trzymaj nutę 'gate_ms'
    // - gdy zaraz zagramy następną nutę, OFF wydłużamy do (teraz + overlap)
    const uint64_t on_at  = now;
    const uint64_t min_off= on_at + gate_ms;
    const uint64_t off_at = min_off + static_cast<uint64_t>(eng_.overlap_ms);

    // Jeśli poprzednia nuta tego patternu gra – wydłuż jej OFF do "teraz + overlap"
    if (st.last_on_valid) {
      extend_last_off_(st.last_on_ch, st.last_on_note, on_at + static_cast<uint64_t>(eng_.overlap_ms));
    }

    // Wyślij ON i zaplanuj OFF
    send_on_(ch, note, s.velocity, on_at);
    schedule_off_(off_at, ch, note);

    st.last_on_valid = true;
    st.last_on_ch    = ch;
    st.last_on_note  = note;
  }

  // MIDI wyjście
  void send_on_(uint8_t ch, uint8_t note, uint8_t vel, uint64_t t) {
    ports::MidiMsg m{ static_cast<uint8_t>(0x90 | ch), note, vel, t };
    out_.send(m);
  }
  void send_off_(uint8_t ch, uint8_t note, uint64_t t) {
    ports::MidiMsg m{ static_cast<uint8_t>(0x80 | ch), note, 0, t };
    out_.send(m);
  }
};

} // namespace core
