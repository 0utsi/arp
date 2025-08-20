#pragma once
#include <memory>
#include "ports/Midi.hpp"
#include "ports/Clock.hpp"

namespace desktop_midi {
  std::unique_ptr<ports::IMidiIn>  makeIn (const ports::IClock& clk);
  std::unique_ptr<ports::IMidiOut> makeOut();
}
