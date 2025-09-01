#pragma once
#include <cstdint>
#include "ports/Clock.hpp"

// Prosty zegar w ms kontrolowany przez AudioProcessor:
// - set_ms(0) w prepareToPlay()
// - advance_ms(1) w pętli processBlock co 1 ms "odrobionego" czasu
class HostClock final : public ports::IClock
{
public:
  uint64_t now_ms() const override { return (uint64_t) currentMs_; }

  void set_ms (double ms)    { currentMs_ = ms; }
  void advance_ms (double d) { currentMs_ += d; }

private:
  double currentMs_ { 0.0 };
};
