#pragma once
#include <array>      // stałe bufory (bez alokacji)
#include <cstdint>
#include <optional>
#include "ports/Midi.hpp"
#include "ports/Clock.hpp"

namespace core {

/**
 * ArpConfig — parametry pracy silnika.
 * Utrzymujemy minimalny, czytelny zestaw pod prototyp PC i łatwy port na MCU.
 */
struct ArpConfig {
  double  bpm = 120.0;          // tempo (uderzenia na minutę)
  int     division = 2;         // ile kroków na ćwierćnutę: 1=1/4, 2=1/8, 4=1/16
  uint8_t channel = 1;          // kanał MIDI (1..16)

  // Legato / ciągłość:
  int     gate_percent = 100;   // ile % długości kroku trzymać nutę (100 => tyle co krok)
  int     overlap_ms   = 12;    // dodatkowe ms nakładki nowej nuty na starą (tie)

  // Wspinaczka oktawowa (żeby single-note NIE był tremolo):
  int     octave_min = 0;       // od której oktawy startować (0 = bazowa)
  int     octave_max = 2;       // do której oktawy iść (2 => +24 półtonów)
  int     octave_step_every = 2; // co ile kroków zwiększyć oktawę o +12 (2 = co dwa kroki)
};

/**
 * ArpEngine — minimalny, deterministyczny arpeggiator:
 * - limit 8 nut,
 * - krokowy harmonogram,
 * - planowanie NoteOff tak, by NIGDY nie było „dziur”.
 */
class ArpEngine {
public:
  ArpEngine(ports::IMidiOut& out, const ports::IClock& clock)
    : out_(out), clock_(clock) { recalc_timing_(); }

  void set_config(const ArpConfig& c) { cfg_ = c; recalc_timing_(); }

  // Wejście MIDI z klawiatury: NoteOn/NoteOff
  void on_midi_in(const ports::MidiMsg& m) {
    const uint8_t status = (m.status & 0xF0);
    const uint8_t note   = m.data1;
    const uint8_t vel    = m.data2;

    if (status == 0x90 && vel > 0) {
      add_note_(note);
    } else if (status == 0x80 || (status == 0x90 && vel == 0)) {
      remove_note_(note);
    }
  }

  // Wywołuj co ~1 ms (pętla główna)
  void tick() {
    const uint64_t now = clock_.now_ms();

    // 1) Wyślij wszystkie NoteOff, na które przyszła pora
    flush_offs_(now);

    // 2) Jeśli to pierwsze uruchomienie, ustaw start czasu kroku
    if (!next_step_ms_) next_step_ms_ = now;

    // 3) Jeżeli przyszedł moment kroku — zrób krok
    while (next_step_ms_ && now >= *next_step_ms_) {
      do_step_(*next_step_ms_);
      *next_step_ms_ += step_ms_;  // nast. krok za step_ms_
    }
  }

private:
  // ──────────────────────────────────────────────────────────────────────────
  // Dane „muzyczne”

  std::array<uint8_t, 8> held_{};   // trzymane nuty (posortowane rosnąco)
  std::size_t held_size_{0};        // realna liczba trzymanych
  std::size_t note_cursor_{0};      // indeks po „held_” (arp "up")
  uint64_t    step_index_{0};       // licznik kroków (do swing/oktaw itd.)

  // ──────────────────────────────────────────────────────────────────────────
  // Harmonogram i czas

  uint64_t step_ms_{250};                 // czas jednego kroku (ms)
  std::optional<uint64_t> next_step_ms_{};// kiedy zagrać kolejny krok (ms)
  uint64_t gate_ms_{250};                 // ile ms trzymać nutę (min. długość)
  
  struct PendingOff {
    uint64_t t_ms;
    uint8_t  ch;
    uint8_t  note;
  };
  // Stały bufor „offów”, żeby nie alokować (16 zaplanowanych OFF w zupełności starczy)
  std::array<PendingOff, 16> off_buf_{};
  std::size_t off_count_{0};

  // Ostatnia aktywna nuta (żeby zaplanować OFF z tie)
  bool    last_on_valid_{false};
  uint8_t last_on_ch_{0};
  uint8_t last_on_note_{0};

  // Zależności
  ports::IMidiOut&     out_;
  const ports::IClock& clock_;
  ArpConfig            cfg_{};

  // ──────────────────────────────────────────────────────────────────────────
  // Krok muzyczny: wybór nuty, obliczenie oktawy i planowanie ON/OFF

  void do_step_(uint64_t t0_ms) {
    if (held_size_ == 0) { step_index_++; return; }

    // 1) Wybór bazowej nuty: jeśli jest akord → idziemy "up", jeśli jedna nuta → też bierzemy ją
    const uint8_t base = held_[note_cursor_ % held_size_];
    note_cursor_ = (note_cursor_ + 1) % (held_size_ > 0 ? held_size_ : 1);

    // 2) Wspinaczka oktawowa (żeby single-note nie był tremolo)
    const int span = (cfg_.octave_max >= cfg_.octave_min) 
                   ? (cfg_.octave_max - cfg_.octave_min + 1) : 1;
    const int every = (cfg_.octave_step_every > 0) ? cfg_.octave_step_every : 1;
    const int climb = static_cast<int>(step_index_ / every) % span;
    const int octave = cfg_.octave_min + climb;

    int note_i = static_cast<int>(base) + 12 * octave;
    if (note_i < 0)   note_i = 0;
    if (note_i > 127) note_i = 127;
    const uint8_t note = static_cast<uint8_t>(note_i);

    // 3) Kanał/velocity + czasy ON/OFF tak, by NIE było dziur
    const uint8_t ch   = static_cast<uint8_t>((cfg_.channel - 1) & 0x0F);
    const uint64_t on_at  = t0_ms;
    // minimalny gate: tyle co krok (gate_ms_) lub dłużej
    const uint64_t min_off = t0_ms + gate_ms_;
    // dodatkowa nakładka (tie) — nowa nuta pojawia się, a stara gaśnie dopiero po overlap_ms
    const uint64_t off_at  = min_off + static_cast<uint64_t>( (cfg_.overlap_ms > 0) ? cfg_.overlap_ms : 0 );

    // 4) Legato z poprzednią nutą: jeśli gra coś aktualnie, przesuń jej OFF co najmniej na "teraz + overlap"
    if (last_on_valid_) {
      // Spróbuj wydłużyć ostatni OFF w buforze (jeśli dotyczy tej samej nuty)
      extend_off_for_last_on_(on_at + static_cast<uint64_t>( (cfg_.overlap_ms > 0) ? cfg_.overlap_ms : 0 ));
    }

    // 5) Wyślij ON i zaplanuj OFF dla bieżącej nuty
    send_on_(ch, note, /*vel*/100, on_at);
    schedule_off_(off_at, ch, note);

    // 6) Zapamiętaj, co teraz gra
    last_on_valid_ = true;
    last_on_ch_    = ch;
    last_on_note_  = note;

    step_index_++;
  }

  // ──────────────────────────────────────────────────────────────────────────
  // Obsługa OFF-ów bez alokacji

  void schedule_off_(uint64_t t, uint8_t ch, uint8_t note) {
    if (off_count_ < off_buf_.size()) {
      off_buf_[off_count_++] = PendingOff{t, ch, note};
    } else {
      // bufor pełny — w prototypie po prostu wyślij od razu (bezpiecznik)
      send_off_(ch, note, t);
    }
  }

  void extend_off_for_last_on_(uint64_t new_time) {
    // Szukamy ostatniego wpisu OFF odpowiadającego last_on_note_/ch
    for (std::size_t i = off_count_; i > 0; --i) {
      auto& p = off_buf_[i-1];
      if (p.ch == last_on_ch_ && p.note == last_on_note_) {
        if (p.t_ms < new_time) p.t_ms = new_time;
        return;
      }
    }
    // Jeśli nie znaleźliśmy (np. nie było jeszcze OFF zaplanowanego) — nic nie robimy; schedule_off_ zrobi swoje.
  }

  void flush_offs_(uint64_t now) {
    // wysyłamy wszystkie OFF z terminem <= now
    std::size_t w = 0;
    for (std::size_t r = 0; r < off_count_; ++r) {
      const auto& p = off_buf_[r];
      if (p.t_ms <= now) {
        send_off_(p.ch, p.note, now);
      } else {
        off_buf_[w++] = p;
      }
    }
    off_count_ = w;
    if (off_count_ == 0) last_on_valid_ = false; // nic już nie gra
  }

  // ──────────────────────────────────────────────────────────────────────────
  // MIDI helpers

  void send_on_(uint8_t ch, uint8_t note, uint8_t vel, uint64_t t) {
    ports::MidiMsg m{ static_cast<uint8_t>(0x90 | ch), note, vel, t };
    out_.send(m);
  }
  void send_off_(uint8_t ch, uint8_t note, uint64_t t) {
    ports::MidiMsg m{ static_cast<uint8_t>(0x80 | ch), note, 0, t };
    out_.send(m);
  }

  // ──────────────────────────────────────────────────────────────────────────
  // Held-notes: stały bufor 8 elementów, posortowany rosnąco (dla „up”)

  void add_note_(uint8_t n) {
    // duplikatu nie dodajemy
    for (std::size_t i = 0; i < held_size_; ++i) if (held_[i] == n) return;

    if (held_size_ < held_.size()) {
      // wstaw w porządku rosnącym
      std::size_t pos = 0;
      while (pos < held_size_ && held_[pos] < n) ++pos;
      for (std::size_t j = held_size_; j > pos; --j) held_[j] = held_[j-1];
      held_[pos] = n;
      ++held_size_;
      if (note_cursor_ >= held_size_) note_cursor_ = 0;
    }
    // jeśli pełne — ignorujemy (limit 8)
  }

  void remove_note_(uint8_t n) {
    for (std::size_t i = 0; i < held_size_; ++i) {
      if (held_[i] == n) {
        for (std::size_t j = i + 1; j < held_size_; ++j) held_[j-1] = held_[j];
        --held_size_;
        if (note_cursor_ >= held_size_) note_cursor_ = 0;
        break;
      }
    }
    // Jeśli puściliśmy wszystko — natychmiastowy OFF ostatniej nuty
    if (held_size_ == 0 && last_on_valid_) {
      const uint64_t now = clock_.now_ms();
      schedule_off_(now, last_on_ch_, last_on_note_);
      last_on_valid_ = false;
    }
  }

  // ──────────────────────────────────────────────────────────────────────────
  // Przeliczenia czasu

  void recalc_timing_() {
    const double q_ms = 60000.0 / (cfg_.bpm > 0 ? cfg_.bpm : 120.0);           // ćwierćnuta
    step_ms_ = static_cast<uint64_t>( q_ms / (cfg_.division > 0 ? cfg_.division : 2) ); // długość kroku
    if (step_ms_ == 0) step_ms_ = 1;

    // Gate liczony od kroku; NoteOff i tak jest „co najmniej” gate_ms + overlap
    const int gp = (cfg_.gate_percent < 1 ? 1 : (cfg_.gate_percent > 200 ? 200 : cfg_.gate_percent));
    gate_ms_ = static_cast<uint64_t>( step_ms_ * gp / 100 );
    if (gate_ms_ < 1) gate_ms_ = 1;
  }
};

} // namespace core
